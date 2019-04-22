// Bliss Brass
// HW2 part C code
// Changed out %s in the kernel module for %d per the TA
// Which made the garbade data go away but gave compiler warnings

#include <stdio.h> 
#include <fcntl.h> 
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

extern int errno;

int main() {      
    // 
    int sz = 0, rz = 0;
    char *c = (char *) calloc(100, sizeof(char));
    int *v = (int *) calloc(100, sizeof(int));

    // HHH is the file made using:
    // sudo mknod /dev/HHH c 241 0
    // Where "HHH" is a randomly chosen name "c" is for character
    // driver. "241" is the major number randomly assigned after
    // a 'sudo insmod' and 0 is the minor number
    int fd = open("/dev/HHH", O_RDWR);
 
    printf("fd = %d\n", fd); 

    rz = read(fd, v, 10);

    printf("int sys is %d \n", *v);
    int daz = 17;
 
    //sz = write(fd, "17'\0'", strlen("17'\0'"));
    //sz = write(fd, "17'\0'", 4);
    sz = write(fd, &daz, 4);

    read(fd, v, 4);
    printf("int sys is now %d and sz is %d and rz is %d\n", *v, sz, rz);
      
    if (fd ==-1) { 
        // print which type of error have in a code 
        printf("Error Number % d\n", errno);  
          
        // print program detail "Success or failure" 
        perror("Program");                  
    } 
    return 0; 
} 