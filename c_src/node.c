#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

#define MAX_KEY_LEN 256
#define MAX_VAL_LEN 1024
#define BUFFER_SIZE 2048

// Key-Value Store structure (Linked List)
typedef struct kv_entry {
    char key[MAX_KEY_LEN];
    char value[MAX_VAL_LEN];
    struct kv_entry *next;
} kv_entry_t;

kv_entry_t *store_head = NULL;
pthread_mutex_t store_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t wal_mutex = PTHREAD_MUTEX_INITIALIZER;
FILE *wal_file = NULL;

// Peer configuration
typedef struct {
    char ip[64];
    int port;
    int is_alive;
} peer_t;

peer_t peers[10];
int num_peers = 0;
int my_port = 8000;
int my_id = 1;

// Function declarations
void parse_peers(char *peers_str);
void *client_handler(void *arg);
int forward_to_peers(const char *command);
void recover_from_peers();
void recover_from_wal();
void append_wal(const char *cmd, const char *key, const char *val);

// KV Operations
void put_kv(const char *key, const char *value, int write_wal) {
    pthread_mutex_lock(&store_mutex);
    kv_entry_t *curr = store_head;
    int found = 0;
    while (curr != NULL) {
        if (strcmp(curr->key, key) == 0) {
            strncpy(curr->value, value, MAX_VAL_LEN);
            found = 1;
            break;
        }
        curr = curr->next;
    }
    if (!found) {
        kv_entry_t *new_entry = malloc(sizeof(kv_entry_t));
        strncpy(new_entry->key, key, MAX_KEY_LEN);
        strncpy(new_entry->value, value, MAX_VAL_LEN);
        new_entry->next = store_head;
        store_head = new_entry;
    }
    pthread_mutex_unlock(&store_mutex);
    
    // Write-Ahead Logging
    if (write_wal) append_wal("PUT", key, value);
}

char* get_kv(const char *key) {
    pthread_mutex_lock(&store_mutex);
    kv_entry_t *curr = store_head;
    while (curr != NULL) {
        if (strcmp(curr->key, key) == 0) {
            char *val = strdup(curr->value);
            pthread_mutex_unlock(&store_mutex);
            return val;
        }
        curr = curr->next;
    }
    pthread_mutex_unlock(&store_mutex);
    return NULL;
}

void del_kv(const char *key, int write_wal) {
    pthread_mutex_lock(&store_mutex);
    kv_entry_t *curr = store_head;
    kv_entry_t *prev = NULL;
    while (curr != NULL) {
        if (strcmp(curr->key, key) == 0) {
            if (prev == NULL) store_head = curr->next;
            else prev->next = curr->next;
            free(curr);
            break;
        }
        prev = curr;
        curr = curr->next;
    }
    pthread_mutex_unlock(&store_mutex);
    
    // Write-Ahead Logging
    if (write_wal) append_wal("DEL", key, NULL);
}

// ---------------------------------------------------------
// FEATURE 1: Write-Ahead Logging (Disk Persistence)
// ---------------------------------------------------------
void append_wal(const char *cmd, const char *key, const char *val) {
    pthread_mutex_lock(&wal_mutex);
    if (wal_file) {
        if (val) fprintf(wal_file, "%s %s %s\n", cmd, key, val);
        else fprintf(wal_file, "%s %s\n", cmd, key);
        fflush(wal_file); // Flush to disk immediately
    }
    pthread_mutex_unlock(&wal_mutex);
}

void recover_from_wal() {
    wal_file = fopen("/data/wal.log", "a+");
    if (!wal_file) {
        wal_file = fopen("wal.log", "a+"); // Fallback
    }
    if (!wal_file) return;

    rewind(wal_file);
    char line[BUFFER_SIZE];
    int count = 0;
    while (fgets(line, sizeof(line), wal_file)) {
        char cmd[16], key[MAX_KEY_LEN], val[MAX_VAL_LEN];
        if (sscanf(line, "%15s %255s %1023[^\n]", cmd, key, val) >= 2) {
            if (strcmp(cmd, "PUT") == 0) put_kv(key, val, 0); // 0 = don't write to WAL again
            else if (strcmp(cmd, "DEL") == 0) del_kv(key, 0);
            count++;
        }
    }
    fseek(wal_file, 0, SEEK_END);
    printf("[Node %d] Replayed %d operations from WAL disk.\n", my_id, count);
}

// Network Helpers
int send_tcp(const char *ip, int port, const char *msg, char *reply, int reply_size) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        struct hostent *he = gethostbyname(ip);
        if (he == NULL) { close(sock); return -1; }
        memcpy(&serv_addr.sin_addr, he->h_addr_list[0], he->h_length);
    }
    
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 500000; // 1.5 second timeout
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sock);
        return -1;
    }

    send(sock, msg, strlen(msg), 0);
    
    if (reply != NULL && reply_size > 0) {
        int bytes = recv(sock, reply, reply_size - 1, 0);
        if (bytes >= 0) reply[bytes] = '\0';
    }

    close(sock);
    return 0;
}

int forward_to_peers(const char *command) {
    int success_count = 0;
    for (int i = 0; i < num_peers; i++) {
        char reply[BUFFER_SIZE] = {0};
        if (send_tcp(peers[i].ip, peers[i].port, command, reply, sizeof(reply)) == 0) {
            if (strncmp(reply, "OK", 2) == 0) {
                success_count++;
            }
        }
    }
    return success_count; // Returns number of successful replicas
}

// ---------------------------------------------------------
// FEATURE 2: Heartbeat Protocol Thread
// ---------------------------------------------------------
void *heartbeat_thread(void *arg) {
    while(1) {
        sleep(5);
        for(int i=0; i<num_peers; i++) {
            char reply[64] = {0};
            if (send_tcp(peers[i].ip, peers[i].port, "PING\n", reply, sizeof(reply)) == 0 && strncmp(reply, "PONG", 4) == 0) {
                if (!peers[i].is_alive) {
                    printf("[Heartbeat] Node %s:%d is now ALIVE.\n", peers[i].ip, peers[i].port);
                    peers[i].is_alive = 1;
                }
            } else {
                if (peers[i].is_alive) {
                    printf("[Heartbeat] Node %s:%d is DEAD!\n", peers[i].ip, peers[i].port);
                    peers[i].is_alive = 0;
                }
            }
        }
    }
    return NULL;
}

// Main thread handling client/peer requests
void *client_handler(void *arg) {
    int client_sock = *(int*)arg;
    free(arg);
    
    char buffer[BUFFER_SIZE];
    int bytes_read = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        char cmd[16], key[MAX_KEY_LEN], val[MAX_VAL_LEN];
        
        int parsed = sscanf(buffer, "%15s %255s %1023[^\n]", cmd, key, val);
        
        if (strcmp(cmd, "PING") == 0) {
            send(client_sock, "PONG\n", 5, 0);
        }
        else if (strcmp(cmd, "PUT") == 0 && parsed >= 3) {
            // ---------------------------------------------------------
            // FEATURE 3: Quorum Write Consensus
            // ---------------------------------------------------------
            put_kv(key, val, 1);
            char forward_cmd[BUFFER_SIZE];
            snprintf(forward_cmd, sizeof(forward_cmd), "IPUT %s %s\n", key, val);
            
            // Wait for peers to acknowledge
            int acks = forward_to_peers(forward_cmd);
            int total_stored = acks + 1; // Peers + Local
            
            if (total_stored >= 2) {
                char resp[128];
                snprintf(resp, sizeof(resp), "OK_QUORUM_W=%d\n", total_stored);
                send(client_sock, resp, strlen(resp), 0);
            } else {
                send(client_sock, "WARN_STORED_ONLY_LOCALLY\n", 25, 0);
            }
        } 
        else if (strcmp(cmd, "IPUT") == 0 && parsed >= 3) {
            put_kv(key, val, 1);
            send(client_sock, "OK\n", 3, 0);
        }
        else if (strcmp(cmd, "GET") == 0 && parsed >= 2) {
            char *v = get_kv(key);
            if (v) {
                char resp[BUFFER_SIZE];
                snprintf(resp, sizeof(resp), "VALUE %s\n", v);
                send(client_sock, resp, strlen(resp), 0);
                free(v);
            } else {
                send(client_sock, "NOT_FOUND\n", 10, 0);
            }
        }
        else if (strcmp(cmd, "DEL") == 0 && parsed >= 2) {
            del_kv(key, 1);
            char forward_cmd[BUFFER_SIZE];
            snprintf(forward_cmd, sizeof(forward_cmd), "IDEL %s\n", key);
            forward_to_peers(forward_cmd);
            send(client_sock, "OK\n", 3, 0);
        }
        else if (strcmp(cmd, "IDEL") == 0 && parsed >= 2) {
            del_kv(key, 1);
            send(client_sock, "OK\n", 3, 0);
        }
        else if (strcmp(cmd, "SYNC") == 0) {
            pthread_mutex_lock(&store_mutex);
            kv_entry_t *curr = store_head;
            while (curr != NULL) {
                char line[BUFFER_SIZE];
                snprintf(line, sizeof(line), "IPUT %s %s\n", curr->key, curr->value);
                send(client_sock, line, strlen(line), 0);
                curr = curr->next;
            }
            pthread_mutex_unlock(&store_mutex);
            send(client_sock, "END_SYNC\n", 9, 0);
        }
        else {
            send(client_sock, "ERROR_BAD_COMMAND\n", 18, 0);
        }
    }
    
    close(client_sock);
    return NULL;
}

void parse_peers(char *peers_str) {
    if (!peers_str) return;
    char *token = strtok(peers_str, ",");
    while (token != NULL && num_peers < 10) {
        char ip[64];
        int port;
        if (sscanf(token, "%63[^:]:%d", ip, &port) == 2) {
            strncpy(peers[num_peers].ip, ip, 64);
            peers[num_peers].port = port;
            peers[num_peers].is_alive = 1;
            num_peers++;
        }
        token = strtok(NULL, ",");
    }
}

// On startup, fetch missing data from active peer
void recover_from_peers() {
    for (int i = 0; i < num_peers; i++) {
        printf("[Node %d] Attempting network sync from %s:%d...\n", my_id, peers[i].ip, peers[i].port);
        
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) continue;

        struct sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(peers[i].port);
        
        struct hostent *he = gethostbyname(peers[i].ip);
        if (he == NULL) { close(sock); continue; }
        memcpy(&serv_addr.sin_addr, he->h_addr_list[0], he->h_length);

        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == 0) {
            send(sock, "SYNC\n", 5, 0);
            
            char buffer[BUFFER_SIZE];
            FILE *f = fdopen(sock, "r");
            while (fgets(buffer, sizeof(buffer), f) != NULL) {
                if (strncmp(buffer, "END_SYNC", 8) == 0) break;
                char cmd[16], key[MAX_KEY_LEN], val[MAX_VAL_LEN];
                if (sscanf(buffer, "%15s %255s %1023[^\n]", cmd, key, val) >= 3) {
                    if (strcmp(cmd, "IPUT") == 0) {
                        put_kv(key, val, 1); // write to WAL too
                    }
                }
            }
            fclose(f);
            printf("[Node %d] Network sync complete!\n", my_id);
            return;
        }
        close(sock);
    }
    printf("[Node %d] No peers available for network sync.\n", my_id);
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);

    char *env_id = getenv("NODE_ID");
    if (env_id) my_id = atoi(env_id);
    
    char *env_port = getenv("PORT");
    if (env_port) my_port = atoi(env_port);
    
    char *env_peers = getenv("PEERS");
    if (env_peers) parse_peers(env_peers);
    
    printf("========================================\n");
    printf("[Node %d] Starting Advanced KV Store\n", my_id);
    printf("========================================\n");
    
    // 1. Recover from Disk (WAL)
    recover_from_wal();
    
    // 2. Recover from Network (Peers)
    recover_from_peers();

    // 3. Start Heartbeat Thread
    pthread_t hb_thread;
    pthread_create(&hb_thread, NULL, heartbeat_thread, NULL);
    pthread_detach(hb_thread);
    
    // 4. Setup TCP server
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(my_port);
    
    if (bind(server_sock, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(1);
    }
    
    if (listen(server_sock, 10) < 0) {
        perror("Listen failed");
        exit(1);
    }
    
    printf("[Node %d] Ready for connections on port %d.\n", my_id, my_port);
    
    while (1) {
        int *client_sock = malloc(sizeof(int));
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        *client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len);
        if (*client_sock >= 0) {
            pthread_t thread_id;
            pthread_create(&thread_id, NULL, client_handler, (void*)client_sock);
            pthread_detach(thread_id);
        } else {
            free(client_sock);
        }
    }
    
    return 0;
}
