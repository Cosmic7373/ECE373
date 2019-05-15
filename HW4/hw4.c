// Bliss Brass
// 5-11-19
// HW4 Making a LED blink on the ethernet driver
// via Userspace program

#include <stdio.h> 
#include <fcntl.h> 
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <stdint.h>

static int dev_mem_fd;
extern int errno;

#define GPRC 0x4074 // Good Packets Received Count register, page 367
#define led_control 0xE00

int main() {
    // BAR = Base Address Register
    void * BAR;
    // Get these from lspci -vvvv
    uint32_t mem_window_sz = 128000; // Size of memory at Region 0
    uint64_t eth1_region0 = 0xf2200000; 
    uint32_t * LED_addr;
    uint32_t * GPRC_addr;
    uint32_t * RCTL_addr;
    int LED_default;
    int RCTL = 0x100; // Receive Control register, page 314
    int rc;

    // Opening kernels memory file
    dev_mem_fd = open("/dev/mem", O_RDWR);

    if (dev_mem_fd == -1) { 
        // print which type of error have in a code 
        printf("Error Number % d\n", errno);  
        // print program detail "Success or failure" 
        perror("Program");                  
    }

    // Mapping the device 
    BAR = mmap(NULL, mem_window_sz, PROT_READ | PROT_WRITE, MAP_SHARED, 
                dev_mem_fd, eth1_region0);

    if (BAR == MAP_FAILED) {
        printf("Mapping failed\n");
        return -1;
    }

    RCTL_addr = (uint32_t*) (BAR + RCTL);
    GPRC_addr = (uint32_t *) (BAR + GPRC);
    LED_addr = (uint32_t *) (BAR + led_control);

    // Saving starting value
    LED_default = *LED_addr;
    // Enabling receives by writing a 1 to bit 1
    *RCTL_addr = 0x2;
    printf("Current LEDCTL value is %x\n", LED_default);
    printf("The GPRC reg address is %p and it's contents are %x\n", GPRC_addr, *GPRC_addr);

    // Doing some reads writes to turn on LEDS starting with LED2 and LED0
    // For LED bit info see page 304 of:
    // https://pdos.csail.mit.edu/6.828/2018/readings/hardware/8254x_GBe_SDM.pdf
    *LED_addr = 0x800080;
    sleep(2);
    *LED_addr = 0x000000;
    sleep(2);
    
    for (int j = 5; j > 0; j--) {
        *LED_addr = 0x80000000;
        for (int i = 4; i > 0; i--) {
            sleep(1);
            *LED_addr = *LED_addr >> 8;
        }
    }

    // Restoring starting value
    *LED_addr = LED_default;
    printf("The GPRC register contents are %x\n", *GPRC_addr);
    
    // Undoing the assignments of mmap
    rc = munmap(BAR, mem_window_sz);

    if (rc == -1) { 
        printf("Error Number % d\n", errno);  
        perror("Program");                  
    }

    printf("Finished\n");

    return 0;
}