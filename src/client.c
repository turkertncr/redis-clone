#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <ws2tcpip.h>
#define INVALID_SOCKET_VAL INVALID_SOCKET
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
typedef int SOCKET;
#define INVALID_SOCKET_VAL (-1)
#define SOCKET_ERROR (-1)
#define closesocket close
#define WSAGetLastError() errno
#define WSACleanup() ((void) 0)
#endif

#include <include/client.h>
#include <include/server.h>
#include <include/command_handler.h>
#include <include/repl.h>

void clear_terminal() {
#if defined(_WIN32) || defined(_WIN64)
        system("cls");
#else
        system("clear");
#endif
}

int run_client() {
    SOCKET sock;
    struct sockaddr_in server_addr;
    char buf[16384];
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        printf("Failed to initialize Winsock: %d\n", WSAGetLastError());
        return 1;
    }
#endif

    if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET_VAL) {
        printf("Could not create socket: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Could not connect to server on port %d: %d\n", PORT, WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    sds pending = 0;
    resp_status status;
    while (1) {
        if (fgets(buf, sizeof(buf), stdin) == NULL) break;
        buf[strcspn(buf, "\r\n")] = '\0';

        struct split splitted = split_cmd(buf);
        if (splitted.len == 0) {
            free(splitted.args);
            continue;
        }

        if (strcmp(splitted.args[0], "quit") == 0) {
            free(splitted.args);
            break;
        }

        if (strcmp(splitted.args[0], "clear") == 0) {
            clear_terminal();
            continue;
        }

        sds req = build_resp(splitted.args, splitted.len);
        send(sock, req, (int) sdslen(req), 0);

        free(splitted.args);
        sdsfree(req);

        while (1) {
            int received = recv(sock, buf, sizeof(buf) - 1, 0);
            if (received > 0) {
                buf[received] = '\0';

                if (!pending) pending = sdsnewlen(buf, received);
                else pending = sdsappendlen(pending, buf, received);

                resp_object *resp = 0;
                sds next;
                status = parse_resp(pending, pending + sdslen(pending), &resp, &next);

                if (status == ERROR) {
                    sdsfree(pending);
                    pending = 0;
                    free_respobj(resp);
                    goto close;
                    return 1;
                }

                if (status == INCOMPLETE) { continue; }

                if (status == OK) {
                    printf("Received: %s\n", pending);
                    sdsfree(pending);
                    pending = 0;
                    free_respobj(resp);
                    break;
                }
            } else {
                printf("No response from server.\n");
                sdsfree(pending);
                pending = 0;
                goto close;
            }
        }
    }

    close:
        closesocket(sock);
        WSACleanup();

    return 0;
}