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
    // Return value ints
    int wz = 0, rz = 0, rz2 = 0;
    int buffer = 17;
    int v = 0;
    // Also works with the below and using *v in the printf and just v in the read()
    //int *v = (int *) calloc(100, sizeof(int));
 
    // HHH is the file made using:
    // sudo mknod /dev/HHH c 241 0
    // Where "HHH" is a randomly chosen name "c" is for character
    // driver. "241" is the major number randomly assigned after
    // a 'sudo insmod' and 0 is the minor number
    int fd = open("/dev/HHH", O_RDWR);
 
    printf("fd = %d\n", fd); 
    rz = read(fd, &v, 10);
    printf("%d is starting syscall_val\n", v);
 
    close(fd);
    fd = open("/dev/HHH", O_RDWR);
 
    //sz = write(fd, "17'\0'", 4);
    wz = write(fd, &buffer, sizeof(int));
    rz2 = read(fd, &v, 10);

    printf("%d is updated syscall_val\n", v);
    printf("wz is %d, rz is %d and rz2 is %d\n", wz, rz, rz2);
      
    if (fd ==-1) { 
        // print which type of error have in a code 
        printf("Error Number % d\n", errno);  
          
        // print program detail "Success or failure" 
        perror("Program");                  
    } 
    return 0; 
} 