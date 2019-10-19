#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#define PORT 8080

/*
void handle_client(int fd)
{
    char response_data[2048];
    char data[2048] = "HTTP/1.1 200 OK\r\n";
    
}
*/
int main()
{
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};
    if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("ERROR: Socket failed");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    if(bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("ERROR: Bind failed");
        exit(EXIT_FAILURE);
    }
    if(listen(server_fd, 3) < 0)
    {
        perror("ERROR: Listen failed");
        exit(EXIT_FAILURE);
    }
    while(1)
    {
        if((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0)
        {
            perror("ERROR: Could not accept socket");
            exit(EXIT_FAILURE);
        }
        int client_data = read(client_socket, buffer, sizeof buffer);
        if(client_data < 0)
        {
            perror("ERROR: Could not read from socket");
            exit(EXIT_FAILURE);
        }
        printf("%s\n", buffer);
        client_data = write(client_socket, "I got your message", 18);
        //handle_client(client_socket);
        close(client_socket);
    }
    
    close(server_fd);
    return 0;
}

