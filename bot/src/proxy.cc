#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fmt/base.h>
#include <fmt/color.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "game.hh"
#include "messages.hh"
#include "network.hh"
#include "simple-farming-bot.hh"

constexpr const int BUFFER_SIZE = 2048;

namespace dfs
{
    std::vector<int> s_Sockets;
    bool s_Running;
    int s_ProxySock = 0;

    void handle_sigint(int signum) {
        (void)signum;
        fmt::println("Gracefully shutting down...");

        // Close all open sockets
        for (int sockfd : s_Sockets) {
            if (sockfd >= 0) {
                shutdown(sockfd, SHUT_RD);
                fmt::println("Closed {}", sockfd);
            }
        }

        if (s_ProxySock >= 0) {
            close(s_ProxySock);
        }

        s_Running = false;
    }

    struct Handshake
    {
        uint8_t Address[16];
        socklen_t Addrlen;
        in_port_t Port;
    };

    static std::tuple<uint64_t, int> decode_uvarint(const uint8_t *data, size_t length) {
        uint64_t value = 0;
        int shift = 0;
        int bytes_read = 0;

        for (size_t i = 0; i < length; i++) {
            value |= (static_cast<uint64_t>(data[i] & 0x7F) << shift); // Extract 7 bits and add to value
            bytes_read++;
            if ((data[i] & 0x80) == 0) { // MSB = 0 indicates the end
                return {value, bytes_read};
            }

            shift += 7;
            if (shift >= 64) { // Overflow error
                return {0, -2};
            }
        }

        // If we run out of bytes before completing the value
        return {0, -1};
    }

    Proxy::Proxy(int port, const GameData &game_data)
        : m_Port(port)
        , m_GameData(game_data) {
    }

    void Proxy::HandleConnection(int client_sock) const {
        Handshake h{};

        fmt::println("Waiting for handshake...");
        if (recv(client_sock, &h, sizeof(Handshake), 0) == -1) {
            fmt::println(stderr, "Handshake failed");
            close(client_sock);
            return;
        }

        sockaddr_in6 target_server{};
        target_server.sin6_family = AF_INET6;
        target_server.sin6_port = h.Port;
        memcpy(target_server.sin6_addr.s6_addr, h.Address, 16);

        char server_ip[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &target_server.sin6_addr, server_ip, INET6_ADDRSTRLEN);

        // Socket for connecting to the target server
        int server_sock = socket(AF_INET6, SOCK_STREAM, 0);
        if (server_sock < 0) {
            fmt::println(stderr, "Server socket creation failed");
            close(client_sock);
            return;
        }

        fmt::println("Connecting to server {}:{}...", server_ip, htons(target_server.sin6_port));
        if (connect(server_sock, (sockaddr *)&target_server, h.Addrlen) < 0) {
            perror("Connection to server failed");
            close(client_sock);
            close(server_sock);
            return;
        }

        s_Sockets.push_back(client_sock);
        s_Sockets.push_back(server_sock);

        fmt::println("Client connected to server: {}:{}", server_ip, htons(target_server.sin6_port));

        // TODO: Set real map ids
        SimpleFarmingBot bot({69420, 42069}, m_GameData, server_sock);

        // Directly start the bot. We may want to dynamically start it.
        bot.Run();

        auto client_thread = std::thread([&]() {
            uint8_t read_buffer[BUFFER_SIZE];

            std::vector<uint8_t> buffer;
            buffer.reserve(BUFFER_SIZE);

            while (true) {
                ssize_t bytes_read = recv(client_sock, read_buffer, BUFFER_SIZE, 0);

                if (bytes_read < 0) {
                    fmt::println("Client: read error");
                    shutdown(server_sock, SHUT_RD);
                    break;
                } else if (bytes_read == 0) {
                    fmt::println("Client: connection closed");
                    shutdown(server_sock, SHUT_RD);
                    break;
                }

                // Append the read buffer to the current buffer
                for (ssize_t i = 0; i < bytes_read; i++) {
                    buffer.push_back(read_buffer[i]);
                }

                uint8_t *payload = buffer.data();
                size_t buffer_length = buffer.size();

                fmt::print(fmt::fg(fmt::color::cyan), "Client => Server: {} bytes\n", bytes_read);

                while (true) {
                    auto [msg_length, len_offset] = decode_uvarint(payload, buffer_length);

                    if (msg_length == 0) {
                        // We read all of the buffer
                        buffer.clear();
                        break;
                    }

                    if (msg_length > buffer_length - len_offset) {
                        // We need to read some more to interpret this message
                        break;
                    }

                    // Ignore the message length header
                    auto to_send = m_MessageHandler.HandleMessage(payload, msg_length, len_offset, bot.GetDescriptor());

                    // To send may be 0 if we intercept and cancel the message
                    if (to_send.size() > 0) {
                        // Send the packet to the server
                        send(server_sock, to_send.data(), to_send.size(), 0);
                    }

                    // Advance our cursor
                    payload += len_offset;
                    payload += msg_length;

                    // Update the length of the buffer
                    buffer_length -= len_offset;
                    buffer_length -= msg_length;
                }
            }

            close(server_sock);
        });

        auto server_thread = std::thread([&]() {
            uint8_t read_buffer[BUFFER_SIZE];

            std::vector<uint8_t> buffer;
            buffer.reserve(BUFFER_SIZE);

            while (true) {
                ssize_t bytes_read = recv(server_sock, read_buffer, BUFFER_SIZE, 0);

                if (bytes_read < 0) {
                    fmt::println("Server: read error");
                    shutdown(client_sock, SHUT_RD);
                    break;
                } else if (bytes_read == 0) {
                    fmt::println("Server: connection closed");
                    shutdown(client_sock, SHUT_RD);
                    break;
                }

                send(client_sock, read_buffer, bytes_read, 0);

                // Append the read buffer to the current buffer
                for (ssize_t i = 0; i < bytes_read; i++) {
                    buffer.push_back(read_buffer[i]);
                }

                uint8_t *payload = buffer.data();
                size_t buffer_length = buffer.size();

                fmt::print(fmt::fg(fmt::color::dark_cyan), "Server => Client: {} bytes\n", bytes_read);

                while (true) {
                    auto [msg_length, uvarint_bytes_read] = decode_uvarint(payload, buffer_length);

                    if (msg_length == 0) {
                        // We read all of the buffer
                        buffer.clear();
                        break;
                    }

                    if (msg_length > buffer_length - uvarint_bytes_read) {
                        // We need to read some more to interpret this message
                        break;
                    }

                    m_MessageHandler.HandleMessage(payload, msg_length, uvarint_bytes_read, bot.GetDescriptor());

                    // Advance our cursor
                    payload += uvarint_bytes_read;
                    payload += msg_length;

                    // Update the length of the buffer
                    buffer_length -= msg_length;
                    buffer_length -= uvarint_bytes_read;
                }
            }

            close(client_sock);
        });

        // Wait for all threads to end
        client_thread.join();
        server_thread.join();

        bot.Stop();
    }

    void Proxy::Run() {
        signal(SIGINT, handle_sigint);

        int proxy_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (proxy_sock < 0) {
            fmt::println(stderr, "Socket creation failed");
            return;
        }

        int opt = 1;
        if (setsockopt(proxy_sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
            fmt::println("Failed to set socket opts");

        sockaddr_in proxy_addr{};
        proxy_addr.sin_family = AF_INET;
        proxy_addr.sin_port = htons(m_Port);
        proxy_addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(proxy_sock, (struct sockaddr *)&proxy_addr, sizeof(proxy_addr)) < 0) {
            fmt::println(stderr, "Bind failed");
            close(proxy_sock);
            return;
        }

        if (listen(proxy_sock, 10) < 0) {
            fmt::println(stderr, "Listen failed");
            close(proxy_sock);
            return;
        }

        fmt::println("Proxy listening on port {}", m_Port);

        s_ProxySock = proxy_sock;
        s_Running = true;

        while (s_Running) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            int client_sock = accept(proxy_sock, (struct sockaddr *)&client_addr, &client_len);
            if (client_sock < 0) {
                fmt::println(stderr, "Accept failed");
                continue;
            }

            fmt::println("Accepted connection");
            m_Clients.push_back(std::thread(&Proxy::HandleConnection, this, client_sock));
        }

        close(proxy_sock);
    }

    Proxy::~Proxy() {
        for (auto &c : m_Clients) {
            if (c.joinable()) {
                c.join();
            }
        }
    }
} // namespace dfs
