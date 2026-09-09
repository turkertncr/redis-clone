#include <stdio.h>
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

static size_t build_command(char *buf, size_t capacity, int argc, char **argv) {
    size_t len = append_fmt(buf, capacity, 0, "*%d\r\n", argc - 1);
    for (int i = 0; i < argc; i++) {
        sds val = sdsnew(argv[i]);
        len = append_b(buf, capacity, len, val);
        sdsfree(val);
    }
    return len;
}

int run_client() {
    SOCKET sock;
    struct sockaddr_in server_addr;
    char buf[16384];
    char out[16384];
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

    while (1) {
        fgets(buf, sizeof(buf), stdin);
        buf[strcspn(buf, "\n")] = '\0';

        struct split splitted = split_cmd(buf);
        if (splitted.len == 0) {
            free(splitted.args);
            continue;
        }

        if (strcmp(splitted.args[0], "quit") == 0) {
            free(splitted.args);
            break;
        }

        getall_ctx ctx = build_resp(splitted.args, splitted.len);
        send(sock, ctx.buf, (int) ctx.len, 0);

        int received = recv(sock, buf, sizeof(buf) - 1, 0);
        if (received > 0) {
            buf[received] = '\0';
            printf("Received: %s\n", buf);
        } else {
            printf("No response from server.\n");
        }
        free(splitted.args);
    }

    closesocket(sock);
    WSACleanup();

    return 0;
}