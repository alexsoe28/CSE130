#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>
int main(int argc, char *argv[])
{
    char buff[32768] = "";
    size_t bytes = 1;
    int fileDescriptor;
    
    for (int i = 1; i < argc; i++)
    {
        fileDescriptor = open(argv[i], O_RDONLY);
        if(strncmp(argv[i],"-", 1) == 0)
        {
            while(true)
            {
                memset(buff, 0, sizeof buff);
                read(0, buff, sizeof(buff) - 1);
                write(1, buff, sizeof(buff) - 1);
            }
        }
        else if(fileDescriptor < 0 && strncmp(argv[i],"-", 1) != 0)
        {
            fprintf(stdout, "dog: %s : No such file or directory\n", argv[i]);
        }
        else
        {
            while(bytes != 0){
                memset(buff, 0, sizeof buff);
                bytes = read(fileDescriptor, buff, sizeof(buff) - 1);
                write(1, buff, bytes);
            }
            bytes = 1;
            close(fileDescriptor); 
        } 
    }
    return 0;        
}
