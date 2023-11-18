#ifndef naming_H
#define naming_H
#define NAMING_SERVER_PORT 8080
#define MAX_STORAGE_SERVERS 10

struct StorageServerInfo
{
    char ip_address[16];
    int nm_port;
    int client_port;
    char accessible_paths[1024];
};
extern struct StorageServerInfo storageServerInfo;

struct ThreadArgs
{
    int socket;
    char request_type;
};
struct StorageServer
{
    struct StorageServerInfo info;
};

struct StorageServer storage_servers[MAX_STORAGE_SERVERS];
int num_storage_servers = 0;
void processStorageServerInfo(const struct StorageServerInfo *ss_info);
void *handleStorageServer(void *arg);
int findStorageServerPort(const char *path, int *port);
void *handleClient(void *arg);
int sendCommandToStorageServer(int storage_server_index, char command);

#endif