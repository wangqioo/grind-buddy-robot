import time


VOICE_LATENCY_PREFIX = "[VOICE-LATENCY]"
TAG = __name__


def reset_voice_latency(conn, event, detail=None, now=None):
    """Start a per-connection latency trace and emit the first event."""
    timestamp = time.monotonic() if now is None else now
    conn.voice_latency_trace = {
        "start": timestamp,
        "last": timestamp,
        "seen": set(),
    }
    mark_voice_latency(conn, event, detail=detail, now=timestamp)


def mark_voice_latency_once(conn, event, detail=None, now=None):
    trace = _ensure_trace(conn, now)
    if event in trace["seen"]:
        return
    trace["seen"].add(event)
    mark_voice_latency(conn, event, detail=detail, now=now)


def mark_voice_latency(conn, event, detail=None, now=None):
    timestamp = time.monotonic() if now is None else now
    trace = _ensure_trace(conn, timestamp)
    total = timestamp - trace["start"]
    delta = timestamp - trace["last"]
    trace["last"] = timestamp

    session_id = getattr(conn, "session_id", "-") or "-"
    sentence_id = getattr(conn, "sentence_id", "-") or "-"
    message = (
        f"{VOICE_LATENCY_PREFIX} event={event} "
        f"total={total:.3f}s delta={delta:.3f}s "
        f"session={session_id} sentence={sentence_id}"
    )
    if detail:
        message = f"{message} detail={detail}"

    conn.logger.bind(tag=TAG).info(message)


def _ensure_trace(conn, now):
    timestamp = time.monotonic() if now is None else now
    trace = getattr(conn, "voice_latency_trace", None)
    if trace is None:
        trace = {
            "start": timestamp,
            "last": timestamp,
            "seen": set(),
        }
        conn.voice_latency_trace = trace
    return trace
