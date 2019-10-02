#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>
int main(int argc, char *argv[])
{
    size_t bytes;
    char buff[32];
    size_t fileDescriptor = open(argv[2], O_RDONLY);
    if(fileDescriptor < 0){
        fprintf(stderr, "dog: %s : No such file or directory", argv[2]);
    }
    else{
        bytes = read(fileDescriptor, buff, sizeof(buff) - 1);
        write(1, buff, bytes);
    }    
    close(fileDescriptor); 
    return 0;        
}
