
/**
 * @file step3.c
 * @author David Pescariu; Alexandro Vidican [g=0 s=1 e=8 / x=0 y=8]
 * @brief Internet Protocols Mini-Project - 2026
 *
 * ## Resources
 *  - Requirements: https://el.el.obs.utcluj.ro/pi/pdf/miniproject_ip_2026.pdf
 *  - Reference: https://el.el.obs.utcluj.ro/pi/pdf/Lab_5.pdf
 *  - Webpage: https://el.el.obs.utcluj.ro/pi/en_index.htm
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// ---------------------------------- Macros ----------------------------------

#define DEBUG 1
#define RECV_BUFFER_SIZE 1024
#define MAX_COMMAND_SIZE 1024

// ---------------------------------- Consts ----------------------------------

const int server_port = 22018;
const char* expected_cmd = "08#";
const char* confirmation_msg = "Command recognized!\n";
const char* unknown_cmd_msg = "Command not implemented\n";

// -------------------------------- Utilities ---------------------------------

/**
 * @brief Prints a debug message if `DEBUG` is enabled.
 * @param msg Message, shown like so: `[DEBUG] {msg}`
 */
void debug_msg(const char* msg) {
    if (DEBUG) printf("[DEBUG] %s\n", msg);
}

/**
 * @brief Sends all bytes from a buffer through a TCP socket.
 *
 * @param sockfd Socket file descriptor.
 * @param data Data to send.
 * @param len Number of bytes to send.
 *
 * @returns `0` on success, `-1` otherwise.
 */
int send_all(int sockfd, const char* data, size_t len) {
    size_t sent = 0;

    while (sent < len) {
        ssize_t bytes_sent = send(
            sockfd,
            data + sent,
            len - sent,
            0);

        if (bytes_sent < 0) {
            debug_msg("send returned less than 0 bytes sent");
            perror("Failed to send response");
            return -1;
        }

        if (bytes_sent == 0) {
            debug_msg("Connection was closed while sending");
            return -1;
        }

        sent += (size_t)bytes_sent;
    }

    return 0;
}

/**
 * @brief Receives a command from the connected IPv4 client.
 *
 * The command is considered complete when the '#' character
 * is received. The resulting string is null-terminated.
 *
 * @param sockfd Connected client socket.
 * @param command Buffer where the command will be stored.
 * @param command_size Maximum capacity of the command buffer.
 *
 * @returns `0` on success, `-1` on error or incomplete command.
 */
int receive_command(int sockfd, char* command, size_t command_size) {
    char recv_buffer[RECV_BUFFER_SIZE];
    size_t command_len = 0;

    while (command_len < command_size - 1) {
        ssize_t bytes_received = recv(
            sockfd,
            recv_buffer,
            sizeof(recv_buffer),
            0);

        if (bytes_received < 0) {
            debug_msg("recv returned less than 0 bytes");
            perror("Failed to receive command");
            return -1;
        }

        if (bytes_received == 0) {
            debug_msg("Client disconnected before completing command");
            return -1;
        }

        // Process the received bytes until '#' is encountered:
        for (ssize_t i = 0; i < bytes_received; i++) {
            command[command_len++] = recv_buffer[i];

            if (recv_buffer[i] == '#') {
                command[command_len] = '\0';
                debug_msg("Received a complete command");
                return 0;
            }

            if (command_len >= command_size - 1) {
                break;
            }
        }
    }

    command[command_len] = '\0';
    fprintf(stderr, "Command exceeded maximum allowed size\n");
    return -1;
}

/**
 * @brief Processes the received command and sends the appropriate
 * response to the connected IPv4 client.
 *
 * @param sockfd Connected client socket.
 * @param command Received command.
 *
 * @returns `0` on success, `-1` otherwise.
 */
int process_command(int sockfd, const char* command) {
    debug_msg("process_command: started");

    printf("Received command: %s\n", command);

    if (strcmp(command, expected_cmd) == 0) {
        debug_msg("Recognized command");

        return send_all(
            sockfd,
            confirmation_msg,
            strlen(confirmation_msg));
    }

    debug_msg("Unrecognized command");

    return send_all(
        sockfd,
        unknown_cmd_msg,
        strlen(unknown_cmd_msg));
}

// ----------------------------------- Main -----------------------------------

int main(void) {
    /** @brief Stores status results from different operations. */
    int status;

    /** @brief File descriptor for the IPv4 server socket. */
    int server_sockfd;

    /** @brief File descriptor for the currently connected client. */
    int client_sockfd;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    socklen_t client_addr_len;

    char command[MAX_COMMAND_SIZE];

    // 1. Create an IPv4 TCP socket:
    debug_msg("server: creating IPv4 TCP socket");

    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_sockfd == -1) {
        perror("Failed to create IPv4 server socket");
        return EXIT_FAILURE;
    }

    // 2. Allow immediate reuse of the server port:
    int opt = 1;

    status = setsockopt(
        server_sockfd,
        SOL_SOCKET,
        SO_REUSEADDR,
        &opt,
        sizeof(opt));

    if (status == -1) {
        perror("Failed to set SO_REUSEADDR");
        close(server_sockfd);
        return EXIT_FAILURE;
    }

    // 3. Configure the server address:
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // 4. Bind the socket to the specified port:
    debug_msg("server: attempting to bind");

    status = bind(
        server_sockfd,
        (struct sockaddr*)&server_addr,
        sizeof(server_addr));

    if (status == -1) {
        perror("Failed to bind server socket");
        close(server_sockfd);
        return EXIT_FAILURE;
    }

    debug_msg("server: bind successful");

    // 5. Start listening for incoming connections:
    debug_msg("server: starting to listen");

    status = listen(server_sockfd, 5);

    if (status == -1) {
        perror("Failed to listen on server socket");
        close(server_sockfd);
        return EXIT_FAILURE;
    }

    printf("IPv4 server listening on port %d\n", server_port);

    // 6. Continuously accept incoming client connections:
    while (1) {
        client_addr_len = sizeof(client_addr);

        debug_msg("server: waiting for a client connection");

        client_sockfd = accept(
            server_sockfd,
            (struct sockaddr*)&client_addr,
            &client_addr_len);

        if (client_sockfd == -1) {
            perror("Failed to accept client connection");
            continue;
        }

        printf(
            "Client connected from %s:%d\n",
            inet_ntoa(client_addr.sin_addr),
            ntohs(client_addr.sin_port));

        // 7. Receive a command from the connected client:
        status = receive_command(
            client_sockfd,
            command,
            sizeof(command));

        if (status == 0) {
            // 8. Process the command and send a response:
            status = process_command(client_sockfd, command);

            if (status == -1) {
                debug_msg("Failed to send command response");
            }
        }

        // 9. Close the client socket:
        close(client_sockfd);
        debug_msg("client: closed the socket");
    }

    // The server normally runs indefinitely:
    close(server_sockfd);

    return EXIT_SUCCESS;
}
