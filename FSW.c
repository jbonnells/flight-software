#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

// The listening port on this machine
#define PORT 5005

// Available FSW states
typedef enum {
    STATE_RESTARTING,
    STATE_READY,
    STATE_SAFE_MODE,
    STATE_BBQ_MODE
} state_t;

// Available FSW commands
typedef enum {
    CMD_SAFE_MODE_ENABLE,
    CMD_SAFE_MODE_DISABLE,
    CMD_SHOW_CMDS_RCVD,
    CMD_SHOW_NUM_SAFES,
    CMD_SHOW_UPTIME,
    CMD_RESET_CMD_CNTR,
    CMD_SHUTDOWN,
    CMD_INVALID
} command_t;

// Socket Handling
int sockfd;
static struct sockaddr_in client_addr;
static socklen_t client_len;

// State machine variables
state_t current_state = STATE_RESTARTING;
time_t start_time;

// Counters
int command_counter = 0;
int safe_mode_counter = 0;
int invalid_counter = 0;

int socket_setup() {
    struct sockaddr_in server_addr;

    // Create the socket file descriptor (choose protocol automatically)
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    // Configure and bind the socket
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(PORT);
    bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));

    return 0;
}

int send_msg(char* data, size_t size) {
    printf("Sending message: %s\n", data);

    int bytes_sent = sendto(sockfd, data, size, 0, (struct sockaddr *)&client_addr, client_len);
    if (bytes_sent == -1)
        printf("Error sending bytes\n");

    return bytes_sent;
}

char* state_enum_to_str(state_t s) {
    switch (s) {
        case STATE_RESTARTING: return "RESTARTING";
        case STATE_READY:      return "READY";
        case STATE_SAFE_MODE:  return "SAFE MODE";
        case STATE_BBQ_MODE:   return "BBQ MODE";
        default: break;
    }
    return "INVALID";
}

command_t parse_command(const char* command_str) {
    if (strcmp(command_str, "SAFE_MODE_ENABLE") == 0)
        return CMD_SAFE_MODE_ENABLE;
    else if (strcmp(command_str, "SAFE_MODE_DISABLE") == 0)
        return CMD_SAFE_MODE_DISABLE;
    else if (strcmp(command_str, "SHOW_CMDS_RCVD") == 0)
        return CMD_SHOW_CMDS_RCVD;
    else if (strcmp(command_str, "SHOW_NUM_SAFES") == 0)
        return CMD_SHOW_NUM_SAFES;
    else if (strcmp(command_str, "SHOW_UPTIME") == 0)
        return CMD_SHOW_UPTIME;
    else if (strcmp(command_str, "RESET_CMD_CNTR") == 0)
        return CMD_RESET_CMD_CNTR;
    else if (strcmp(command_str, "SHUTDOWN") == 0)
        return CMD_SHUTDOWN;
    else
        return CMD_INVALID;
}

void handle_command(const char* command_str) {
    printf("Processing command: %s\n", command_str);

    // dont process further if system is still restarting
    if (current_state == STATE_RESTARTING) {
        printf("FSW still initializing\n");
        return;
    }

    command_t command = parse_command(command_str);
    char msg_buf[64]; // outgoing buffer
    bool cmd_valid = false;

    // state machine for handling incoming commands.
    //
    // if current state is not consistent with command requirement, cmd_valid does not get set
    // and an error message gets sent back to client along with invalid_counter incrementing.
    // if invalid_counter gets to 5 and 8, state transitions to SAFE and BBQ respectively.
    switch(command) {

        case CMD_SAFE_MODE_ENABLE:
            if (current_state != STATE_BBQ_MODE && current_state != STATE_SAFE_MODE) {
                current_state = STATE_SAFE_MODE;
                safe_mode_counter++;
                snprintf(msg_buf, sizeof(msg_buf), "Safe Mode Enabled");
                cmd_valid = true;
            }
            break;

        case CMD_SAFE_MODE_DISABLE:
            current_state = STATE_READY;
            snprintf(msg_buf, sizeof(msg_buf), "Safe Mode Disabled");
            cmd_valid = true;
            break;

        case CMD_SHOW_CMDS_RCVD:
            if (current_state != STATE_BBQ_MODE && current_state != STATE_SAFE_MODE) {
                snprintf(msg_buf, sizeof(msg_buf), "Number of commands: %d", command_counter);
                cmd_valid = true;
            }
            break;

        case CMD_SHOW_NUM_SAFES:
            if (current_state != STATE_BBQ_MODE) {
                snprintf(msg_buf, sizeof(msg_buf), "Number of safe modes: %d", safe_mode_counter);
                cmd_valid = true;
            }
            break;

        case CMD_SHOW_UPTIME:
            if (current_state != STATE_BBQ_MODE && current_state != STATE_SAFE_MODE) {
                snprintf(msg_buf, sizeof(msg_buf), "System Up-time: %f seconds", ((double) (time(NULL) - start_time)));
                cmd_valid = true;
            }
            break;

        case CMD_RESET_CMD_CNTR:
            if (current_state != STATE_BBQ_MODE && current_state != STATE_SAFE_MODE) {
                command_counter = 0;
                snprintf(msg_buf, sizeof(msg_buf), "Command Counter Reset: %d", command_counter);
                cmd_valid = true;
            }
            break;

        case CMD_SHUTDOWN:
            snprintf(msg_buf, sizeof(msg_buf), "Shutdown Initiated. Current State: %s", state_enum_to_str(current_state));
            send_msg(msg_buf, strlen(msg_buf)); // send message now before program exits
            close(sockfd);
            exit(0);

        case CMD_INVALID:
        default:
            break;
    }

    // check if command was validated, setting an error message if it wasnt
    if (!cmd_valid) {
        snprintf(msg_buf, sizeof(msg_buf), "Invalid Command Received/State Configuration");
        invalid_counter++;

        // check to see if the SAFE_MODE / BBQ_MODE conditions have hit
        if (invalid_counter >= 5 && invalid_counter < 8)
            current_state = STATE_SAFE_MODE;
        else if (invalid_counter >= 8)
            current_state = STATE_BBQ_MODE;

    }
    else {
        // command was valid, so reset the 'invalid' counter
        invalid_counter = 0;
        command_counter++;
    }

    // send the response back to the client
    send_msg(msg_buf, strlen(msg_buf));
}

int main() {
    // used to keep track of elapsed system time
    start_time = time(NULL);

    // 10s sleep for initialization. fflush to force console printing
    printf("Initializing... ");
    for (int i = 1; i <= 10; i++) {
        sleep(1);
        printf("%d. ", i);
        fflush(stdout);
    }
    printf("FSW Ready.\n");
    fflush(stdout);

    // set-up and bind to local socket
    socket_setup();

    // transition to READY state to start accepting commands
    current_state = STATE_READY;

    // begin processing loop. call recvfrom() and wait until we have incoming data.
    // Once data has been received, it is passed to handle_command() for parsing/processing.
    // The nominal exit for this loop/program is when a CMD_SHUTDOWN command is received and exit() is called.
    char buffer[64];
    while (true) {
        memset(buffer, 0, sizeof(buffer)); // clear data buffer before receive
        recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &client_len);
        handle_command(buffer);
        printf("Current State: %s\tCmds Rcvd: %d\tInvalid Cmds: %d\tSafe Modes: %d\n\n", state_enum_to_str(current_state), command_counter, invalid_counter, safe_mode_counter);
    }

    close(sockfd);
    return 0;
}