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
#include <math.h>
#define PORT 80

const size_t buffSize = 4097;
const size_t fileNameSize = 27;
const size_t cacheSize = 4;
int logFd;
size_t offSet = 0;
bool logFileExists = false;
pthread_mutex_t offSetLock;

size_t cachePosition = 0;
bool cachingFlag = false;
std::deque<std::string> cacheContents;
std::deque<std::string> cacheQueue;

struct context{
    pthread_cond_t cond;
    pthread_mutex_t lock;
    std::deque<int> clientQueue;
};

/*
void cacheFile(char fileNameChar[], std::string fileContents)
{
    std::string fileNameString = fileNameChar;
    if(cachePosition )
    {
        for(int i = 0; i < cacheSize; i++)
        {
            if(cacheQueue[i] == )
            {
            
            }

        }
    }
}
*/
size_t getOffSet(size_t logLength)
{
    offSetLock = PTHREAD_MUTEX_INITIALIZER;
    
    pthread_mutex_lock(&(offSetLock));
    size_t currLogPos = offSet;
    offSet += logLength;
    pthread_mutex_unlock(&(offSetLock));
   
    return currLogPos;
}

size_t printPUTLogHeader(char fileNameChar[], ssize_t contLength)
{
    if(logFileExists == false)
    {
        return 0;
    }
    std::string endMessage = "========\n";
    std::string contLenString = std::to_string((int)contLength);
    std::string fileNameString = fileNameChar;
    std::string putLog = "PUT " + fileNameString + " length " + contLenString + "\n";
   
    int numLines = (ceil((double)contLength/20.0));

    size_t putOffSet = putLog.length() + (9 * numLines) + 
                       (3 * contLength) + 9;
    
    size_t startPosition = getOffSet(putOffSet);
    pwrite(logFd, putLog.c_str(), putLog.length(), startPosition);
    
    size_t endMsgOffSet = startPosition + putOffSet - 9;
    pwrite(logFd, endMessage.c_str(), endMessage.length(), endMsgOffSet);
    startPosition += (size_t)putLog.length();
    return startPosition;
}

std::string convertCharToHex(char c)
{
    char hex[3];
    sprintf(hex, " %02X", c);
    std::string hexString = hex;
    return hexString;
}

std::string convertLineNumber(size_t totalReadSize)
{
    char buffer[9];
    sprintf(buffer, "%08zd", totalReadSize);
    std::string lineNumString = buffer;
    return lineNumString;
}

size_t printPUTLog(size_t totalReadSize, size_t readSize, char fileContents[], size_t startPosition, bool last)
{
    if(logFileExists == false)
    {
        return 0;
    }
    size_t newPosition = startPosition;
    for(size_t i = 0; i < readSize; i++)
    {
        if(totalReadSize % 20 == 0)
        {
            std::string lineNumString = convertLineNumber(totalReadSize);
            pwrite(logFd, lineNumString.c_str(), lineNumString.length(), newPosition);
            newPosition += lineNumString.length();
        }
        std::string hexData = convertCharToHex(fileContents[i]);
        pwrite(logFd, hexData.c_str(), hexData.length(), newPosition);
        newPosition += hexData.length();
        if(totalReadSize % 20 == 19)
        {
            char newLine[] = "\n";
            pwrite(logFd, newLine, 1, newPosition);
            newPosition++;
        }
        totalReadSize++;
    }
    if(last == true && totalReadSize % 20 != 0)
    {
        char newLine[] = "\n";
        pwrite(logFd, newLine, 1, newPosition);
        newPosition++;
    }
    return newPosition;
}

void printGETLog(char fileNameChar[])
{   
    if(logFileExists == false)
    {
        return;
    }
    char get[50] = {0};
    strcpy(get, "GET ");
    strcat(get, fileNameChar);
    strcat(get, " length 0\n========\n");
    size_t logOffSet = getOffSet((size_t)strlen(get));
    pwrite(logFd, get, strlen(get), logOffSet);
}

void printErrorLog(char fileNameChar[], char command[], std::string response)
{
    if(logFileExists == false)
    {
        return;
    }
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
    char buffer[buffSize] = {0};
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
    //Initialize the pointer to the end of our new string with the Null character
    *p = '\0';
    if(n <= 0)
    {
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
    if(fileName.length() > fileNameSize || fileName.length() < fileNameSize)
    {
        return false;
    }
    //Check if fileName has proper characters
    char fileNameChar[fileNameSize] = {0};
    strcpy(fileNameChar, fileName.c_str());
    for(size_t i = 0; i < fileNameSize; i++)
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
    std::string token = "";
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
    char put[] = "PUT";
    std::string response = "";
    int fileInBytes = 1;
    bool fileExists = false;
    bool ContentLength = false;
    bool fileRead = false;
    size_t startPosition;
    size_t totalReadSize = 0;

    char cacheBuffer[buffSize];
    std::string cacheFileContents;

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
    
    startPosition = printPUTLogHeader(fileNameChar, contLength);
    //Write/create file
    if(contLength != -1)
    {   
        int newfd = open(fileNameChar, O_CREAT | O_RDWR | O_TRUNC, 0777);
        while(contLength > 0)
        {
            if((size_t)contLength <= buffSize)
            {
                int readSize = recv(client_fd, fileContents, contLength, 0);
                if(readSize == 0)
                {
                    char msg[] = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
                    write(client_fd, msg, strlen(msg));
                    response = "500";
                    printErrorLog(fileNameChar, put, response);
                    close(client_fd);
                    return;
                }
                //Copy data to a cache string
                memset(cacheBuffer, 0, sizeof cacheBuffer);
                memcpy(cacheBuffer, fileContents, readSize);
                std::string temp(cacheBuffer, readSize);
                cacheFileContents += temp;

                //Print Log
                printPUTLog(totalReadSize, readSize, fileContents, startPosition, true);

                //Write to the new File
                write(newfd, fileContents, readSize);
				contLength = contLength - readSize;
				continue;
            }
            int readSize = recv(client_fd, fileContents, buffSize, 0);
            if(readSize == 0)
            {
                char msg[] = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
                write(client_fd, msg, strlen(msg));
                response = "500";
                printErrorLog(fileNameChar, put, response);
                close(client_fd);
                return;
            }
            //Copy data to a cache string
            memset(cacheBuffer, 0, sizeof cacheBuffer);
            memcpy(cacheBuffer, fileContents, readSize);
            std::string temp(cacheBuffer, readSize);
            cacheFileContents += temp;
            
            //Print log
            startPosition = printPUTLog(totalReadSize, readSize, fileContents, startPosition, false);
            totalReadSize += readSize;

            //Write to the new file
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
                    char http_header[] = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n"; 
                    write(client_fd, http_header, strlen(http_header));      
                }
                else
                {
                    char http_header[] = "HTTP/1.1 201 Created\r\nContent-Length: 0\r\n\r\n"; 
                    write(client_fd, http_header, strlen(http_header)); 
                }
                fileRead = true;
            }
            printPUTLog(fileInBytes, contLength, fileContents, startPosition, false);
            write(newfd, fileContents, fileInBytes);
        }
        close(newfd);
    }
    printf("%s\n", cacheFileContents.c_str());
    printf("The content length is: %lu\n", cacheFileContents.length());
    if(fileExists == true && ContentLength == true)
    {
        char http_header[] = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n"; 
        write(client_fd, http_header, strlen(http_header));      
    }
    if(fileExists == false && ContentLength == true)
    {
        char http_header[] = "HTTP/1.1 201 Created\r\nContent-Length: 0\r\n\r\n"; 
        write(client_fd, http_header, strlen(http_header)); 
    }
}

//Handles GET requests
void handleGET(char fileNameChar[], int client_fd)
{
    char fileContents[buffSize] = {0};
    std::string response = "";
    char get[] = "GET";
    int fileInBytes = 1;
    
    //Check if the file exists
    if(access(fileNameChar, F_OK) == -1)
    {
        char msg[] = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        write(client_fd, msg, strlen(msg));
        response = "404";
        printErrorLog(fileNameChar, get, response);
        return;
    }
    if(access(fileNameChar, R_OK) == -1)
    {
        char msg[] = "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n";
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
        printGETLog(fileNameChar);
    }
    close(newfd);

}

void handleClient(int client_fd)
{
    std::string header = readHeader(client_fd); 
    std::string response = "";
    while(!header.empty())
    {
        ssize_t contLength = getContentLength(header);
        std::string fileName = getFileName(header);

        //Turn C++ String to C-String
        char fileNameChar[fileNameSize];
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
        header = readHeader(client_fd);
        
    }
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
    size_t numThreads = 1;

    if(argc == 1 || argc > 7)
    {
        perror("Usage: ./httpserver [address] [PORT]");
        exit(EXIT_FAILURE);
    }

    while((opt = getopt(argc, argv, ":l:N:c")) != -1)
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
            case 'c':
                cachingFlag = true;
                break;
            case '?':
                fprintf(stderr, "ERROR: Unknown option %c\n", optopt);
                exit(EXIT_FAILURE);
            case ':':
                fprintf(stderr, "ERROR: %c requires an argument\n", optopt);
                exit(EXIT_FAILURE);
        }
    }

    if(argc - optind >= 1){
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

    if(argc - optind == 2)
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
    
    //Respond to get and put requests
    while(true)
    {
        while((client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) > 0)
        {
            pthread_mutex_lock(&(c.lock));
            c.clientQueue.push_back(client_fd);
            pthread_cond_signal(&(c.cond));
            pthread_mutex_unlock(&(c.lock));
        }
    }
    return 0;
}

