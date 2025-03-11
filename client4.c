/*
Name: Abraheem Zadron
Date: March 11, 2025
Description: Multi-File Chunking and Hashing Client
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <cjson/cJSON.h>
#include <openssl/evp.h>
#include <netinet/in.h>

#define CHUNK_SIZE (500 * 1024)
#define BUFFER_SIZE 4096
#define MULTICAST_GROUP "239.128.1.1"
#define CHUNK_DIR "CHUNKS"
#define ARCHIVE_DIR "ArchiveFILES"
#define DATA_DIR "DATA"

void compute_sha256(unsigned char *data, int length, char *output_hash)
{
    // storing
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    // creating the hash
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (mdctx == 0)
    {
        perror("Failed to create hash");
        return;
    }
    // initializign
    EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
    // process
    EVP_DigestUpdate(mdctx, data, length);
    // finalizing
    EVP_DigestFinal_ex(mdctx, hash, &hash_len);
    EVP_MD_CTX_free(mdctx);

    // Convert hash to hex string
    for (int i = 0; i < hash_len; i++)
    {
        sprintf(output_hash + (i * 2), "%02x", hash[i]);
    }
    output_hash[hash_len * 2] = '\0';
}

void process_file(const char *filepath, const char *filename, int sock, struct sockaddr_in *server_addr)
{
    int rc;
    FILE *file = fopen(filepath, "rb");
    if (!file)
    {
        perror("File open failed");
        return;
    }

    // Get file size by reading the entire file
    int file_size = 0;
    unsigned char buffer[BUFFER_SIZE];
    int bytes_read;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0)
    {
        file_size += bytes_read;
    }

    if (file_size == 0)
    {
        printf("Error: File %s is empty.\n", filename);
        fclose(file);
        return;
    }
    // Reset file pointer
    fseek(file, 0, SEEK_SET);

    int chunk_count = 0;
    char full_file_hash[EVP_MAX_MD_SIZE * 2 + 1];

    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);

    rc = mkdir(CHUNK_DIR, 0777);

    char chunk_hashes[100][EVP_MAX_MD_SIZE * 2 + 1];

    while (1)
    {
        unsigned char buffer[CHUNK_SIZE];
        int bytes_read = fread(buffer, 1, CHUNK_SIZE, file);
        if (bytes_read == 0)
            break;
        EVP_DigestUpdate(mdctx, buffer, bytes_read);
        // computing the chunk
        char chunk_hash[EVP_MAX_MD_SIZE * 2 + 1];
        compute_sha256(buffer, bytes_read, chunk_hash);
        strcpy(chunk_hashes[chunk_count], chunk_hash);

        char chunk_filename[256];
        sprintf(chunk_filename, "%s/%s", CHUNK_DIR, chunk_hash);

        FILE *chunk_file = fopen(chunk_filename, "wb");
        if (!chunk_file)
        {
            perror("Chunk file open failed");
        }
        else
        {
            rc = fwrite(buffer, 1, bytes_read, chunk_file);
            if (rc < bytes_read)
            {
                printf("Only wrote %d of %d bytes for chunk %d\n", rc, bytes_read, chunk_count + 1);
            }
            fclose(chunk_file);
        }

        chunk_count++;
    }

    fclose(file);

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    EVP_DigestFinal_ex(mdctx, hash, &hash_len);
    EVP_MD_CTX_free(mdctx);

    for (int i = 0; i < hash_len; i++)
    {
        sprintf(full_file_hash + (i * 2), "%02x", hash[i]);
    }
    full_file_hash[hash_len * 2] = '\0';

    // Create JSON metadata object
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "filename", filename);
    cJSON_AddNumberToObject(json, "fileSize", file_size);
    cJSON_AddNumberToObject(json, "numberOfChunks", chunk_count);

    cJSON *chunk_array = cJSON_CreateArray();
    for (int i = 0; i < chunk_count; i++)
    {
        cJSON_AddItemToArray(chunk_array, cJSON_CreateString(chunk_hashes[i]));
    }
    cJSON_AddItemToObject(json, "chunk_hashes", chunk_array);
    cJSON_AddStringToObject(json, "fullFileHash", full_file_hash);

    rc = mkdir(DATA_DIR, 0777);
    char *json_string = cJSON_Print(json);
    char json_filename[512];
    snprintf(json_filename, sizeof(json_filename), "DATA/%s.json", filename);

    FILE *json_file = fopen(json_filename, "w");
    if (json_file)
    {
        fprintf(json_file, "%s", json_string);
        fclose(json_file);
    }
    // Send JSON data to the server
    int sent_bytes = sendto(sock, json_string, strlen(json_string), 0,
                            (struct sockaddr *)server_addr, sizeof(*server_addr));
    if (sent_bytes < 0)
    {
        perror("Failed to send data");
    }
    else
    {
        printf("Sent JSON metadata for %s\n", filename);
    }
}

void process_directory(int sock, struct sockaddr_in *server_addr)
{
    // List of image file names
    char *files[] = {
        "Agora.jpeg", "AngkorWat.jpeg", "Athens.jpeg", "BangkokRiver.jpeg",
        "BangkokTemple.jpeg", "IstanbulStreet.jpeg", "OdeonofHerodes.jpeg",
        "OldBazaar.jpeg", "SiemReapTukTuk.jpeg", "SingaporeHarbor.jpeg",
        "SydneyBridge.jpeg", "SydneyOperaHouse.jpeg"};
    int num_files = sizeof(files) / sizeof(files[0]); // Count files
    for (int i = 0; i < num_files; i++)
    {
        char filepath[512]; // Store full file path
        snprintf(filepath, sizeof(filepath), "%s/%s", ARCHIVE_DIR, files[i]);

        process_file(filepath, files[i], sock, server_addr);
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    int port = atoi(argv[1]);
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        perror("Socket has creation failed");
        exit(1);
    }
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, MULTICAST_GROUP, &server_addr.sin_addr) <= 0)
    {
        perror("Invalid multicast IP address");
        exit(1);
    }
    process_directory(sock, &server_addr);

    printf("I hashed them all.\n");

    close(sock);
    return 0;
}
