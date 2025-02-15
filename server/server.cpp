#include "FSW.hpp"

Server::Server() : current_state(State::RESTARTING), command_counter(0), safe_mode_counter(0), invalid_counter(0),
                   start_time(std::chrono::system_clock::now()) {
    setup_socket();
}

Server::~Server() {
    close(sockfd);
}

void Server::setup_socket() {
    sockaddr_in server_addr{};
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(PORT);
    bind(sockfd, (sockaddr*)&server_addr, sizeof(server_addr));
}

void Server::send_msg(const std::string& msg) {
    std::cout << "Sending message: " << msg << std::endl;
    sendto(sockfd, msg.c_str(), msg.size(), 0, (sockaddr*)&client_addr, client_len);
}

std::string Server::state_to_string(State state) {
    static const std::unordered_map<State, std::string> state_map = {
        {State::RESTARTING, "RESTARTING"},
        {State::READY, "READY"},
        {State::SAFE_MODE, "SAFE MODE"},
        {State::BBQ_MODE, "BBQ MODE"}
    };
    return state_map.at(state);
}

Command Server::parse_command(const std::string& command_str) {
    static const std::unordered_map<std::string, Command> command_map = {
        {"SAFE_MODE_ENABLE", Command::SAFE_MODE_ENABLE},
        {"SAFE_MODE_DISABLE", Command::SAFE_MODE_DISABLE},
        {"SHOW_CMDS_RCVD", Command::SHOW_CMDS_RCVD},
        {"SHOW_NUM_SAFES", Command::SHOW_NUM_SAFES},
        {"SHOW_UPTIME", Command::SHOW_UPTIME},
        {"RESET_CMD_CNTR", Command::RESET_CMD_CNTR},
        {"SHUTDOWN", Command::SHUTDOWN}
    };
    return command_map.contains(command_str) ? command_map.at(command_str) : Command::INVALID;
}

void Server::handle_command(const std::string& command_str) {
    std::cout << "Processing command: " << command_str << std::endl;
    if (current_state == State::RESTARTING) {
        std::cout << "FSW still initializing" << std::endl;
        return;
    }

    Command command = parse_command(command_str);
    std::string response;
    bool cmd_valid = false;

    switch (command) {
        case Command::SAFE_MODE_ENABLE:
            if (current_state != State::BBQ_MODE && current_state != State::SAFE_MODE) {
                current_state = State::SAFE_MODE;
                safe_mode_counter++;
                response = "Safe Mode Enabled";
                cmd_valid = true;
            }
            break;
        case Command::SAFE_MODE_DISABLE:
            current_state = State::READY;
            response = "Safe Mode Disabled";
            cmd_valid = true;
            break;
        case Command::SHOW_CMDS_RCVD:
            response = "Number of commands: " + std::to_string(command_counter);
            cmd_valid = true;
            break;
        case Command::SHOW_NUM_SAFES:
            response = "Number of safe modes: " + std::to_string(safe_mode_counter);
            cmd_valid = true;
            break;
        case Command::SHOW_UPTIME:
            response = "System Up-time: " + std::to_string(
                std::chrono::duration<double>(std::chrono::system_clock::now() - start_time).count()) + " seconds";
            cmd_valid = true;
            break;
        case Command::RESET_CMD_CNTR:
            command_counter = 0;
            response = "Command Counter Reset: " + std::to_string(command_counter);
            cmd_valid = true;
            break;
        case Command::SHUTDOWN:
            response = "Shutdown Initiated. Current State: " + state_to_string(current_state);
            send_msg(response);
            exit(0);
        default:
            break;
    }

    if (!cmd_valid) {
        response = "Invalid Command Received/State Configuration";
        invalid_counter++;
        if (invalid_counter >= 5 && invalid_counter < 8) current_state = State::SAFE_MODE;
        else if (invalid_counter >= 8) current_state = State::BBQ_MODE;
    } else {
        invalid_counter = 0;
        command_counter++;
    }

    send_msg(response);
}

void Server::run() {
    std::cout << "Initializing... ";
    for (int i = 1; i <= 10; i++) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << i << ". ";
    }
    std::cout << "FSW Ready." << std::endl;

    current_state = State::READY;
    char buffer[64];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        recvfrom(sockfd, buffer, sizeof(buffer), 0, (sockaddr*)&client_addr, &client_len);
        handle_command(buffer);
        std::cout << "Current State: " << state_to_string(current_state) 
                  << "\tCmds Rcvd: " << command_counter 
                  << "\tInvalid Cmds: " << invalid_counter 
                  << "\tSafe Modes: " << safe_mode_counter << "\n\n";
    }
}

int main() {
    Server server;
    server.run();
    return 0;
}