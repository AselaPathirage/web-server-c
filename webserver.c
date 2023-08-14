// Index :  - Asela Pathirage

// compile : gcc webserver.c -o webserver.exe -lws2_32
// run : .\webserver.exe

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#define MAX_REQUEST_SIZE 2047

struct clientDetails {
    SOCKET socket;
    socklen_t address_length;
    struct sockaddr_storage address;
    char request[MAX_REQUEST_SIZE + 1];
    int received;
    struct clientDetails *next;
};

static struct clientDetails *clients = NULL;

struct clientDetails *getClient(SOCKET s) {
    struct clientDetails *clienti = clients;
    while (clienti) {
        if (clienti->socket == s)
            break;
        clienti = clienti->next;
    }
    if (clienti)
        return clienti;

    struct clientDetails *new = (struct clientDetails *)calloc(1, sizeof(struct clientDetails));
    new->address_length = sizeof(new->address);
    new->next = clients;
    clients = new;
    return new;
}

void removeClient(struct clientDetails *client) {
    closesocket(client->socket);
    struct clientDetails **p = &clients;
    while (*p) {
        if (*p == client) {
            *p = client->next;
            free(client);
            return;
        }
        p = &(*p)->next;
    }
    fprintf(stderr, "removeClient.\n");
    exit(1);
}

// errors
void error400(struct clientDetails *client) {
    const char *code400 =
        "HTTP/1.1 400 Bad Request\r\n"
        "Connection: close\r\n"
        "Content-Length: 30\r\n\r\n400 - Bad Request\nTry Again";
    send(client->socket, code400, strlen(code400), 0);
    removeClient(client);
}

void error404(struct clientDetails *client) {
    const char *code404 =
        "HTTP/1.1 404 Not Found\r\n"
        "Connection: close\r\n"
        "Content-Length: 18\r\n\r\n404 - Not Found :(";
    send(client->socket, code404, strlen(code404), 0);
    removeClient(client);
}

// File Handling
const char *contentType(const char *path) {
    const char *extension = strrchr(path, '.');
    if (extension) {
        if (strcmp(extension, ".css") == 0)
            return "text/css";
        if (strcmp(extension, ".gif") == 0)
            return "image/gif";
        if (strcmp(extension, ".html") == 0)
            return "text/html";
        if (strcmp(extension, ".jpeg") == 0)
            return "image/jpeg";
        if (strcmp(extension, ".jpg") == 0)
            return "image/jpeg";
        if (strcmp(extension, ".png") == 0)
            return "image/png";
        if (strcmp(extension, ".pdf") == 0)
            return "application/pdf";
        if (strcmp(extension, ".svg") == 0)
            return "image/svg+xml";
        if (strcmp(extension, ".txt") == 0)
            return "text/plain";
    }
    return "application/octet-stream";
}

int main() {
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data)) {
        printf("Failed wsastartup...\n");
        return 1;
    } else {
        printf("wsa done...\n");
    }

    printf("Configuring...\n");
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(8000);
    server_address.sin_addr.s_addr = INADDR_ANY;

    printf("Creating socket...\n");
    SOCKET socket_listen;
    socket_listen = socket(AF_INET, SOCK_STREAM, 0);

    printf("Binding socket to local address...\n");
    if (bind(socket_listen, (struct sockaddr *)&server_address, sizeof(server_address))) {  // bind() returns 0 on success
        printf("bind() failed.\n");
        return 1;
    }

    printf("Listening...\n");
    if (listen(socket_listen, 20) < 0) {
        printf("listen() failed. \n");
        return 1;
    }

    //---------------------------------------------------------------------------------

    while (1) {
        printf("Waiting for connection...\n");
        fd_set reads;
        FD_ZERO(&reads);
        FD_SET(socket_listen, &reads);
        SOCKET max_socket = socket_listen;
        struct clientDetails *ci = clients;

        while (ci) {
            FD_SET(ci->socket, &reads);
            if (ci->socket > max_socket)
                max_socket = ci->socket;
            ci = ci->next;
        }

        if (select(max_socket + 1, &reads, NULL, NULL, NULL) < 0) {
            printf("select() failed. \n");
            exit(EXIT_FAILURE);
        }

        if (FD_ISSET(socket_listen, &reads)) {
            struct clientDetails *client = getClient(-1);
            client->socket = accept(socket_listen, (struct sockaddr *)&(client->address), &(client->address_length));
        }

        ci = clients;
        while (ci) {
            struct clientDetails *next = ci->next;
            if (FD_ISSET(ci->socket, &reads)) {
                int bytesReceived = recv(ci->socket, ci->request + ci->received, MAX_REQUEST_SIZE - ci->received, 0);
                if (bytesReceived < 1) {
                    printf("Disconnect from client.\n");
                    removeClient(ci);
                } else {
                    ci->received += bytesReceived;
                    ci->request[ci->received] = 0;
                    char *q = strstr(ci->request, "\r\n\r\n");
                    if (q) {
                        *q = 0;
                        if (strncmp("GET /", ci->request, 5)) {
                            error400(ci);
                        } else {
                            char *path = ci->request + 4;
                            char *end_path = strstr(path, " ");
                            if (!end_path) {
                                error400(ci);
                            } else {
                                *end_path = 0;
                                if (strcmp(path, "/") == 0)
                                    path = "/index.html";
                                if (strlen(path) > 100) {
                                    error400(ci);
                                    continue;
                                }
                                char full_path[128];
                                sprintf(full_path, "%s", ++path);
                                char *p = full_path;
                                while (*p) {
                                    if (*p == '/')
                                        *p = '\\';
                                    ++p;
                                }
                                FILE *filePointer = fopen(full_path, "rb");
                                if (!filePointer) {
                                    error404(ci);
                                    continue;
                                }
                                fseek(filePointer, 0L, SEEK_END);
                                size_t cl = ftell(filePointer);
                                rewind(filePointer);
                                const char *ct = contentType(full_path);
                                char buffer[1024];

                                sprintf(buffer, "HTTP/1.1 200 OK\r\n");
                                send(ci->socket, buffer, strlen(buffer), 0);
                                sprintf(buffer, "Connection: close\r\n");
                                send(ci->socket, buffer, strlen(buffer), 0);
                                sprintf(buffer, "Content-Length: %u\r\n", cl);
                                send(ci->socket, buffer, strlen(buffer), 0);
                                sprintf(buffer, "Content-Type: %s\r\n", ct);
                                send(ci->socket, buffer, strlen(buffer), 0);
                                sprintf(buffer, "\r\n");
                                send(ci->socket, buffer, strlen(buffer), 0);
                                int r = fread(buffer, 1, 1024, filePointer);
                                while (r) {
                                    send(ci->socket, buffer, r, 0);
                                    r = fread(buffer, 1, 1024, filePointer);
                                }

                                fclose(filePointer);
                                removeClient(ci);
                            }
                        }
                    }
                }
            }
            ci = next;
        }
    }
    //------------------------------------------------

    printf("Closing listening socket...\n");
    closesocket(socket_listen);
    WSACleanup();
    printf("Finished.\n");

    return 0;
}