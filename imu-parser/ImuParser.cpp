#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <arpa/inet.h>
#include <cstring>
#include <thread>
#include <chrono>

#define ADDR "127.0.0.1"
#define PORT 5005

// socket Handling
int sockfd;
static struct sockaddr_in client_addr;
static socklen_t client_len;

struct IMUPacket
{
    uint32_t packet_count;
    float x_rate_rdps;
    float y_rate_rdps;
    float z_rate_rdps;
};

int setupSocket()
{
    struct sockaddr_in server_addr;

    // create the socket file descriptor (choose protocol automatically)
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    // configure and bind the socket to localhost network
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ADDR);
    server_addr.sin_port = htons(PORT);
    bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));

    return 0;
}

int setupSerial(std::string port)
{
    // open the passed-in serial port with read/write permissions
    int fd = open(port.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1)
    {
        std::cerr << "Failed to open UART." << std::endl;
        return 1;
    }

    struct termios tty{};
    if (tcgetattr(fd, &tty) != 0)
    {
        perror("tcgetattr error");
        return -1;
    }

    // set the baud rate to 921600
    cfsetospeed(&tty, B921600);
    cfsetispeed(&tty, B921600);
    tty.c_cflag = CS8 | CLOCAL | CREAD;
    tty.c_iflag = IGNPAR;
    tty.c_oflag = 0;
    tty.c_lflag = 0;

    // flush the stream and begin processing
    tcflush(fd, TCIFLUSH);
    tcsetattr(fd, TCSANOW, &tty);
    return fd;
}

// byte swap a 32-bit integer
uint32_t swapEndian(uint32_t value)
{
    return ((value >> 24) & 0x000000FF) |
           ((value >> 8) & 0x0000FF00) |
           ((value << 8) & 0x00FF0000) |
           ((value << 24) & 0xFF000000);
}

// this function is called periodically and is used to read serial data
// coming from the simulated IMU device
bool readIMU(int fd, IMUPacket &packet)
{
    uint8_t buffer[100];
    const uint8_t header[4] = {0x7F, 0xF0, 0x1C, 0xAF};

    while (true)
    {
        // no data to read
        if (read(fd, buffer, 1) != 1)
        {
            std::cout << "no data to read" << std::endl;
            return false;
        }

        // check the first byte
        if (buffer[0] == header[0])
        {
            // attempt to get remaining 3 bytes of header
            if (read(fd, buffer + 1, 3) != 3)
                return false;

            // header matches, continue to read rest of message
            if (memcmp(buffer, header, 4) == 0)
                break;
        }
    }

    // read remaining 16 bytes in the buffer
    if (read(fd, buffer + 4, 16) != 16)
        return false;

    // convert packet count (uint32_t)
    memcpy(&packet.packet_count, buffer + 4, 4);
    packet.packet_count = ntohl(packet.packet_count);

    // convert floating point values
    uint32_t temp;
    memcpy(&temp, buffer + 8, 4);
    temp = ntohl(temp);
    memcpy(&packet.x_rate_rdps, &temp, 4);

    memcpy(&temp, buffer + 12, 4);
    temp = ntohl(temp);
    memcpy(&packet.y_rate_rdps, &temp, 4);

    memcpy(&temp, buffer + 16, 4);
    temp = ntohl(temp);
    memcpy(&packet.z_rate_rdps, &temp, 4);

    return true;
}

// called once valid data has been received, will broadcast mirrored data back out
void sendBroadcast(const IMUPacket &packet)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in serverAddr{};

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(5005);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    char buffer[100];
    snprintf(buffer, sizeof(buffer), "Packet %u | X=%.2f, Y=%.2f, Z=%.2f",
             packet.packet_count, packet.x_rate_rdps, packet.y_rate_rdps, packet.z_rate_rdps);

    sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    close(sock);
}

// processing thread that will continuously read in and broadcast IMU data packets
void imuThread(std::string port)
{
    int fd = setupSerial(port);
    if (fd == -1)
    {
        std::cout << "bad fd" << std::endl;
        return;
    }

    // loop with an 80 ms delay (12.5 Hz) and broadcast if packet was read successfully
    while (true)
    {
        IMUPacket packet;
        if (readIMU(fd, packet))
        {
            std::cout << "Packet Count: " << packet.packet_count << ", X: " << packet.x_rate_rdps
                      << ", Y: " << packet.y_rate_rdps << ", Z: " << packet.z_rate_rdps << std::endl;
            sendBroadcast(packet);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(80));
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << "<serial_port>" << std::endl;
        return 1;
    }

    std::string serialPort = argv[1];
    std::thread imuThreadObj(imuThread, serialPort);
    imuThreadObj.join();

    return 0;
}