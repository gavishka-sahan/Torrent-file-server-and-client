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
#include <signal.h>
#include <sys/wait.h>
#include <dirent.h>  

#define PORT 6025             //IT223'6025'0
#define SERVADDR INADDR_ANY
#define BUFFER_SIZE 8192      //buffer size for data transmission
#define CHUNK_SIZE 1024       //size of file chunk
#define MAX_FILENAME_LENGTH 256 //max length for filename
#define MAX_FILES 100         //max file limit in list

/*********************************************************function declaration*********************************************************/
void sigchld_handler(int s);
int list_files(const char* dir_path, int clisock_fd);
char* get_filename(const char * dir_path, int index);
int send_file(const char* file_path, int clisock_fd);
void server_log(const char* client_ip, int client_port, const char* file_name);

int servsock_fd, clisock_fd;
struct sockaddr_in serv_addr, cli_addr;
socklen_t clilen;
char buffer[BUFFER_SIZE];
char chunk[CHUNK_SIZE];

/*********************************************************main function*********************************************************/
int main() {
    struct sigaction sact;
    sact.sa_handler = sigchld_handler;
    sigemptyset(&sact.sa_mask);
    sact.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if(sigaction(SIGCHLD, &sact, 0) == -1) {
        perror("sigaction");
        exit(1);
    }
    
    //socket creation
    servsock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(servsock_fd < 0){
        perror("error on creating socket");
        return -1;
    }
    
    //server address initialization
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = htonl(SERVADDR);
    
    //bind socket
    if(bind(servsock_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
        perror("error on binding");
        return -1;
    }
    
    //listen for incoming connection
    listen(servsock_fd, 100);
    clilen = sizeof(cli_addr);
    printf("waiting for client connection...\n");
    
    while(1){
        //accept incoming connection
        clisock_fd = accept(servsock_fd, (struct sockaddr *) &cli_addr, &clilen);
        if(clisock_fd < 0){
            perror("error on accepting");
            continue;
        }
        
        char client_ip[INET_ADDRSTRLEN];
        int client_port;
        
        //get ip addr and port number
        if(getpeername(clisock_fd, (struct sockaddr *) &cli_addr, &clilen) == 0){
            inet_ntop(AF_INET, &(cli_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
            client_port = ntohs(cli_addr.sin_port);
            printf("client connection from IP %s and port: %d\n", client_ip, client_port);
        }
        else{
            perror("Error getting client name");
            close(clisock_fd);
            continue;
        }
        
        //fork
        pid_t pid = fork();
        
        //child process
        if(pid == 0){
            close(servsock_fd);
            
            //send available files to client
            write(clisock_fd, "Available Files:\n", 17);   
            int file_count = list_files("/home/sahan/Desktop/NPassignment/server_files", clisock_fd);
            if(file_count == 0	){
                write(clisock_fd, "0 files available\n", 18);
                close(clisock_fd);
                exit(0);
            }
            
            //send end marker
            char end_marker[] = "END_OF_LIST\n";
            write(clisock_fd, end_marker, strlen(end_marker));
            
            //client acknowledgement
            char ack_buffer[4];
            if (recv(clisock_fd, ack_buffer, 3, 0) <= 0 || strncmp(ack_buffer, "ACK", 3) != 0) { 
                fprintf(stderr, "Failed to receive ACK from client\n");
                close(clisock_fd);
                exit(1);
            }
            
            //receive file index
            ssize_t bytes_read = read(clisock_fd, buffer, BUFFER_SIZE - 1);
            if(bytes_read > 0){
                buffer[bytes_read] = '\0';
                int file_index = atoi(buffer);
                
                //get filename that matches the file index
                char* filename = get_filename("/home/sahan/Desktop/NPassignment/server_files", file_index);
                if(filename != NULL){ 
                    //send file to client
                    char file_path[BUFFER_SIZE];
                    snprintf(file_path, sizeof(file_path), "/home/sahan/Desktop/NPassignment/server_files/%s", filename);
                    if(send_file(file_path, clisock_fd)){
                        //fprintf(stderr, "failed to send file\n");
                    }
                            
                    server_log(client_ip, client_port, filename);
                }
                else{
                    const char* error_msg = "Invalid file index\n";
                    write(clisock_fd, error_msg, strlen(error_msg));
                    
                    server_log(client_ip, client_port, NULL);
                }
            }
            else{
                perror("Error reading from client");
            }
                        
            close(clisock_fd); //close client connection
            printf("-file transfer complete. connection closed\n");
            exit(0);
        }
        //parent process
        else if(pid > 0){
            close(clisock_fd);
        }
        //error on fork
        else{
            perror("error on fork");
            close(clisock_fd);
        }
    }
    return 0;
}

/*********************************************************signal handler function*********************************************************/
void sigchld_handler(int s){
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

/*********************************************************list files and send list function*********************************************************/
int list_files(const char* dir_path, int clisock_fd){
    DIR *directory;
    struct dirent *dir;
    char file_info[1024];
    int file_count = 0;
    
    directory = opendir(dir_path);
    if(directory) {
        while ((dir = readdir(directory)) != NULL && file_count < MAX_FILES){
            if(dir->d_type == DT_REG) {
                file_count++;
                snprintf(file_info, sizeof(file_info), "%d. %s\n", file_count, dir->d_name);
                write(clisock_fd, file_info, strlen(file_info));
            }
        }
        closedir(directory);
    }
    else {
        perror("failed to open directory");
        return -1;
    }
    return file_count;
}

/*********************************************************get filename function*********************************************************/
char* get_filename(const char* dir_path, int index) {
    DIR *directory;
    struct dirent *dir;
    int current_index = 0;
    static char filename[256];
   
    directory = opendir(dir_path);
    if (directory) {
        while ((dir = readdir(directory)) != NULL) {
            if (dir->d_type == DT_REG) {
                current_index++;
                if (current_index == index) {
                    strncpy(filename, dir->d_name, sizeof(filename) - 1);
                    filename[sizeof(filename) - 1] = '\0';
                    closedir(directory);
                    return filename;
                }
            }
        }
        closedir(directory);
        fprintf(stderr, "File index %d not found\n", index);
    } else {
        perror("Failed to open directory");
    }
    return NULL;
}

/*********************************************************send file function*********************************************************/
int send_file(const char* file_path, int clisock_fd){
    memset(chunk, 0, CHUNK_SIZE);

    FILE* file = fopen(file_path, "rb");
    if(file == NULL){ 
        perror("Error opening file");
        return -1;
    }
    
    const char* filename = strrchr(file_path, '/');
    filename = filename ? filename + 1 : file_path;
    
    int filename_length = strlen(filename);
    
    if(filename_length >= MAX_FILENAME_LENGTH) {
        fprintf(stderr, "filename too long\n");
        fclose(file);
        return -1;
    }
    
    if(send(clisock_fd, &filename_length, sizeof(int), 0) < 0 || send(clisock_fd, filename, filename_length, 0) < 0){ 
        perror("error sending filename");
        fclose(file);
        return -1;
    }
    
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    if(file_size == -1){
        perror("Error getting file size");
        fclose(file);
        return -1;
    }
    fseek(file, 0, SEEK_SET);
    if(send(clisock_fd, &file_size, sizeof(long), 0) < 0){
        perror("error sending file size");
        fclose(file);
        return -1;
    }
    
    size_t bytes_read;
    int total_sent = 0;
    int retry_count = 0;
    const int max_retries = 5;  
    
    while(total_sent < file_size && retry_count < max_retries){
        bytes_read = fread(chunk, 1, CHUNK_SIZE, file);
        if(bytes_read <= 0){
            break;
        }
        
        ssize_t bytes_sent = send(clisock_fd, chunk, bytes_read, 0);
        if(bytes_sent < 0){
            if(errno == ECONNRESET || errno == EPIPE){
                perror("connection reset by peer");
                retry_count++;
                sleep(1);
                continue;
            }
            else{
                perror("Error on sending file");
                fclose(file);
                return -1;
            }
        }
        total_sent += bytes_sent;
    }
    
    fclose(file);
    
    if(total_sent < file_size){
        fprintf(stderr, "failed to send entire file after %d retries\n", max_retries);
        return -1;
    }
    
    return total_sent;
}

/*********************************************************function for server log file*********************************************************/
void server_log(const char* client_ip, int client_port, const char* file_name){
    FILE* log_file = fopen("log_serv0250.txt", "a");
    if(log_file == NULL){
        perror("Error on opening log file");
        return;
    }
    
    time_t now;
    time(&now);
    char date_time[26];
    ctime_r(&now, date_time);
    date_time[24] = '\0';
    
    fprintf(log_file, "[%s] - client: %s:%d, file:%s\n", date_time, client_ip, client_port, file_name ? file_name : "No file transferred");
    
    fclose(log_file);
    log_file = fopen("log_serv0250.txt", "a");
    
    fclose(log_file);
}
