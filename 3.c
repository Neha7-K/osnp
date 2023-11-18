#include "defs.h"

void communicateWithStorageServer(int storage_server_port)
{
    int ss_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (ss_socket == -1)
    {
        perror("Socket creation failed");
        exit(1);
    }

    struct sockaddr_in ss_address;
    ss_address.sin_family = AF_INET;
    ss_address.sin_port = htons(storage_server_port);
    ss_address.sin_addr.s_addr = inet_addr(NAMING_SERVER_IP); // Update this with the actual storage server IP

    if (connect(ss_socket, (struct sockaddr *)&ss_address, sizeof(ss_address)) == -1)
    {
        perror("Connection to the storage server failed");
        close(ss_socket);
        exit(1);
    }

    // Send a message to the storage server
    char message[] = "READ";
    if (send(ss_socket, message, sizeof(message), 0) == -1)
    {
        perror("Sending message to storage server failed");
        close(ss_socket);
        exit(1);
    }

    // Add any additional communication with the storage server as needed

    close(ss_socket);
}

int main()
{
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1)
    {
        perror("Socket creation failed");
        exit(1);
    }

    struct sockaddr_in ns_address;
    ns_address.sin_family = AF_INET;
    ns_address.sin_port = htons(NAMING_SERVER_PORT);
    ns_address.sin_addr.s_addr = inet_addr(NAMING_SERVER_IP);

    if (connect(client_socket, (struct sockaddr *)&ns_address, sizeof(ns_address)) == -1)
    {
        perror("Connection to the naming server failed");
        close(client_socket);
        exit(1);
    }

    // Send the request type 'P' to the naming server
    char request_type = 'P';
    if (send(client_socket, &request_type, sizeof(request_type), 0) == -1)
    {
        perror("Sending request type to naming server failed");
        close(client_socket);
        exit(1);
    }
    sleep(10);
    char path[] = "./1.c";
    if (send(client_socket, path, sizeof(path), 0) == -1)
    {
        perror("Sending path to naming server failed");
        close(client_socket);
        exit(1);
    }

    int storage_server_port;
    if (recv(client_socket, &storage_server_port, sizeof(storage_server_port), 0) == -1)
    {
        perror("Receiving storage server port from naming server failed");
        close(client_socket);
        exit(1);
    }

    if (storage_server_port > 0)
    {
        printf("Storage server port for path '%s': %d\n", path, storage_server_port);

        // Communicate with the storage server
        communicateWithStorageServer(storage_server_port);
    }
    else
    {
        printf("Path '%s' not found on any storage server.\n", path);
    }

    close(client_socket);

    return 0;
}
