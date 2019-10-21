#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <string>
#include <arpa/inet.h>
#define PORT 8080

int main(int argc, char *argv[])
{
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int const buffSize = 4097;
    char buffer[buffSize];

    //Check if the user inputs too many arguments
    if(argc > 2)
    {
        perror("ERROR: Invalid arguments");
        exit(EXIT_FAILURE);
    }

    //Check if the socket is setup succesfully
    if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("ERROR: Socket failed");
        exit(EXIT_FAILURE);
    }

    //Initialize port and address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    int user_port;
    if(argc < 1)
    {
        address.sin_port = htons(PORT);
    }
    else if (argc < 2)
    {
        address.sin_port = htons(PORT);
        address.sin_addr.s_addr = inet_addr(argv[1]);   
    }
    else if(sscanf(argv[2], "%d", &user_port) != -1)
    {
        address.sin_port = htons(user_port);
        address.sin_addr.s_addr = inet_addr(argv[1]);       
    } 
    else
    {
        perror("ERROR: Invalid ");
        exit(EXIT_FAILURE);
    }

    //Bind address to server
    if(bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("ERROR: Bind failed");
        exit(EXIT_FAILURE);
    }

    //Listen for a client socket
    if(listen(server_fd, 3) < 0)
    {
        perror("ERROR: Listen failed");
        exit(EXIT_FAILURE);
    }

    //Respond to get and put requests
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
        close(client_socket);
    }
    
    close(server_fd);
    return 0;
}

