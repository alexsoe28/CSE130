#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>
int main(int argc, char *argv[])
{
    size_t bytes = 1;
    char buff[32768];
    int fileDescriptor; 
    for (int i = 1; i < argc; i++)
    {
        fileDescriptor = open(argv[i], O_RDONLY);
        if(fileDescriptor < 0){
            fprintf(stdout, "dog: %s : No such file or directory\n", argv[i]);
        }
        else{
            while(bytes != 0){
                bytes = read(fileDescriptor, buff, sizeof(buff) - 1);
                write(1, buff, bytes);
            }
            bytes = 1;
        }
        close(fileDescriptor); 
    }
    return 0;        
}
