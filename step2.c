/**
 * @file step2.c
 * @author David Pescariu; Alexandro Vidican [g=0 s=1 e=8 / x=0 y=8]
 * @brief Internet Protocols Mini-Project - 2026
 *
 * ## Resources
 *  - Requirements: https://el.el.obs.utcluj.ro/pi/pdf/miniproject_ip_2026.pdf
 *  - Reference: https://el.el.obs.utcluj.ro/pi/pdf/Lab_5.pdf
 *  - Webpage: https://el.el.obs.utcluj.ro/pi/en_index.htm
 */

#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// ---------------------------------- Macros ----------------------------------

#define DEBUG 1
#define RECV_BUFFER_SIZE (1024 * 1024)

// ---------------------------------- Consts ----------------------------------

const char* fetch_url = "www.google.com";
const char* fetch_port = "80";
const char* expected_cmd = "08#";
const char* request = "GET / HTTP/1.0\r\n\r\n";
const char* output_file = "index.html";

// -------------------------------- Utilities ---------------------------------

/**
 * @brief Prints a debug message if `DEBUG` is enabled.
 * @param msg Message, shown like so: `[DEBUG] {msg}`
 */
void debug_msg(char* msg) {
    if (DEBUG) printf("[DEBUG] %s\n", msg);
}

/**
 * @brief Resolve the fetch_url into an IPv6 list of addresses, which can be
 * accessed via `result` and `result->ai_next` (until `ai_next` is null)
 *
 * @param result Stores the result from `getaddrinfo`. Upon calling, `getaddrinfo`
 * will update the pointer address itself, which is why we have a double pointer.
 *
 * @returns `0` on a successful query, `-1` otherwise.
 */
int get_server_info(struct addrinfo** result) {
    int status;
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;       // We want to resolve the IPv6 address of the server
    hints.ai_socktype = SOCK_STREAM;  // Stream Socket (TCP)

    status = getaddrinfo(fetch_url, fetch_port, &hints, result);
    if (status) {
        // If we have a non-zero status, getaddrinfo failed, and we can get a string
        // reprezentation of the error with gai_strerror:
        fprintf(stderr, "get_server_info error: %s\n", gai_strerror(status));
        return -1;
    }

    // Success, the data is returned via `result`.
    debug_msg("get_server_info: ok");
    return 0;
}

/**
 * @brief Prints the IPv6 address that we connected to.
 * @param used_addr_info The `addrinfo` we used to create the socket & connect.
 */
void print_server_address(struct addrinfo* used_addr_info) {
    char ip[INET6_ADDRSTRLEN];

    // Extract the IPv6 string from the addrinfo result:
    inet_ntop(
        AF_INET6,
        &((struct sockaddr_in6*)used_addr_info->ai_addr)->sin6_addr,
        // ^^ casting the pointer to the correct struct that contains the IPv6 info
        ip,
        sizeof(ip));

    printf("Connected to %s:%s on %s\n", fetch_url, fetch_port, ip);
}

// ----------------------------------- Main -----------------------------------

int main(void) {
    /** @brief (Reused) Stores status results from different operations. */
    int status;

    /** @brief The File Descriptor for the socket in use for the server calls. */
    int server_sockfd;

    struct addrinfo *addr_res, *curr_addr_res;
    addr_res = NULL;
    curr_addr_res = NULL;

    // 1. Get the remote server address:
    status = get_server_info(&addr_res);
    if (status) {
        // If this fetch fails, we cannot start the server, so quit now:
        fprintf(stderr, "Failed to get remote server address, quitting.\n");
        return EXIT_FAILURE;
    }

    // 2. Connect to the remote server
    for (curr_addr_res = addr_res; curr_addr_res != NULL; curr_addr_res = curr_addr_res->ai_next) {
        debug_msg("remote_server: attempting to create a socket");

        server_sockfd = socket(
            curr_addr_res->ai_family,
            curr_addr_res->ai_socktype,
            curr_addr_res->ai_protocol);

        if (server_sockfd == -1) {
            // We weren't able to open a socket with this address, check the next one:
            perror("Failed to open remote server socket");
            continue;
        }

        // Attempt to connect:
        debug_msg("remote_server: attempting to connect");
        if (connect(server_sockfd, curr_addr_res->ai_addr, curr_addr_res->ai_addrlen) == 0) {
            // Success, we can exit this loop (since we found an address that we
            // could connect to)
            debug_msg("remote_server: connected");
            break;
        }

        // We failed to connect, close this socket, then try the next address:
        perror("Failed to connect");
        close(server_sockfd);
        server_sockfd = -1;
    }

    // 2b. If we exit the loop and `curr_addr_res` is now NULL, it means we failed to
    // connect to all fetched addresses, and we can exit now, since we're unable to
    // fetch the page from the server.
    if (curr_addr_res == NULL) {
        fprintf(stderr, "Could not connect to the remote server, quitting.\n");
        freeaddrinfo(addr_res);
        return EXIT_FAILURE;
    }

    // 2c. Print the used address:
    print_server_address(curr_addr_res);

    size_t sent = 0;
    size_t request_len = strlen(request);

    while (sent < request_len) {
        ssize_t bytes_sent = send(
            server_sockfd,
            request + sent,
            request_len - sent,
            0);

        if (bytes_sent < 0) {
            debug_msg("send returned less than 0 bytes sent");
            perror("Failed to send request");
            close(server_sockfd);
            freeaddrinfo(addr_res);
            return EXIT_FAILURE;
        }

        if (bytes_sent == 0) {
            debug_msg("Connection was closed while sending the request");
            close(server_sockfd);
            freeaddrinfo(addr_res);
            return EXIT_FAILURE;
        }

        sent += (size_t)bytes_sent;
    }

    printf("GET request sent\n");

    FILE* out = fopen(output_file, "wb");
    if (out == NULL) {
        perror("Failed to open output file");
        close(server_sockfd);
        freeaddrinfo(addr_res);
        return EXIT_FAILURE;
    }

    char recv_buffer[RECV_BUFFER_SIZE];
    while (1) {
        ssize_t bytes_received = recv(
            server_sockfd,
            recv_buffer,
            sizeof(recv_buffer),
            0);

        if (bytes_received < 0) {
            debug_msg("recv returned less than 0 bytes");
            perror("Failed to receive data from remote server");
            close(server_sockfd);
            freeaddrinfo(addr_res);
            return EXIT_FAILURE;
        }

        if (bytes_received == 0) {
            break;
        }

        size_t bytes_written;

        /**
        bytes_written = fwrite(recv_buffer, 1, (size_t)bytes_received, stdout);
        if (bytes_written != (size_t)bytes_received) {
                perror("Could not write the result to stdout");
                close(server_sockfd);
                freeaddrinfo(addr_res);
                return EXIT_FAILURE;
        }
        */

        bytes_written = fwrite(recv_buffer, 1, (size_t)bytes_received, out);
        if (bytes_written != (size_t)bytes_received) {
            perror("Could not write the result to the file");
            close(server_sockfd);
            freeaddrinfo(addr_res);
            return EXIT_FAILURE;
        }
    }

    printf("Wrote output file %s\n", output_file);
    fclose(out);
    close(server_sockfd);
    debug_msg("remote_server: closed the socket");
    freeaddrinfo(addr_res);

    return EXIT_SUCCESS;
}