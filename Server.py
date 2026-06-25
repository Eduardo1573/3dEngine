import argparse
import json
import math
import socket
import threading
import time
from dataclasses import dataclass

TICK_RATE = 30
SOCKET_TIMEOUT = 0.1
PLAYER_VISUAL_WIDTH = 0.5
PLAYER_VISUAL_HEIGHT = 1.25
PLAYER_VISUAL_DEPTH = 0.5
players = {}
players_lock = threading.Lock()
next_player_id = 1


@dataclass(frozen=True)
class Vec3:
    x: float
    y: float
    z: float

    def __add__(self, other):
        return Vec3(self.x + other.x, self.y + other.y, self.z + other.z)

    def __sub__(self, other):
        return Vec3(self.x - other.x, self.y - other.y, self.z - other.z)

    def __mul__(self, scale):
        return Vec3(self.x * scale, self.y * scale, self.z * scale)


@dataclass(frozen=True)
class OrientedBox:
    position: Vec3
    dimensions: Vec3
    yaw: float = 0.0
    pitch: float = 0.0


def world_to_box_local(point, box):
    translated = point - box.position
    sin_yaw = math.sin(box.yaw)
    cos_yaw = math.cos(box.yaw)
    sin_pitch = math.sin(box.pitch)
    cos_pitch = math.cos(box.pitch)

    yaw_local = Vec3(
        translated.x * cos_yaw - translated.z * sin_yaw,
        translated.y,
        translated.x * sin_yaw + translated.z * cos_yaw,
    )

    return Vec3(
        yaw_local.x,
        yaw_local.y * cos_pitch - yaw_local.z * sin_pitch,
        yaw_local.y * sin_pitch + yaw_local.z * cos_pitch,
    )


def segment_aabb_hit_fraction(start, end, dimensions):
    half = dimensions * 0.5
    direction = end - start
    t_min = 0.0
    t_max = 1.0

    for origin, delta, low, high in (
        (start.x, direction.x, -half.x, half.x),
        (start.y, direction.y, -half.y, half.y),
        (start.z, direction.z, -half.z, half.z),
    ):
        if abs(delta) < 1e-8:
            if origin < low or origin > high:
                return None
            continue

        inv_delta = 1.0 / delta
        t1 = (low - origin) * inv_delta
        t2 = (high - origin) * inv_delta
        if t1 > t2:
            t1, t2 = t2, t1

        t_min = max(t_min, t1)
        t_max = min(t_max, t2)
        if t_min > t_max:
            return None

    return t_min


def bullet_box_hit_fraction(bullet_start, bullet_end, box_position, box_dimensions, yaw=0.0, pitch=0.0):
    box = OrientedBox(box_position, box_dimensions, yaw, pitch)
    local_start = world_to_box_local(bullet_start, box)
    local_end = world_to_box_local(bullet_end, box)
    return segment_aabb_hit_fraction(local_start, local_end, box.dimensions)


def bullet_hits_box(bullet_start, bullet_end, box_position, box_dimensions, yaw=0.0, pitch=0.0):
    return bullet_box_hit_fraction(
        bullet_start,
        bullet_end,
        box_position,
        box_dimensions,
        yaw,
        pitch,
    ) is not None


def player_damage_box(player_state):
    x = clean_float(player_state.get("x"), 0.0)
    y = clean_float(player_state.get("y"), 1.0)
    z = clean_float(player_state.get("z"), -10.0)
    yaw = clean_float(player_state.get("yaw"), 0.0)
    pitch = clean_float(player_state.get("pitch"), 0.0)

    return OrientedBox(
        position=Vec3(x, y - 0.375, z),
        dimensions=Vec3(PLAYER_VISUAL_WIDTH, PLAYER_VISUAL_HEIGHT, PLAYER_VISUAL_DEPTH),
        yaw=yaw,
        pitch=pitch,
    )


def bullet_hits_player(bullet_start, bullet_end, player_state):
    box = player_damage_box(player_state)
    return bullet_box_hit_fraction(
        bullet_start,
        bullet_end,
        box.position,
        box.dimensions,
        box.yaw,
        box.pitch,
    )


def default_color(player_id):
    palette = [
        [255, 80, 80],
        [80, 170, 255],
        [100, 255, 150],
        [255, 220, 90],
        [220, 120, 255],
        [255, 150, 80],
    ]
    return palette[(player_id - 1) % len(palette)]


def clean_color(value, fallback):
    if not isinstance(value, list) or len(value) != 3:
        return fallback
    try:
        return [max(0, min(255, int(channel))) for channel in value]
    except (TypeError, ValueError):
        return fallback


def clean_float(value, fallback):
    try:
        return float(value)
    except (TypeError, ValueError):
        return fallback


def send_json(conn, message):
    data = json.dumps(message, separators=(",", ":")) + "\n"
    conn.sendall(data.encode("utf-8"))


def remove_player(player_id):
    with players_lock:
        player = players.pop(player_id, None)
    if player is not None:
        try:
            player["conn"].close()
        except OSError:
            pass
        print(f"Player {player_id} disconnected")


def update_player(player_id, message):
    with players_lock:
        player = players.get(player_id)
        if player is None:
            return

        state = player["state"]
        state["name"] = str(message.get("name", state["name"]))[:24]
        state["color"] = clean_color(message.get("color"), state["color"])
        state["x"] = clean_float(message.get("x"), state["x"])
        state["y"] = clean_float(message.get("y"), state["y"])
        state["z"] = clean_float(message.get("z"), state["z"])
        state["yaw"] = clean_float(message.get("yaw"), state["yaw"])
        state["pitch"] = clean_float(message.get("pitch"), state["pitch"])
        state["last_seen"] = round(time.time(), 3)


def handle_client(conn, address, player_id):
    try:
        conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        conn.settimeout(SOCKET_TIMEOUT)
        send_json(conn, {"type": "welcome", "id": player_id})
        buffer = ""

        while True:
            try:
                chunk = conn.recv(4096)
            except socket.timeout:
                continue

            if not chunk:
                break

            buffer += chunk.decode("utf-8", errors="ignore")

            while "\n" in buffer:
                line, buffer = buffer.split("\n", 1)
                if not line:
                    continue

                try:
                    message = json.loads(line)
                except json.JSONDecodeError:
                    continue

                if message.get("type") in ("hello", "state"):
                    update_player(player_id, message)
    except OSError:
        pass
    finally:
        remove_player(player_id)


def broadcaster():
    delay = 1 / TICK_RATE
    while True:
        with players_lock:
            snapshot = [player["state"].copy() for player in players.values()]
            targets = [(player_id, player["conn"]) for player_id, player in players.items()]

        message = {"type": "snapshot", "players": snapshot}
        dead_players = []

        for player_id, conn in targets:
            try:
                send_json(conn, message)
            except OSError:
                dead_players.append(player_id)

        for player_id in dead_players:
            remove_player(player_id)

        time.sleep(delay)


def serve(host, port):
    global next_player_id
    bind_host = host or "0.0.0.0"

    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_socket.bind((bind_host, port))
    server_socket.listen(32)
    print(f"Multiplayer server listening on {bind_host}:{port}")

    threading.Thread(target=broadcaster, daemon=True).start()

    try:
        while True:
            conn, address = server_socket.accept()
            with players_lock:
                player_id = next_player_id
                next_player_id += 1
                players[player_id] = {
                    "conn": conn,
                    "address": address,
                    "state": {
                        "id": player_id,
                        "name": f"Player {player_id}",
                        "color": default_color(player_id),
                        "x": 0.0,
                        "y": 1.0,
                        "z": -10.0,
                        "yaw": 0.0,
                        "pitch": 0.0,
                        "last_seen": round(time.time(), 3),
                    },
                }

            print(f"Player {player_id} connected from {address[0]}:{address[1]}")
            threading.Thread(
                target=handle_client,
                args=(conn, address, player_id),
                daemon=True,
            ).start()
    finally:
        server_socket.close()


def parse_args():
    parser = argparse.ArgumentParser(description="3dEngine multiplayer server")
    parser.add_argument("--host", default=None)
    parser.add_argument("--port", type=int, default=5555)
    return parser.parse_args()


if __name__ == "__main__":
    args = parse_args()
    serve(args.host, args.port)
