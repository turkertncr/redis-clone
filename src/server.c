#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <ws2tcpip.h>
#define poll(fds, nfds, timeout) WSAPoll((fds), (nfds), (timeout))
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket close
#define WSAGetLastError() errno
#define WSACleanup() ((void) 0)
#endif

#include <include/server.h>
#include <include/command_handler.h>

void handle_client_request(const SOCKET* client_sock, resp_object* resp_obj, hash_table* ht)
{
    char response[16384];
    execute_command(resp_obj, ht, response, sizeof(response));

    if (response[0] != '\0') {
        send(*client_sock, response, (size_t) strlen(response), 0);
    }
}

int init_socket(hash_table *ht)
{
    if (ht == NULL) return 1;

    SOCKET server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    struct pollfd fds[MAX_CONNECTIONS + 1];
    char buff[1024];
    int nfds = 1;
#ifdef _WIN32
    int client_size;

    WSADATA wsa;
    printf("Initializing Winsock...\n");
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        printf("Failed. Error Code : %d\n", WSAGetLastError());
        return 1;
    }
#else
    socklen_t client_size;
#endif

    if ((server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
        printf("Could not create socket : %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }
    printf("Socket created.\n");

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Bind failed with error code : %d\n", WSAGetLastError());
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }
    printf("Bind done.\n");

    listen(server_socket, 3);
    printf("Waiting for incoming connections on port %d...\n", PORT);

    client_size = sizeof(struct sockaddr_in);

    for (int i = 1; i <= MAX_CONNECTIONS; i++) {
        fds[i].fd = -1;
        fds[i].events = 0;
    }

    fds[0].fd = server_socket;
    fds[0].events = POLLIN;

    while (1) {
        int poll_result = poll(fds, nfds, -1);
        if (poll_result < 0) break;

        if (fds[0].revents & POLLIN) {
            if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_size)) < 0) {
                exit(EXIT_FAILURE);
            }

            int added = 0;
            for (int i = 1; i <= MAX_CONNECTIONS; i++) {
                if (fds[i].fd == -1) {
                    fds[i].fd = client_socket;
                    fds[i].events = POLLIN;
                    if (i >= nfds) {
                        nfds = i + 1;
                    }
                    added = 1;
                    break;
                }
            }

            if (!added) {
                printf("Server full. Rejecting connection.\n");
                closesocket(client_socket);
            }
        }

        for (int i = 1; i <= MAX_CONNECTIONS; i++) {
            struct pollfd *f = &fds[i];
            if (f->fd == -1) continue;

            if (f->revents & POLLIN) {
                size_t read_bytes = recv(f->fd, buff, sizeof(buff) - 1, 0);
                if (read_bytes <= 0) {
                    closesocket(f->fd);
                    f->fd = -1;
                } else {
                    buff[read_bytes] = '\0';
                    printf("Received: %s \n", buff);

                    resp_object *resp_obj = parse_resp(buff);
                    if (resp_obj == NULL) {
                        closesocket(f->fd);
                        f->fd = -1;
                        continue;
                    }

                    if (resp_obj->type == RESP_nil) {
                        closesocket(f->fd);
                        f->fd = -1;
                        free(resp_obj);
                        continue;
                    }

                    size_t response_size = 16384;
                    char response[response_size];

                    execute_command(resp_obj, ht, response, response_size);
                    printf("%s\n", response);

                    send(fds[i].fd, response, strlen(response), 0);
                    free_respobj(resp_obj);
                }
            }
        }
    }

    closesocket(server_socket);
    WSACleanup();

    return 0;
}