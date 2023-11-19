#include "2.h"

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

void createFile()
{
    FILE *file = fopen("new_file.txt", "w");
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
    sleep(1);
    if (send(ns_socket, ss_info, sizeof(struct StorageServerInfo), 0) == -1)
    {
        perror("Sending storage server info to naming server failed");
        close(ns_socket);
        exit(1);
    }
}

void *sendInfoToNamingServer(void *arg)
{
    int ns_socket = *((int *)arg);

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

    // Send request type to the naming server
    char request_type = 'I';
    if (send(ns_socket, &request_type, sizeof(request_type), 0) == -1)
    {
        perror("Error sending request type to naming server");
        close(ns_socket);
        exit(1);
    }

    printf("Request type sent to naming server\n");

    // Introduce a delay before sending the storage server information
    

    // Send storage server information to the naming server
    if (send(ns_socket, &ss_info, sizeof(struct StorageServerInfo), 0) == -1)
    {
        perror("Sending storage server info to naming server failed");
        close(ns_socket);
        exit(1);
    }

    printf("Storage server information sent to naming server\n");

    pthread_exit(NULL);
}

void *receiveCommandsFromNamingServer(void *arg)
{
    int ns_socket = *((int *)arg);

    char command[MAX_COMMAND_SIZE];
  while(1){
    if (recv(ns_socket, command, sizeof(command), 0) == -1)
    {
        perror("Receiving command failed");
        close(ns_socket);
        exit(1);
    }
  if(strcmp(command,"") == 0)
  continue;
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
  }
    pthread_exit(NULL);
}

void *handleClientRequest(void *arg)
{
    int client_socket = *((int *)arg);

    char command[MAX_COMMAND_SIZE];
    if (recv(client_socket, command, sizeof(command), 0) == -1)
    {
        perror("Receiving command from client failed");
        close(client_socket);
        pthread_exit(NULL);
    }

    printf("Received command from client: %s\n", command);

    if (strcmp(command, "READ") == 0)
    {
        FILE *file = fopen("1.c", "r");

        // Check if the file is opened successfully
        if (file == NULL)
        {
            printf("Unable to open the file.\n");
            // Send an error message to the client or handle it accordingly
        }
        else
        {
            // Read and send the contents of the file character by character
            char c;
            while ((c = fgetc(file)) != EOF)
            {
                printf("%c", c);
            }

            // Close the file
            fclose(file);
        }
    }

    close(client_socket);
    pthread_exit(NULL);
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

    pthread_t send_thread, receive_thread, client_thread;

    // Create threads for sending and receiving information from the naming server
    if (pthread_create(&send_thread, NULL, sendInfoToNamingServer, &ns_socket) != 0)
    {
        perror("Failed to create send thread");
        close(ns_socket);
        exit(1);
    }

    if (pthread_create(&receive_thread, NULL, receiveCommandsFromNamingServer, &ns_socket) != 0)
    {
        perror("Failed to create receive thread");
        close(ns_socket);
        exit(1);
    }

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

    while (1)
    {
        int client_socket = accept(ss_socket, NULL, NULL);
        if (client_socket == -1)
        {
            perror("Accepting client connection failed");
            continue; // Continue to the next iteration to keep listening
        }

        // Create a thread to handle client requests
        if (pthread_create(&client_thread, NULL, handleClientRequest, &client_socket) != 0)
        {
            perror("Failed to create client thread");
            close(client_socket);
        }
        /*if (pthread_join(receive_thread, NULL) != 0)
{
    perror("Failed to join receive thread");
    exit(1);
}*/
    }

    // Close the naming server socket
    close(ns_socket);

    // Close the storage server socket
    close(ss_socket);

    return 0;
}
