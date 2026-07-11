import copy


SENSITIVE_HEADER_NAMES = (
    "authorization",
    "proxy-authorization",
    "x-api-key",
    "api-key",
    "apikey",
    "token",
    "sec-websocket-key",
    "cookie",
    "set-cookie",
)

EXPECTED_CONNECTION_CLOSE_MESSAGES = (
    "no close frame received or sent",
    "sent 1000",
    "received 1000",
    "going away",
    "connection closed",
    "websocket connection is closed",
)


def sanitize_headers(headers) -> dict:
    sanitized = {}
    for key, value in dict(headers).items():
        key_text = str(key)
        lower_key = key_text.lower()
        if any(sensitive in lower_key for sensitive in SENSITIVE_HEADER_NAMES):
            sanitized[key] = "***"
        else:
            sanitized[key] = copy.deepcopy(value)
    return sanitized


def is_expected_connection_close(error: Exception) -> bool:
    class_name = error.__class__.__name__.lower()
    if "connectionclosed" in class_name:
        return True

    message = str(error).lower()
    return any(pattern in message for pattern in EXPECTED_CONNECTION_CLOSE_MESSAGES)
