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
        if(strncmp(argv[i],"-", 1) == 0)
        {
            while(true)
            {
                char buffer [32768] = "";
                read(0, buffer, sizeof(buff) - 1);
                write(1, buffer, sizeof(buff) - 1);
            }
        }
        else
        {
            if(fileDescriptor < 0 && strncmp(argv[i],"-", 1) != 0)
            {
                fprintf(stdout, "dog: %s : No such file or directory\n", argv[i]);
            }
            fileDescriptor = open(argv[i], O_RDONLY);
            while(bytes != 0){
                bytes = read(fileDescriptor, buff, sizeof(buff) - 1);
                write(1, buff, bytes);
            }
            bytes = 1;
            close(fileDescriptor); 
        } 
    }
    return 0;        
}
