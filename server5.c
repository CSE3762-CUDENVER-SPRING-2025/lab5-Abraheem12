/*
Name: Abraheem Zadron
Date: March 20, 2025
Description: Linked list for lab5 server side
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cjson/cJSON.h>

#define BUFFER_SIZE 4096
#define MULTICAST_GROUP "239.128.1.1"
#define MAXPEERS 100

struct FileInfo
{
    char filename[100];
    char fullFileHash[65];
    char clientIP[MAXPEERS][INET_ADDRSTRLEN];
    int clientPort[MAXPEERS];
    int numberOfPeers;
    struct FileInfo *next;
    int numberOfChunks;
};
struct FileInfo *head = NULL;

struct FileInfo *searchForFile(const char *hash)
{ // Start from the head of the list
    struct FileInfo *current = head;

    while (current != NULL)
    { // Loop through each file
        int match = 1;

        // Compare each character of the hash
        for (int i = 0; i < 64; i++)
        { // SHA-256 hashes
            if (current->fullFileHash[i] != hash[i])
            { // Mismatch found
                match = 0;
                break;
            }
        }
        if (match)
        {
            // Return file if the hashes match
            return current;
        }
        // Move to the next file
        current = current->next;
    }
    // No match found
    return NULL;
}

void displayFiles()
{
    struct FileInfo *current = head;
    printf("\n=========================================\n");
    printf("Stored File Information\n");
    printf("=========================================\n");
    while (current)
    {
        printf("Filename: %s\n", current->filename);
        printf("    Full Hash: %s\n", current->fullFileHash);
        printf("    Number of Chunks: %d\n", current->numberOfChunks);
        printf("    Number of Peers: %d\n", current->numberOfPeers);

        for (int i = 0; i < current->numberOfPeers; i++)
        {
            printf("    Client %d: %s, Client Port: %d\n", i + 1, current->clientIP[i], current->clientPort[i]);
        }
        printf("-----------------------------------\n");
        current = current->next;
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Error: No port number!!! or something else \n");
        exit(1);
    }

    int port = atoi(argv[1]);
    int sock;
    struct sockaddr_in server_addr, client_addr;
    char buffer[BUFFER_SIZE];

    // Create UDP socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        perror("Socket creation failed");
        exit(1);
    }
    // Allow multiple sockets to bind to the same address
    int reuse = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        perror("setsockopt failed");
        exit(1);
    }
    // Configure server address for multicast
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    // Bind socket to the multicast port
    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Binding failed");
        exit(1);
    }

    // Join multicast group
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_GROUP);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
    {
        perror("Multicast group join failed");
        exit(1);
    }
    printf("Server listening on port %d...\n", port);

    while (1)
    {
        socklen_t addr_len = sizeof(client_addr);
        int recv_len = recvfrom(sock, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr *)&client_addr, &addr_len);
        if (recv_len < 0)
        {
            perror("Receive failed");
            continue;
        }
        // Null-terminate received data
        buffer[recv_len] = '\0';

        // Debugging
        printf("\nReceived JSON data:\n%s\n", buffer);

        // Parse JSON data
        cJSON *json = cJSON_Parse(buffer);
        if (json == 0)
        {
            printf("Failed to parse JSON\n");
            continue;
        }
        // Extract file metadata
        cJSON *filename = cJSON_GetObjectItem(json, "filename");
        cJSON *fileSize = cJSON_GetObjectItem(json, "fileSize");
        cJSON *numberOfChunks = cJSON_GetObjectItem(json, "numberOfChunks");
        cJSON *chunk_hashes = cJSON_GetObjectItem(json, "chunk_hashes");
        cJSON *fullFileHash = cJSON_GetObjectItem(json, "fullFileHash");

        // Structured Output
        printf("\n=========================================\n");
        printf(" File Metadata Received\n");
        printf("=========================================\n");

        if (filename)
            printf("Filename: %s\n", filename->valuestring);
        if (fileSize)
        { // to use the correct sizing
            double size = fileSize->valuedouble;
            if (size >= 1024 * 1024)
                printf("File Size: %.2f MB\n", size / (1024 * 1024));
            else if (size >= 1024)
                printf("File Size: %.2f KB\n", size / 1024);
            else
                printf("File Size: %.0f bytes\n", size);
        }
        if (numberOfChunks)
            printf("Number of Chunks: %d\n", numberOfChunks->valueint);
        if (fullFileHash)
            printf("Full File Hash: %s\n", fullFileHash->valuestring);

        printf("\nChunk Hashes:\n");
        if (chunk_hashes)
        {
            int chunk_count = cJSON_GetArraySize(chunk_hashes);
            for (int i = 0; i < chunk_count; i++)
            {
                cJSON *chunk_hash = cJSON_GetArrayItem(chunk_hashes, i);
                printf("Chunk %d: Hash: %s\n", i + 1, chunk_hash->valuestring);
            }
        }
        printf("=========================================\n");

        // Extract client information
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        int client_port = ntohs(client_addr.sin_port);

        struct FileInfo *file_entry = searchForFile(fullFileHash->valuestring);
        // Check if the file already exists in the linked list
        if (file_entry)
        {
            int exists = 0;
            // loop through all of the peers
            for (int i = 0; i < file_entry->numberOfPeers; i++)
            { // check if the clientIP is already stored
                if (strcmp(file_entry->clientIP[i], client_ip) == 0)
                {
                    exists = 1;
                    break;
                }
            }
            // if the client is new and we don't have 101 peers
            if (!exists && file_entry->numberOfPeers < MAXPEERS)
            { // add the new client
                strcpy(file_entry->clientIP[file_entry->numberOfPeers], client_ip);
                file_entry->clientPort[file_entry->numberOfPeers] = client_port;
                file_entry->numberOfPeers++;
                printf("Client IP: %s Port number: %d added for file: %s\n", client_ip, client_port, file_entry->filename);
            }
        }
        // If file not found, add it to linked list
        else
        {
            struct FileInfo *newFile = (struct FileInfo *)malloc(sizeof(struct FileInfo));
            // copying the metadata
            strcpy(newFile->filename, filename->valuestring);
            strcpy(newFile->fullFileHash, fullFileHash->valuestring);
            strcpy(newFile->clientIP[0], client_ip);
            newFile->clientPort[0] = client_port;
            newFile->numberOfPeers = 1;
            newFile->next = head;
            head = newFile;
            // store number of chunks
            newFile->numberOfChunks = numberOfChunks->valueint;
            printf("New file registered: %s (Hash: %s)\n", newFile->filename, newFile->fullFileHash);
        }
        // show the clients and updated version of the linked list
        displayFiles();
        cJSON_Delete(json);
    }

    close(sock);
    return 0;
}
