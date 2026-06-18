import argparse
import json
import socket
import threading
import time
import subprocess
import requests

TICK_RATE = 30
SOCKET_TIMEOUT = 0.1
players = {}
players_lock = threading.Lock()
next_player_id = 1


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


def serve(host, port, ngrok):

    result = subprocess.run(
    ["ipconfig", "getifaddr", "en0"],
    capture_output=True,
    text=True,
    check=True
    )
    local_ip = ngrok or result.stdout.replace('\n', '')
    body = {"local_ip": local_ip}
    print(f'Published Local IP: {local_ip}')
    requests.post(
        url="https://api.npoint.io/0e339466ee57dc00420e",
        json=body
    )

    global next_player_id

    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_socket.bind((host, port))
    server_socket.listen(32)
    print(f"Multiplayer server listening on {host}:{port}")

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
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=5555)
    parser.add_argument("--ngrok", default=None)
    return parser.parse_args()


if __name__ == "__main__":
    args = parse_args()
    serve(args.host, args.port)
