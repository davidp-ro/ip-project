/**
 * @author David Pescariu; Alexandro Vidican [g=0 s=1 e=8 / x=0 y=8]
 * @brief Internet Protocols Mini-Project - 2026
 *
 * ## Resources
 *  - Requirements: https://el.el.obs.utcluj.ro/pi/pdf/miniproject_ip_2026.pdf
 *  - Reference: https://el.el.obs.utcluj.ro/pi/pdf/Lab_5.pdf
 *  - Webpage: https://el.el.obs.utcluj.ro/pi/en_index.htm
 */

#include <arpa/inet.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// ---------------------------------- Macros ----------------------------------

#define DEBUG 1
#define LISTEN_QUEUE_SIZE 5             // Pending connections queue, as in the lab reference
#define MAX_COMMAND_SIZE 1024           // Max command length, including '\0'
#define RECV_BUFFER_SIZE (1024 * 1024)  // Max HTTP reply size

// ---------------------------------- Consts ----------------------------------

const int server_port = 22018;
const char* expected_cmd = "08#";
const char* fetch_url = "www.google.com";
const char* fetch_port = "80";
const char* request = "GET / HTTP/1.0\r\n\r\n";
const char* output_file_name = "index.html";
const char* unknown_cmd_msg = "Command not implemented\n";
const char* fetch_failed_msg = "Could not retrieve the web page\n";

/** @brief Holds the HTTP reply; static -> stored on the heap. */
static char recv_buffer[RECV_BUFFER_SIZE];

// -------------------------------- Utilities ---------------------------------

/**
 * @brief Prints a debug message if `DEBUG` is enabled.
 * @param msg Message, shown like so: `[DEBUG] {msg}`
 */
void debug_msg(const char* msg) {
    if (DEBUG) printf("[DEBUG] %s\n", msg);
}

/**
 * @brief Closes a socket, reporting a failure.
 * @param sockfd Socket file descriptor.
 * @param name Socket description, used in the error message.
 */
void close_socket(int sockfd, const char* name) {
    if (close(sockfd) == -1) {
        fprintf(stderr, "Failed to close the %s socket: %s\n", name, strerror(errno));
    }
}

/**
 * @brief Sends all bytes from a buffer through a TCP socket.
 *
 * `MSG_NOSIGNAL` makes send() fail with `EPIPE` when the peer has closed the connection before the
 * transmission is done, instead of raising `SIGPIPE`, to avoid crashing the server.
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
        ssize_t bytes_sent = send(sockfd, data + sent, len - sent, MSG_NOSIGNAL);

        if (bytes_sent < 0) {
            perror("Failed to send data");
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
 * @brief Builds the path of the saved page: `$HOME/index.html`.
 *
 * @param path Buffer where the path is stored.
 * @param path_size Capacity of the buffer.
 *
 * @returns `0` on success, `-1` otherwise.
 */
int build_output_path(char* path, size_t path_size) {
    const char* home = getenv("HOME");

    if (home == NULL) {
        fprintf(stderr, "HOME is not set\n");
        return -1;
    }

    // Safely write the path:
    int written = snprintf(path, path_size, "%s/%s", home, output_file_name);

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "Output file path is too long\n");
        return -1;
    }

    return 0;
}

/**
 * @brief Saves the page to a file.
 *
 * @param path Path of the file.
 * @param data Page contents.
 * @param len Number of bytes to write.
 *
 * @returns `0` on success, `-1` otherwise.
 */
int save_page(const char* path, const char* data, size_t len) {
    FILE* out = fopen(path, "wb");

    if (out == NULL) {
        fprintf(stderr, "Failed to open %s: %s\n", path, strerror(errno));
        return -1;
    }

    if (fwrite(data, 1, len, out) != len) {
        fprintf(stderr, "Failed to write %s: %s\n", path, strerror(errno));
        if (fclose(out) == EOF) perror("Failed to close the output file");
        return -1;
    }

    // fclose() flushes the buffered data, so write errors can also show up here:
    if (fclose(out) == EOF) {
        fprintf(stderr, "Failed to close %s: %s\n", path, strerror(errno));
        return -1;
    }

    printf("Saved the page to %s (%zu bytes)\n", path, len);
    return 0;
}

// ------------------------------- IPv6 client --------------------------------

/**
 * @brief Resolve the fetch_url into an IPv6 list of addresses, which can be accessed via `result`
 * and `result->ai_next` (until `ai_next` is null)
 *
 * @param result Stores the result from `getaddrinfo`. Upon calling, `getaddrinfo` will update the
 * pointer address itself, which is why we have a double pointer.
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
        // representation of the error with gai_strerror:
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
void print_server_address(const struct addrinfo* used_addr_info) {
    char ip[INET6_ADDRSTRLEN];

    // Extract the IPv6 string from the addrinfo result:
    const char* res = inet_ntop(
        AF_INET6,
        &((struct sockaddr_in6*)used_addr_info->ai_addr)->sin6_addr,
        // ^^ casting the pointer to the correct struct that contains the IPv6 info
        ip,
        sizeof(ip));

    if (res == NULL) {
        perror("Failed to convert the remote server address");
        return;
    }

    printf("Connected to %s:%s on [%s]\n", fetch_url, fetch_port, ip);
}

/**
 * @brief Connects to the first reachable address from the list.
 *
 * @param addr_list Result of `get_server_info`.
 *
 * @returns The connected socket, or `-1` if no address could be reached.
 */
int connect_to_remote_server(const struct addrinfo* addr_list) {
    const struct addrinfo* curr;

    for (curr = addr_list; curr != NULL; curr = curr->ai_next) {
        debug_msg("remote_server: attempting to create a socket");

        int sockfd = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol);

        if (sockfd == -1) {
            // We weren't able to open a socket with this address, check the next one:
            perror("Failed to open remote server socket");
            continue;
        }

        debug_msg("remote_server: attempting to connect");

        if (connect(sockfd, curr->ai_addr, curr->ai_addrlen) == 0) {
            debug_msg("remote_server: connected");
            print_server_address(curr);
            return sockfd;
        }

        // We failed to connect, close this socket, then try the next address:
        perror("Failed to connect");
        close_socket(sockfd, "remote server");
    }

    fprintf(stderr, "Could not connect to %s\n", fetch_url);
    return -1;
}

/**
 * @brief Receives the HTTP reply into `recv_buffer`, until the server closes the connection.
 *
 * @param sockfd Connected socket.
 * @param response_len Set to the number of bytes received.
 *
 * @returns `0` on success, `-1` otherwise.
 */
int receive_response(int sockfd, size_t* response_len) {
    size_t len = 0;

    while (1) {
        if (len == sizeof(recv_buffer)) {
            fprintf(stderr, "Response exceeds %zu bytes\n", sizeof(recv_buffer));
            return -1;
        }

        ssize_t bytes_received = recv(sockfd, recv_buffer + len, sizeof(recv_buffer) - len, 0);

        if (bytes_received < 0) {
            perror("Failed to receive data from remote server");
            return -1;
        }

        if (bytes_received == 0) break;

        len += (size_t)bytes_received;
    }

    printf("Received %zu bytes from %s\n", len, fetch_url);

    *response_len = len;
    return 0;
}

/**
 * @brief Locates the HTML inside the HTTP reply, by searching for the first empty line, which
 * indicates where the HTTP headers end.
 *
 * @param response_len Length of the reply in `recv_buffer`.
 * @param body Set to the start of the body.
 * @param body_len Set to the length of the body.
 *
 * @returns `0` on success, `-1` if the end of the headers is not found.
 */
int find_body(size_t response_len, const char** body, size_t* body_len) {
    for (size_t i = 0; i + 4 <= response_len; i++) {
        if (memcmp(recv_buffer + i, "\r\n\r\n", 4) == 0) {
            *body = recv_buffer + i + 4;
            *body_len = response_len - i - 4;
            return 0;
        }
    }

    fprintf(stderr, "Remote server reply has no end of headers (Malformed Response)\n");
    return -1;
}

/**
 * @brief Downloads the page from the HTTP IPv6 server into `recv_buffer`, by opening a new
 * connection every request.
 *
 * @param response_len Set to the length of the reply.
 * @returns `0` on success, `-1` otherwise.
 */
int fetch_page(size_t* response_len) {
    struct addrinfo* addr_list = NULL;
    int status = -1;

    if (get_server_info(&addr_list) == -1) return -1;

    int remote_sockfd = connect_to_remote_server(addr_list);
    freeaddrinfo(addr_list);

    if (remote_sockfd == -1) return -1;

    if (send_all(remote_sockfd, request, strlen(request)) == 0) {
        printf("GET request sent\n");
        status = receive_response(remote_sockfd, response_len);
    }

    close_socket(remote_sockfd, "remote server");
    debug_msg("remote_server: closed the socket");

    return status;
}

// ------------------------------- IPv4 server --------------------------------

/**
 * @brief Receives a command from the connected IPv4 client.
 *
 * @param sockfd Connected client socket.
 * @param command Buffer where the command will be stored.
 * @param command_size Maximum capacity of the command buffer.
 *
 * @returns `0` on success, `1` if the client disconnected, -1` on errors.
 */
int receive_command(int sockfd, char* command, size_t command_size) {
    size_t command_len = 0;
    char c;

    while (command_len < command_size - 1) {
        ssize_t bytes_received = recv(sockfd, &c, 1, 0);

        if (bytes_received < 0) {
            perror("Failed to receive command");
            return -1;
        }

        if (bytes_received == 0) {
            if (command_len == 0) {
                debug_msg("Client disconnected");
                return 1;
            }

            debug_msg("Client disconnected before completing command");
            return -1;
        }

        command[command_len++] = c;

        if (c == '#') {
            command[command_len] = '\0';
            debug_msg("Received a complete command");
            return 0;
        }
    }

    command[command_len] = '\0';
    fprintf(stderr, "Command exceeded maximum allowed size\n");
    return -1;
}

/**
 * @brief Fetches the page, saves it and sends it to the client. If any step fails before sending,
 * the client receives the `fetch_failed_msg` message instead.
 *
 * @param sockfd Connected client socket.
 * @param output_path Path of the saved page.
 *
 * @returns `0` if the page or the error message was sent, `-1` otherwise.
 */
int fetch_save_and_send_remote_page(int sockfd, const char* output_path) {
    size_t response_len = 0;
    const char* body = NULL;
    size_t body_len = 0;

    if (fetch_page(&response_len) == -1 ||
        find_body(response_len, &body, &body_len) == -1 ||
        save_page(output_path, body, body_len) == -1) {
        // Fetching the page, parsing the body or saving the page failed -> send the failure msg
        fprintf(stderr, "Failed to retrieve the page, notifying the client\n");
        return send_all(sockfd, fetch_failed_msg, strlen(fetch_failed_msg));
    }

    // Otherwise, send the page to the client
    if (send_all(sockfd, body, body_len) == -1) return -1;

    printf("Sent the page to the client\n");
    return 0;
}

/**
 * @brief Processes the received command and sends the response to the connected IPv4 client.
 *
 * @param sockfd Connected client socket.
 * @param command Received command.
 * @param output_path Path of the saved page.
 *
 * @returns `0` on success, `-1` otherwise.
 */
int process_command(int sockfd, const char* command, const char* output_path) {
    debug_msg("process_command: started");

    printf("Received command: %s\n", command);

    // If this is the expected command, do our task:
    if (strcmp(command, expected_cmd) == 0) {
        debug_msg("Recognized command");
        return fetch_save_and_send_remote_page(sockfd, output_path);
    }

    debug_msg("Unrecognized command");

    // Otherwise, send the "Command not implemented" message:
    return send_all(sockfd, unknown_cmd_msg, strlen(unknown_cmd_msg));
}

/**
 * @brief Creates the listening IPv4 TCP socket.
 * @returns The socket, or `-1` if the server cannot be started.
 */
int create_server_socket(void) {
    struct sockaddr_in server_addr;

    // Create an IPv4 TCP socket:
    debug_msg("server: creating IPv4 TCP socket");

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("Failed to create IPv4 server socket");
        return -1;
    }

    // Allow binding the port again right after a restart:
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("Failed to set SO_REUSEADDR");
        close_socket(sockfd, "server");
        return -1;
    }

    // Configure the server address to listen on all (INADDR_ANY) IPv4 addresses of the server:
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);  // Binds on our given port
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the socket to the specified port:
    debug_msg("server: attempting to bind");
    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Failed to bind server socket");
        close_socket(sockfd, "server");
        return -1;
    }

    // Start listening for incoming connections:
    debug_msg("server: starting to listen");
    if (listen(sockfd, LISTEN_QUEUE_SIZE) == -1) {
        perror("Failed to listen on server socket");
        close_socket(sockfd, "server");
        return -1;
    }

    printf("IPv4 server listening on port %d\n", server_port);
    return sockfd;
}

/**
 * @brief Serves one client until it disconnects or an error occurs.
 * @param sockfd Connected client socket.
 * @param output_path Path of the saved page.
 */
void handle_client(int sockfd, const char* output_path) {
    char command[MAX_COMMAND_SIZE];

    while (1) {
        int status = receive_command(sockfd, command, sizeof(command));
        if (status == 1) {
            debug_msg("client: disconnected");
            break;
        }

        if (status == -1) {
            debug_msg("client: error receiving command");
            break;
        }

        if (process_command(sockfd, command, output_path) == -1) {
            debug_msg("client: failed to send command response");
            break;
        }
    }
}

// ----------------------------------- Main -----------------------------------

int main(void) {
    /** @brief File descriptor for the IPv4 server socket. */
    int server_sockfd;

    /** @brief Path of the saved page, `$HOME/index.html`. */
    char output_path[PATH_MAX];

    // Determine where the page will be saved, crash if unable to set correctly:
    if (build_output_path(output_path, sizeof(output_path)) == -1) {
        return EXIT_FAILURE;
    }
    printf("Pages will be saved to %s\n", output_path);

    // Create the listening socket:
    server_sockfd = create_server_socket();
    if (server_sockfd == -1) {
        return EXIT_FAILURE;  // Crash if we cannot bind to the socket
    }

    // Serve clients (one at a time):
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        char client_ip[INET_ADDRSTRLEN];

        debug_msg("server: waiting for a client connection");

        int client_sockfd = accept(
            server_sockfd,
            (struct sockaddr*)&client_addr,
            &client_addr_len);

        if (client_sockfd == -1) {
            perror("Failed to accept client connection");
            continue;
        }

        // Get the client IP for logging (will not crash/stop the request on failures)
        if (inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip)) == NULL) {
            perror("Failed to convert the client address");
        } else {
            printf("Client connected from %s:%d\n", client_ip, ntohs(client_addr.sin_port));
        }

        handle_client(client_sockfd, output_path);
        close_socket(client_sockfd, "client");
        debug_msg("client: closed the socket");
    }

    close_socket(server_sockfd, "server");
    return EXIT_SUCCESS;
}
