#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#define NAMING_SERVER_IP "127.0.0.1"
#define NAMING_SERVER_PORT 8080
#define STORAGE_sERVER_IP ""
void communicateWithStorageServer(int storage_server_port, const char *file_path)
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
    char message[] = "WRITE";
    if (send(ss_socket, message, sizeof(message), 0) == -1)
    {
        perror("Sending message to storage server failed");
        close(ss_socket);
        exit(1);
    }
    char message2[] = "Hell";
    sendMessageToServer(ss_socket, message2);
    // handleReadCommand(ss_socket, file_path);

    close(ss_socket);
}

void handleReadCommand(int socket, const char *file_path)
{
    // Receive the file size
    long file_size;
    if (recv(socket, &file_size, sizeof(file_size), 0) == -1)
    {
        perror("Receiving file size failed");
        close(socket);
        return;
    }

    // Allocate a buffer to store the file content
    char *file_content = (char *)malloc(file_size);
    if (!file_content)
    {
        perror("Memory allocation failed");
        close(socket);
        return;
    }

    // Receive the file content in chunks
    size_t total_bytes_received = 0;
    while (total_bytes_received < file_size)
    {
        size_t bytes_received = recv(socket, file_content + total_bytes_received, file_size - total_bytes_received, 0);
        if (bytes_received == -1)
        {
            perror("Receiving file content failed");
            free(file_content);
            close(socket);
            return;
        }
        total_bytes_received += bytes_received;
    }

    // Now, 'file_content' contains the received file content
    // Print the file content
    printf("Received file content for path '%s':\n%s\n", file_path, file_content);

    // Don't forget to free the allocated memory
    free(file_content);
}
// Add a function to send a message to the server
void sendMessageToServer(int socket, const char *message)
{
    
    long message_size = strlen(message) + 1; // Include the null terminator
    if (send(socket, &message_size, sizeof(message_size), 0) == -1)
    {
        perror("Sending message size to server failed");
        close(socket);
        exit(1);
    }

    // Send the message content in chunks
    size_t chunk_size = 4096; // Adjust the chunk size as needed
    size_t total_bytes_sent = 0;

    while (total_bytes_sent < message_size)
    {
        size_t remaining_bytes = message_size - total_bytes_sent;
        size_t current_chunk_size = remaining_bytes < chunk_size ? remaining_bytes : chunk_size;

        if (send(socket, message + total_bytes_sent, current_chunk_size, 0) == -1)
        {
            perror("Sending message content to server failed");
            close(socket);
            exit(1);
        }

        total_bytes_sent += current_chunk_size;
    }
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

    char path[] = "./1.c";
    if (send(client_socket, path, strlen(path) + 1, 0) == -1)
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
        communicateWithStorageServer(storage_server_port, path);
    }
    else
    {
        printf("Path '%s' not found on any storage server.\n", path);
    }

    close(client_socket);

    return 0;
}
