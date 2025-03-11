/*
Name: Abraheem Zadron
Date: March 11, 2025
Description: Multi-File Chunking and Hashing Server
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

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
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
        perror("setsockopt(SO_REUSEADDR) failed");
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
    }

    close(sock);
    return 0;
}
