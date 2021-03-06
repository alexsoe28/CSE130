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
#include <sys/select.h>
#define PORT 80

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
        if(strstr(buffer, "\r\n\r\n") != nullptr)
        {
            break;
        }

        //Check if we have exceeded the header buffer
        if(len == 0)
        {
            break;
        }
    }
    //Initialize the pointer to the end of our new string with the Null character
    *p = '\0';
    if(n < 0)
    {
        close(fd);
        return std::string(buffer);
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
    char fileContents[buffSize];
    int fileInBytes = 1;
    bool fileExists = false;
    bool ContentLength = false;
    bool fileRead = false;

    if(contLength != -1)
    {
        ContentLength = true;
    }
    //Checks if the file exists
    if(access(fileNameChar, F_OK) == 0)
    {
        fileExists = true;
        if(access(fileNameChar, W_OK) == -1)
        {
            char msg[46] = "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n";
            write(client_fd, msg, strlen(msg));
            return;
        }

    }
    if(fileExists == true && ContentLength == true)
    {
        char http_header[39] = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n"; 
        write(client_fd, http_header, strlen(http_header));      
    }
    if(fileExists == false && ContentLength == true)
    {
        char http_header[44] = "HTTP/1.1 201 Created\r\nContent-Length: 0\r\n"; 
        write(client_fd, http_header, strlen(http_header)); 
    }

    //Write/create file
    if(contLength != -1)
    {    
        int newfd = open(fileNameChar, O_CREAT | O_WRONLY | O_TRUNC, 0777);
        while(contLength > 0)
        {
            if((size_t)contLength <= buffSize)
            {
                ssize_t readSize = read(client_fd, fileContents, contLength);
                write(newfd, fileContents, readSize);
				contLength = contLength - readSize;
				continue;
            }
            ssize_t readSize = read(client_fd, fileContents, buffSize);
            write(newfd, fileContents, readSize);
            contLength = contLength - buffSize; 
        }
        close(newfd);
    }
    else
    {
        //If there is no content length print until eof
        int newfd = open(fileNameChar, O_CREAT | O_WRONLY | O_TRUNC, 0777);
        while(fileInBytes != 0)
        {
            memset(fileContents, 0, buffSize);
            fileInBytes = read(client_fd, fileContents, buffSize);
            if(fileRead == false)
            {
                if(fileExists == true)
                {
                    char http_header[39] = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n"; 
                    write(client_fd, http_header, strlen(http_header));      
                }
                else
                {
                    char http_header[44] = "HTTP/1.1 201 Created\r\nContent-Length: 0\r\n\r\n"; 
                    write(client_fd, http_header, strlen(http_header)); 
                }
                fileRead = true;
            }
            write(newfd, fileContents, fileInBytes);
        }
        close(newfd);
    }
}

//Handles GET requests
void handleGET(char fileNameChar[], int client_fd)
{
    char fileContents[buffSize];
    int fileInBytes = 1;

    //Check if the file exists
    if(access(fileNameChar, F_OK) == -1)
    {
        char msg[46] = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        write(client_fd, msg, strlen(msg));
        return;
    }
    if(access(fileNameChar, R_OK) == -1)
    {
        char msg[46] = "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n";
        write(client_fd, msg, strlen(msg));
        return;
    }

    //Print out content length with header
    struct stat statVal;
    stat(fileNameChar, &statVal);
    off_t contentLength = statVal.st_size;
    std::string clientHeader = "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(contentLength) + "\r\n\r\n";
    write(client_fd, clientHeader.c_str(), clientHeader.length());
    
    //Print to client
    int newfd = open(fileNameChar, O_RDONLY);
    while(fileInBytes != 0)
    {
        memset(fileContents, 0, buffSize);
        fileInBytes = read(newfd, fileContents, buffSize);
        write(client_fd, fileContents, fileInBytes);
    }
    close(fileInBytes);
}

void handle(int client_fd)
{
    std::string header = readHeader(client_fd);
    while(!header.empty())
    {
        ssize_t contLength = getContentLength(header);
        std::string fileName = getFileName(header);

        //Turn C++ String to C-String
        char fileNameChar[fileSize];
        strcpy(fileNameChar, fileName.c_str());
        
        //Respond to a PUT command
        if(strstr(header.c_str(), "PUT") == nullptr && strstr(header.c_str(), "GET") == nullptr)
        {
            char msg[39] = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
            write(client_fd, msg, strlen(msg));
            close(client_fd);
            continue;
        }

        //Check if valid fileName
        if(isCorrectFileName(fileName) == false)
        {
            char msg[39] = "HTTP/1.1 400 Internal Server Error\r\n\r\n";
            write(client_fd, msg, strlen(msg));
            close(client_fd);
            continue;
        }
        //Respond to PUT
        if(strstr(header.c_str(), "PUT") != nullptr)
        {   
            handlePUT(fileNameChar, contLength, client_fd);
        }
        
        //Respond to GET
        if(strstr(header.c_str(), "GET") != nullptr)
        {
            handleGET(fileNameChar, client_fd);
        }
        //Retrieve Header, file name, and content length
        header = readHeader(client_fd); 
		
    }   
}


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
        perror("Usage: ./httpserver [address] [PORT]");
        exit(EXIT_FAILURE);
    }
    else if (argc == 2)
    {
        if ( (he = gethostbyname(argv[1]) ) == NULL ) {
            exit(EXIT_FAILURE);
        }
        address.sin_port = htons(PORT);
        memcpy(&address.sin_addr, he->h_addr_list[0], he->h_length);       
    }
    else if(sscanf(argv[2], "%d", &user_port) != -1)
    {
        if ( (he = gethostbyname(argv[1]) ) == NULL ) {
            exit(EXIT_FAILURE);
        }
        address.sin_port = htons(user_port);
        memcpy(&address.sin_addr, he->h_addr_list[0], he->h_length);
    } 
    else
    {
        perror("ERROR: Invalid format for httpserver");
        exit(EXIT_FAILURE);
    }

    //Bind address to server
    if(bind(server_fd, (struct sockaddr *)&address, sizeof address) < 0)
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
        handle(client_fd);
    }
    
    return 0;
}

