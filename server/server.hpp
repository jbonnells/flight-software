// server.hpp
#ifndef SERVER_HPP
#define SERVER_HPP

#include <iostream>
#include <string>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

constexpr int PORT = 5005;

enum class State {
    RESTARTING,
    READY,
    SAFE_MODE,
    BBQ_MODE
};

enum class Command {
    SAFE_MODE_ENABLE,
    SAFE_MODE_DISABLE,
    SHOW_CMDS_RCVD,
    SHOW_NUM_SAFES,
    SHOW_UPTIME,
    RESET_CMD_CNTR,
    SHUTDOWN,
    INVALID
};

class Server {
public:
    Server();
    ~Server();
    void run();

private:
    int sockfd;
    sockaddr_in client_addr{};
    socklen_t client_len{};
    State current_state;
    int command_counter;
    int safe_mode_counter;
    int invalid_counter;
    std::chrono::time_point<std::chrono::system_clock> start_time;

    void setup_socket();
    void handle_command(const std::string& command_str);
    Command parse_command(const std::string& command_str);
    void send_msg(const std::string& msg);
    std::string state_to_string(State state);
};

#endif // SERVER_HPP