// Bliss Brass
// HW6 ECE 373 6-1-19
// Driver that blinks LEDS as it gets packets of data into is Descriptor Ring

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>		// headers that may be red lined are because
#include <linux/slab.h>		// they must be compiled with the Makefile's
#include <linux/uaccess.h>	// special compilation path
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>

#define DEVCNT 1
#define DEVNAME "BINKY"

// Prototypes
static int pci_blinkDriver_probe(struct pci_dev *pdev, const struct pci_device_id *ent);
static void pci_blinkDriver_remove(struct pci_dev *pdev);
static void make_DMA (struct pci_dev *pdev);

char blinkDriver_name[] = "blinkDriver";
char NODENAME[] = "ECE_LED";		// Name of the node in /dev
static dev_t mydev_node;

// Taking a value in an array, casting it, and returning it’s address
#define E1000_RX_DESC(R, i) (&(((struct e1000_rx_descriptor *) ((R).rxDE))[i]))

#define CTRL 0x00004				// Device Control Register
#define ICR 0x000C0					// Interrupt Cause Register
#define IMS 0x000D0					// Interrupt Mask Set/Read register
#define IMC 0x000D8					// Interrupt Mask Clear register
#define RCTL 0x00100				// Receive Control Register
#define LEDCTL 0x00E00				// LED Control Register
#define RDBAL0 0x02800				// Descriptor base address low offset
#define RDBAH0 0x02804				// Descriptor base address high offset
#define RDLEN0 0x02808				// Receive Descriptors Length (all 16)
#define RDH0 0x02810				// Receive Descriptor Head
#define RDT0 0x02818				// Receive Descriptor Tail

static struct mydev_dev {
	struct cdev cdev;
	long syscall_val;
	long led_initial_val;
	struct class * myClass;			// Some special kernel C class shit
	long LED;
	struct e1000_rx_ring * rx_ring;
} mydev;

static struct myPCI {
	void *hw_addr;
} myPci;

/*
// Model for a descriptor
struct e1000_rx_descriptor {
    __le64 buffer_addr;     // Address of the Descriptor's data buffer
    union {        			// Union makes these two 32 bit data point to the same place as the 64 buffer
        __le32 data;
        struct {
            __le16 length;  // Data Buffer length
            u8 cso;         // Checksum offset
            u8 cmd;         // Descriptor Control
        } flags;
    } lower;
    union {
        __le32 data;
        struct {
            u8 status;      // Descriptor Status
            u8 css;         // Checksum Start
            __le16 special; // Reserved
        } fields;
    } upper;
};*/

struct e1000_rx_descriptor {
    __le64 buffer_addr;     // Address of the Descriptor's data buffer
    union {        			// Union makes these two 32 bit data point to the same place as the 64 buffer
        __le32 data;
        struct {
            __le16 VLAN_tag;  // Data Buffer length
            u8 errors;         // Checksum offset
            u8 status;         // Descriptor Control
        } flags;
    } lower;
    union {
        __le32 data;
        struct {
			__le16 packet_CheckSum;
            //u8 status;      // Descriptor Status
            //u8 css;         // Checksum Start
            __le16 length; // Reserved
        } fields;
    } upper;
};

struct e1000_rx_ring {
	/* physical address of the descriptor ring */
	dma_addr_t dma;
	/* length of descriptor ring in bytes */
	unsigned int size;
	/* number of descriptors in the ring */
	unsigned int count;
	/* next descriptor to associate a buffer with */
	unsigned int next_to_use;
	/* next descriptor to check for DD status bit */
	unsigned int next_to_clean;
	/* array of buffer information structs */
	struct e1000_rx_buffer * buffer_info;
	struct e1000_rx_descriptor * rxDE;
};

// Buffer that holds the data in each Descriptor
struct e1000_rx_buffer {
	union {
		struct page *page;	/* jumbo: alloc_page */
		u8 *data; 			/* else, netdev_alloc_frag */
	} rxbuf;
	dma_addr_t dma;
};

// A struct for 'work'
struct work_cont {
	struct work_struct real_work;
	int    arg;
} work_cont;

struct work_cont *test_wq;

static const struct pci_device_id pci_blinkDriverTable[] = {
	// Numbers specific to Intel and the ethernet driver
	{ PCI_DEVICE(0x8086, 0x100e) },
	{},
};

// Controls what gets called when module is 'insmod' or 'rmmod'
static struct pci_driver pci_blinkDriver = {
	.name = "Blink Driver",			// Driver name, shows up in lspci -vvvv
	.id_table = pci_blinkDriverTable,
	.probe = pci_blinkDriver_probe,
	.remove = pci_blinkDriver_remove,
};

// This shows up under /sys/module/hw6/parameters
static int blink_rate = 2;
module_param(blink_rate, int, S_IRUSR | S_IWUSR);

static void thread_function(struct work_struct * work_arg) {
	// *c_ptr now points to a copy of work_cont for local use
	// Never did figure out how this function worked.
	//struct work_cont *c_ptr = container_of(work_arg, struct work_cont, real_work);
	struct e1000_rx_descriptor * desc;
	struct e1000_rx_buffer * buf;
	u8 * data;
	u32 tail;
	int LED = 0;

	// Sleeping for 0.5 seconds
	msleep(500);
	LED = readl(myPci.hw_addr + LEDCTL);
	writel(0xFFFFFF7F & LED, myPci.hw_addr + LEDCTL);		// Turning OFF LED 0
	
	// Looping while tail is not equal to head
	while ((tail = (readl(myPci.hw_addr + RDT0) + 1) % 16) != readl(myPci.hw_addr + RDH0)) {
		desc = &mydev.rx_ring->rxDE[tail];
		buf = &mydev.rx_ring->buffer_info[tail];
		data = mydev.rx_ring->buffer_info[tail].rxbuf.data;
	
		printk(KERN_INFO "Processed descriptor %u\n", tail);
		printk(KERN_INFO "Status: 0x%02hhX\n", desc->lower.flags.status);
		printk(KERN_INFO "Length: 0x%04hX\n", desc->upper.fields.length);
		printk(KERN_INFO "Head is %d\n", readl(myPci.hw_addr + RDH0));
		printk(KERN_INFO "Tail is %d\n", readl(myPci.hw_addr + RDT0));
		
		// Updating tail
		writel(tail, myPci.hw_addr + RDT0);

		LED = readl(myPci.hw_addr + LEDCTL);
		if (tail % 2 == 0) {
			// Tail was even
			writel(0xFFF7FFF & LED, myPci.hw_addr + LEDCTL);
		}
		else {
			// Tail was odd
			writel(0x8000 | LED, myPci.hw_addr + LEDCTL);
		}
	}
}

// Callback function for Timer
static irqreturn_t irq_handler(int irq, void * data) {
	unsigned int RXDMT0 = 0x10;
	u32 cause;
	u32 irq_mask;
	//printk(KERN_INFO "IRQ HIT\n"); printk can sleep in a IRQ so is dangerous!
	cause = readl(myPci.hw_addr + ICR) & RXDMT0;

	if (cause != RXDMT0) {
		return IRQ_NONE;
	}

	// Getting current IRQ config
	irq_mask = readl(myPci.hw_addr + IMS);
	writel(0xFFFFFFFF, myPci.hw_addr + IMC);		// Disabling all Interrupts
	// Turning on LED 0
	writel(0x80 | readl(myPci.hw_addr + LEDCTL), myPci.hw_addr + LEDCTL);

	schedule_work(&test_wq->real_work);				// Queueing up work
	writel(RXDMT0, myPci.hw_addr + ICR);			// Clearing the interrupt to allow a new one
	writel(irq_mask, myPci.hw_addr + IMS);			// Re-enabling Interrupts

	return IRQ_HANDLED;
}

// Sets up the pci component of the module
static int pci_blinkDriver_probe(struct pci_dev *pdev, const struct pci_device_id *ent) {
	// BAR = Base Address Register
	resource_size_t mmio_start, mmio_len;
	unsigned long barMask;
	printk(KERN_INFO "Blink Driver PCI Probe called\n");

	// Needed for IRQ to go off properly
	pci_enable_device(pdev);


	// Get BAR mask
	// IORESOURCE_MEM gives us a BAR mask related to all the MEM things
	// As opposed to an IORESOURCE_IO it might give us Region 2 in lspci
	barMask = pci_select_bars(pdev, IORESOURCE_MEM);
	printk(KERN_INFO "Barmask %lx", barMask);

	// Reserving BAR areas / mapping them after we got the barMask
	if (pci_request_selected_regions(pdev, barMask, blinkDriver_name)) {
		printk(KERN_ERR "Request of selected regions failed\n");

		goto unregister_selected_regions;
	}

	// Getting the start of this region
	mmio_start = pci_resource_start(pdev, 0);
	// The length of this region
	mmio_len = pci_resource_len(pdev, 0);

	printk(KERN_INFO "mmio start: %lx", (unsigned long) mmio_start);
	printk(KERN_INFO "mmio len: %lx", (unsigned long) mmio_len);

	// THIS VERY IMPORTANT. ioremap IS RETURNING THE BASE ADDRESS OF THE PCI CHIP!
	// So you use hw_addr + offset to access the registers you want
	if (!(myPci.hw_addr = ioremap(mmio_start, mmio_len))) {
		printk(KERN_ERR "ioremap failed\n");
		goto unregister_ioremap;
	}

	// If everything is good then
	// readl returns the contents at that memory address
	mydev.led_initial_val = readl(myPci.hw_addr + 0xE00);
	printk(KERN_INFO "Initial starting led val is %lx\n", mydev.led_initial_val);

	// Turning on LED 2
	writel(0x800000, myPci.hw_addr + LEDCTL);

	// Setting up the work queue
	test_wq = kmalloc(sizeof(*test_wq), GFP_KERNEL);
	INIT_WORK(&test_wq->real_work, thread_function);
	

    // Setting up DMA
    make_DMA(pdev);

	return 0;

unregister_ioremap:
	iounmap(myPci.hw_addr);

unregister_selected_regions:
	pci_release_selected_regions(pdev, pci_select_bars(pdev, IORESOURCE_MEM));

	return -1;
}

// Setting up Direct Memory Access
static void make_DMA (struct pci_dev *pdev) {
	unsigned int reg;
    int i;
	int ctrlTemp, err, ret;
	struct e1000_rx_ring * rx_ring;
	printk(KERN_INFO "Made it to DMA!\n");


	// Making space for the ring of descriptors
    rx_ring = kzalloc(sizeof(struct e1000_rx_ring), GFP_KERNEL);

	mydev.rx_ring = rx_ring;

	rx_ring->count = 16;
	rx_ring->next_to_use = rx_ring->next_to_clean = 0;
	rx_ring->size = sizeof(struct e1000_rx_descriptor) * 16;

	// Disabling interrupts by writings 1's to the IMC register
	writel(0xFFFFFFFF, myPci.hw_addr + IMC);
	// Writing a 1 to the RST bit in the CTRL register to reset the e1000 device
	writel(0x04000000, myPci.hw_addr + CTRL);
	msleep(10); // Small safety delay for testing
	writel(0xFFFFFFFF, myPci.hw_addr + IMC);				// Disabling interrupts again after reset

	// Allocating memory spots for the buffers spots?
	// Allocates memory for an array apparently 'kcalloc'
	rx_ring->buffer_info = kcalloc(rx_ring->count, sizeof(struct e1000_rx_buffer), GFP_KERNEL);

	// Making 16 descriptors
	rx_ring->rxDE = dma_alloc_coherent(&pdev->dev, rx_ring->size, &rx_ring->dma, GFP_KERNEL);
	// Bit shifting the dma handle over 32 bits because 2 registers are needed to hold the full 64 bit address
	reg = (rx_ring->dma >> 32) & 0xFFFFFFFF;

	// Writing those 32 upper bits to the upper register
	writel(reg, myPci.hw_addr + RDBAH0);
	reg = rx_ring->dma & 0xFFFFFFFF;
	writel(reg, myPci.hw_addr + RDBAL0);

	// Writing the Descriptor ring length to hardware
  	writel(rx_ring->size, myPci.hw_addr + RDLEN0);
	// Setting Head to 0 and Tail to 15 because head 'follows' tail automatically in hardware
  	writel(0, myPci.hw_addr + RDH0);
  	writel(0xF, myPci.hw_addr + RDT0);

	for (i = 0; i < 16; i++) {
		struct e1000_rx_descriptor * rrxx = E1000_RX_DESC(*rx_ring, i);
		u8 * buffer = kzalloc(2048, GFP_KERNEL);

		// Pinning indivual buffers as readable only
		// dma_single gives more individual control than dma_alloc
		rx_ring->buffer_info[i].rxbuf.data = buffer;
		rx_ring->buffer_info[i].dma = dma_map_single(&pdev->dev, buffer, 2048, DMA_FROM_DEVICE);
		rrxx->buffer_addr = cpu_to_le64(rx_ring->buffer_info[i].dma);
		//printk(KERN_INFO "buffer is %llx", rx_ring->buffer_info[i].dma);
	}

	// Writing to the Auto Negotiation forcing the Link Up
	ctrlTemp = readl(myPci.hw_addr + CTRL);
	writel(ctrlTemp | 0x60, myPci.hw_addr + CTRL);
	// Enable Receive Control Register, Unicast and Multicast Promiscuous mode
	writel(0x1A, myPci.hw_addr + RCTL);
	
	// Getting an interrupt and tasking irq_handler to be called when it happens
	err = request_irq(pdev->irq, irq_handler, 0, "ece_IRQ", rx_ring);
	if (err) {
		ret = -EFAULT;
	}

	// Enable Interrupts on bit 4 "Receive Descriptor Minimum Threshold"
	writel (0x10, myPci.hw_addr + IMS);
}

static void dma_freeUp(struct pci_dev *pdev) {
	int i;

	// Free the descriptors 
	for (i = 0; i < 16; i++) {
		if (mydev.rx_ring->buffer_info[i].dma)
			dma_unmap_single(&pdev->dev, mydev.rx_ring->buffer_info[i].dma, 2048, DMA_FROM_DEVICE);
			kfree(mydev.rx_ring->buffer_info[i].rxbuf.data);
	}

	if (mydev.rx_ring->rxDE){
		dma_free_coherent(&pdev->dev, mydev.rx_ring->size, mydev.rx_ring->rxDE, mydev.rx_ring->dma);
		mydev.rx_ring->rxDE = NULL;
	}

	kfree(mydev.rx_ring->buffer_info);
	mydev.rx_ring->buffer_info = NULL;
}

// Handles tear down for pci removal
static void pci_blinkDriver_remove(struct pci_dev *pdev) {
	printk(KERN_INFO "Removing e1000 HW6 PCI remove called\n");

	// Freeing the IRQ that was called
	free_irq(pdev->irq, mydev.rx_ring);
	pci_disable_device(pdev);					// Undoes the pci_enable in probe

	// Cleaning up the descriptor ring
	dma_freeUp(pdev);

	iounmap(myPci.hw_addr);
	pci_release_selected_regions(pdev, pci_select_bars(pdev, IORESOURCE_MEM));
}

// Called when the module is 'opened'
static int example5_open(struct inode *inode, struct file *file) {
	//mydev.syscall_val = blink_rate;
	//mydev.LED = 0x00;

	printk(KERN_INFO "HW6 file Opened--------------------\n");

	return 0;
}

// Called when something tries to read from the module
static ssize_t example5_read(struct file *file, char __user *buf, size_t len, loff_t *offset) {
	/* Get a local kernel buffer set aside */
	int ret;
	int head = 0, tail = 0;

	if (*offset >= sizeof(int))
		return 0;

	/* Make sure our user wasn't bad... */
	if (!buf) {
		ret = -EINVAL;
		goto out;
	}

	// Packing Head and Tail into 1 INT to be send to userspace and unpacked
	head = readl(myPci.hw_addr + RDH0);
	tail = readl(myPci.hw_addr + RDT0);
	printk(KERN_INFO "User got HEAD from us %d\n", head);
	printk(KERN_INFO "User got TAIL from us %d\n", tail);

	head = head << 8;											// Shifting head to the upper 8 of 16 bits
	printk(KERN_INFO "User got HEAD from us %d\n", head);
	head = head | tail;											// Adding tail to the lower 8 bits
	printk(KERN_INFO "User got HEAD from us %d\n", head);

	// Copying to userspace the current blink_rate
	if (copy_to_user(buf, &head, sizeof(int))) {
		ret = -EFAULT;
		goto out;
	}
	ret = sizeof(int);
	*offset += len;

	// Printing to dmesg the current value of syscall_val
	printk(KERN_INFO "User read() from us %d\n", head);

out:
	return ret;
}

static ssize_t example5_write(struct file *file, const char __user *buf, size_t len, loff_t *offset) {
	/* Have local kernel memory ready */
	char *kern_buf;
	int ret;

	/* Make sure our user isn't bad... */
	if (!buf) {
		ret = -EINVAL;
		goto out;
	}

	// kernel version of calloc()
	kern_buf = kzalloc(sizeof(int), GFP_KERNEL);

	/* ...and make sure it's good to go */
	if (!kern_buf) {
		ret = -ENOMEM;
		goto out;
	}

	/* Copy from the user-provided buffer */
	if (copy_from_user(kern_buf, buf, len)) {
		ret = -EFAULT;
		goto mem_out;
	}

	// Assigning whats in the buffer to syscall_val
	mydev.syscall_val = *(int*) kern_buf;
	ret = len;

	if ((mydev.syscall_val < -4294967296) || (mydev.syscall_val > 4294967296)) {
		printk(KERN_INFO "Overflow error, value is greater than a 2^32: %lx", mydev.syscall_val);
		return -1;
	}
	else if (mydev.syscall_val < 0) {
		ret = -EINVAL;
		printk(KERN_INFO "Blink rate was less than zero %d\n", blink_rate);
		goto mem_out;
	}

	blink_rate = mydev.syscall_val;
	// print what userspace gave us
	printk(KERN_INFO "blink_rate is \"%d\"\n", blink_rate);

mem_out:
	kfree(kern_buf);
out:
	return ret;
}

// Special name "file_operations" that designates what to call when module receives any
// of the following system calls
/* File operations for our device */
static struct file_operations mydev_fops = {
	.owner = THIS_MODULE,
	.open = example5_open,
	.read = example5_read,
	.write = example5_write,
};

// DEVCNT sets number of minor numbers
static int __init blinkDriver_init(void) {
	printk(KERN_INFO "ECE373 HW6 loading... blink_rate = %d\n", blink_rate);

	// Getting major and minor device numbers
	if (alloc_chrdev_region(&mydev_node, 0, DEVCNT, DEVNAME)) {
		printk(KERN_ERR "alloc_chrdev_region() failed!\n");
		return -1;
	}

	printk(KERN_INFO "Allocated %d devices at major: %d\n", DEVCNT,
	       MAJOR(mydev_node));

	// Initialize the character device and add it to the kernel
	cdev_init(&mydev.cdev, &mydev_fops);
	mydev.cdev.owner = THIS_MODULE;

	if (cdev_add(&mydev.cdev, mydev_node, DEVCNT)) {
		printk(KERN_ERR "cdev_add() failed!\n");
		/* clean up chrdev allocation */
		unregister_chrdev_region(mydev_node, DEVCNT);

		return -1;
	}

	if (pci_register_driver(&pci_blinkDriver)) {
		printk(KERN_ERR "pci_register_driver failed\n");
		goto unreg_pci_driver;
	}

	// These mext 2 if statements perform the same task as mknod
	if ((mydev.myClass = class_create(THIS_MODULE, "led_devTEST")) == NULL) {
        printk(KERN_ERR "class_create failed\n");
        goto destroy_class;
    }

    if (device_create(mydev.myClass, NULL, mydev_node, NULL, NODENAME) == NULL) {
        printk(KERN_ERR "device_create failed\n");
        goto unreg_dev_create;
    }

	// Successful init
	return 0;

unreg_pci_driver:
	pci_unregister_driver(&pci_blinkDriver);

	//return -1;

unreg_dev_create:
    device_destroy(mydev.myClass, mydev_node);

destroy_class:
    class_destroy(mydev.myClass);

	return -1;
}

// Just a standard module cleanup function after a "sudo rmmod NAME"
static void __exit example5_exit(void) {
	// 
	//flush_work(&test_wq->real_work);
	//flush_workqueue(*test_wq->real_work);
	//destroy_workqueue(*test_wq->real_work);
	cancel_work_sync(&test_wq->real_work);

	pci_unregister_driver(&pci_blinkDriver);

	/* destroy the cdev */
	cdev_del(&mydev.cdev);

	/* clean up the devices */
	unregister_chrdev_region(mydev_node, DEVCNT);
	device_destroy(mydev.myClass, mydev_node);
	class_destroy(mydev.myClass);

	printk(KERN_INFO "ECE373 HW6 module unloaded!\n");
}

MODULE_AUTHOR("Bliss Brass");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
module_init(blinkDriver_init);
module_exit(example5_exit);