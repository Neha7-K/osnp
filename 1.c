#include "1.h"

pthread_mutex_t storage_servers_mutex = PTHREAD_MUTEX_INITIALIZER;

void processStorageServerInfo(const struct StorageServerInfo *ss_info)
{
    pthread_mutex_lock(&storage_servers_mutex);

    if (num_storage_servers < MAX_STORAGE_SERVERS)
    {
        memcpy(&storage_servers[num_storage_servers].info, ss_info, sizeof(struct StorageServerInfo));
        num_storage_servers++;
        printf("Received information for SS:\n");
        printf("IP Address: %s\n", ss_info->ip_address);
        printf("NM Port: %d\n", ss_info->nm_port);
        printf("Client Port: %d\n", ss_info->client_port);
        printf("Address of ss: %s\n", ss_info->absolute_address);
        printf("Accessible Paths:\n%s\n", ss_info->accessible_paths);
    }
    else
    {
        printf("Storage server array is full. Cannot store information for SS: %s:%d\n", ss_info->ip_address, ss_info->nm_port);
    }

    pthread_mutex_unlock(&storage_servers_mutex);
}

void *handleStorageServer(void *arg)
{
    struct ThreadArgs *thread_args = (struct ThreadArgs *)arg;
    char request_type = thread_args->request_type;
    int ss_socket = thread_args->socket;

    // Receive storage server information
    struct StorageServerInfo ss_info;
    if (recv(ss_socket, &ss_info, sizeof(ss_info), 0) <= 0)
    {
        perror("Receiving storage server info failed");
        close(ss_socket);
        free(thread_args);
        pthread_exit(NULL);
    }

    processStorageServerInfo(&ss_info);
    int count = 0;
    while (count != -1)
    {
        for (int i = 0; i < num_clients; i++)
        {
            if (ss_info.client_port == client_com[i].storageport)
            {
                count = 1;
                client_com[i].storageport = -1;
                if (send(ss_socket, &i, sizeof(i), 0) <= 0)
                {
                    perror("Receiving storage server info failed");
                    close(ss_socket);
                    free(thread_args);
                    pthread_exit(NULL);
                }
                if (send(ss_socket, client_com[i].command, sizeof(client_com[i].command), 0) <= 0)
                {
                    perror("Receiving storage server info failed");
                    close(ss_socket);
                    free(thread_args);
                    pthread_exit(NULL);
                }
            }
        }
        sleep(1);
    }
    free(thread_args);
    pthread_exit(NULL);
}
void copyFile(const char *source, const char *destination)
{
    FILE *source_file = fopen(source, "rb");
    if (!source_file)
    {
        perror("Error opening source file");
        return;
    }
    FILE *dest_file = fopen(destination, "wb");
    if (!dest_file)
    {
        perror("Error opening destination file");
        fclose(source_file);
        return;
    }

    char buffer[1024];
    size_t bytesRead;

    while ((bytesRead = fread(buffer, 1, sizeof(buffer), source_file)) > 0)
    {
        if (fwrite(buffer, 1, bytesRead, dest_file) != bytesRead)
        {
            perror("Error writing to destination file");
            break;
        }
    }
    fclose(source_file);
    fclose(dest_file);
}
void *handleClient(void *arg)
{
    struct ThreadArgs *thread_args = (struct ThreadArgs *)arg;
    int client_socket = thread_args->socket;
    char request_type = thread_args->request_type;

    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));

    ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    char a[1024];
    strcpy(a, buffer);
    char command[50];
    char path[974];
    char *token = strtok(a, " ");
    int storage_server_port = -1;
    strcpy(command, token);
    if (strcmp(command, "COPY") == 0)
    {
        char source[10000];
        char destination[100000];
        if (sscanf(buffer, "%s %s %s", command, source, destination) != 3)
        {
            perror("Invalid COPY command format");
            close(client_socket);
            free(thread_args);
            pthread_exit(NULL);
        }
        int source_storage_port = -1;
        if (findStorageServerPort(source, &source_storage_port))
        {
            int destination_storage_port = -1;
            if (findStorageServerPort(destination, &destination_storage_port))
            {
                copyFile(source, destination);
                printf("File copied from %s to %s\n", source, destination);
                storage_server_port = 0;
            }
        }
    }
    else
    {
        while (token != NULL)
        {
            strcpy(path, token);
            printf("%s\n", path);
            token = strtok(NULL, " ");
        }

        if (bytes_received <= 0)
        {
            perror("Receiving data from client failed");
            close(client_socket);
            free(thread_args);
            pthread_exit(NULL);
        }

        printf("Received data from client: %s\n", buffer);

        strcpy(client_com[num_clients].command, buffer);
        printf("Command from client: %s\n", command);
        printf("Path from client: %s\n", path);

        if (findStorageServerPort(path, &storage_server_port))
        {
            printf("Client requested path: %s\n", path);
            printf("Found storage server port: %d\n", storage_server_port);
            client_com[num_clients].storageport = storage_server_port;
            // Send the storage server port back to the client
            if (send(client_socket, &storage_server_port, sizeof(storage_server_port), 0) == -1)
            {
                perror("Sending storage server port to client failed");
            }
            num_clients++;
        }
        else
        {
            printf("Client requested path: %s\n", path);
            printf("Path not found in accessible paths\n");

            // Send an error message to the client
            storage_server_port = -1;
            if (send(client_socket, &storage_server_port, sizeof(storage_server_port), 0) == -1)
            {
                perror("Sending error message to client failed");
            }
            num_clients++;
        }
    }

    close(client_socket);
    free(thread_args);

    pthread_exit(NULL);
}

int findStorageServerPort(const char *path, int *port)
{
    pthread_mutex_lock(&storage_servers_mutex);

    // Search for the path in the list of accessible_paths in storage_servers
    for (int i = 0; i < num_storage_servers; i++)
    {
        printf("a");

        if (strstr(storage_servers[i].info.absolute_address, path) == 0)
        {
            printf("1");
            char checkpath[1024];
            strcpy(checkpath,path);
            strcpy(checkpath, checkpath + strlen(storage_servers[i].info.absolute_address));
            char newPath[strlen(checkpath) + 2];  // +2 for the dot and null terminator
            strcpy(newPath, ".");
            strcat(newPath, checkpath);
            strcpy(checkpath, newPath);
            printf("%s\n",newPath);
            if (strstr(storage_servers[i].info.accessible_paths, newPath) != NULL)
            {
                *port = storage_servers[i].info.client_port;
                pthread_mutex_unlock(&storage_servers_mutex);
                return 1; // Path found
            }
        }
    }

    pthread_mutex_unlock(&storage_servers_mutex);
    return 0; // Path not found
}

int sendCommandToStorageServer(int storage_server_port, char command[])
{

    int ss_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (ss_socket == -1)
    {
        perror("Socket creation failed");
        return -1;
    }

    struct sockaddr_in ss_address;
    ss_address.sin_family = AF_INET;
    ss_address.sin_port = htons(storage_servers[0].info.nm_port);
    ss_address.sin_addr.s_addr = inet_addr(storage_servers[0].info.ip_address);

    if (connect(ss_socket, (struct sockaddr *)&ss_address, sizeof(ss_address)) == -1)
    {
        perror("Connection to the storage server failed");
        close(ss_socket);
        return -1;
    }

    // Send the length of the command first
    size_t command_length = strlen(command);
    if (send(ss_socket, &command_length, sizeof(command_length), 0) == -1)
    {
        perror("Sending command length to storage server failed");
        close(ss_socket);
        return -1;
    }

    // Send the command string
    if (send(ss_socket, command, command_length, 0) == -1)
    {
        perror("Sending command to storage server failed");
        close(ss_socket);
        return -1;
    }

    close(ss_socket);
    return 0;
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
    ns_address.sin_addr.s_addr = INADDR_ANY;

    if (bind(ns_socket, (struct sockaddr *)&ns_address, sizeof(ns_address)) == -1)
    {
        perror("Bind failed");
        close(ns_socket);
        exit(1);
    }

    if (listen(ns_socket, 5) == -1)
    {
        perror("Listen failed");
        close(ns_socket);
        exit(1);
    }

    while (1)
    {
        int client_socket = accept(ns_socket, NULL, NULL);
        if (client_socket == -1)
        {
            perror("Accepting client connection failed");
            continue; // Continue to the next iteration to keep listening
        }

        char request_type;
        if (recv(client_socket, &request_type, sizeof(request_type), 0) <= 0)
        {
            perror("Receiving request type from client failed");
            close(client_socket);
            continue;
        }

        struct ThreadArgs *thread_args = malloc(sizeof(struct ThreadArgs));
        thread_args->socket = client_socket;
        thread_args->request_type = request_type;

        // Create a thread based on the request type
        pthread_t thread;
        if (request_type == 'I')
        {
            if (pthread_create(&thread, NULL, handleStorageServer, thread_args) != 0)
            {
                perror("Failed to create storage server thread");
                free(thread_args);
                close(client_socket);
            }
        }
        else if (request_type == 'P')
        {
            if (pthread_create(&thread, NULL, handleClient, thread_args) != 0)
            {
                perror("Failed to create client thread");
                free(thread_args);
                close(client_socket);
            }
        }
        else
        {
            free(thread_args);
            close(client_socket);
        }
    }
    close(ns_socket);
    return 0;
}
