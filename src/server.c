#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "alloc.h"
#include "queue.h"
#include "redis_object.h"

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <ws2tcpip.h>
#define poll(fds, nfds, timeout) WSAPoll((fds), (nfds), (timeout))
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket close
#define WSAGetLastError() errno
#define WSACleanup() ((void) 0)
#endif

#ifdef _WIN32
#define WOULD_BLOCK(err) ((err) == WSAEWOULDBLOCK)
#else
#define WOULD_BLOCK(err) ((err) == EWOULDBLOCK || (err) == EAGAIN)
#endif

#define DBG(fmt, ...) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, __LINE__, __func__ )

#include <server.h>
#include <command_handler.h>
#include <repl.h>

void print_status(const resp_status *status) {
    DBG("status: ");
    switch (*status) {
        case OK: DBG("OK"); break;
        case INCOMPLETE: DBG("INCOMPLETE"); break;
        case ERROR: DBG("ERROR"); break;
        default: DBG("UNKNOWN"); break;
    }
    DBG("\n");
}

void drop_client(struct pollfd *fds, client_ctx *clients, int *nfds, int i) {
    sdsfree(clients[i].out);
    sdsfree(clients[i].pending);
    closesocket(fds[i].fd);
    fds[i] = fds[*nfds - 1];
    clients[i] = clients[*nfds - 1];
    fds[*nfds - 1].fd = -1;
    (*nfds)--;
}

void parse_inline(sds inline_command, resp_object **args_out, resp_status *status, sds *next) {
    size_t len = sdslen(inline_command);
    char *nl = memchr(inline_command, '\n', len);
    if (nl == NULL) {
        *status = INCOMPLETE;
        return;
    }

    char *line_end = nl;
    if (line_end > inline_command && line_end[-1] == '\r') line_end--;

    sds line = sdsnewlen(inline_command, line_end - inline_command);
    struct split splitted = split_cmd(line);
    sds resp = build_resp(splitted.args, splitted.len);
    sds discard = NULL;
    *status = parse_resp(resp, resp + sdslen(resp), args_out, &discard);
    sdsfree(resp);
    sdsfree(line);
    zfree(splitted.args);

    *next = nl + 1;
}

int is_inline(const sds in) {
    return in[0] == '*' || in[0] == '$';
}

static void propagate_ttl_correction(queue *q, hash_table *ht, sds key) {
    redis_object *obj = lookup_redis_object(ht, key);
    if (obj == NULL || obj->expires_at == -1) return;

    sds aof_pexpire = sdsempty();
    append_fmt(&aof_pexpire, "*3\r\n$9\r\nPEXPIREAT\r\n");
    append_b(&aof_pexpire, key);
    append_fmt(&aof_pexpire, "$%d\r\n%lld\r\n", snprintf(NULL, 0, "%lld", obj->expires_at), obj->expires_at);
    queue_push(q, aof_pexpire);
}

int init_socket(hash_table *ht, queue *q)
{
    if (!ht) return 1;

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
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
#else
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

            #ifdef _WIN32
                setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY, (const char *)&opt, sizeof(opt));
            #else
                if (setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0) {
                    perror("setsockopt TCP_NODELAY failed");
                }
            #endif
                fds[nfds].fd = client_socket;
                fds[nfds].events = POLLIN;
                fds[nfds].revents = 0;
                clients[nfds].authenticated = !auth_required();
                clients[nfds].pending = NULL;
                clients[nfds].out = NULL;
                nfds++;
            }
        }

        for (int i = 1; i < nfds; i++) {
            struct pollfd *f = &fds[i];
            client_ctx *client = &clients[i];

            if (f->revents & POLLOUT) {
                int client_dropped = 0;
                while (sdslen(client->out) > 0) {
                    long sent = send(fds[i].fd, client->out, sdslen(client->out), 0);
                    if (sent > 0) {
                        sds trim = sdsnewlen(client->out + sent, sdslen(client->out) - sent);
                        sdsfree(client->out);
                        client->out = trim;
                        continue;
                    }

                    if (WOULD_BLOCK(WSAGetLastError())) break;

                    sdsfree(client->out);
                    client->out = NULL;
                    drop_client(fds, clients, &nfds, i--);
                    client_dropped = 1;
                    break;
                }
                if (client_dropped) continue;

                if (sdslen(client->out) == 0) {
                    f->events &= ~POLLOUT;
                }
            }

            if (f->revents & POLLIN) {
                long read_bytes = recv(f->fd, buff, sizeof(buff) - 1, 0);
                if (read_bytes <= 0) {
                    drop_client(fds, clients, &nfds, i--);
                } else {
                    buff[read_bytes] = '\0';

                    resp_object *args = NULL;
                    resp_status status;

                    if (client->pending == NULL) client->pending = sdsnewlen(buff, read_bytes);
                    else client->pending = sdsappendlen(client->pending, buff, read_bytes);

                main:
                    sds next = NULL;
                    if (sdslen(client->pending) == 0) continue;
                    if (!is_inline(client->pending)) {
                        parse_inline(client->pending, &args, &status, &next);
                    } else {
                        status = parse_resp(client->pending, client->pending + sdslen(client->pending), &args, &next);
                    }

                    if (status == ERROR) {
                        fprintf(stderr, "PARSE ERROR on: %.*s\n", (int) sdslen(client->pending), client->pending);
                        sdsfree(client->pending);
                        drop_client(fds, clients, &nfds, i--);
                        continue;
                    }

                    if (status == INCOMPLETE) continue;

                    if (status == OK) {
                        sds out = sdsempty();
                        size_t offset = next - client->pending;

                        const command *cmd = execute_command(args, ht, &clients[i], &out);
                        if (cmd && (cmd->flags & CMD_WRITE) && out[0] != '-' && q) {
                            if (!(cmd->flags & CMD_TTL)) {
                                queue_push(q, sdsnewlen(client->pending, offset));
                            }
                            if (cmd->flags & (CMD_TTL | CMD_SET_TTL)) {
                                propagate_ttl_correction(q, ht, get_bulk_at(args, 1));
                            }
                        }

                        size_t len = sdslen(out);
                        if (client->out != NULL && sdslen(client->out) > 0) {
                            client->out = sdsappendlen(client->out, out, len);
                        } else {
                            long sent_bytes = send(fds[i].fd, out, len, 0);
                            if (sent_bytes < 0) {
                                if (WOULD_BLOCK(WSAGetLastError())) {
                                    if (client->out == NULL) client->out = sdsempty();
                                    client->out = sdsappendlen(client->out, out, len);
                                    f->events |= POLLOUT;
                                } else {
                                    sdsfree(out);
                                    free_respobj(args);
                                    drop_client(fds, clients, &nfds, i--);
                                    continue;
                                }
                            } else if ((size_t) sent_bytes < len) {
                                if (client->out == NULL) client->out = sdsempty();
                                client->out = sdsappendlen(client->out, out + sent_bytes, len - sent_bytes);
                                f->events |= POLLOUT;
                            }
                        }

                        sdsfree(out);
                        free_respobj(args);

                        if (next != NULL) {
                            size_t new_len = sdslen(client->pending) - offset;

                            sds tmp = sdsnewlen(next, new_len);
                            sdsfree(client->pending);
                            client->pending = tmp;
                            next = 0;
                            goto main;
                        }
                    }
                }
            }
        }
    }

    closesocket(server_socket);
    WSACleanup();

    return 0;
}
