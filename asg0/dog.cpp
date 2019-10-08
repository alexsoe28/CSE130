#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <err.h>
#include <unistd.h>
int main(int argc, char *argv[])
{
    int fileDescriptor;
    char buff[32768] = "";
    size_t bytes = 1;
    size_t endOfFile = 1;

    //Checks the stdin for the key char "-" and then prints user input to stdout   
    for (int i = 1; i < argc; i++)
    {
        fileDescriptor = open(argv[i], O_RDONLY);
        if(strncmp(argv[i],"-", 1) == 0)
        {
            while(true)
            {
                memset(buff, 0, sizeof buff);
                endOfFile = read(0, buff, sizeof(buff) - 1);
                //If the user types ctrl-D, stop reading from stdin
                if(endOfFile != 0)
                {
                    write(1, buff, sizeof(buff) - 1);
                }
                else
                {
                    break;
                }
            }
        }
        //If the the name is not a file and is not "-" print an error
        else if(fileDescriptor < 0 && strncmp(argv[i],"-", 1) != 0)
        {
           err(1, "%s", argv[i]); 
        }
        //If the file does exist print out the contents.
        else
        {
            while(bytes != 0)
            {
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
