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
#include <string.h>
#include <deque>
#include <pthread.h>
#define PORT 80

const size_t buffSize = 4097;
const size_t fileSize = 27;
int logFd;
size_t offSet = 0;
bool logFileExists = false;
pthread_mutex_t offSetLock;

struct context{
    pthread_cond_t cond;
    pthread_mutex_t lock;
    std::deque<int> clientQueue;
};

size_t getOffSet(size_t logLength)
{
    offSetLock = PTHREAD_MUTEX_INITIALIZER;
    
    pthread_mutex_lock(&(offSetLock));
    size_t currLogPos = offSet;
    offSet += logLength;
    pthread_mutex_unlock(&(offSetLock));
   
    printf("Current Log Position is %zd\n", currLogPos);
    return currLogPos;
}

void printGETLog(char fileNameChar[])
{    
    char get[50];
    strcpy(get, "GET ");
    strcat(get, fileNameChar);
    strcat(get, " length 0\n========\n");
    size_t logOffSet = getOffSet((size_t)strlen(get));
    pwrite(logFd, get, strlen(get), logOffSet);
}

void printErrorLog(char fileNameChar[], char command[], std::string response)
{
    std::string fileNameString(fileNameChar);
    std::string commandString(command);
    std::string errorLog;
    
    errorLog = "FAIL: " + commandString + " " + fileNameString 
        + " HTTP/1.1 --- response " + response +"\n========\n";
    size_t logOffSet = getOffSet((size_t)errorLog.length());
    pwrite(logFd, errorLog.c_str(), errorLog.length(), logOffSet);   
}

//Function for returning the header as string
std::string readHeader(int fd)
{
    char buffer[buffSize];
    char *p;
    size_t len = buffSize - 1;
    ssize_t n;
    p = buffer;

    //While the we can read from the client socket   
    while((n = recv(fd, p, 1, 0)) > 0)
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
    //printf("The fd = %zd and the content is %s\n\n", n, buffer);
    //Initialize the pointer to the end of our new string with the Null character
    *p = '\0';
    if(n <= 0)
    {
        printf("Hey the recv returned 0\n");
        return std::string(buffer);
    }
    if (len == 0)
    {
        std::cerr << "Failed on fd = " << fd << " n = " << n << "\n";
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
    char put[4] = "PUT";
    std::string response;
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
            response = "403";
            printErrorLog(fileNameChar, put, response);
            return;
        }

    }

    //Write/create file
    if(contLength != -1)
    {   
        int newfd = open(fileNameChar, O_CREAT | O_RDWR | O_TRUNC, 0777);
        while(contLength > 0)
        {
            if((size_t)contLength <= buffSize)
            {
                int readSize = recv(client_fd, fileContents, contLength, 0);
                printf("readSize = %d\n", readSize);
                if(readSize == 0)
                {
                    char msg[39] = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
                    write(client_fd, msg, strlen(msg));
                    response = "500";
                    printErrorLog(fileNameChar, put, response);
                    close(client_fd);
                    return;
                }
                write(newfd, fileContents, readSize);
				contLength = contLength - readSize;
				continue;
            }
            int readSize = recv(client_fd, fileContents, buffSize, 0);
            if(readSize == 0)
            {
                char msg[39] = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
                write(client_fd, msg, strlen(msg));
                response = "500";
                printErrorLog(fileNameChar, put, response);
                close(client_fd);
                return;
            }
            write(newfd, fileContents, readSize);
            contLength = contLength - readSize;
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
            fileInBytes = recv(client_fd, fileContents, buffSize, 0);
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
    if(fileExists == true && ContentLength == true)
    {
        char http_header[39] = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n"; 
        write(client_fd, http_header, strlen(http_header));      
    }
    if(fileExists == false && ContentLength == true)
    {
        char http_header[44] = "HTTP/1.1 201 Created\r\nContent-Length: 0\r\n\r\n"; 
        write(client_fd, http_header, strlen(http_header)); 
    }
}

//Handles GET requests
void handleGET(char fileNameChar[], int client_fd)
{
    printf("We are in the Get function\n");
    std::fflush(stdout);
    char fileContents[buffSize];
    std::string response;
    char get[4] = "GET";
    int fileInBytes = 1;
    
    //Check if the file exists
    if(access(fileNameChar, F_OK) == -1)
    {
        char msg[46] = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        write(client_fd, msg, strlen(msg));
        response = "404";
        printErrorLog(fileNameChar, get, response);
        return;
    }
    if(access(fileNameChar, R_OK) == -1)
    {
        char msg[46] = "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n";
        write(client_fd, msg, strlen(msg));
        response = "403";
        printErrorLog(fileNameChar, get, response);
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
    if(logFileExists == true){
        printf("Printing Get to Logfile\n");
        printGETLog(fileNameChar);
    }
    close(newfd);

}

void handleClient(int client_fd)
{
    std::string header = readHeader(client_fd); 
    std::string response;
    while(!header.empty())
    {
        ssize_t contLength = getContentLength(header);
        std::string fileName = getFileName(header);

        //Turn C++ String to C-String
        char fileNameChar[fileSize];
        strcpy(fileNameChar, fileName.c_str());
        
        //Return error is not a PUT or GET command
        if(strstr(header.c_str(), "PUT") == nullptr && strstr(header.c_str(), "GET") == nullptr)
        {
            char msg[39] = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
            write(client_fd, msg, strlen(msg));
            close(client_fd);
            continue;
        }

        //Respond to PUT
        if(strstr(header.c_str(), "PUT") != nullptr)
        {   
            if(isCorrectFileName(fileName) == false)
            {
                char msg[39] = "HTTP/1.1 400 Internal Server Error\r\n\r\n";
                write(client_fd, msg, strlen(msg));
                response = "400";
                char put[4] = "PUT";
                printErrorLog(fileNameChar, put, response);
            }
            else
            {
                handlePUT(fileNameChar, contLength, client_fd);
            }
        }
        //Respond to GET
        if(strstr(header.c_str(), "GET") != nullptr)
        {
            if(isCorrectFileName(fileName) == false)
            {
                char msg[39] = "HTTP/1.1 400 Internal Server Error\r\n\r\n";
                write(client_fd, msg, strlen(msg));
                response = "400";
                char get[4] = "GET";
                printErrorLog(fileNameChar, get, response);
            }
            else{
                handleGET(fileNameChar, client_fd);
            }
        }
        printf("Reading next header\n");
        header = readHeader(client_fd);
        
    }
    printf("Done with Client!\n");
    close(client_fd);
}

void * workerFunction(void* arg)
{
    context* c = (context*)arg;
    while(true)
    {
        pthread_mutex_lock(&(c->lock));
        while(c->clientQueue.empty())
        {
            pthread_cond_wait(&(c->cond), &(c->lock));
        }
        int client_fd = c->clientQueue.front();
        c->clientQueue.pop_front();
        pthread_mutex_unlock(&(c->lock));
        std::cerr << "working on " << client_fd << "\n";
        handleClient(client_fd);
    }
    return NULL; 
}

int main(int argc, char *argv[])
{
    int server_fd, client_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    
    //Initialize Mutexes and condition variables
    pthread_t workers;
    context c;
    c.cond = PTHREAD_COND_INITIALIZER;
    c.lock = PTHREAD_MUTEX_INITIALIZER;
    
    //Check if the socket is setup succesfully
    if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("ERROR: Socket failed");
        exit(EXIT_FAILURE);
    }

    //Initialize port, address, logfile, and threadcount
    struct hostent *he;
    address.sin_family = AF_INET;
    int user_port;
    int opt;
    char *logFileName = nullptr;
    size_t numThreads = 4;

    if(argc == 1 || argc > 7)
    {
        perror("Usage: ./httpserver [address] [PORT]");
        exit(EXIT_FAILURE);
    }

    while((opt = getopt (argc, argv, ":l:N:")) != -1)
    {
        switch(opt)
        {
            case 'l':
                logFileName = optarg;
                logFileExists = true;
                break;

            case 'N':
                if(sscanf(optarg, "%zd", &numThreads) == 0)
                {
                    fprintf(stderr, "ERROR: %c requires an integer argument \n", optopt);
                    exit(EXIT_FAILURE);
                }
                break;
            case '?':
                fprintf(stderr, "ERROR: Unknown option %c\n", optopt);
                exit(EXIT_FAILURE);
            case ':':
                fprintf(stderr, "ERROR: %c requires an argument\n", optopt);
                exit(EXIT_FAILURE);
        }
    }

    if(argv[optind] != nullptr){
        if((he = gethostbyname(argv[optind])) == nullptr)
        {
            perror("ERROR: Invalid host name or ip");
            exit(EXIT_FAILURE);
        }
        else
        {
            memcpy(&address.sin_addr, he->h_addr_list[0], he->h_length);
        }
    }
    else
    {
        perror("ERROR: httpserver requires an address name/ip");
        exit(EXIT_FAILURE);
    }

    if(argv[optind + 1] != nullptr)
    {
        if(sscanf(argv[optind + 1], "%d", &user_port) != -1)
        {
            address.sin_port = htons(user_port);
        }
        else
        {
            address.sin_port = htons(PORT);
        }
    }    

    //Initialize logfile 
    if(logFileExists == true)
    {
        printf("Log File exists!\n");
        logFd = open(logFileName, O_CREAT | O_WRONLY | O_TRUNC, 0777);
    }

    //Initialize threads;
    for(size_t i = 0; i < numThreads; i++)
    {
        pthread_create(&workers, NULL, workerFunction, (void*)&c);
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
    
    printf("There are %zd threads\n", numThreads);
    //Respond to get and put requests
    while(true)
    {
        while((client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) > 0)
        {
            pthread_mutex_lock(&(c.lock));
            std::cerr << "Adding fd " << client_fd << "\n";
            c.clientQueue.push_back(client_fd);
            pthread_cond_signal(&(c.cond));
            pthread_mutex_unlock(&(c.lock));
        }
    }
    return 0;
}

