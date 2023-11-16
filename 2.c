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
struct StorageServerInfo {
    char ip_address[16];
    int nm_port;
    int client_port;
    char accessible_paths[4096];
};
// Function to recursively collect accessible paths
void collectAccessiblePaths(const char *dir_path, char *accessible_paths, int *pos, int size) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        perror("Opening directory failed");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG || entry->d_type == DT_DIR) {
            struct stat statbuf;
            char full_path[512]; // Adjust this size as needed
            snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

            if (stat(full_path, &statbuf) == 0) {
                if (S_ISDIR(statbuf.st_mode)) {
                    // It's a directory
                    if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                        // Skip current and parent directory entries
                        int len = snprintf(accessible_paths + (*pos), size - (*pos), "%s\n", full_path);
                        (*pos) += len;
                        collectAccessiblePaths(full_path, accessible_paths, pos, size);
                    }
                } else if (S_ISREG(statbuf.st_mode)) {
                    // It's a regular file
                    int len = snprintf(accessible_paths + (*pos), size - (*pos), "%s\n", full_path);
                    (*pos) += len;
                }
            }
        }
    }

    closedir(dir);
}

void createFile() {
    FILE *file = fopen("new_file.txt", "w");
    if (file == NULL) {
        perror("File creation failed");
    }
    fclose(file);
    printf("Empty file created: new_file.txt\n");
}

void createDirectory() {
    if (mkdir("new_directory", 0777) == -1) {
        perror("Directory creation failed");
    }
    printf("Empty directory created: new_directory\n");
}

void sendStorageServerInfoToNamingServer(int ns_socket, const struct StorageServerInfo *ss_info) {
    char request_type = 'I';
    if (send(ns_socket, &request_type, sizeof(request_type), 0) == -1) {
        perror("Sending request type to naming server failed");
        close(ns_socket);
        exit(1);
    }

    if (send(ns_socket, ss_info, sizeof(struct StorageServerInfo), 0) == -1) {
        perror("Sending storage server info to naming server failed");
        close(ns_socket);
        exit(1);
    }
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
    ns_address.sin_addr.s_addr = inet_addr(NAMING_SERVER_IP);

    if (connect(ns_socket, (struct sockaddr *)&ns_address, sizeof(ns_address)) == -1) {
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
    if(recv(ns_socket,command,sizeof(command),0) == -1)
    {
        perror("Receiving command failed\n");
    }
     printf("Received command: %s\n",command);   
      if (strcmp(command, "CREATE_FILE") == 0) {
        createFile();
    } else if (strcmp(command, "CREATE_DIRECTORY") == 0) {
        createDirectory();
    } else {
        printf("Invalid command\n");
    }
    // Close the naming server socket
    close(ns_socket);

    int ss_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (ss_socket == -1) {
        perror("Socket creation failed");
        exit(1);
    }

    struct sockaddr_in ss_address;
    ss_address.sin_family = AF_INET;
    ss_address.sin_port = htons(STORAGE_SERVER_PORT);
    ss_address.sin_addr.s_addr = INADDR_ANY;

    if (bind(ss_socket, (struct sockaddr *)&ss_address, sizeof(ss_address)) == -1) {
        perror("Bind failed");
        close(ss_socket);
        exit(1);
    }

    if (listen(ss_socket, 5) == -1) {
        perror("Listen failed");
        close(ss_socket);
        exit(1);
    }

    time_t last_accessible_paths_time = time(NULL);

   while (1) {
    int client_socket = accept(ss_socket, NULL, NULL);
    if (client_socket == -1) {
        perror("Accepting client connection failed");
        continue;  // Continue to the next iteration to keep listening
    }

    char command[100000];

    if (recv(client_socket, command, sizeof(command), 0) == -1) {
        perror("Receiving command failed\n");
        close(client_socket);
        continue;
    }

    printf("Received command: %s\n", command);

    close(client_socket);
   

}

// Close the server socket outside the loop
close(ss_socket);

return 0;
}
