// Bliss Brass
// HW6 Userspace program that prompts the driver
// 6-1-19
// Reads from a pci ethernet module's LED register and writes to it.

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
    int wz = 0, wz2 = 2, rz = 0, rz2 = 0;
    // 0x80 correspondes to bit 7 in the LED control register
    // See Intel manual section 9.2.2.14 LED Control - LEDCTL (0x00E00; RW)
    // https://www.intel.com/content/dam/doc/datasheet/82583v-gbe-controller-datasheet.pdf
    int buffer = 5;
    int v = 0;
    int shift = 0;

    // Must either 'sudo' this program or enable user permissions
    // on ECE_LED with 'sudo chmod 666 /dev/ECE_LED'
    int fd = open("/dev/ECE_LED", O_RDWR);
 
    printf("fd = %d\n", fd); 
    rz = read(fd, &v, 10);

    shift = v >> 8;
    printf("%d is head \n", shift);

    shift = v & 0x000000FF;
    printf("%d is tail \n", shift);
 
    close(fd);
    fd = open("/dev/ECE_LED", O_RDWR);
 
    //wz = write(fd, &buffer, sizeof(int));
    rz2 = read(fd, &v, 10);

    printf("%d is unpacked value\n", v);
    printf("wz is %d, rz is %d and rz2 is %d\n", wz, rz, rz2);
      
    if (fd ==-1 || wz ==-1 || rz ==-1 || rz2 ==-1) { 
        // print which type of error have in a code 
        printf("Error Number % d\n", errno);  
          
        // print program detail "Success or failure" 
        perror("Program");                  
    }
    return 0; 
} 