#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>

#define NAMING_SERVER_IP "127.0.0.1"
#define NAMING_SERVER_PORT 8080
#define MAX_COMMAND_SIZE 1024


struct StorageServerInfo {
    char ip_address[16];
    int nm_port;
    int client_port;
    char accessible_paths[1024]; // Adjust the size as needed
};

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

// Function to recursively collect accessible paths
void collectAccessiblePaths(const char *dir_path, char *accessible_paths, int *pos, int size) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
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

int main() {
    int ss_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (ss_socket == -1) {
        perror("Socket creation failed");

    }

    struct sockaddr_in ns_address;
    ns_address.sin_family = AF_INET;    
    ns_address.sin_port = htons(NAMING_SERVER_PORT);
    ns_address.sin_addr.s_addr = inet_addr(NAMING_SERVER_IP);

    if (connect(ss_socket, (struct sockaddr *)&ns_address, sizeof(ns_address)) == -1) {
        perror("Connection to the naming server failed");
        close(ss_socket);

    }

    // Collect accessible paths
    char accessible_paths[4096]; // Adjust the size as needed
    int current_pos = 0;
    collectAccessiblePaths(".", accessible_paths, &current_pos, sizeof(accessible_paths));
    accessible_paths[current_pos] = '\0'; // Null-terminate the string

    // Prepare and send storage server details with accessible paths
    struct StorageServerInfo ss_info;
    strcpy(ss_info.ip_address, "192.168.1.100");
    ss_info.nm_port = 8888;
    ss_info.client_port = 11; // Placeholder for now
    strcpy(ss_info.accessible_paths, accessible_paths);

    // Send the request type 'I' to the naming server
    char request_type = 'I';
    if (send(ss_socket, &request_type, sizeof(request_type), 0) == -1) {
        perror("Sending request type to naming server failed");
        close(ss_socket);
    }

    // Send the storage server information
    if (send(ss_socket, &ss_info, sizeof(ss_info), 0) == -1) {
        perror("Sending storage server info to naming server failed");
        close(ss_socket);
   
    }

  
  
    char command[100000];
    
     if (recv(ss_socket, command, sizeof(command), 0) == -1) {
        perror("Receiving command failed\n");
        close(ss_socket);
    }
    printf("Received command: %s\n",command);   
      if (strcmp(command, "CREATE_FILE") == 0) {
        createFile();
    } else if (strcmp(command, "CREATE_DIRECTORY") == 0) {
        createDirectory();
    } else {
        printf("Invalid command\n");
    }

   close(ss_socket);

    return 0;
}
