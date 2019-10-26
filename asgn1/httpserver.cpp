#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <string>
#include <arpa/inet.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netdb.h>
#define PORT 8080

const size_t buffSize = 4097;
const size_t fileSize = 27;
//Function for returning the header as string
std::string readHeader(int fd)
{
    char buffer[buffSize];
    char *p;
    size_t len = buffSize - 1;
    ssize_t n;
    p = buffer;

    //While the we can read from the client socket
    while((n = read(fd, p, len)) > 0)
    {
        len = len - n;
        p = p + n;

        //Checks if we are at the end of the header
        if(strstr(buffer, "\n\r") != nullptr)
        {
            break;
        }

        //Check if we have exceeded the header buffer
        if(len == 0)
        {
            break;
        }
    }
    //printf("%zd\n", len);
    //Initialize the pointer to the end of our new string with the Null character
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

//Parse through the Header for a content length number
ssize_t getContentLength(std::string Header)
{
    char const* cl = strstr(Header.c_str(), "Content-Length:");
    if(cl != nullptr)
    {
        ssize_t length;
        int contentLength = sscanf(cl, "Content-Length: %zd", &length);
        if(contentLength > 0)
        {
            return length;
        }
        else
        {
            perror("ERROR: No Content Length given");
            exit(EXIT_FAILURE);
        }
    }
    return -1;
}

//Check if file is correctly formatted
bool isCorrectFileName(std::string fileName)
{

    //Check if fileName is the correct size
    if(fileName.length() > fileSize || fileName.length() < fileSize)
    {
        return false;
    }
    //Check if fileName has proper characters
    char fileNameChar[fileSize];
    strcpy(fileNameChar, fileName.c_str());
    for(size_t i = 0; i < fileSize; i++)
    {
        if(fileNameChar[i] == 45 || fileNameChar[i] == 95 || 
          (fileNameChar[i] >= 48 && fileNameChar[i] <= 57) || 
          (fileNameChar[i] >= 65 && fileNameChar[i] <= 90) || 
          (fileNameChar[i] >= 97 && fileNameChar[i] <= 122))
        {
            continue;
        }
        else
        {
            return false;
        }
    }
    return true;
}

//Parse header for the name of the file
std::string getFileName(std::string header)
{
    //Use string stream to extract tokens
    std::vector <std::string> tokenVector;
    std::istringstream tokStream(header);
    std::string token;
    while(std::getline(tokStream, token, ' '))
    {
        tokenVector.push_back(token);
    }
    std::string fileName = tokenVector[1];
    return fileName;
}

//Handles when the client requests a PUT command
void handlePUT(char fileNameChar[], ssize_t contLength, int client_fd)
{
    char const http_header[19] = "HTTP/1.1 200 OK\r\n";
    write(client_fd, http_header, sizeof http_header);
    char fileContents[buffSize];
    int file = 1;
    if(contLength != -1){

        int newfd = open(fileNameChar, O_CREAT | O_WRONLY | O_TRUNC, 0777);
        while(contLength > 0)
        {
            if((size_t)contLength <= buffSize)
            {
                read(client_fd, fileContents, contLength);
                write(newfd, fileContents, contLength);
                break;
            }
            read(client_fd, fileContents, buffSize);
            write(newfd, fileContents, buffSize);
            contLength = contLength - buffSize; 
        }
        close(newfd);
        }
        else
        {
            //If there is no content length print until eof
            int newfd = open(fileNameChar, O_CREAT | O_WRONLY | O_TRUNC, 0777);
            while(file != 0)
            {
                memset(fileContents, 0, buffSize);
                file = read(client_fd, fileContents, buffSize);
                write(newfd, fileContents, buffSize);
            }
            close(newfd);
        }
}

//Handles GET requests
/*
void handleGET(char fileNameChar[], int client_fd)
{
    char fileContents[buffSize];
    int file = 1;
    if(access(fileNameChar, F_OK) == -1)
    {
        
        return;
    }
    if(access(fileNameChar, R_OK) == -1)
    {
        return;
    }
    struct stat statVal;
    stat(, &statVal);
}
*/
int main(int argc, char *argv[])
{
    int server_fd, client_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    //int const buffSize = 4097;
    //char buffer[buffSize];

    //Check if the user inputs too many arguments
    if(argc > 3)
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
    struct hostent *he;
    address.sin_family = AF_INET;
    int user_port;
    if(argc == 1)
    {
        address.sin_port = htons(PORT);
        address.sin_addr.s_addr = INADDR_ANY;
    }
    else if (argc == 2)
    {
        if ( (he = gethostbyname(argv[1]) ) == NULL ) {
            exit(1);
        }
        address.sin_port = htons(PORT);
        memcpy(&address.sin_addr, he->h_addr_list[0], he->h_length);       
    }
    else if(sscanf(argv[2], "%d", &user_port) != -1)
    {
        if ( (he = gethostbyname(argv[1]) ) == NULL ) {
            exit(1); /* error */
        }
        address.sin_port = htons(user_port);
        memcpy(&address.sin_addr, he->h_addr_list[0], he->h_length);
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

        //Retrieve Header, file name, and content length
        std::string header = readHeader(client_fd); 
        ssize_t contLength = getContentLength(header);
        std::string fileName = getFileName(header);
        
        //Check if valid fileName
        if(isCorrectFileName(fileName) == false)
        {
            perror("ERROR: Invalid File Format");
            close(client_fd);
            continue;
        }
        char fileNameChar[fileSize];
        strcpy(fileNameChar, fileName.c_str());

        //Respond to a PUT command
        if(strstr(header.c_str(), "PUT") != nullptr)
        {   
            handlePUT(fileNameChar, contLength, client_fd);
        }
        if(strstr(header.c_str(), "GET") != nullptr)
        {
//            handleGET(fileNameChar, contLength, client_fd);
            printf("%s", "This is a GET command\n");           
        }
        close(client_fd);
    }   
    close(server_fd);
    return 0;
}

