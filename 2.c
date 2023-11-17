#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

#define STORAGE_SERVER_PORT 8888
#define NAMING_SERVER_IP "127.0.0.1"
#define NAMING_SERVER_PORT 8080
#define MAX_COMMAND_SIZE 1024
#define ACCESSIBLE_PATHS_INTERVAL 60 // Send accessible paths every 60 seconds
struct StorageServerInfo
{
    char ip_address[16];
    int nm_port;
    int client_port;
    char accessible_paths[4096];
};
// Function to recursively collect accessible paths
void collectAccessiblePaths(const char *dir_path, char *accessible_paths, int *pos, int size)
{
    DIR *dir = opendir(dir_path);
    if (!dir)
    {
        perror("Opening directory failed");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_REG || entry->d_type == DT_DIR)
        {
            struct stat statbuf;
            char full_path[512]; // Adjust this size as needed
            snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

            if (stat(full_path, &statbuf) == 0)
            {
                if (S_ISDIR(statbuf.st_mode))
                {
                    // It's a directory
                    if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
                    {
                        // Skip current and parent directory entries
                        int len = snprintf(accessible_paths + (*pos), size - (*pos), "%s\n", full_path);
                        (*pos) += len;
                        collectAccessiblePaths(full_path, accessible_paths, pos, size);
                    }
                }
                else if (S_ISREG(statbuf.st_mode))
                {
                    // It's a regular file
                    int len = snprintf(accessible_paths + (*pos), size - (*pos), "%s\n", full_path);
                    (*pos) += len;
                }
            }
        }
    }

    closedir(dir);
}
void handleReadCommand(const char *file_path, int client_socket)
{
    FILE *file = fopen(file_path, "r");
    if (file == NULL)
    {
        perror("Error opening file for reading");
        close(client_socket);
        return;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (send(client_socket, &file_size, sizeof(file_size), 0) == -1)
    {
        perror("Sending file size failed");
        fclose(file);
        close(client_socket);
        return;
    }

    char buffer[4096];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0)
    {
        if (send(client_socket, buffer, bytes_read, 0) == -1)
        {
            perror("Sending file content failed");
            fclose(file);
            close(client_socket);
            return;
        }
    }

    fclose(file);
}

// Modify the handleWriteCommand function to handle WRITE command
void handleWriteCommand(int socket, const char *file_path)
{
    // Receive the message size
    // Receive the message size
    long message_size;
    if (recv(socket, &message_size, sizeof(message_size), 0) == -1)
    {
        perror("Receiving message size failed");
        close(socket);
        return;
    }

    printf("%ld\n", message_size);
    // Allocate a buffer to store the message
    char *message = (char *)malloc(message_size);
    if (!message)
    {
        perror("Memory allocation failed");
        close(socket);
        return;
    }

    // Receive the message content in chunks
    size_t total_bytes_received = 0;
    while (total_bytes_received < message_size)
    {
        size_t bytes_received = recv(socket, message + total_bytes_received, message_size - total_bytes_received, 0);
        if (bytes_received == -1)
        {
            perror("Receiving message content failed");
            free(message);
            close(socket);
            return;
        }
        total_bytes_received += bytes_received;
    }

    // Now, 'message' contains the received message content
    // Write the message to the specified file
    FILE *file = fopen(file_path, "w");
    if (file == NULL)
    {
        perror("Error opening file for writing");
        free(message);
        close(socket);
        return;
    }

    fprintf(file, "%s", message);

    fclose(file);
    free(message);
    printf("Message written to file '%s'\n", file_path);
}

void createFile()
{
    FILE *file = fopen("new_ile.txt", "w");
    if (file == NULL)
    {
        perror("File creation failed");
    }
    fclose(file);
    printf("Empty file created: new_file.txt\n");
}

void createDirectory()
{
    if (mkdir("new_directory", 0777) == -1)
    {
        perror("Directory creation failed");
    }
    printf("Empty directory created: new_directory\n");
}

void sendStorageServerInfoToNamingServer(int ns_socket, const struct StorageServerInfo *ss_info)
{
    char request_type = 'I';
    if (send(ns_socket, &request_type, sizeof(request_type), 0) == -1)
    {
        perror("Sending request type to naming server failed");
        close(ns_socket);
        exit(1);
    }

    if (send(ns_socket, ss_info, sizeof(struct StorageServerInfo), 0) == -1)
    {
        perror("Sending storage server info to naming server failed");
        close(ns_socket);
        exit(1);
    }
}

int main()
{
    int ns_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (ns_socket == -1)
    {
        perror("Socket creation failed");
        exit(1);
    }

    struct sockaddr_in ns_address;
    ns_address.sin_family = AF_INET;
    ns_address.sin_port = htons(NAMING_SERVER_PORT);
    ns_address.sin_addr.s_addr = inet_addr(NAMING_SERVER_IP);

    if (connect(ns_socket, (struct sockaddr *)&ns_address, sizeof(ns_address)) == -1)
    {
        perror("Connection to the naming server failed");
        close(ns_socket);
        exit(1);
    }

    // Collect accessible paths
    char accessible_paths[4096];
    int current_pos = 0;
    collectAccessiblePaths(".", accessible_paths, &current_pos, sizeof(accessible_paths));
    accessible_paths[current_pos] = '\0';

    // Prepare storage server information
    struct StorageServerInfo ss_info;
    strcpy(ss_info.ip_address, "127.0.0.1"); // Update with the actual IP
    ss_info.nm_port = NAMING_SERVER_PORT;    // Update with the actual port
    ss_info.client_port = STORAGE_SERVER_PORT;
    strcpy(ss_info.accessible_paths, accessible_paths);

    // Send storage server information to the naming server
    sendStorageServerInfoToNamingServer(ns_socket, &ss_info);
    char command[10000];
    if (recv(ns_socket, command, sizeof(command), 0) == -1)
    {
        perror("Receiving command failed\n");
    }
    printf("Received command: %s\n", command);
    if (strcmp(command, "CREATE_FILE") == 0)
    {
        createFile();
    }
    else if (strcmp(command, "CREATE_DIRECTORY") == 0)
    {
        createDirectory();
    }
    else
    {
        printf("Invalid command\n");
    }
    // Close the naming server socket
    close(ns_socket);

    int ss_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (ss_socket == -1)
    {
        perror("Socket creation failed");
        exit(1);
    }

    struct sockaddr_in ss_address;
    ss_address.sin_family = AF_INET;
    ss_address.sin_port = htons(STORAGE_SERVER_PORT);
    ss_address.sin_addr.s_addr = INADDR_ANY;

    if (bind(ss_socket, (struct sockaddr *)&ss_address, sizeof(ss_address)) == -1)
    {
        perror("Bind failed");
        close(ss_socket);
        exit(1);
    }

    if (listen(ss_socket, 5) == -1)
    {
        perror("Listen failed");
        close(ss_socket);
        exit(1);
    }

    time_t last_accessible_paths_time = time(NULL);

    while (1)
    {
        int client_socket = accept(ss_socket, NULL, NULL);
        if (client_socket == -1)
        {
            perror("Accepting client connection failed");
            continue; // Continue to the next iteration to keep listening
        }

        char command[100000];

        if (recv(client_socket, command, sizeof(command), 0) == -1)
        {
            perror("Receiving command failed\n");
            close(client_socket);
            continue;
        }

        printf("Received command: %s\n", command);
        fflush(stdout);
        if (strncmp(command, "READ", 4) == 0)
        {
            // Handle the "READ" command
            // Assuming a specific file path for reading
            const char *file_path = "/home/user/Downloads/osnp-main/new_file.txt";
            handleReadCommand(file_path, client_socket);
        }
        if (strncmp(command, "WRITE", 5) == 0)
        {
            printf("ddd\n");
            const char *file_path = "/home/user/Downloads/osnp-main/new_file.txt";
            handleWriteCommand(client_socket, file_path);
        }
        close(client_socket);
    }

    // Close the server socket outside the loop
    close(ss_socket);

    return 0;
}
