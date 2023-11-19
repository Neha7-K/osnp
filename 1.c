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
    int count=0;
    while(count != -1){
    for(int i=0;i<num_clients;i++)
    {
        if(ss_info.client_port == client_com[i].storageport)
        {   count=1;
            client_com[i].storageport=-1;
            if(send(ss_socket, client_com[i].command, sizeof(client_com[i].command),0) <= 0)
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

void *handleClient(void *arg)
{
    struct ThreadArgs *thread_args = (struct ThreadArgs *)arg;
    int client_socket = thread_args->socket;
    char request_type = thread_args->request_type;
   

    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));

    ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);

    if (bytes_received <= 0)
    {
        perror("Receiving data from client failed");
        close(client_socket);
        free(thread_args);
        pthread_exit(NULL);
    }

    printf("Received data from client: %s\n", buffer);
    char command[50]; 
    char path[974];   
    if (sscanf(buffer, "%s %s", command, path) != 2)
    {
        perror("Invalid command format");
        close(client_socket);
        free(thread_args);
        pthread_exit(NULL);
    }
    strcpy(client_com[num_clients].command,command);
    printf("Command from client: %s\n", command);
    printf("Path from client: %s\n", path);

    int storage_server_port = -1;
    
    if (findStorageServerPort(path, &storage_server_port))
    {
        printf("Client requested path: %s\n", path);
        printf("Found storage server port: %d\n", storage_server_port);
        client_com[num_clients].storageport=storage_server_port;
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
        if (strstr(storage_servers[i].info.accessible_paths, path) != NULL)
        {
            *port = storage_servers[i].info.client_port;
            pthread_mutex_unlock(&storage_servers_mutex);
            return 1; // Path found
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
            // Handle unknown request type
            free(thread_args);
            close(client_socket);
        }
    }

    // Cleanup and close the naming server socket
    close(ns_socket);
    return 0;
}
