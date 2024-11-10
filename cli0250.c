#include <stdio.h>
#include <stdlib.h>       
#include <time.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>              
#include <sys/uio.h>                  
#include <arpa/inet.h>
#include <netinet/in.h> 
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define BUFFER_SIZE 8192    //buffer size for data transfer
#define PORT 6025           //IT223'6025'0
#define IPADDR "127.0.0.1"
#define DOWN_DIR "/home/sahan/Desktop/NPassignment/files/downloads"

/*********************************************************function declaration*********************************************************/
void progress_bar(long total_size, long current_size);

/*********************************************************main function*********************************************************/
int main(int argc, char *argv[]){
    int sockfd;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];
    
    //create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0){
        perror("error on create socket");
        return -1;
    }
    
    //server address initialization
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = inet_addr(IPADDR);
    
    //connect to server
    printf("Connecting to server at %s:%d\n", IPADDR, PORT);
    if(connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
        perror("Error on connection");
        return -1;
    }
    printf("Connected to the server.\n");
    
    char full_list[16384] = {0};    //buffer to store file list
    int total_received = 0;
    int list_complete = 0;
    int bytes_received = 0; 
    
    //wait untill full file list received
    while(!list_complete && total_received < sizeof(full_list) - 1) {
        memset(buffer, 0, BUFFER_SIZE);
        bytes_received = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
        
        if(bytes_received <= 0){
            printf("Error receiving file list\n");
            close(sockfd);
            return -1;
        }
        
        //check the full_list buffer and append data
        if(total_received + bytes_received < sizeof(full_list)) {
            memcpy(full_list + total_received, buffer, bytes_received);
            total_received += bytes_received;
            
            if(strstr(buffer, "END_OF_LIST")) {
                list_complete = 1;
                char *end = strstr(full_list, "END_OF_LIST");
                if(end) *end = '\0';
            }
        } 
        else {
            printf("File list too large to receive\n"); 
            close(sockfd);
            return -1;
        }
    }
    
    if(total_received > 0){
        printf("%s", full_list);
    }
    
    send(sockfd, "ACK", 3, 0);
    
    int file_index = 0;
    char req[10];
    int max_files = 0;
    
    //count file list
    char *temp = full_list;
    while ((temp = strstr(temp, "\n")) != NULL) {
        max_files++;
        temp++;
    }
    max_files--;
    
    printf("Enter the file number to download : ");
    scanf("%d", &file_index);
    if(file_index <= 0 || file_index > max_files) {
        printf("invalid file index\n");
        close(sockfd);
        return -1;
    }
    
    //send file index to server
    sprintf(req, "%d", file_index);
    send(sockfd, req, strlen(req), 0);
    
    char filename[256] = {0};
    int filename_length = 0;
    
    
    
    //get file length
    ssize_t recv_result = recv(sockfd, &filename_length, sizeof(int), MSG_WAITALL);
    if(recv_result != sizeof(int) || filename_length <= 0 || filename_length >= sizeof(filename)) {
        printf("invalid filename length received: %d\n", filename_length);
        close(sockfd);
        return -1;
    }
    
    //get filename
    recv_result = recv(sockfd, filename, filename_length, MSG_WAITALL);
    if (recv_result != filename_length){
        printf("error receiving filename\n");
        close(sockfd);
        return -1;
    }
    filename[filename_length] = '\0';
    
    printf("Downloading %s\n", filename);
    
    // Create the download directory 
    struct stat st = {0};
    if (stat(DOWN_DIR, &st) == -1) {
        mkdir(DOWN_DIR, 0700);
    }
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s/%s", DOWN_DIR, filename);

    long total_size = 0;
    recv_result = recv(sockfd, &total_size, sizeof(long), MSG_WAITALL);
    if(recv_result != sizeof(long) || total_size <= 0){
        printf("error receiving file size or invalid size\n");
        close(sockfd);
        return -1;
    }
    
    //open file for writting
    FILE *file = fopen(full_path, "wb");
    if(file == NULL){
        printf("Error opening file for writing\n");
        close(sockfd);
        return -1;
    }
    
    long current_size = 0;
    
    //loop until fil download
    while(current_size < total_size) {
        memset(buffer, 0, BUFFER_SIZE);
        size_t to_receive = (total_size - current_size) < BUFFER_SIZE ? (total_size - current_size) : BUFFER_SIZE;
        
        size_t bytes_received = recv(sockfd, buffer, to_receive, MSG_WAITALL);
        if(bytes_received <= 0) {
            printf("Error receiving file data\n");
            fclose(file);
            close(sockfd);
            return -1;
        }
        
        size_t written = fwrite(buffer, 1, bytes_received, file);
        if(written != bytes_received){
            printf("error writing to file\n");
            fclose(file);
            close(sockfd);
            return -1; 
        }
        
        current_size += bytes_received;
        progress_bar(total_size, current_size);
    }
    
    fclose(file); //close file
    close(sockfd); //close socket
    
    //check if full file downloaded
    if(current_size == total_size){
        printf("\nFile download complete. File saved as %s\n", filename);
    } else {
        printf("\nError: file download incomplete\n");
    }
    
    return 0;
}

void progress_bar(long total_size, long current_size){
  static int last_percentage = -1;
  int percentage = (int)((current_size * 100) / total_size);
  if(percentage != last_percentage) {
    printf("progress: %d%%\n", percentage);
    last_percentage = percentage;
  }
}
