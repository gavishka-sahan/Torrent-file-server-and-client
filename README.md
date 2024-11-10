# Torrent-file-server-and-client

<h3> Steps to install server and client </h3>

<p>in server,
   1. add 'srv0250.c' file and create 'log_serv0250.txt' in same directory in the server.
   2. Create a folder directory called 'server_files' in the server to store server media.
   3. edit file location to 'server_files' in line 117, 136, 139 in srv0250.c.
   	if(list_files("/path/to/the/directory/server_files", clisock_fd) == 0){
   	char* filename = get_filename("/path/to/the/directory/server_files", file_index);
   	snprintf(file_path, sizeof(file_path), "/path/to/the/directory/server_files/%s", filename);
   4. open terminal in same directory as srv0250.c.
   5. in terminal, execute "gcc -o server srv0250.c".
   6. then execute "./server".
   7. Now, server is online.
   
   
in client,
   1. add 'cli0250.c' file to directory.
   2. Open terminal in same directory.
   3. execute command "gcc -o client cli0250.c".
   4. then execute "./client <Server IP address> 6025" (replace <Server IP address> with ip address of server)
   5. Now, client is online. To download files from serve, follow the instruction in client terminal.
</p>
