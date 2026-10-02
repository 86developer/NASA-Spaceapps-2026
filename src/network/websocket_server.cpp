#include "websocket_server.hpp"
#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <cstdint>

#if defined(_WIN32) || defined(_WIN64)
    #pragma comment(lib, "ws2_32.lib")
#endif

namespace vitalgrid {

// ============================================================================
// Internal Clean SHA-1 & Base64 Implementation (RFC 6455 Handshake)
// ============================================================================
namespace crypto {

static inline uint32_t rol(uint32_t value, size_t bits) {
    return (value << bits) | (value >> (32 - bits));
}

static inline uint32_t blk(const uint32_t block[16], size_t i) {
    return rol(block[(i + 13) & 15] ^ block[(i + 8) & 15] ^ block[(i + 2) & 15] ^ block[i & 15], 1);
}

static inline void R0(const uint32_t block[16], uint32_t v, uint32_t &w, uint32_t x, uint32_t y, uint32_t &z, size_t i) {
    z += ((w & (x ^ y)) ^ y) + block[i] + 0x5a827999 + rol(v, 5);
    w = rol(w, 30);
}

static inline void R1(uint32_t block[16], uint32_t v, uint32_t &w, uint32_t x, uint32_t y, uint32_t &z, size_t i) {
    block[i & 15] = blk(block, i);
    z += ((w & (x ^ y)) ^ y) + block[i & 15] + 0x5a827999 + rol(v, 5);
    w = rol(w, 30);
}

static inline void R2(uint32_t block[16], uint32_t v, uint32_t &w, uint32_t x, uint32_t y, uint32_t &z, size_t i) {
    block[i & 15] = blk(block, i);
    z += (w ^ x ^ y) + block[i & 15] + 0x6ed9eba1 + rol(v, 5);
    w = rol(w, 30);
}

static inline void R3(uint32_t block[16], uint32_t v, uint32_t &w, uint32_t x, uint32_t y, uint32_t &z, size_t i) {
    block[i & 15] = blk(block, i);
    z += (((w | x) & y) | (w & x)) + block[i & 15] + 0x8f1bbcdc + rol(v, 5);
    w = rol(w, 30);
}

static inline void R4(uint32_t block[16], uint32_t v, uint32_t &w, uint32_t x, uint32_t y, uint32_t &z, size_t i) {
    block[i & 15] = blk(block, i);
    z += (w ^ x ^ y) + block[i & 15] + 0xca62c1d6 + rol(v, 5);
    w = rol(w, 30);
}

static void sha1_transform(uint32_t digest[], uint32_t block[]) {
    uint32_t a = digest[0];
    uint32_t b = digest[1];
    uint32_t c = digest[2];
    uint32_t d = digest[3];
    uint32_t e = digest[4];

    R0(block, a, b, c, d, e, 0);  R0(block, e, a, b, c, d, 1);  R0(block, d, e, a, b, c, 2);  R0(block, c, d, e, a, b, 3);
    R0(block, b, c, d, e, a, 4);  R0(block, a, b, c, d, e, 5);  R0(block, e, a, b, c, d, 6);  R0(block, d, e, a, b, c, 7);
    R0(block, c, d, e, a, b, 8);  R0(block, b, c, d, e, a, 9);  R0(block, a, b, c, d, e, 10); R0(block, e, a, b, c, d, 11);
    R0(block, d, e, a, b, c, 12); R0(block, c, d, e, a, b, 13); R0(block, b, c, d, e, a, 14); R0(block, a, b, c, d, e, 15);
    R1(block, e, a, b, c, d, 16); R1(block, d, e, a, b, c, 17); R1(block, c, d, e, a, b, 18); R1(block, b, c, d, e, a, 19);
    R2(block, a, b, c, d, e, 20); R2(block, e, a, b, c, d, 21); R2(block, d, e, a, b, c, 22); R2(block, c, d, e, a, b, 23);
    R2(block, b, c, d, e, a, 24); R2(block, a, b, c, d, e, 25); R2(block, e, a, b, c, d, 26); R2(block, d, e, a, b, c, 27);
    R2(block, c, d, e, a, b, 28); R2(block, b, c, d, e, a, 29); R2(block, a, b, c, d, e, 30); R2(block, e, a, b, c, d, 31);
    R2(block, d, e, a, b, c, 32); R2(block, c, d, e, a, b, 33); R2(block, b, c, d, e, a, 34); R2(block, a, b, c, d, e, 35);
    R2(block, e, a, b, c, d, 36); R2(block, d, e, a, b, c, 37); R2(block, c, d, e, a, b, 38); R2(block, b, c, d, e, a, 39);
    R3(block, a, b, c, d, e, 40); R3(block, e, a, b, c, d, 41); R3(block, d, e, a, b, c, 42); R3(block, c, d, e, a, b, 43);
    R3(block, b, c, d, e, a, 44); R3(block, a, b, c, d, e, 45); R3(block, e, a, b, c, d, 46); R3(block, d, e, a, b, c, 47);
    R3(block, c, d, e, a, b, 48); R3(block, b, c, d, e, a, 49); R3(block, a, b, c, d, e, 50); R3(block, e, a, b, c, d, 51);
    R3(block, d, e, a, b, c, 52); R3(block, c, d, e, a, b, 53); R3(block, b, c, d, e, a, 54); R3(block, a, b, c, d, e, 55);
    R3(block, e, a, b, c, d, 56); R3(block, d, e, a, b, c, 57); R3(block, c, d, e, a, b, 58); R3(block, b, c, d, e, a, 59);
    R4(block, a, b, c, d, e, 60); R4(block, e, a, b, c, d, 61); R4(block, d, e, a, b, c, 62); R4(block, c, d, e, a, b, 63);
    R4(block, b, c, d, e, a, 64); R4(block, a, b, c, d, e, 65); R4(block, e, a, b, c, d, 66); R4(block, d, e, a, b, c, 67);
    R4(block, c, d, e, a, b, 68); R4(block, b, c, d, e, a, 69); R4(block, a, b, c, d, e, 70); R4(block, e, a, b, c, d, 71);
    R4(block, d, e, a, b, c, 72); R4(block, c, d, e, a, b, 73); R4(block, b, c, d, e, a, 74); R4(block, a, b, c, d, e, 75);
    R4(block, e, a, b, c, d, 76); R4(block, d, e, a, b, c, 77); R4(block, c, d, e, a, b, 78); R4(block, b, c, d, e, a, 79);

    digest[0] += a;
    digest[1] += b;
    digest[2] += c;
    digest[3] += d;
    digest[4] += e;
}

static std::vector<uint8_t> sha1(const std::string& input) {
    uint32_t digest[5] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0};
    uint64_t total_bits = input.size() * 8;
    std::vector<uint8_t> data(input.begin(), input.end());
    data.push_back(0x80);
    while ((data.size() % 64) != 56) {
        data.push_back(0x00);
    }
    for (int i = 7; i >= 0; --i) {
        data.push_back(static_cast<uint8_t>((total_bits >> (i * 8)) & 0xFF));
    }

    for (size_t chunk = 0; chunk < data.size(); chunk += 64) {
        uint32_t block[16];
        for (size_t i = 0; i < 16; ++i) {
            block[i] = (static_cast<uint32_t>(data[chunk + i * 4]) << 24) |
                       (static_cast<uint32_t>(data[chunk + i * 4 + 1]) << 16) |
                       (static_cast<uint32_t>(data[chunk + i * 4 + 2]) << 8) |
                       (static_cast<uint32_t>(data[chunk + i * 4 + 3]));
        }
        sha1_transform(digest, block);
    }

    std::vector<uint8_t> result(20);
    for (size_t i = 0; i < 5; ++i) {
        result[i * 4] = static_cast<uint8_t>((digest[i] >> 24) & 0xFF);
        result[i * 4 + 1] = static_cast<uint8_t>((digest[i] >> 16) & 0xFF);
        result[i * 4 + 2] = static_cast<uint8_t>((digest[i] >> 8) & 0xFF);
        result[i * 4 + 3] = static_cast<uint8_t>(digest[i] & 0xFF);
    }
    return result;
}

static std::string base64_encode(const std::vector<uint8_t>& data) {
    static const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    int val = 0, valb = -6;
    for (uint8_t c : data) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(tbl[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(tbl[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

} // namespace crypto

// ============================================================================
// WebSocketServer Implementation
// ============================================================================

WebSocketServer::WebSocketServer(int port) : port_(port) {
#if defined(_WIN32) || defined(_WIN64)
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

WebSocketServer::~WebSocketServer() {
    stop();
#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif
}

bool WebSocketServer::start() {
    if (running_.load()) return true;

    listen_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket_ == INVALID_SOCKET_VAL) {
        std::cerr << "[WS SERVER ERROR] Failed to create socket." << std::endl;
        return false;
    }

    int opt = 1;
#if defined(_WIN32) || defined(_WIN64)
    setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(static_cast<uint16_t>(port_));

    if (bind(listen_socket_, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR_VAL) {
        std::cerr << "[WS SERVER ERROR] Failed to bind port " << port_ << std::endl;
        close_client(listen_socket_);
        listen_socket_ = INVALID_SOCKET_VAL;
        return false;
    }

    if (listen(listen_socket_, SOMAXCONN) == SOCKET_ERROR_VAL) {
        std::cerr << "[WS SERVER ERROR] Listen failed on port " << port_ << std::endl;
        close_client(listen_socket_);
        listen_socket_ = INVALID_SOCKET_VAL;
        return false;
    }

    running_.store(true);
    accept_thread_ = std::thread(&WebSocketServer::accept_loop, this);

    std::cout << "[VITALGRID WS SERVER] Online and listening on ws://127.0.0.1:" << port_ << std::endl;
    return true;
}

void WebSocketServer::stop() {
    if (!running_.load()) return;
    running_.store(false);

    if (listen_socket_ != INVALID_SOCKET_VAL) {
        close_client(listen_socket_);
        listen_socket_ = INVALID_SOCKET_VAL;
    }

    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }

    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (socket_t sock : active_clients_) {
        close_client(sock);
    }
    active_clients_.clear();
}

void WebSocketServer::accept_loop() {
    while (running_.load()) {
        sockaddr_in client_addr{};
#if defined(_WIN32) || defined(_WIN64)
        int addr_len = sizeof(client_addr);
#else
        socklen_t addr_len = sizeof(client_addr);
#endif
        socket_t client_sock = accept(listen_socket_, (struct sockaddr*)&client_addr, &addr_len);
        if (client_sock == INVALID_SOCKET_VAL) {
            if (!running_.load()) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }

        std::thread(&WebSocketServer::handle_client, this, client_sock).detach();
    }
}

bool WebSocketServer::perform_handshake(socket_t client_socket, const std::string& request) {
    const std::string key_header = "Sec-WebSocket-Key: ";
    size_t key_pos = request.find(key_header);
    if (key_pos == std::string::npos) {
        return false;
    }

    size_t key_start = key_pos + key_header.size();
    size_t key_end = request.find("\r\n", key_start);
    if (key_end == std::string::npos) return false;

    std::string client_key = request.substr(key_start, key_end - key_start);
    // Trim whitespace
    client_key.erase(client_key.find_last_not_of(" \t\r\n") + 1);

    static const std::string WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string combined = client_key + WS_GUID;
    std::vector<uint8_t> sha = crypto::sha1(combined);
    std::string accept_key = crypto::base64_encode(sha);

    std::ostringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n"
             << "Upgrade: websocket\r\n"
             << "Connection: Upgrade\r\n"
             << "Sec-WebSocket-Accept: " << accept_key << "\r\n"
             << "\r\n";

    std::string resp_str = response.str();
    send(client_socket, resp_str.data(), static_cast<int>(resp_str.size()), 0);
    return true;
}

void WebSocketServer::handle_client(socket_t client_socket) {
    char buffer[4096];
    int bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0) {
        close_client(client_socket);
        return;
    }
    buffer[bytes_read] = '\0';
    std::string request(buffer, bytes_read);

    if (!perform_handshake(client_socket, request)) {
        close_client(client_socket);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        active_clients_.push_back(client_socket);
    }
    std::cout << "[WS SERVER] New Crew Dragon UI client connected. Total clients: " 
              << connected_clients_count() << std::endl;

    // Client frame loop
    while (running_.load()) {
        uint8_t header[2];
        int res = recv(client_socket, (char*)header, 2, 0);
        if (res <= 0) break;

        uint8_t opcode = header[0] & 0x0F;
        bool masked = (header[1] & 0x80) != 0;
        uint64_t payload_len = header[1] & 0x7F;

        if (opcode == 0x8) { // Connection Close
            break;
        }

        if (payload_len == 126) {
            uint8_t ext[2];
            recv(client_socket, (char*)ext, 2, 0);
            payload_len = (static_cast<uint64_t>(ext[0]) << 8) | ext[1];
        } else if (payload_len == 127) {
            uint8_t ext[8];
            recv(client_socket, (char*)ext, 8, 0);
            payload_len = 0;
            for (int i = 0; i < 8; ++i) {
                payload_len = (payload_len << 8) | ext[i];
            }
        }

        uint8_t mask_key[4] = {0, 0, 0, 0};
        if (masked) {
            recv(client_socket, (char*)mask_key, 4, 0);
        }

        std::vector<char> payload(payload_len);
        size_t total_recv = 0;
        while (total_recv < payload_len) {
            int n = recv(client_socket, payload.data() + total_recv, static_cast<int>(payload_len - total_recv), 0);
            if (n <= 0) break;
            total_recv += n;
        }

        if (masked) {
            for (size_t i = 0; i < payload_len; ++i) {
                payload[i] ^= mask_key[i % 4];
            }
        }

        if (opcode == 0x9) { // Ping -> send Pong
            uint8_t pong_header[2] = {0x8A, 0x00};
            send(client_socket, (char*)pong_header, 2, 0);
            continue;
        }

        if (opcode == 0x1) { // Text frame
            std::string msg(payload.begin(), payload.end());
            if (on_message_) {
                on_message_(client_socket, msg);
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto it = std::find(active_clients_.begin(), active_clients_.end(), client_socket);
        if (it != active_clients_.end()) {
            active_clients_.erase(it);
        }
    }
    close_client(client_socket);
    std::cout << "[WS SERVER] Client disconnected. Remaining: " << connected_clients_count() << std::endl;
}

bool WebSocketServer::send_message(socket_t client_socket, const std::string& message) {
    std::vector<uint8_t> frame;
    frame.push_back(0x81); // FIN = 1, Opcode = 1 (text)

    size_t len = message.size();
    if (len <= 125) {
        frame.push_back(static_cast<uint8_t>(len));
    } else if (len <= 65535) {
        frame.push_back(126);
        frame.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(len & 0xFF));
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i) {
            frame.push_back(static_cast<uint8_t>((len >> (i * 8)) & 0xFF));
        }
    }

    frame.insert(frame.end(), message.begin(), message.end());
    int sent = send(client_socket, (const char*)frame.data(), static_cast<int>(frame.size()), 0);
    return sent != SOCKET_ERROR_VAL;
}

void WebSocketServer::broadcast(const std::string& message) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (socket_t sock : active_clients_) {
        send_message(sock, message);
    }
}

size_t WebSocketServer::connected_clients_count() const {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    return active_clients_.size();
}

void WebSocketServer::close_client(socket_t client_socket) {
#if defined(_WIN32) || defined(_WIN64)
    closesocket(client_socket);
#else
    close(client_socket);
#endif
}

} // namespace vitalgrid
