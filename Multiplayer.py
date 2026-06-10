import json
import socket
import threading
import time


SEND_RATE = 30


class MultiplayerClient:
    def __init__(self, host, port, name, color, enabled=True):
        self.host = host
        self.port = port
        self.name = name
        self.color = color
        self.enabled = enabled
        self.player_id = None
        self.connected = False
        self.socket = None
        self.file = None
        self.players = {}
        self.lock = threading.Lock()
        self.send_lock = threading.Lock()
        self.state_lock = threading.Lock()
        self.latest_state = None
        self.closed = threading.Event()

    def connect(self):
        if not self.enabled:
            return False

        try:
            self.socket = socket.create_connection((self.host, self.port), timeout=3)
            self.socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            self.file = self.socket.makefile("r", encoding="utf-8", newline="\n")
            self.connected = True
            threading.Thread(target=self._receive_loop, daemon=True).start()
            threading.Thread(target=self._send_loop, daemon=True).start()
            self._send_now({
                "type": "hello",
                "name": self.name,
                "color": list(self.color),
            })
            print(f"Connected to multiplayer server {self.host}:{self.port}")
            return True
        except OSError as exc:
            self.connected = False
            print(f"Could not connect to multiplayer server {self.host}:{self.port}: {exc}")
            return False

    def _receive_loop(self):
        try:
            for line in self.file:
                try:
                    message = json.loads(line)
                except json.JSONDecodeError:
                    continue

                if message.get("type") == "welcome":
                    self.player_id = message.get("id")
                elif message.get("type") == "snapshot":
                    players = {}
                    for player in message.get("players", []):
                        player_id = player.get("id")
                        if player_id is not None:
                            players[player_id] = player
                    with self.lock:
                        self.players = players
        except OSError:
            pass
        finally:
            self.connected = False

    def _send_now(self, message):
        if not self.connected or self.socket is None:
            return False

        data = json.dumps(message, separators=(",", ":")) + "\n"
        try:
            with self.send_lock:
                self.socket.sendall(data.encode("utf-8"))
            return True
        except OSError:
            self.connected = False
            return False

    def _send_loop(self):
        delay = 1 / SEND_RATE
        while not self.closed.is_set():
            if not self.connected:
                break

            with self.state_lock:
                state = self.latest_state

            if state is not None and not self._send_now(state):
                break

            time.sleep(delay)

    def send_state(self, x, y, z, yaw, pitch):
        if not self.connected:
            return False

        state = {
            "type": "state",
            "name": self.name,
            "color": list(self.color),
            "x": round(x, 3),
            "y": round(y, 3),
            "z": round(z, 3),
            "yaw": round(yaw, 4),
            "pitch": round(pitch, 4),
        }

        with self.state_lock:
            self.latest_state = state
        return True

    def get_other_players(self):
        with self.lock:
            players = list(self.players.values())
        return [player for player in players if player.get("id") != self.player_id]

    def hud_text(self):
        if not self.enabled:
            return "NET OFF"
        if not self.connected:
            return "NET OFFLINE"
        return f"NET {len(self.get_other_players()) + 1}P"

    def close(self):
        self.closed.set()
        self.connected = False
        try:
            if self.socket is not None:
                self.socket.shutdown(socket.SHUT_RDWR)
                self.socket.close()
        except OSError:
            pass
