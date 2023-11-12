#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#define NAMING_SERVER_IP "127.0.0.1"
#define NAMING_SERVER_PORT 8080

int main() {
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Socket creation failed");
        exit(1);
    }

    struct sockaddr_in ns_address;
    ns_address.sin_family = AF_INET;
    ns_address.sin_port = htons(NAMING_SERVER_PORT);
    ns_address.sin_addr.s_addr = inet_addr(NAMING_SERVER_IP);

    if (connect(client_socket, (struct sockaddr *)&ns_address, sizeof(ns_address)) == -1) {
        perror("Connection to the naming server failed");
        close(client_socket);
        exit(1);
    }

    // Send the request type 'P' to the naming server
    char request_type = 'P';
    if (send(client_socket, &request_type, sizeof(request_type), 0) == -1) {
        perror("Sending request type to naming server failed");
        close(client_socket);
        exit(1);
    }

    // Send the path to the naming server
    char path[] = "./1.c"; // Change this to the desired path
    if (send(client_socket, path, sizeof(path), 0) == -1) {
        perror("Sending path to naming server failed");
        close(client_socket);
        exit(1);
    }

    int storage_server_port;
    if (recv(client_socket, &storage_server_port, sizeof(storage_server_port), 0) == -1) {
        perror("Receiving storage server port from naming server failed");
        close(client_socket);
        exit(1);
    }

    if (storage_server_port > 0) {
        printf("Storage server port for path '%s': %d\n", path, storage_server_port);
    } else {
        printf("Path '%s' not found on any storage server.\n", path);
    }

    close(client_socket);

    return 0;
}