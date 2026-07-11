import json
import os
from aiohttp import web
from core.api.base_handler import BaseHandler
from core.device_admission import DeviceAdmission

class DeviceAdminHandler(BaseHandler):
    def __init__(self, config: dict, device_admission: DeviceAdmission):
        super().__init__(config)
        self.device_admission = device_admission
        self.config_path = os.path.join(os.getcwd(), "config.yaml") # Assuming default config path

    async def handle_get(self, request):
        """Serve the simple admin page."""
        html = """
        <!DOCTYPE html>
        <html>
        <head>
            <title>Xiaozhi Device Binding</title>
            <style>
                body { font-family: sans-serif; display: flex; justify-content: center; align-items: center; height: 100vh; background-color: #f0f2f5; }
                .card { background: white; padding: 2rem; border-radius: 8px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); width: 300px; text-align: center; }
                input { font-size: 1.5rem; text-align: center; width: 100%; padding: 10px; margin: 20px 0; border: 1px solid #ccc; border-radius: 4px; letter-spacing: 5px;}
                button { background: #1890ff; color: white; border: none; padding: 10px 20px; border-radius: 4px; font-size: 1rem; cursor: pointer; width: 100%; }
                button:hover { background: #40a9ff; }
                .success { color: green; display: none; margin-top: 10px; }
                .error { color: red; display: none; margin-top: 10px; }
            </style>
        </head>
        <body>
            <div class="card">
                <h2>Bind Device</h2>
                <p>Enter the 6-digit code shown on your device screen.</p>
                <input type="text" id="code" maxlength="6" placeholder="000000">
                <button onclick="bind()">Activate</button>
                <div id="msg_success" class="success">Binding Successful! Device will reconnect automatically.</div>
                <div id="msg_error" class="error">Invalid or Expired Code</div>
            </div>
            <script>
                async function bind() {
                    const code = document.getElementById('code').value;
                    const btn = document.querySelector('button');
                    const successDiv = document.getElementById('msg_success');
                    const errorDiv = document.getElementById('msg_error');

                    btn.disabled = true;
                    successDiv.style.display = 'none';
                    errorDiv.style.display = 'none';

                    try {
                        const response = await fetch('/xiaozhi/admin/bind', {
                            method: 'POST',
                            headers: {'Content-Type': 'application/json'},
                            body: JSON.stringify({code: code})
                        });
                        const data = await response.json();

                        if (data.success) {
                            successDiv.style.display = 'block';
                            successDiv.textContent = "Success! MAC: " + data.mac;
                        } else {
                            errorDiv.style.display = 'block';
                            errorDiv.textContent = data.message || "Error";
                            btn.disabled = false;
                        }
                    } catch (e) {
                        errorDiv.style.display = 'block';
                        errorDiv.textContent = "Network Error";
                        btn.disabled = false;
                    }
                }
            </script>
        </body>
        </html>
        """
        return web.Response(text=html, content_type="text/html")

    async def handle_bind(self, request):
        """Handle the binding API request."""
        try:
            data = await request.json()
            code = data.get("code")
            if not code:
                 return web.json_response({"success": False, "message": "Code is required"})

            # 1. Verify Code with DeviceAdmission
            mac = self.device_admission.verify_activation_code(code)
            if not mac:
                return web.json_response({"success": False, "message": "Invalid or Expired Code"})

            # 2. Add to Whitelist (Update Config File or Memory)
            self._add_to_whitelist(mac)

            return web.json_response({"success": True, "mac": mac})
        except Exception as e:
            return web.json_response({"success": False, "message": str(e)})

    def _add_to_whitelist(self, mac: str):
        """Add MAC to allowed_devices in config.yaml."""
        import yaml

        # 1. Update In-Memory Config (Immediate Effect)
        auth_config = self.config["server"].get("auth", {})
        allowed = set(auth_config.get("allowed_devices", []))
        allowed.add(mac)
        auth_config["allowed_devices"] = list(allowed)
        self.config["server"]["auth"] = auth_config
        self.device_admission.allow_device(mac)

        # 2. Persist to File (Permanent Effect)
        # Note: In a real production app, use a DB. Here we edit yaml directly for simplicity.
        try:
            with open(self.config_path, 'r', encoding='utf-8') as f:
                yaml_config = yaml.safe_load(f)

            server_conf = yaml_config.get("server", {})
            auth_conf = server_conf.get("auth", {})

            current_list = auth_conf.get("allowed_devices", [])
            if mac not in current_list:
                current_list.append(mac)
                auth_conf["allowed_devices"] = current_list
                server_conf["auth"] = auth_conf
                yaml_config["server"] = server_conf

                with open(self.config_path, 'w', encoding='utf-8') as f:
                    yaml.dump(yaml_config, f, allow_unicode=True)
        except Exception as e:
            print(f"Failed to persist config: {e}")
