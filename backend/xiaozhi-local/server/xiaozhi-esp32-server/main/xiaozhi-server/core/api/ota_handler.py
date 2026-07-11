import json
import time
import base64
import hashlib
import hmac
import os
import re
import glob
from typing import Dict, List, Tuple
from aiohttp import web

from core.device_admission import DeviceAdmission
from core.ota_contract import (
    build_firmware_download_url,
    choose_firmware_update,
    parse_device_report,
    resolve_websocket_url,
)
from core.utils.connection_log import sanitize_headers
from core.utils.util import get_local_ip, get_vision_url
from core.api.base_handler import BaseHandler

TAG = __name__


def _safe_basename(filename: str) -> str:
    # Prevent directory traversal
    return os.path.basename(filename)


class OTAHandler(BaseHandler):
    def __init__(self, config: dict, device_admission: DeviceAdmission = None):
        super().__init__(config)
        self.device_admission = device_admission or DeviceAdmission(config)

        # firmware storage
        self.bin_dir = os.path.join(os.getcwd(), "data", "bin")
        # cache structure: { 'updated_at': timestamp, 'ttl': seconds, 'files_by_model': { model: [(version, filename), ...] } }
        self._bin_cache: Dict = {
            "updated_at": 0,
            "ttl": config.get("firmware_cache_ttl", 30),
            "files_by_model": {},
        }

    def verify_activation_code(self, code: str) -> str:
        return self.device_admission.verify_activation_code(code)

    def _refresh_bin_cache_if_needed(self):
        now = int(time.time())
        ttl = int(self._bin_cache.get("ttl", 30))
        if now - int(
            self._bin_cache.get("updated_at", 0)
        ) < ttl and self._bin_cache.get("files_by_model"):
            return

        files_by_model: Dict[str, List[Tuple[str, str]]] = {}
        try:
            if not os.path.isdir(self.bin_dir):
                os.makedirs(self.bin_dir, exist_ok=True)

            # match files like model_1.2.3.bin (allow dots, dashes, underscores in model and version)
            pattern = os.path.join(self.bin_dir, "*.bin")
            for path in glob.glob(pattern):
                fname = os.path.basename(path)
                # filename format: {model}_{version}.bin
                m = re.match(r"^(.+?)_([0-9][A-Za-z0-9\.\-_]*)\.bin$", fname)
                if not m:
                    # skip files not conforming to naming rule
                    continue
                model = m.group(1)
                version = m.group(2)
                files_by_model.setdefault(model, []).append((version, fname))

            self._bin_cache["files_by_model"] = files_by_model
            self._bin_cache["updated_at"] = now
            self.logger.bind(tag=TAG).info(
                f"Firmware cache refreshed: {len(files_by_model)} models"
            )
        except Exception as e:
            self.logger.bind(tag=TAG).error(f"刷新固件缓存失败: {e}")
            # keep previous cache if any

    def generate_password_signature(self, content: str, secret_key: str) -> str:
        """生成MQTT密码签名

        Args:
            content: 签名内容 (clientId + '|' + username)
            secret_key: 密钥

        Returns:
            str: Base64编码的HMAC-SHA256签名
        """
        try:
            hmac_obj = hmac.new(
                secret_key.encode("utf-8"), content.encode("utf-8"), hashlib.sha256
            )
            signature = hmac_obj.digest()
            return base64.b64encode(signature).decode("utf-8")
        except Exception as e:
            self.logger.bind(tag=TAG).error(f"生成MQTT密码签名失败: {e}")
            return ""

    def _get_websocket_url(self, local_ip: str, port: int) -> str:
        """获取websocket地址

        Args:
            local_ip: 本地IP地址
            port: 端口号

        Returns:
            str: websocket地址
        """
        server_config = self.config["server"]
        websocket_config = server_config.get("websocket", "")
        return resolve_websocket_url(websocket_config, local_ip, port)

    async def handle_post(self, request):
        """处理 OTA POST 请求

        This handler will:
        - read device id/client id (as before)
        - attempt to determine device model and current firmware version (prefer headers, fallback to body)
        - check data/bin for newer firmware for that model
        - if found a newer firmware, set firmware.url to the download endpoint
        """
        try:
            data = await request.text()
            self.logger.bind(tag=TAG).debug(f"OTA请求方法: {request.method}")
            self.logger.bind(tag=TAG).debug(
                f"OTA请求头: {sanitize_headers(request.headers)}"
            )
            self.logger.bind(tag=TAG).debug(f"OTA请求数据: {data}")

            data_json = {}
            try:
                data_json = json.loads(data) if data else {}
            except Exception:
                data_json = {}

            device_report = parse_device_report(dict(request.headers), data_json)
            device_id = device_report.device_id
            client_id = device_report.client_id
            device_model = device_report.model
            device_version = device_report.version
            self.logger.bind(tag=TAG).info(f"OTA请求设备ID: {device_id}")
            self.logger.bind(tag=TAG).info(f"OTA请求ClientID: {client_id}")

            server_config = self.config["server"]
            # websocket_port is used to construct websocket URL (server["port"])
            websocket_port = int(server_config.get("port", 8000))
            local_ip = get_local_ip()

            return_json = {
                "server_time": {
                    "timestamp": int(round(time.time() * 1000)),
                    "timezone_offset": server_config.get("timezone_offset", 8) * 60,
                },
                "firmware": {
                    "version": device_version,
                    "url": "",
                },
            }

            # existing mqtt/websocket logic (unchanged)
            mqtt_gateway_endpoint = server_config.get("mqtt_gateway")

            if mqtt_gateway_endpoint:  # 如果配置了非空字符串
                # 尝试从请求数据中获取设备型号（已解析 above）
                try:
                    group_id = f"GID_{device_model}".replace(":", "_").replace(" ", "_")
                except Exception as e:
                    self.logger.bind(tag=TAG).error(f"获取设备型号失败: {e}")
                    group_id = "GID_default"

                mac_address_safe = device_id.replace(":", "_")
                mqtt_client_id = f"{group_id}@@@{mac_address_safe}@@@{mac_address_safe}"

                # 构建用户数据
                user_data = {"ip": "unknown"}
                try:
                    user_data_json = json.dumps(user_data)
                    username = base64.b64encode(user_data_json.encode("utf-8")).decode(
                        "utf-8"
                    )
                except Exception as e:
                    self.logger.bind(tag=TAG).error(f"生成用户名失败: {e}")
                    username = ""

                # 生成密码
                password = ""
                signature_key = server_config.get("mqtt_signature_key", "")
                if signature_key:
                    password = self.generate_password_signature(
                        mqtt_client_id + "|" + username, signature_key
                    )
                    if not password:
                        password = ""  # 签名失败则留空，由设备决定是否允许无密码
                else:
                    self.logger.bind(tag=TAG).warning("缺少MQTT签名密钥，密码留空")

                # 构建MQTT配置（直接使用 mqtt_gateway 字符串）
                return_json["mqtt"] = {
                    "endpoint": mqtt_gateway_endpoint,
                    "client_id": mqtt_client_id,
                    "username": username,
                    "password": password,
                    "publish_topic": "device-server",
                    "subscribe_topic": f"devices/p2p/{mac_address_safe}",
                }
                self.logger.bind(tag=TAG).info(f"为设备 {device_id} 下发MQTT网关配置")

            else:  # 未配置 mqtt_gateway，下发 WebSocket
                provision = self.device_admission.provision_websocket(
                    device_id=device_id,
                    client_id=client_id,
                    activation_host=request.host,
                )
                if provision.activation:
                    return_json["activation"] = provision.activation
                    self.logger.bind(tag=TAG).info(
                        f"Device {device_id} not allowed. Generated Code: {provision.activation.get('code', '')}"
                    )
                # NOTE: use websocket_port here
                return_json["websocket"] = {
                    "url": self._get_websocket_url(local_ip, websocket_port),
                    "token": provision.token,
                }
                self.logger.bind(tag=TAG).info(
                    f"未配置MQTT网关，为设备 {device_id} 下发WebSocket配置"
                )

            # Now check firmware files for updates
            try:
                self._refresh_bin_cache_if_needed()
                files_by_model = self._bin_cache.get("files_by_model", {})
                candidates = files_by_model.get(device_model, [])

                self.logger.bind(tag=TAG).info(
                    f"查找型号 {device_model} 的固件，找到 {len(candidates)} 个候选"
                )

                update = choose_firmware_update(device_version, candidates)

                if update:
                    chosen_version, filename = update
                    chosen_url = build_firmware_download_url(
                        get_vision_url(self.config), filename
                    )
                    return_json["firmware"]["version"] = chosen_version
                    return_json["firmware"]["url"] = chosen_url
                    self.logger.bind(tag=TAG).info(
                        f"为设备 {device_id} 下发固件 {chosen_version} [如果地址前缀有误，请检查配置文件中的server.vision_explain]-> {chosen_url} "
                    )
                else:
                    self.logger.bind(tag=TAG).info(
                        f"设备 {device_id} 固件已是最新: {device_version}"
                    )

            except Exception as e:
                self.logger.bind(tag=TAG).error(f"检查固件版本时出错: {e}")

            response = web.Response(
                text=json.dumps(return_json, separators=(",", ":")),
                content_type="application/json",
            )
        except Exception as e:
            self.logger.bind(tag=TAG).error(f"OTA POST处理异常: {e}")
            return_json = {"success": False, "message": "request error."}
            response = web.Response(
                text=json.dumps(return_json, separators=(",", ":")),
                content_type="application/json",
            )
        finally:
            self._add_cors_headers(response)
            return response

    async def handle_get(self, request):
        """处理 OTA GET 请求"""
        try:
            server_config = self.config["server"]
            local_ip = get_local_ip()
            # use websocket port for websocket URL
            websocket_port = int(server_config.get("port", 8000))
            websocket_url = self._get_websocket_url(local_ip, websocket_port)
            message = f"OTA接口运行正常，向设备发送的websocket地址是：{websocket_url}"
            response = web.Response(text=message, content_type="text/plain")
        except Exception as e:
            self.logger.bind(tag=TAG).error(f"OTA GET请求异常: {e}")
            response = web.Response(text="OTA接口异常", content_type="text/plain")
        finally:
            self._add_cors_headers(response)
            return response

    async def handle_download(self, request):
        """
        下载固件接口
        URL: /xiaozhi/ota/download/{filename}
        - 只允许下载 data/bin 目录下的 .bin 文件
        - filename 必须是 basename 且匹配安全的模式
        """
        try:
            fname = request.match_info.get("filename", "")
            if not fname:
                raise web.HTTPBadRequest(text="filename required")

            # sanitize
            fname = _safe_basename(fname)
            # pattern: allow letters, numbers, dot, underscore, dash
            if not re.match(r"^[A-Za-z0-9\.\-_]+\.bin$", fname):
                raise web.HTTPBadRequest(text="invalid filename")

            file_path = os.path.join(self.bin_dir, fname)
            # ensure realpath is under bin_dir
            file_real = os.path.realpath(file_path)
            bin_dir_real = os.path.realpath(self.bin_dir)
            if (
                not file_real.startswith(bin_dir_real + os.sep)
                and file_real != bin_dir_real
            ):
                raise web.HTTPForbidden(text="forbidden")

            if not os.path.isfile(file_real):
                raise web.HTTPNotFound(text="file not found")

            # use FileResponse to stream file
            resp = web.FileResponse(path=file_real)
        except web.HTTPError as e:
            resp = e
        except Exception as e:
            self.logger.bind(tag=TAG).error(f"固件下载异常: {e}")
            resp = web.Response(text="download error", status=500)
        finally:
            try:
                self._add_cors_headers(resp)
            except Exception:
                pass
            return resp
