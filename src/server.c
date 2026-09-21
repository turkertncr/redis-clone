#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

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
#include <include/repl.h>

void handle_client_request(const SOCKET* client_sock, resp_object* resp_obj, hash_table* ht, client_ctx* client)
{
    char response[16384];
    execute_command(resp_obj, ht, client, response, sizeof(response));

    if (response[0] != '\0') {
        send(*client_sock, response, (size_t) strlen(response), 0);
    }
}

void drop_client(struct pollfd *fds, client_ctx *clients, int *nfds, int i) {
    closesocket(fds[i].fd);
    fds[i] = fds[*nfds - 1];
    clients[i] = clients[*nfds - 1];
    fds[*nfds - 1].fd = -1;
    (*nfds)--;
}

int init_socket(hash_table *ht)
{
    if (ht == NULL) return 1;

    SOCKET server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    struct pollfd fds[MAX_CONNECTIONS + 1];
    client_ctx clients[MAX_CONNECTIONS + 1];
    char buff[16384];
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

    int opt = 1;
#ifdef _WIN32
    // Windows için (Gerekirse SO_REUSEADDR yerine direkt geçerlidir)
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
#else
    // Linux/WSL için mutlak çözüm
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEADDR failed");
    }
#endif

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

    listen(server_socket, 128);
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

            if (nfds > MAX_CONNECTIONS) {
                printf("Server full. Rejecting connection.\n");
                closesocket(client_socket);
            } else {
            #ifdef _WIN32
                u_long mode = 1;
                ioctlsocket(client_socket, FIONBIO, &mode);
            #else
                int flags = fcntl(client_socket, F_GETFL, 0);
                fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);
            #endif
                fds[nfds].fd = client_socket;
                fds[nfds].events = POLLIN;
                fds[nfds].revents = 0;
                clients[nfds].authenticated = !auth_required();
                nfds++;
            }
        }

        for (int i = 1; i < nfds; i++) {
            struct pollfd *f = &fds[i];

            if (f->revents & POLLIN) {
                long read_bytes = recv(f->fd, buff, sizeof(buff) - 1, 0);
                if (read_bytes <= 0) {
                    drop_client(fds, clients, &nfds, i--);
                } else {
                    buff[read_bytes] = '\0';

                    resp_object *resp_obj = 0;
                    if (buff[0] != '*' && buff[0] != '$') {
                        struct split splitted = split_cmd(buff);
                        getall_ctx ctx = build_resp(splitted.args, splitted.len);
                        resp_obj = parse_resp(ctx.buf, ctx.buf + ctx.len);
                        free(ctx.buf);
                        free(splitted.args);
                    } else {
                        resp_obj = parse_resp(buff, buff + read_bytes);
                    }

                    if (resp_obj == NULL) {
                        drop_client(fds, clients, &nfds, i--);
                        continue;
                    }

                    if (resp_obj->type == RESP_nil) {
                        drop_client(fds, clients, &nfds, i--);
                        free(resp_obj);
                        continue;
                    }

                    size_t response_size = 16384;
                    char response[response_size];

                    execute_command(resp_obj, ht, &clients[i], response, response_size);

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
