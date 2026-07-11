def _clamp(value, minimum, maximum):
    return minimum if value < minimum else maximum if value > maximum else value


def _as_finite_number(value):
    try:
        number = float(value)
    except (TypeError, ValueError):
        return None
    if number != number or number > 1.0e30 or number < -1.0e30:
        return None
    return number


def _finite_numbers(values, count):
    if not values or len(values) < count:
        return None
    numbers = []
    for value in values[:count]:
        number = _as_finite_number(value)
        if number is None:
            return None
        numbers.append(number)
    return numbers


def _confidence_percent(confidence):
    number = _as_finite_number(confidence)
    if number is None:
        return 0
    if isinstance(confidence, float):
        return int(round(number * 100))
    return int(round(number))


def select_primary_face(faces):
    primary = None
    primary_area = -1
    for face in faces or ():
        box = _finite_numbers(face.get("box"), 4)
        euler = _finite_numbers(face.get("euler"), 3)
        if not box or not euler:
            continue
        width = box[2]
        height = box[3]
        if width <= 0 or height <= 0:
            continue
        area = width * height
        if area > primary_area:
            primary = face
            primary_area = area
    return primary


def make_primary_face_observation(faces, frame_size):
    frame_dimensions = _finite_numbers(frame_size, 2)
    if not frame_dimensions:
        raise ValueError("frame_size must contain width and height")

    frame_width, frame_height = frame_dimensions
    if frame_width <= 0 or frame_height <= 0:
        raise ValueError("frame dimensions must be positive")

    primary = select_primary_face(faces)
    if not primary:
        return None

    box = _finite_numbers(primary["box"], 4)
    euler = _finite_numbers(primary["euler"], 3)
    if not box or not euler:
        return None

    x, y, width, height = box
    pitch, yaw, roll = euler
    confidence = primary.get("confidence", 100)
    confidence_percent = _confidence_percent(confidence)

    center_x = int(round(((x + width / 2) * 2000 / frame_width) - 1000))
    center_y = int(round(((y + height / 2) * 2000 / frame_height) - 1000))
    normalized_width = int(round(width * 1000 / frame_width))
    normalized_height = int(round(height * 1000 / frame_height))

    return {
        "center_x": _clamp(center_x, -1000, 1000),
        "center_y": _clamp(center_y, -1000, 1000),
        "width": _clamp(normalized_width, 0, 1000),
        "height": _clamp(normalized_height, 0, 1000),
        "pitch_cd": _clamp(int(round(pitch * 100)), -32768, 32767),
        "yaw_cd": _clamp(int(round(yaw * 100)), -32768, 32767),
        "roll_cd": _clamp(int(round(roll * 100)), -32768, 32767),
        "confidence": _clamp(confidence_percent, 0, 100),
    }
