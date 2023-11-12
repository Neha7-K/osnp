#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define NAMING_SERVER_PORT 8080
#define MAX_STORAGE_SERVERS 10

struct StorageServerInfo {
    char ip_address[16];
    int nm_port;
    int client_port;
    char accessible_paths[1024];
};

struct StorageServer {
    struct StorageServerInfo info;
};

struct StorageServer storage_servers[MAX_STORAGE_SERVERS];
int num_storage_servers = 0;

void processStorageServerInfo(const struct StorageServerInfo *ss_info) {
    if (num_storage_servers < MAX_STORAGE_SERVERS) {
        memcpy(&storage_servers[num_storage_servers].info, ss_info, sizeof(struct StorageServerInfo));
        num_storage_servers++;
        printf("Received information for SS:\n");
        printf("IP Address: %s\n", ss_info->ip_address);
        printf("NM Port: %d\n", ss_info->nm_port);
        printf("Client Port: %d\n", ss_info->client_port);
        printf("Accessible Paths:\n%s\n", ss_info->accessible_paths);
    } else {
        printf("Storage server array is full. Cannot store information for SS: %s:%d\n", ss_info->ip_address, ss_info->nm_port);
    }
}

int findStorageServerPort(const char *path, int *port) {
    // Search for the path in the list of accessible_paths in storage_servers
    for (int i = 0; i < num_storage_servers; i++) {
        if (strstr(storage_servers[i].info.accessible_paths, path) != NULL) {
            *port = storage_servers[i].info.client_port;
            return 1; // Path found
        }
    }
    return 0; // Path not found
}

int main() {
    int ns_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (ns_socket == -1) {
        perror("Socket creation failed");
        exit(1);
    }

    struct sockaddr_in ns_address;
    ns_address.sin_family = AF_INET;
    ns_address.sin_port = htons(NAMING_SERVER_PORT);
    ns_address.sin_addr.s_addr = INADDR_ANY;

    if (bind(ns_socket, (struct sockaddr *)&ns_address, sizeof(ns_address)) == -1) {
        perror("Bind failed");
        close(ns_socket);
        exit(1);
    }

    if (listen(ns_socket, 5) == -1) {
        perror("Listen failed");
        close(ns_socket);
        exit(1);
    }

    while (1) {
        int ss_socket = accept(ns_socket, NULL, NULL);

        char request_type;
        if (recv(ss_socket, &request_type, sizeof(request_type), 0) <= 0) {
            perror("Receiving request type from client failed");
            close(ss_socket);
            continue;
        }

        if (request_type == 'I') { // 'I' indicates information about a storage server
            struct StorageServerInfo ss_info;
            if (recv(ss_socket, &ss_info, sizeof(ss_info), 0) <= 0) {
                perror("Receiving storage server info failed");
                close(ss_socket);
                continue;
            }
            processStorageServerInfo(&ss_info);
        } else if (request_type == 'P') { // 'P' indicates a path request from a client
            char path[1024];
            if (recv(ss_socket, path, sizeof(path), 0) <= 0) {
                perror("Receiving path from client failed");
                close(ss_socket);
                continue;
            }
            int storage_server_port;
            if (findStorageServerPort(path, &storage_server_port)) {
                 printf("%s\n",path);
                if (send(ss_socket, &storage_server_port, sizeof(storage_server_port), 0) == -1) {
                    perror("Sending storage server port to client failed");
                }
            } else {
                printf("%s\n",path);
                storage_server_port = -1;
                if (send(ss_socket, &storage_server_port, sizeof(storage_server_port), 0) == -1) {
                    perror("Sending error message to client failed");
                }
            }
        }

        close(ss_socket);
    }

    close(ns_socket);
    return 0;
}