#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <string>
#include <arpa/inet.h>
#include <iostream>
#define PORT 8080

const size_t buffSize = 4097;

std::string readHeader(int fd)
{
    char buffer[buffSize];
    char *p;
    size_t len = buffSize - 1;
    ssize_t n;
    p = buffer;

    while((n = read(fd, p, len)) > 0)
    {
        len = len - n;
        p = p + n;
        if(strstr(buffer, "\n\r") != nullptr)
        {
            printf("%s\n", "We have reached the end of the header");
            break;
        }
        if(len == 0)
        {
            break;
        }
    }
    *p = '\0';
    if(n < 0)
    {
        perror("ERROR: Can't read Client Header");
        exit(EXIT_FAILURE);
    }
    if (len == 0)
    {
        perror("ERROR: Exceeded max header size");
        exit(EXIT_FAILURE);
    }
    return std::string(buffer);
}

int main(int argc, char *argv[])
{
    char buff[buffSize];
    char const http_header[2048] = "HTTP/1.1 200 OK\r\n";
    int server_fd, client_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    //int const buffSize = 4097;
    //char buffer[buffSize];

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
    if(argc == 1)
    {
        address.sin_port = htons(PORT);
    }
    else if (argc == 2)
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
        if((client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0)
        {
            perror("ERROR: Could not accept socket");
            exit(EXIT_FAILURE);
        }
        write(client_fd, http_header, sizeof http_header);
        std::string header = readHeader(client_fd); 
        printf("%s\n", header.c_str());
        std::cout << header.length() << std::endl;
        read(client_fd, buff, sizeof buff - 1);
        write(1, buff, sizeof buff - 1);
        close(client_fd);
    }
    
    close(server_fd);
    return 0;
}

