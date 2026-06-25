#define GL_SILENCE_DEPRECATION

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#include <GLUT/glut.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/glut.h>
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace {

constexpr float kPi = 3.14159265358979323846f;

#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
#endif

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Color {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
};

struct Vertex {
    Vec3 position;
    Color color;
};

using GLBufferSize = std::ptrdiff_t;

#ifdef _WIN32
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#endif
#ifndef GL_ELEMENT_ARRAY_BUFFER
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#endif
#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW 0x88E4
#endif
#ifndef GL_DYNAMIC_DRAW
#define GL_DYNAMIC_DRAW 0x88E8
#endif
#endif

void loadRequiredOpenGlFunctions();
void gpuGenBuffers(GLsizei count, GLuint* buffers);
void gpuBindBuffer(GLenum target, GLuint buffer);
void gpuBufferData(GLenum target, GLBufferSize size, const GLvoid* data, GLenum usage);
void gpuDeleteBuffers(GLsizei count, const GLuint* buffers);

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    GLuint vertexBuffer = 0;
    GLuint indexBuffer = 0;

    void upload(GLenum usage = GL_STATIC_DRAW)
    {
        if (vertices.empty() || indices.empty()) {
            return;
        }

        if (vertexBuffer == 0) {
            gpuGenBuffers(1, &vertexBuffer);
        }
        if (indexBuffer == 0) {
            gpuGenBuffers(1, &indexBuffer);
        }

        gpuBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
        gpuBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLBufferSize>(vertices.size() * sizeof(Vertex)),
            vertices.data(),
            usage);

        gpuBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
        gpuBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            static_cast<GLBufferSize>(indices.size() * sizeof(std::uint32_t)),
            indices.data(),
            usage);
    }

    void draw() const
    {
        if (vertexBuffer == 0 || indexBuffer == 0 || indices.empty()) {
            return;
        }

        gpuBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
        gpuBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);

        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);

        glVertexPointer(
            3,
            GL_FLOAT,
            sizeof(Vertex),
            reinterpret_cast<const GLvoid*>(offsetof(Vertex, position)));
        glColorPointer(
            3,
            GL_FLOAT,
            sizeof(Vertex),
            reinterpret_cast<const GLvoid*>(offsetof(Vertex, color)));

        glDrawElements(
            GL_TRIANGLES,
            static_cast<GLsizei>(indices.size()),
            GL_UNSIGNED_INT,
            nullptr);

        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);
    }

    void destroy()
    {
        if (vertexBuffer != 0) {
            gpuDeleteBuffers(1, &vertexBuffer);
            vertexBuffer = 0;
        }
        if (indexBuffer != 0) {
            gpuDeleteBuffers(1, &indexBuffer);
            indexBuffer = 0;
        }
    }
};

#ifdef _WIN32
using GlGenBuffersProc = void (APIENTRY*)(GLsizei, GLuint*);
using GlBindBufferProc = void (APIENTRY*)(GLenum, GLuint);
using GlBufferDataProc = void (APIENTRY*)(GLenum, GLBufferSize, const GLvoid*, GLenum);
using GlDeleteBuffersProc = void (APIENTRY*)(GLsizei, const GLuint*);

GlGenBuffersProc gGlGenBuffers = nullptr;
GlBindBufferProc gGlBindBuffer = nullptr;
GlBufferDataProc gGlBufferData = nullptr;
GlDeleteBuffersProc gGlDeleteBuffers = nullptr;

PROC getOpenGlProcAddress(const char* name)
{
    PROC proc = wglGetProcAddress(name);
    if (
        proc != nullptr &&
        proc != reinterpret_cast<PROC>(1) &&
        proc != reinterpret_cast<PROC>(2) &&
        proc != reinterpret_cast<PROC>(3) &&
        proc != reinterpret_cast<PROC>(-1)) {
        return proc;
    }

    static HMODULE openGlLibrary = LoadLibraryA("opengl32.dll");
    if (openGlLibrary == nullptr) {
        return nullptr;
    }
    return GetProcAddress(openGlLibrary, name);
}

template <typename Proc>
Proc loadOpenGlProc(const char* name)
{
    PROC proc = getOpenGlProcAddress(name);
    if (proc == nullptr) {
        throw std::runtime_error(std::string("OpenGL function is unavailable: ") + name);
    }
    return reinterpret_cast<Proc>(proc);
}

void loadRequiredOpenGlFunctions()
{
    gGlGenBuffers = loadOpenGlProc<GlGenBuffersProc>("glGenBuffers");
    gGlBindBuffer = loadOpenGlProc<GlBindBufferProc>("glBindBuffer");
    gGlBufferData = loadOpenGlProc<GlBufferDataProc>("glBufferData");
    gGlDeleteBuffers = loadOpenGlProc<GlDeleteBuffersProc>("glDeleteBuffers");
}

void gpuGenBuffers(GLsizei count, GLuint* buffers)
{
    gGlGenBuffers(count, buffers);
}

void gpuBindBuffer(GLenum target, GLuint buffer)
{
    gGlBindBuffer(target, buffer);
}

void gpuBufferData(GLenum target, GLBufferSize size, const GLvoid* data, GLenum usage)
{
    gGlBufferData(target, size, data, usage);
}

void gpuDeleteBuffers(GLsizei count, const GLuint* buffers)
{
    gGlDeleteBuffers(count, buffers);
}
#else
void loadRequiredOpenGlFunctions()
{
}

void gpuGenBuffers(GLsizei count, GLuint* buffers)
{
    glGenBuffers(count, buffers);
}

void gpuBindBuffer(GLenum target, GLuint buffer)
{
    glBindBuffer(target, buffer);
}

void gpuBufferData(GLenum target, GLBufferSize size, const GLvoid* data, GLenum usage)
{
    glBufferData(target, static_cast<GLsizeiptr>(size), data, usage);
}

void gpuDeleteBuffers(GLsizei count, const GLuint* buffers)
{
    glDeleteBuffers(count, buffers);
}
#endif

struct Camera {
    Vec3 position {0.0f, 1.0f, -10.0f};
    float yaw = 0.0001f;
    float pitch = 0.0001f;
    float fovDegrees = 90.0f;
};

struct Aabb {
    Vec3 min;
    Vec3 max;
};

struct PlayerPhysics {
    Vec3 velocity;
    bool onGround = false;
};

struct Options {
    int width = 1280;
    int height = 720;
    bool fullscreen = true;
    bool validateSceneOnly = false;
    bool multiplayerEnabled = true;
    std::string host = "127.0.0.1";
    int port = 5555;
    std::string playerName = "Player";
    std::array<int, 3> playerColor {100, 255, 150};
    std::string objPath;
};

struct RemotePlayer {
    int id = 0;
    std::string name;
    Color color;
    Vec3 position;
    float yaw = 0.0f;
    float pitch = 0.0f;
};

class MultiplayerClient {
public:
    ~MultiplayerClient();

    bool connectToServer(const Options& options);
    void close();
    void sendState(const Camera& camera);
    std::vector<RemotePlayer> otherPlayers() const;
    std::string hudText() const;
    bool enabled() const { return enabled_; }

private:
    bool initializeSockets();
    bool sendJson(const std::string& json);
    void receiveLoop(SocketHandle socket);
    void processLine(const std::string& line);
    void storeSnapshot(const std::vector<RemotePlayer>& players);

    bool enabled_ = false;
    std::atomic<bool> connected_ {false};
    std::atomic<bool> closed_ {true};
    int playerId_ = 0;
    SocketHandle socket_ = kInvalidSocket;
    std::thread receiveThread_;
    mutable std::mutex socketMutex_;
    mutable std::mutex playersMutex_;
    std::mutex sendMutex_;
    std::vector<RemotePlayer> players_;
    std::string host_;
    int port_ = 5555;
    std::string name_;
    std::array<int, 3> color_ {100, 255, 150};
#ifdef _WIN32
    bool winsockStarted_ = false;
#endif
};

Options gOptions;
Camera gCamera;
PlayerPhysics gPhysics;
MultiplayerClient gMultiplayer;
std::vector<Mesh> gMeshes;
Mesh gRemotePlayersMesh;
Mesh gHandsMesh;
std::vector<Aabb> gBoxColliders;
std::array<bool, 256> gKeys {};
std::array<bool, 512> gSpecialKeys {};
std::chrono::steady_clock::time_point gLastFrameTime;
std::chrono::steady_clock::time_point gMouseWarpStarted;
std::uint64_t gFrameCount = 0;
float gFrameMs = 0.0f;
float gWalkBobBlend = 0.0f;
float gWalkBobOffset = 0.0f;
float gNetworkSendAccumulator = 0.0f;
bool gMouseInitialized = false;
bool gMouseWarpPending = false;
int gLastMouseX = 0;
int gLastMouseY = 0;
#ifdef __APPLE__
bool gMouseCursorDetached = false;
#endif

constexpr float kPlayerRadius = 0.28f;
constexpr float kPlayerHeight = 1.8f;
constexpr float kPlayerEyeHeight = 1.0f;
constexpr float kMoveSpeed = 3.0f;
constexpr float kGravity = -18.0f;
constexpr float kJumpVelocity = 7.0f;
constexpr float kGroundY = 0.0f;
constexpr float kPlaneMinX = -10.0f;
constexpr float kPlaneMaxX = 10.0f;
constexpr float kPlaneMinZ = -10.0f;
constexpr float kPlaneMaxZ = 10.0f;
constexpr float kWalkBobAmplitude = 0.035f;
constexpr float kWalkBobFramesPerCycle = 60.0f;
constexpr float kWalkBobSmoothing = 12.0f;
constexpr float kNetworkSendRate = 30.0f;

struct CharacterPart {
    Vec3 localCenter;
    Vec3 size;
    float colorScale = 1.0f;
    bool skin = false;
    bool followsPitch = false;
};

const std::array<CharacterPart, 10>& characterParts()
{
    static const std::array<CharacterPart, 10> parts {{
        // Root is the pelvis point: torso starts at y=0, legs end at y=0.
        {{0.0f, 0.45f, 0.0f}, {0.58f, 0.90f, 0.32f}, 1.00f, false, false}, // torso
        {{0.0f, 1.16f, 0.0f}, {0.38f, 0.38f, 0.38f}, 1.00f, true, true},   // head
        {{-0.43f, 0.56f, 0.0f}, {0.16f, 0.52f, 0.18f}, 0.90f, false, false}, // left upper arm
        {{-0.43f, 0.08f, 0.0f}, {0.15f, 0.44f, 0.16f}, 0.78f, false, false}, // left lower arm
        {{0.43f, 0.56f, 0.0f}, {0.16f, 0.52f, 0.18f}, 0.90f, false, false},  // right upper arm
        {{0.43f, 0.08f, 0.0f}, {0.15f, 0.44f, 0.16f}, 0.78f, false, false},  // right lower arm
        {{-0.16f, -0.27f, 0.0f}, {0.18f, 0.54f, 0.22f}, 0.72f, false, false}, // left upper leg
        {{-0.16f, -0.81f, 0.0f}, {0.17f, 0.54f, 0.20f}, 0.62f, false, false}, // left lower leg
        {{0.16f, -0.27f, 0.0f}, {0.18f, 0.54f, 0.22f}, 0.72f, false, false},  // right upper leg
        {{0.16f, -0.81f, 0.0f}, {0.17f, 0.54f, 0.20f}, 0.62f, false, false},  // right lower leg
    }};
    return parts;
}

float clamp(float value, float low, float high)
{
    return std::max(low, std::min(high, value));
}

float framePhase(float framesPerCycle, float offset = 0.0f)
{
    const float safeFramesPerCycle = std::max(framesPerCycle, 1.0f);
    const double frameInCycle = std::fmod(
        static_cast<double>(gFrameCount),
        static_cast<double>(safeFramesPerCycle));
    return static_cast<float>(frameInCycle / safeFramesPerCycle * static_cast<double>(kPi * 2.0f)) + offset;
}

void advanceAnimationClock()
{
    ++gFrameCount;
}

void resetWalkAnimation()
{
    gWalkBobBlend = 0.0f;
    gWalkBobOffset = 0.0f;
}

void updateWalkAnimation(float dt, bool isWalking)
{
    const float targetBlend = isWalking ? 1.0f : 0.0f;
    const float blendStep = 1.0f - std::exp(-kWalkBobSmoothing * dt);
    gWalkBobBlend += (targetBlend - gWalkBobBlend) * blendStep;
    gWalkBobOffset = std::sin(framePhase(kWalkBobFramesPerCycle)) * kWalkBobAmplitude * gWalkBobBlend;
}

Vec3 operator+(const Vec3& a, const Vec3& b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 operator-(const Vec3& a, const Vec3& b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 operator*(const Vec3& v, float scale)
{
    return {v.x * scale, v.y * scale, v.z * scale};
}

float dot(const Vec3& a, const Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

float length(const Vec3& v)
{
    return std::sqrt(dot(v, v));
}

Vec3 normalize(const Vec3& v)
{
    const float len = length(v);
    if (len <= 0.000001f) {
        return {0.0f, 1.0f, 0.0f};
    }
    return {v.x / len, v.y / len, v.z / len};
}

Color scaledColor(float r, float g, float b, float scale)
{
    return {
        clamp((r / 255.0f) * scale, 0.0f, 1.0f),
        clamp((g / 255.0f) * scale, 0.0f, 1.0f),
        clamp((b / 255.0f) * scale, 0.0f, 1.0f),
    };
}

Color scaledColor(const Color& color, float scale)
{
    return {
        clamp(color.r * scale, 0.0f, 1.0f),
        clamp(color.g * scale, 0.0f, 1.0f),
        clamp(color.b * scale, 0.0f, 1.0f),
    };
}

Color colorFromRgb(const std::array<int, 3>& rgb)
{
    return scaledColor(
        static_cast<float>(rgb[0]),
        static_cast<float>(rgb[1]),
        static_cast<float>(rgb[2]),
        1.0f);
}

std::array<int, 3> parseColorChannels(const std::string& value)
{
    std::array<int, 3> color {100, 255, 150};
    std::istringstream input(value);
    std::string channel;
    for (std::size_t i = 0; i < color.size(); ++i) {
        if (!std::getline(input, channel, ',')) {
            return {100, 255, 150};
        }
        try {
            color[i] = std::max(0, std::min(255, std::stoi(channel)));
        } catch (...) {
            return {100, 255, 150};
        }
    }
    return color;
}

void closeSocketHandle(SocketHandle socket)
{
    if (socket == kInvalidSocket) {
        return;
    }
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

void shutdownSocketHandle(SocketHandle socket)
{
    if (socket == kInvalidSocket) {
        return;
    }
#ifdef _WIN32
    shutdown(socket, SD_BOTH);
#else
    shutdown(socket, SHUT_RDWR);
#endif
}

std::string jsonEscape(const std::string& value)
{
    std::string escaped;
    escaped.reserve(value.size());
    for (char c : value) {
        if (c == '"' || c == '\\') {
            escaped.push_back('\\');
        }
        if (static_cast<unsigned char>(c) >= 0x20) {
            escaped.push_back(c);
        }
    }
    return escaped;
}

std::size_t jsonValueStart(const std::string& json, const std::string& field)
{
    const std::string needle = "\"" + field + "\"";
    const std::size_t fieldPos = json.find(needle);
    if (fieldPos == std::string::npos) {
        return std::string::npos;
    }
    const std::size_t colon = json.find(':', fieldPos + needle.size());
    if (colon == std::string::npos) {
        return std::string::npos;
    }
    std::size_t pos = colon + 1;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }
    return pos;
}

bool extractJsonString(const std::string& json, const std::string& field, std::string& out)
{
    std::size_t pos = jsonValueStart(json, field);
    if (pos == std::string::npos || pos >= json.size() || json[pos] != '"') {
        return false;
    }
    ++pos;

    std::string value;
    bool escaped = false;
    for (; pos < json.size(); ++pos) {
        const char c = json[pos];
        if (escaped) {
            value.push_back(c);
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else if (c == '"') {
            out = value;
            return true;
        } else {
            value.push_back(c);
        }
    }
    return false;
}

bool extractJsonFloat(const std::string& json, const std::string& field, float& out)
{
    const std::size_t pos = jsonValueStart(json, field);
    if (pos == std::string::npos) {
        return false;
    }
    try {
        std::size_t consumed = 0;
        out = std::stof(json.substr(pos), &consumed);
        return consumed > 0;
    } catch (...) {
        return false;
    }
}

bool extractJsonInt(const std::string& json, const std::string& field, int& out)
{
    float value = 0.0f;
    if (!extractJsonFloat(json, field, value)) {
        return false;
    }
    out = static_cast<int>(value);
    return true;
}

bool extractJsonColor(const std::string& json, std::array<int, 3>& out)
{
    std::size_t pos = jsonValueStart(json, "color");
    if (pos == std::string::npos || pos >= json.size() || json[pos] != '[') {
        return false;
    }
    ++pos;

    for (std::size_t i = 0; i < out.size(); ++i) {
        while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
            ++pos;
        }
        try {
            std::size_t consumed = 0;
            const int channel = std::stoi(json.substr(pos), &consumed);
            out[i] = std::max(0, std::min(255, channel));
            pos += consumed;
        } catch (...) {
            return false;
        }
        while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
            ++pos;
        }
        if (i + 1 < out.size()) {
            if (pos >= json.size() || json[pos] != ',') {
                return false;
            }
            ++pos;
        }
    }
    return true;
}

std::vector<std::string> extractPlayerObjects(const std::string& json)
{
    std::vector<std::string> objects;
    std::size_t pos = jsonValueStart(json, "players");
    if (pos == std::string::npos || pos >= json.size() || json[pos] != '[') {
        return objects;
    }

    bool inString = false;
    bool escaped = false;
    int objectDepth = 0;
    std::size_t objectStart = std::string::npos;

    for (++pos; pos < json.size(); ++pos) {
        const char c = json[pos];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (inString) {
            if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                inString = false;
            }
            continue;
        }
        if (c == '"') {
            inString = true;
        } else if (c == '{') {
            if (objectDepth == 0) {
                objectStart = pos;
            }
            ++objectDepth;
        } else if (c == '}') {
            --objectDepth;
            if (objectDepth == 0 && objectStart != std::string::npos) {
                objects.push_back(json.substr(objectStart, pos - objectStart + 1));
                objectStart = std::string::npos;
            }
        } else if (c == ']' && objectDepth == 0) {
            break;
        }
    }

    return objects;
}

bool parseRemotePlayer(const std::string& json, RemotePlayer& player)
{
    std::array<int, 3> rgb {100, 255, 150};
    if (!extractJsonInt(json, "id", player.id)) {
        return false;
    }
    extractJsonString(json, "name", player.name);
    extractJsonColor(json, rgb);
    extractJsonFloat(json, "x", player.position.x);
    extractJsonFloat(json, "y", player.position.y);
    extractJsonFloat(json, "z", player.position.z);
    extractJsonFloat(json, "yaw", player.yaw);
    extractJsonFloat(json, "pitch", player.pitch);
    player.color = colorFromRgb(rgb);
    return true;
}

MultiplayerClient::~MultiplayerClient()
{
    close();
}

bool MultiplayerClient::initializeSockets()
{
#ifdef _WIN32
    if (winsockStarted_) {
        return true;
    }
    WSADATA data {};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        std::cerr << "Could not initialize Winsock\n";
        return false;
    }
    winsockStarted_ = true;
#endif
    return true;
}

bool MultiplayerClient::connectToServer(const Options& options)
{
    close();
    enabled_ = options.multiplayerEnabled;
    if (!enabled_) {
        return false;
    }

    host_ = options.host;
    port_ = options.port;
    name_ = options.playerName;
    color_ = options.playerColor;
    closed_ = false;
    connected_ = false;
    playerId_ = 0;
    {
        std::lock_guard<std::mutex> lock(playersMutex_);
        players_.clear();
    }

    if (!initializeSockets()) {
        closed_ = true;
        return false;
    }

    addrinfo hints {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* addresses = nullptr;
    const std::string portText = std::to_string(port_);
    if (getaddrinfo(host_.c_str(), portText.c_str(), &hints, &addresses) != 0) {
        std::cerr << "Could not resolve multiplayer server " << host_ << ":" << port_ << "\n";
        closed_ = true;
        return false;
    }

    SocketHandle connectedSocket = kInvalidSocket;
    for (addrinfo* address = addresses; address != nullptr; address = address->ai_next) {
        SocketHandle candidate = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (candidate == kInvalidSocket) {
            continue;
        }
        if (::connect(candidate, address->ai_addr, static_cast<int>(address->ai_addrlen)) == 0) {
            connectedSocket = candidate;
            break;
        }
        closeSocketHandle(candidate);
    }
    freeaddrinfo(addresses);

    if (connectedSocket == kInvalidSocket) {
        std::cerr << "Could not connect to multiplayer server " << host_ << ":" << port_ << "\n";
        closed_ = true;
        return false;
    }

    int noDelay = 1;
    setsockopt(
        connectedSocket,
        IPPROTO_TCP,
        TCP_NODELAY,
        reinterpret_cast<const char*>(&noDelay),
        sizeof(noDelay));

    {
        std::lock_guard<std::mutex> lock(socketMutex_);
        socket_ = connectedSocket;
    }
    connected_ = true;
    receiveThread_ = std::thread(&MultiplayerClient::receiveLoop, this, connectedSocket);

    std::ostringstream hello;
    hello << "{\"type\":\"hello\",\"name\":\"" << jsonEscape(name_) << "\",\"color\":["
          << color_[0] << "," << color_[1] << "," << color_[2] << "]}";
    sendJson(hello.str());

    std::cout << "Connected to multiplayer server " << host_ << ":" << port_ << "\n";
    return true;
}

void MultiplayerClient::close()
{
    closed_ = true;
    connected_ = false;

    SocketHandle socketToClose = kInvalidSocket;
    {
        std::lock_guard<std::mutex> lock(socketMutex_);
        socketToClose = socket_;
        socket_ = kInvalidSocket;
    }
    if (socketToClose != kInvalidSocket) {
        shutdownSocketHandle(socketToClose);
        closeSocketHandle(socketToClose);
    }

    if (receiveThread_.joinable()) {
        receiveThread_.join();
    }

#ifdef _WIN32
    if (winsockStarted_) {
        WSACleanup();
        winsockStarted_ = false;
    }
#endif
}

bool MultiplayerClient::sendJson(const std::string& json)
{
    if (!connected_) {
        return false;
    }

    SocketHandle activeSocket = kInvalidSocket;
    {
        std::lock_guard<std::mutex> lock(socketMutex_);
        activeSocket = socket_;
    }
    if (activeSocket == kInvalidSocket) {
        connected_ = false;
        return false;
    }

    std::string data = json;
    data.push_back('\n');
    std::lock_guard<std::mutex> sendLock(sendMutex_);
    std::size_t sent = 0;
    while (sent < data.size()) {
        const int chunk = ::send(
            activeSocket,
            data.data() + sent,
            static_cast<int>(data.size() - sent),
            0);
        if (chunk <= 0) {
            connected_ = false;
            return false;
        }
        sent += static_cast<std::size_t>(chunk);
    }
    return true;
}

void MultiplayerClient::sendState(const Camera& camera)
{
    if (!enabled_ || !connected_) {
        return;
    }

    std::ostringstream state;
    state << std::fixed
          << "{\"type\":\"state\",\"name\":\"" << jsonEscape(name_) << "\",\"color\":["
          << color_[0] << "," << color_[1] << "," << color_[2] << "],"
          << std::setprecision(3)
          << "\"x\":" << camera.position.x << ","
          << "\"y\":" << camera.position.y << ","
          << "\"z\":" << camera.position.z << ","
          << std::setprecision(4)
          << "\"yaw\":" << camera.yaw << ","
          << "\"pitch\":" << camera.pitch << "}";
    sendJson(state.str());
}

void MultiplayerClient::receiveLoop(SocketHandle socket)
{
    std::string buffer;
    char chunk[4096];

    while (!closed_) {
        const int received = recv(socket, chunk, static_cast<int>(sizeof(chunk)), 0);
        if (received <= 0) {
            break;
        }
        buffer.append(chunk, static_cast<std::size_t>(received));

        std::size_t newline = std::string::npos;
        while ((newline = buffer.find('\n')) != std::string::npos) {
            const std::string line = buffer.substr(0, newline);
            buffer.erase(0, newline + 1);
            if (!line.empty()) {
                processLine(line);
            }
        }
    }

    connected_ = false;
    SocketHandle socketToClose = kInvalidSocket;
    {
        std::lock_guard<std::mutex> lock(socketMutex_);
        if (socket_ == socket) {
            socketToClose = socket_;
            socket_ = kInvalidSocket;
        }
    }
    closeSocketHandle(socketToClose);
}

void MultiplayerClient::processLine(const std::string& line)
{
    std::string type;
    if (!extractJsonString(line, "type", type)) {
        return;
    }

    if (type == "welcome") {
        extractJsonInt(line, "id", playerId_);
        return;
    }
    if (type != "snapshot") {
        return;
    }

    std::vector<RemotePlayer> players;
    for (const std::string& object : extractPlayerObjects(line)) {
        RemotePlayer player;
        if (parseRemotePlayer(object, player)) {
            players.push_back(std::move(player));
        }
    }
    storeSnapshot(players);
}

void MultiplayerClient::storeSnapshot(const std::vector<RemotePlayer>& players)
{
    std::lock_guard<std::mutex> lock(playersMutex_);
    players_ = players;
}

std::vector<RemotePlayer> MultiplayerClient::otherPlayers() const
{
    std::lock_guard<std::mutex> lock(playersMutex_);
    std::vector<RemotePlayer> others;
    for (const RemotePlayer& player : players_) {
        if (player.id != playerId_) {
            others.push_back(player);
        }
    }
    return others;
}

std::string MultiplayerClient::hudText() const
{
    if (!enabled_) {
        return "NET OFF";
    }
    if (!connected_) {
        return "NET OFFLINE";
    }
    return "NET " + std::to_string(otherPlayers().size() + 1) + "P";
}

Aabb makeAabb(const Vec3& center, const Vec3& size)
{
    const Vec3 half = size * 0.5f;
    return {
        {center.x - half.x, center.y - half.y, center.z - half.z},
        {center.x + half.x, center.y + half.y, center.z + half.z},
    };
}

Aabb playerBoundsAt(const Vec3& eyePosition)
{
    const float feetY = eyePosition.y - kPlayerEyeHeight;
    return {
        {eyePosition.x - kPlayerRadius, feetY, eyePosition.z - kPlayerRadius},
        {eyePosition.x + kPlayerRadius, feetY + kPlayerHeight, eyePosition.z + kPlayerRadius},
    };
}

bool intersects(const Aabb& a, const Aabb& b)
{
    return
        a.min.x < b.max.x && a.max.x > b.min.x &&
        a.min.y < b.max.y && a.max.y > b.min.y &&
        a.min.z < b.max.z && a.max.z > b.min.z;
}

bool collidesWithBoxes(const Vec3& eyePosition)
{
    const Aabb player = playerBoundsAt(eyePosition);
    for (const Aabb& box : gBoxColliders) {
        if (intersects(player, box)) {
            return true;
        }
    }
    return false;
}

bool isOverPlane(const Vec3& eyePosition)
{
    return
        eyePosition.x >= kPlaneMinX - kPlayerRadius &&
        eyePosition.x <= kPlaneMaxX + kPlayerRadius &&
        eyePosition.z >= kPlaneMinZ - kPlayerRadius &&
        eyePosition.z <= kPlaneMaxZ + kPlayerRadius;
}

void addBoxCollider(const Vec3& center, const Vec3& size)
{
    gBoxColliders.push_back(makeAabb(center, size));
}

int envInt(const char* name, int fallback)
{
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        return fallback;
    }

    try {
        return std::stoi(value);
    } catch (...) {
        return fallback;
    }
}

bool envBool(const char* name, bool fallback)
{
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        return fallback;
    }

    std::string normalized(value);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on") {
        return true;
    }
    if (normalized == "0" || normalized == "false" || normalized == "no" || normalized == "off") {
        return false;
    }
    return fallback;
}

void printUsage(const char* program)
{
    std::cout
        << "Usage: " << program << " [options]\n"
        << "\n"
        << "Options:\n"
        << "  --width N             Window width. Defaults to SCREEN_WIDTH or 1280.\n"
        << "  --height N            Window height. Defaults to SCREEN_HEIGHT or 720.\n"
        << "  --fullscreen          Start fullscreen. Defaults to FULLSCREEN env value.\n"
        << "  --obj PATH            Load and render a Wavefront OBJ mesh.\n"
        << "  --host HOST           Multiplayer server host. Defaults to 127.0.0.1.\n"
        << "  --port N              Multiplayer server port. Defaults to 5555.\n"
        << "  --offline             Disable multiplayer networking.\n"
        << "  --name NAME           Multiplayer display name. Defaults to Player.\n"
        << "  --color R,G,B         Multiplayer color. Defaults to 100,255,150.\n"
        << "  --validate-scene      Build geometry and exit before opening a window.\n"
        << "  --help                Print this help text.\n"
        << "\n"
        << "Controls: W/A/S/D move, mouse looks, Space jumps,\n"
        << "          gravity and collisions are enabled,\n"
        << "          R resets camera, arrow up/down changes FOV, Esc quits.\n";
}

Options parseArgs(int argc, char** argv)
{
    Options options;
    options.width = envInt("SCREEN_WIDTH", options.width);
    options.height = envInt("SCREEN_HEIGHT", options.height);
    options.fullscreen = envBool("FULLSCREEN", options.fullscreen);

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto requireValue = [&](const char* name) -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error(std::string("missing value for ") + name);
            }
            return argv[++i];
        };

        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            std::exit(0);
        } else if (arg == "--width") {
            options.width = std::max(320, std::stoi(requireValue("--width")));
        } else if (arg == "--height") {
            options.height = std::max(240, std::stoi(requireValue("--height")));
        } else if (arg == "--fullscreen") {
            options.fullscreen = true;
        } else if (arg == "--obj") {
            options.objPath = requireValue("--obj");
        } else if (arg == "--host") {
            options.host = requireValue("--host");
        } else if (arg == "--port") {
            options.port = std::max(1, std::min(65535, std::stoi(requireValue("--port"))));
        } else if (arg == "--offline") {
            options.multiplayerEnabled = false;
        } else if (arg == "--name") {
            options.playerName = requireValue("--name").substr(0, 24);
        } else if (arg == "--color") {
            options.playerColor = parseColorChannels(requireValue("--color"));
        } else if (arg == "--validate-scene") {
            options.validateSceneOnly = true;
        } else if (!arg.empty() && arg[0] != '-') {
            options.host = arg;
        } else {
            throw std::runtime_error("unknown option: " + arg);
        }
    }

    return options;
}

Vec3 cameraForward()
{
    const float sinYaw = std::sin(gCamera.yaw);
    const float cosYaw = std::cos(gCamera.yaw);
    const float sinPitch = std::sin(gCamera.pitch);
    const float cosPitch = std::cos(gCamera.pitch);
    return normalize({sinYaw * cosPitch, sinPitch, cosYaw * cosPitch});
}

Vec3 cameraForwardFlat()
{
    return normalize({std::sin(gCamera.yaw), 0.0f, std::cos(gCamera.yaw)});
}

Vec3 cameraRight()
{
    return normalize({std::cos(gCamera.yaw), 0.0f, -std::sin(gCamera.yaw)});
}

void applyMouseLookDelta(int dx, int dy)
{
    if (dx == 0 && dy == 0) {
        return;
    }

    constexpr float sensitivity = 0.003f;
    constexpr int maxDelta = 120;
    dx = std::max(-maxDelta, std::min(maxDelta, dx));
    dy = std::max(-maxDelta, std::min(maxDelta, dy));

    gCamera.yaw -= static_cast<float>(dx) * sensitivity;
    gCamera.pitch -= static_cast<float>(dy) * sensitivity;
    gCamera.pitch = clamp(gCamera.pitch, -kPi / 2.01f, kPi / 2.01f);
}

#ifdef __APPLE__
void restoreMouseCursorAssociation()
{
    if (gMouseCursorDetached) {
        CGAssociateMouseAndMouseCursorPosition(true);
        gMouseCursorDetached = false;
    }
}

void enableRelativeMouseMode()
{
    if (CGAssociateMouseAndMouseCursorPosition(false) == kCGErrorSuccess) {
        gMouseCursorDetached = true;
    }
}

bool consumePlatformMouseMotion()
{
    if (!gMouseCursorDetached) {
        return false;
    }

    int32_t dx = 0;
    int32_t dy = 0;
    CGGetLastMouseDelta(&dx, &dy);
    applyMouseLookDelta(static_cast<int>(dx), static_cast<int>(dy));
    return true;
}
#else
void restoreMouseCursorAssociation()
{
}

void enableRelativeMouseMode()
{
}

bool consumePlatformMouseMotion()
{
    return false;
}
#endif

void pushTriangle(Mesh& mesh, const Vec3& a, const Vec3& b, const Vec3& c, const Color& color)
{
    const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({a, color});
    mesh.vertices.push_back({b, color});
    mesh.vertices.push_back({c, color});
    mesh.indices.push_back(base);
    mesh.indices.push_back(base + 1);
    mesh.indices.push_back(base + 2);
}

void addBox(Mesh& mesh, const Vec3& center, const Vec3& size, const Color& color)
{
    const Vec3 h = size * 0.5f;
    const std::array<Vec3, 8> p {{
        {center.x - h.x, center.y - h.y, center.z + h.z},
        {center.x + h.x, center.y - h.y, center.z + h.z},
        {center.x + h.x, center.y - h.y, center.z - h.z},
        {center.x - h.x, center.y - h.y, center.z - h.z},
        {center.x - h.x, center.y + h.y, center.z + h.z},
        {center.x + h.x, center.y + h.y, center.z + h.z},
        {center.x + h.x, center.y + h.y, center.z - h.z},
        {center.x - h.x, center.y + h.y, center.z - h.z},
    }};
    const std::array<std::array<int, 3>, 12> triangles {{
        {{0, 1, 2}}, {{0, 2, 3}},
        {{4, 6, 5}}, {{4, 7, 6}},
        {{0, 4, 5}}, {{0, 5, 1}},
        {{1, 5, 6}}, {{1, 6, 2}},
        {{2, 6, 7}}, {{2, 7, 3}},
        {{3, 7, 4}}, {{3, 4, 0}},
    }};

    for (const auto& triangle : triangles) {
        pushTriangle(mesh, p[triangle[0]], p[triangle[1]], p[triangle[2]], color);
    }
}

Vec3 orientedBoxPoint(const Vec3& center, const Vec3& local, float yaw, float pitch)
{
    const float sinYaw = std::sin(yaw);
    const float cosYaw = std::cos(yaw);
    const float sinPitch = std::sin(pitch);
    const float cosPitch = std::cos(pitch);

    const Vec3 yawLocal {
        local.x,
        local.y * cosPitch + local.z * sinPitch,
        -local.y * sinPitch + local.z * cosPitch,
    };

    return {
        center.x + yawLocal.x * cosYaw + yawLocal.z * sinYaw,
        center.y + yawLocal.y,
        center.z - yawLocal.x * sinYaw + yawLocal.z * cosYaw,
    };
}

void addOrientedBox(Mesh& mesh, const Vec3& center, const Vec3& size, float yaw, float pitch, const Color& color)
{
    const Vec3 h = size * 0.5f;
    const std::array<Vec3, 8> local {{
        {-h.x, -h.y, +h.z},
        {+h.x, -h.y, +h.z},
        {+h.x, -h.y, -h.z},
        {-h.x, -h.y, -h.z},
        {-h.x, +h.y, +h.z},
        {+h.x, +h.y, +h.z},
        {+h.x, +h.y, -h.z},
        {-h.x, +h.y, -h.z},
    }};

    std::array<Vec3, 8> p {};
    for (std::size_t i = 0; i < local.size(); ++i) {
        p[i] = orientedBoxPoint(center, local[i], yaw, pitch);
    }

    const std::array<std::array<int, 3>, 12> triangles {{
        {{0, 1, 2}}, {{0, 2, 3}},
        {{4, 6, 5}}, {{4, 7, 6}},
        {{0, 4, 5}}, {{0, 5, 1}},
        {{1, 5, 6}}, {{1, 6, 2}},
        {{2, 6, 7}}, {{2, 7, 3}},
        {{3, 7, 4}}, {{3, 4, 0}},
    }};
    const std::array<float, 6> faceShades {{
        0.70f,
        1.15f,
        1.35f,
        1.00f,
        0.80f,
        0.95f,
    }};

    for (std::size_t i = 0; i < triangles.size(); ++i) {
        const auto& triangle = triangles[i];
        pushTriangle(
            mesh,
            p[static_cast<std::size_t>(triangle[0])],
            p[static_cast<std::size_t>(triangle[1])],
            p[static_cast<std::size_t>(triangle[2])],
            scaledColor(color, faceShades[i / 2]));
    }
}

void addBoxObstacle(Mesh& mesh, const Vec3& center, const Vec3& size, const Color& color)
{
    addBox(mesh, center, size, color);
    addBoxCollider(center, size);
}

Vec3 characterLocalToWorld(const Vec3& root, const Vec3& local, float yaw)
{
    const float sinYaw = std::sin(yaw);
    const float cosYaw = std::cos(yaw);
    return {
        root.x + local.x * cosYaw + local.z * sinYaw,
        root.y + local.y,
        root.z - local.x * sinYaw + local.z * cosYaw,
    };
}

void addCharacter(Mesh& mesh, const Vec3& root, float yaw, float pitch, const Color& baseColor)
{
    const Color skinColor = scaledColor(232, 184, 140, 1.0f);
    for (const CharacterPart& part : characterParts()) {
        addOrientedBox(
            mesh,
            characterLocalToWorld(root, part.localCenter, yaw),
            part.size,
            yaw,
            part.followsPitch ? pitch : 0.0f,
            part.skin ? skinColor : scaledColor(baseColor, part.colorScale));
    }
}

void addRemotePlayerCharacter(Mesh& mesh, const RemotePlayer& player)
{
    addCharacter(mesh, player.position, player.yaw, player.pitch, player.color);
}

void rebuildRemotePlayersMesh()
{
    gRemotePlayersMesh.vertices.clear();
    gRemotePlayersMesh.indices.clear();

    const std::vector<RemotePlayer> players = gMultiplayer.otherPlayers();
    gRemotePlayersMesh.vertices.reserve(players.size() * characterParts().size() * 36);
    gRemotePlayersMesh.indices.reserve(players.size() * characterParts().size() * 36);
    for (const RemotePlayer& player : players) {
        addRemotePlayerCharacter(gRemotePlayersMesh, player);
    }

    if (!gRemotePlayersMesh.indices.empty()) {
        gRemotePlayersMesh.upload(GL_DYNAMIC_DRAW);
    }
}

void rebuildHandsMesh(const Vec3& renderPosition)
{
    gHandsMesh.vertices.clear();
    gHandsMesh.indices.clear();

    const Vec3 forward = cameraForward();
    const Vec3 right = cameraRight();
    const Vec3 up = normalize(cross(forward, right));
    const float walkSwing = std::sin(framePhase(36.0f)) * 0.025f * gWalkBobBlend;
    const float handPitch = gCamera.pitch - 0.12f;
    const Color handColor = scaledColor(235, 184, 135, 1.0f);
    const Color sleeveColor = scaledColor(55, 72, 96, 1.0f);

    for (float side : std::array<float, 2> {{-1.0f, 1.0f}}) {
        const float sideSwing = walkSwing * side;
        const Vec3 handCenter =
            renderPosition +
            forward * 0.72f +
            right * (side * 0.28f) +
            up * (-0.28f + sideSwing);
        const Vec3 sleeveCenter =
            renderPosition +
            forward * 0.52f +
            right * (side * 0.34f) +
            up * (-0.35f + sideSwing * 0.45f);

        addOrientedBox(
            gHandsMesh,
            sleeveCenter,
            {0.14f, 0.14f, 0.36f},
            gCamera.yaw + side * 0.08f,
            handPitch,
            sleeveColor);
        addOrientedBox(
            gHandsMesh,
            handCenter,
            {0.17f, 0.18f, 0.24f},
            gCamera.yaw + side * 0.08f,
            handPitch,
            handColor);
    }

    if (!gHandsMesh.indices.empty()) {
        gHandsMesh.upload(GL_DYNAMIC_DRAW);
    }
}

void addOutlinedCell(
    Mesh& mesh,
    const Vec3& origin,
    float cellSize,
    float outlineWidth,
    const Color& fillColor,
    const Color& outlineColor)
{
    const float inset = clamp(outlineWidth, 0.0f, cellSize * 0.49f);
    const float x0 = origin.x;
    const float x1 = origin.x + cellSize;
    const float z0 = origin.z;
    const float z1 = origin.z + cellSize;

    const std::array<Vec3, 8> p {{
        {x0, origin.y, z0},
        {x0 + inset, origin.y, z0 + inset},
        {x1, origin.y, z0},
        {x1 - inset, origin.y, z0 + inset},
        {x1, origin.y, z1},
        {x1 - inset, origin.y, z1 - inset},
        {x0, origin.y, z1},
        {x0 + inset, origin.y, z1 - inset},
    }};

    const std::array<std::array<int, 3>, 10> triangles {{
        {{0, 1, 2}}, {{1, 2, 3}},
        {{2, 3, 4}}, {{3, 4, 5}},
        {{4, 5, 6}}, {{5, 6, 7}},
        {{6, 7, 0}}, {{7, 0, 1}},
        {{1, 3, 5}}, {{1, 5, 7}},
    }};

    for (std::size_t i = 0; i < triangles.size(); ++i) {
        const auto& triangle = triangles[i];
        const Color& color = i < 8 ? outlineColor : fillColor;
        pushTriangle(mesh, p[triangle[0]], p[triangle[1]], p[triangle[2]], color);
    }
}

void addOutlinedCellPlane(
    Mesh& mesh,
    const Vec3& origin,
    int columns,
    int rows,
    float cellSize,
    float outlineWidth,
    const Color& fillColor,
    const Color& outlineColor)
{
    if (columns <= 0 || rows <= 0 || cellSize <= 0.0f) {
        return;
    }

    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            addOutlinedCell(
                mesh,
                {
                    origin.x + static_cast<float>(column) * cellSize,
                    origin.y,
                    origin.z + static_cast<float>(row) * cellSize,
                },
                cellSize,
                outlineWidth,
                fillColor,
                outlineColor);
        }
    }
}

Mesh makeReferenceObjects()
{
    Mesh mesh;
    addBoxObstacle(mesh, {4.0f, 0.5f, 7.0f}, {5.0f, 1.0f, 0.2f}, scaledColor(20, 20, 120, 1.0f));
    addBoxObstacle(mesh, {7.0f, 0.5f, 4.0f}, {5.0f, 1.0f, 0.2f}, scaledColor(20, 20, 120, 1.0f));
    //addBoxObstacle(mesh, {7.0f, 1.3f, 9.0f}, {0.9f, 2.6f, 0.9f}, scaledColor(190, 80, 80, 1.0f));
    //addBoxObstacle(mesh, {10.0f, 0.4f, 12.0f}, {2.2f, 0.8f, 2.2f}, scaledColor(80, 130, 220, 1.0f));
    addOutlinedCellPlane(
        mesh,
        {-10.0f, 0.0f, -10.0f},
        20,
        20,
        1.0f,
        0.03f,
        scaledColor(10, 10, 10, 1.0f),
        scaledColor(24, 24, 24, 1.0f));
    return mesh;
}

std::string resolvePath(const std::string& path)
{
    const auto exists = [](const std::string& candidate) {
        std::ifstream file(candidate);
        return file.good();
    };

    if (exists(path)) {
        return path;
    }

    const std::string fromSource = std::string(PROJECT_SOURCE_DIR) + "/" + path;
    if (exists(fromSource)) {
        return fromSource;
    }

    return path;
}

bool parseObjIndex(const std::string& token, int& vertexIndex, int& normalIndex)
{
    vertexIndex = -1;
    normalIndex = -1;

    const std::size_t firstSlash = token.find('/');
    const std::string vertexText = token.substr(0, firstSlash);
    if (vertexText.empty()) {
        return false;
    }

    vertexIndex = std::stoi(vertexText) - 1;
    if (firstSlash == std::string::npos) {
        return true;
    }

    const std::size_t secondSlash = token.find('/', firstSlash + 1);
    if (secondSlash == std::string::npos) {
        return true;
    }

    const std::string normalText = token.substr(secondSlash + 1);
    if (!normalText.empty()) {
        normalIndex = std::stoi(normalText) - 1;
    }
    return true;
}

Mesh loadObj(const std::string& requestedPath)
{
    const std::string path = resolvePath(requestedPath);
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("could not open OBJ file: " + requestedPath);
    }

    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    Mesh mesh;
    std::string line;
    const Vec3 light = normalize({-1.0f, 1.0f, -1.0f});

    while (std::getline(file, line)) {
        std::istringstream input(line);
        std::string type;
        input >> type;

        if (type == "v") {
            Vec3 v;
            input >> v.x >> v.y >> v.z;
            positions.push_back(v);
        } else if (type == "vn") {
            Vec3 n;
            input >> n.x >> n.y >> n.z;
            normals.push_back(normalize(n));
        } else if (type == "f") {
            std::vector<int> faceVertices;
            std::vector<int> faceNormals;
            std::string token;
            while (input >> token) {
                int vertexIndex = -1;
                int normalIndex = -1;
                if (!parseObjIndex(token, vertexIndex, normalIndex)) {
                    continue;
                }
                if (vertexIndex >= 0 && vertexIndex < static_cast<int>(positions.size())) {
                    faceVertices.push_back(vertexIndex);
                    faceNormals.push_back(normalIndex);
                }
            }

            if (faceVertices.size() < 3) {
                continue;
            }

            for (std::size_t i = 1; i + 1 < faceVertices.size(); ++i) {
                const Vec3 a = positions[static_cast<std::size_t>(faceVertices[0])];
                const Vec3 b = positions[static_cast<std::size_t>(faceVertices[i])];
                const Vec3 c = positions[static_cast<std::size_t>(faceVertices[i + 1])];

                Vec3 normal = normalize(cross(b - a, c - a));
                const int objNormalIndex = faceNormals[0];
                if (objNormalIndex >= 0 && objNormalIndex < static_cast<int>(normals.size())) {
                    normal = normals[static_cast<std::size_t>(objNormalIndex)];
                }

                const float shade = 0.2f + 0.8f * clamp(dot(normal, light), 0.0f, 1.0f);
                pushTriangle(mesh, a, b, c, scaledColor(200, 200, 200, shade));
            }
        }
    }

    std::cout << "Loaded OBJ " << path << " with "
              << mesh.vertices.size() / 3 << " triangles\n";
    return mesh;
}

void drawText(float x, float y, const std::string& text)
{
    glRasterPos2f(x, y);
    for (char c : text) {
        glutBitmapCharacter(GLUT_BITMAP_8_BY_13, c);
    }
}

void drawOverlay()
{
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, gOptions.width, gOptions.height, 0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glColor3f(1.0f, 1.0f, 1.0f);
    std::ostringstream position;
    position << std::fixed << std::setprecision(1)
             << "XYZ " << gCamera.position.x << ", "
             << gCamera.position.y << ", " << gCamera.position.z;

    std::ostringstream stats;
    stats << std::fixed << std::setprecision(2)
          << "FOV " << gCamera.fovDegrees
          << "  frame " << gFrameCount
          << "  " << gFrameMs << " ms";

    drawText(14.0f, 24.0f, "GPU polygon engine");
    drawText(14.0f, 42.0f, position.str());
    drawText(14.0f, 60.0f, stats.str());
    drawText(14.0f, 78.0f, gMultiplayer.hudText());

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);
}

void moveHorizontallyWithCollisions(const Vec3& movement)
{
    Vec3 candidate = gCamera.position;
    candidate.x += movement.x;
    if (!collidesWithBoxes(candidate)) {
        gCamera.position.x = candidate.x;
    }

    candidate = gCamera.position;
    candidate.z += movement.z;
    if (!collidesWithBoxes(candidate)) {
        gCamera.position.z = candidate.z;
    }
}

bool resolveVerticalBoxCollision(Vec3& candidate, float previousFeetY)
{
    Aabb player = playerBoundsAt(candidate);
    for (const Aabb& box : gBoxColliders) {
        if (!intersects(player, box)) {
            continue;
        }

        if (gPhysics.velocity.y <= 0.0f && previousFeetY >= box.max.y - 0.05f) {
            candidate.y = box.max.y + kPlayerEyeHeight;
            gPhysics.velocity.y = 0.0f;
            gPhysics.onGround = true;
            return true;
        }

        const float previousHeadY = previousFeetY + kPlayerHeight;
        if (gPhysics.velocity.y > 0.0f && previousHeadY <= box.min.y + 0.05f) {
            candidate.y = box.min.y - kPlayerHeight + kPlayerEyeHeight;
            gPhysics.velocity.y = 0.0f;
            return true;
        }
    }

    return false;
}

void updateVerticalPhysics(float dt)
{
    const bool jumpPressed = gKeys[' '];
    if (jumpPressed && gPhysics.onGround) {
        gPhysics.velocity.y = kJumpVelocity;
        gPhysics.onGround = false;
    }

    gPhysics.velocity.y += kGravity * dt;

    Vec3 candidate = gCamera.position;
    const float previousFeetY = gCamera.position.y - kPlayerEyeHeight;
    candidate.y += gPhysics.velocity.y * dt;
    gPhysics.onGround = false;

    if (!resolveVerticalBoxCollision(candidate, previousFeetY)) {
        const float candidateFeetY = candidate.y - kPlayerEyeHeight;
        if (gPhysics.velocity.y <= 0.0f && isOverPlane(candidate) && candidateFeetY <= kGroundY) {
            candidate.y = kGroundY + kPlayerEyeHeight;
            gPhysics.velocity.y = 0.0f;
            gPhysics.onGround = true;
        }
    }

    gCamera.position.y = candidate.y;
}

void updateCamera(float dt)
{
    const Vec3 forward = cameraForwardFlat();
    const Vec3 right = cameraRight();
    const Vec3 horizontalStart = gCamera.position;
    Vec3 movement;

    if (gKeys['w'] || gKeys['W']) {
        movement = movement + forward;
    }
    if (gKeys['s'] || gKeys['S']) {
        movement = movement - forward;
    }
    if (gKeys['d'] || gKeys['D']) {
        movement = movement - right;
    }
    if (gKeys['a'] || gKeys['A']) {
        movement = movement + right;
    }

    const float movementLength = length(movement);
    if (movementLength > 0.000001f) {
        moveHorizontallyWithCollisions(normalize(movement) * (kMoveSpeed * dt));
    }

    updateVerticalPhysics(dt);

    const Vec3 horizontalDelta {
        gCamera.position.x - horizontalStart.x,
        0.0f,
        gCamera.position.z - horizontalStart.z,
    };
    updateWalkAnimation(dt, gPhysics.onGround && length(horizontalDelta) > 0.0001f);

    if (gKeys['r'] || gKeys['R']) {
        gCamera = Camera {};
        gPhysics = PlayerPhysics {};
        resetWalkAnimation();
    }
    if (gSpecialKeys[GLUT_KEY_DOWN]) {
        gCamera.fovDegrees = clamp(gCamera.fovDegrees - 45.0f * dt, 30.0f, 120.0f);
    }
    if (gSpecialKeys[GLUT_KEY_UP]) {
        gCamera.fovDegrees = clamp(gCamera.fovDegrees + 45.0f * dt, 30.0f, 120.0f);
    }
}

void updateMultiplayer(float dt)
{
    if (!gMultiplayer.enabled()) {
        return;
    }

    gNetworkSendAccumulator += dt;
    const float sendInterval = 1.0f / kNetworkSendRate;
    if (gNetworkSendAccumulator < sendInterval) {
        return;
    }

    gNetworkSendAccumulator = std::fmod(gNetworkSendAccumulator, sendInterval);
    gMultiplayer.sendState(gCamera);
}

void display()
{
    const auto now = std::chrono::steady_clock::now();
    if (gLastFrameTime.time_since_epoch().count() == 0) {
        gLastFrameTime = now;
    }
    const std::chrono::duration<float> elapsed = now - gLastFrameTime;
    gLastFrameTime = now;
    const float dt = clamp(elapsed.count(), 0.0f, 0.05f);
    gFrameMs = dt * 1000.0f;

    advanceAnimationClock();
    updateCamera(dt);
    updateMultiplayer(dt);
    rebuildRemotePlayersMesh();

    glViewport(0, 0, gOptions.width, gOptions.height);
    glClearColor(0.0f, 0.58f, 0.92f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(
        gCamera.fovDegrees,
        static_cast<double>(gOptions.width) / static_cast<double>(std::max(1, gOptions.height)),
        0.05,
        800.0);

    const Vec3 forward = cameraForward();
    Vec3 renderPosition = gCamera.position;
    renderPosition.y += gWalkBobOffset;
    rebuildHandsMesh(renderPosition);
    const Vec3 target = renderPosition + forward;
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(
        renderPosition.x, renderPosition.y, renderPosition.z,
        target.x, target.y, target.z,
        0.0, 1.0, 0.0);

    for (const Mesh& mesh : gMeshes) {
        mesh.draw();
    }
    gRemotePlayersMesh.draw();
    glDisable(GL_DEPTH_TEST);
    gHandsMesh.draw();
    glEnable(GL_DEPTH_TEST);

    drawOverlay();
    glutSwapBuffers();
}

void idle()
{
    glutPostRedisplay();
}

void reshape(int width, int height)
{
    gOptions.width = std::max(1, width);
    gOptions.height = std::max(1, height);
    gMouseInitialized = false;
    glViewport(0, 0, gOptions.width, gOptions.height);
}

void centerMousePointer()
{
    gMouseWarpPending = true;
    gMouseWarpStarted = std::chrono::steady_clock::now();
    glutWarpPointer(gOptions.width / 2, gOptions.height / 2);
}

void keyboardDown(unsigned char key, int, int)
{
    if (key == 27) {
        gMultiplayer.close();
        gRemotePlayersMesh.destroy();
        gHandsMesh.destroy();
        for (Mesh& mesh : gMeshes) {
            mesh.destroy();
        }
        restoreMouseCursorAssociation();
        std::exit(0);
    }

    gKeys[key] = true;
}

void keyboardUp(unsigned char key, int, int)
{
    gKeys[key] = false;
}

void specialDown(int key, int, int)
{
    if (key >= 0 && key < static_cast<int>(gSpecialKeys.size())) {
        gSpecialKeys[static_cast<std::size_t>(key)] = true;
    }
}

void specialUp(int key, int, int)
{
    if (key >= 0 && key < static_cast<int>(gSpecialKeys.size())) {
        gSpecialKeys[static_cast<std::size_t>(key)] = false;
    }
}

void mouseMotion(int x, int y)
{
    if (consumePlatformMouseMotion()) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const int centerX = gOptions.width / 2;
    const int centerY = gOptions.height / 2;

    if (!gMouseInitialized) {
        gMouseInitialized = true;
        gLastMouseX = x;
        gLastMouseY = y;
        return;
    }

    if (gMouseWarpPending) {
        const bool reachedCenter = std::abs(x - centerX) <= 3 && std::abs(y - centerY) <= 3;
        const bool timedOut = now - gMouseWarpStarted > std::chrono::milliseconds(80);
        if (reachedCenter || timedOut) {
            gMouseWarpPending = false;
            gLastMouseX = x;
            gLastMouseY = y;
        }
        return;
    }

    int dx = x - gLastMouseX;
    int dy = y - gLastMouseY;
    gLastMouseX = x;
    gLastMouseY = y;

    if (std::abs(dx) > gOptions.width / 2 || std::abs(dy) > gOptions.height / 2) {
        centerMousePointer();
        return;
    }

    applyMouseLookDelta(dx, dy);

    constexpr int edgeMargin = 32;
    if (
        x <= edgeMargin ||
        y <= edgeMargin ||
        x >= gOptions.width - edgeMargin ||
        y >= gOptions.height - edgeMargin) {
        centerMousePointer();
    }
}

void initializeGl()
{
    loadRequiredOpenGlFunctions();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glShadeModel(GL_SMOOTH);

    for (Mesh& mesh : gMeshes) {
        mesh.upload();
    }
}

void buildScene()
{
    gBoxColliders.clear();
    gMeshes.push_back(makeReferenceObjects());

    if (!gOptions.objPath.empty()) {
        Mesh obj = loadObj(gOptions.objPath);
        if (!obj.vertices.empty()) {
            gMeshes.push_back(std::move(obj));
        }
    }

    std::size_t triangles = 0;
    for (const Mesh& mesh : gMeshes) {
        triangles += mesh.indices.size() / 3;
    }

    std::cout << "Scene contains " << triangles
              << " GPU-rendered triangles across " << gMeshes.size()
              << " meshes\n";
}

} // namespace

int main(int argc, char** argv)
{
    try {
        gOptions = parseArgs(argc, argv);
        buildScene();
        if (gOptions.validateSceneOnly) {
            return 0;
        }
    } catch (const std::exception& exc) {
        std::cerr << "error: " << exc.what() << "\n\n";
        printUsage(argv[0]);
        return 1;
    }

    gMultiplayer.connectToServer(gOptions);

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(gOptions.width, gOptions.height);
    glutCreateWindow("C++ GPU Polygon Engine");
    std::atexit(restoreMouseCursorAssociation);

    if (gOptions.fullscreen) {
        glutFullScreen();
    }

    initializeGl();
    enableRelativeMouseMode();

    glutSetCursor(GLUT_CURSOR_NONE);

    glutDisplayFunc(display);
    glutIdleFunc(idle);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboardDown);
    glutKeyboardUpFunc(keyboardUp);
    glutSpecialFunc(specialDown);
    glutSpecialUpFunc(specialUp);
    glutPassiveMotionFunc(mouseMotion);
    glutMotionFunc(mouseMotion);

    gLastFrameTime = std::chrono::steady_clock::now();
    glutMainLoop();
    return 0;
}
