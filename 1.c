#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define NAMING_SERVER_PORT 8080
#define MAX_STORAGE_SERVERS 10

struct StorageServerInfo {
    char ip_address[16];
    int nm_port;
    int client_port;
    char accessible_paths[1024];
};
struct ThreadArgs {
    int socket;
    char request_type;
};
struct StorageServer {
    struct StorageServerInfo info;
};

struct StorageServer storage_servers[MAX_STORAGE_SERVERS];
int num_storage_servers = 0;

pthread_mutex_t storage_servers_mutex = PTHREAD_MUTEX_INITIALIZER;

void processStorageServerInfo(const struct StorageServerInfo *ss_info) {
    pthread_mutex_lock(&storage_servers_mutex);

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

    pthread_mutex_unlock(&storage_servers_mutex);
}

void *handleStorageServer(void *arg) {
    struct ThreadArgs *thread_args = (struct ThreadArgs *)arg;
    int ss_socket = thread_args->socket;
    char request_type = thread_args->request_type;

    // Receive storage server information
    struct StorageServerInfo ss_info;
    if (recv(ss_socket, &ss_info, sizeof(ss_info), 0) <= 0) {
        perror("Receiving storage server info failed");
        close(ss_socket);
        free(thread_args);
        pthread_exit(NULL);
    }

    processStorageServerInfo(&ss_info);

    // Placeholder: Add your storage server processing logic here
    char command[] = "CREATE_FILE";
    if (send(ss_socket, &command, sizeof(command), 0) == -1) {
        perror("Sending command to storage server failed");
    }

    close(ss_socket);
    free(thread_args);
    pthread_exit(NULL);
}

int findStorageServerPort(const char *path, int *port);
void *handleClient(void *arg) {
    struct ThreadArgs *thread_args = (struct ThreadArgs *)arg;
    int client_socket = thread_args->socket;
    char request_type = thread_args->request_type;

    // Receive the path from the client
    printf("Entering handleClient\n");

    char path[1024];
    memset(path, 0, sizeof(path));  // Initialize the buffer to zero

    printf("Before recv\n");
    ssize_t bytes_received = recv(client_socket, path, sizeof(path), 0);
    printf("After recv\n");

    if (bytes_received <= 0) {
        perror("Receiving path from client failed");
        close(client_socket);
        free(thread_args);
        pthread_exit(NULL);
    }

    printf("Received path from client: %s\n", path);

    int storage_server_port = -1;

   

    // Use the findStorageServerPort function to get the storage server port
    printf("1234");
    if (findStorageServerPort(path, &storage_server_port)) {
        printf("Client requested path: %s\n", path);
        printf("Found storage server port: %d\n", storage_server_port);

        // Send the storage server port back to the client
        if (send(client_socket, &storage_server_port, sizeof(storage_server_port), 0) == -1) {
            perror("Sending storage server port to client failed");
        }
    } else {
        printf("Client requested path: %s\n", path);
        printf("Path not found in accessible paths\n");

        // Send an error message to the client
        storage_server_port = -1;
        if (send(client_socket, &storage_server_port, sizeof(storage_server_port), 0) == -1) {
            perror("Sending error message to client failed");
        }
    }

   

    // Placeholder: Add more client processing logic as needed

    close(client_socket);
    free(thread_args);
    printf("Exiting handleClient\n");
    pthread_exit(NULL);
}

int findStorageServerPort(const char *path, int *port) {
    pthread_mutex_lock(&storage_servers_mutex);

    // Search for the path in the list of accessible_paths in storage_servers
    for (int i = 0; i < num_storage_servers; i++) {
        if (strstr(storage_servers[i].info.accessible_paths, path) != NULL) {
            *port = storage_servers[i].info.client_port;
            pthread_mutex_unlock(&storage_servers_mutex);
            return 1; // Path found
        }
    }

    pthread_mutex_unlock(&storage_servers_mutex);
    return 0; // Path not found
}

int sendCommandToStorageServer(int storage_server_index, char command) {
    int ss_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (ss_socket == -1) {
        perror("Socket creation failed");
        return -1;
    }

    struct sockaddr_in ss_address;
    ss_address.sin_family = AF_INET;
    ss_address.sin_port = htons(storage_servers[storage_server_index].info.nm_port);
    ss_address.sin_addr.s_addr = inet_addr(storage_servers[storage_server_index].info.ip_address);

    if (connect(ss_socket, (struct sockaddr *)&ss_address, sizeof(ss_address)) == -1) {
        perror("Connection to the storage server failed");
        close(ss_socket);
        return -1;
    }

    if (send(ss_socket, &command, sizeof(command), 0) == -1) {
        perror("Sending command to storage server failed");
        close(ss_socket);
        return -1;
    }

    close(ss_socket);
    return 0;
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
        printf("reed\n");
        int client_socket = accept(ns_socket, NULL, NULL);
        if (client_socket == -1) {
            perror("Accepting client connection failed");
            continue; // Continue to the next iteration to keep listening
        }

        char request_type;
        if (recv(client_socket, &request_type, sizeof(request_type), 0) <= 0) {
            perror("Receiving request type from client failed");
            close(client_socket);
            continue;
        }

        struct ThreadArgs *thread_args = malloc(sizeof(struct ThreadArgs));
        thread_args->socket = client_socket;
        thread_args->request_type = request_type;

        // Create a thread based on the request type
        pthread_t thread;
        if (request_type == 'I') {
            if (pthread_create(&thread, NULL, handleStorageServer, thread_args) != 0) {
                perror("Failed to create storage server thread");
                free(thread_args);
                close(client_socket);
            }
        } else if (request_type == 'P') {
            if (pthread_create(&thread, NULL, handleClient, thread_args) != 0) {
                perror("Failed to create client thread");
                free(thread_args);
                close(client_socket);
            }
        } else {
            // Handle unknown request type
            free(thread_args);
            close(client_socket);
        }
    }

    // Cleanup and close the naming server socket
    close(ns_socket);
    return 0;
}
