// Bliss Brass
// HW3 part C code
// 4-30-19
// Changed out %s in the kernel module for %d per the TA
// Which made the garbade data go away but gave compiler warnings
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
    int buffer = 0x80, buffer2 = 0x00;
    int v = 0;

    // BINKY is the file made using:
    // sudo mknod /dev/BINKY c 241 0
    // Where "BINKY" is a randomly chosen name "c" is for character
    // driver. "241" is the major number randomly assigned after
    // a 'sudo insmod' and 0 is the minor number
    // Follow up with a sudo chmod 666 /dev/BINKY for access
    int fd = open("/dev/BINKY", O_RDWR);
 
    printf("fd = %d\n", fd); 
    rz = read(fd, &v, 10);
    printf("%d is starting led_val\n", v);
 
    close(fd);
    fd = open("/dev/BINKY", O_RDWR);
 
    wz = write(fd, &buffer, sizeof(int));
    rz2 = read(fd, &v, 10);

    printf("%d is updated led_val\n", v);

    sleep(2);
    wz2 = write(fd, &buffer2, sizeof(int));

    printf("wz is %d, wz2 is %d, rz is %d and rz2 is %d\n", wz, wz2, rz, rz2);
      
    if (fd ==-1 || wz ==-1 || wz2 ==-1 || rz ==-1 || rz2 ==-1) { 
        // print which type of error have in a code 
        printf("Error Number % d\n", errno);  
          
        // print program detail "Success or failure" 
        perror("Program");                  
    }
    return 0; 
} 