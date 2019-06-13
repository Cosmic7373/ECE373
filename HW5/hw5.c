// Bliss Brass
// HW5 ECE 373 5-25-19
// Loading a Linux module that can read/write from userspace

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

#define DEVCNT 1
#define DEVNAME "BINKY"
int clow = 0;

// Prototypes
static int pci_blinkDriver_probe(struct pci_dev *pdev, const struct pci_device_id *ent);
static void pci_blinkDriver_remove(struct pci_dev *pdev);

char blinkDriver_name[] = "blinkDriver";
char NODENAME[] = "ECE_LED";		// Name of the node in /dev
static dev_t mydev_node;
static struct timer_list my_timer;
struct timespec tm;

static struct mydev_dev {
	struct cdev cdev;
	long syscall_val;
	long led_initial_val;
	//long led_current_val;
	struct class * myClass;			// Some special kernel C class shit
	long LED;
	int openTracker;
} mydev;

static struct myPCI {
	//struct pci_dev *pdev;
	void *hw_addr;
} myPci;

static const struct pci_device_id pci_blinkDriverTable[] = {
	// Numbers specific to Intel and the ethernet driver
	{ PCI_DEVICE(0x8086, 0x100e) },
	{},
};

static struct pci_driver pci_blinkDriver = {
	.name = "Blink Driver",			// Driver name, shows up in lspci -vvvv
	.id_table = pci_blinkDriverTable,
	.probe = pci_blinkDriver_probe,
	.remove = pci_blinkDriver_remove,
};

// This shows up under /sys/modules/hw5/parameters
// With "hw5" seemingly coming from the hw5.c filename
// Setting it's default is 15 and giving it permisions with the S_
// Update with 'sudo insmod hw5.ko blink_rate=NUMBER'
static int blink_rate = 2;
module_param(blink_rate, int, S_IRUSR | S_IWUSR);

// Callback function for Timer
void my_callback(struct timer_list *list) {
	// XOR'ing the LED value everytime this function is called
	mydev.LED = mydev.LED ^ 0x80;

	tm = current_kernel_time();
	printk("Current num secs (from callback): %ld", tm.tv_sec);

	writel(mydev.LED, myPci.hw_addr + 0xE00);

	printk(KERN_INFO "Blink Rate is NOW 50%% of %d", blink_rate);
	printk(KERN_INFO "Repeat test--------------------%d\n", clow++);

	mod_timer(&my_timer, jiffies + msecs_to_jiffies((blink_rate * 1000) / 2));
}

// Sets up the pci component of the module
static int pci_blinkDriver_probe(struct pci_dev *pdev, const struct pci_device_id *ent) {
	// BAR = Base Address Register
	resource_size_t mmio_start, mmio_len;
	unsigned long barMask;
	printk(KERN_INFO "Blink Driver PCI Probe called\n");

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

	return 0;

unregister_ioremap:
	iounmap(myPci.hw_addr);

unregister_selected_regions:
	pci_release_selected_regions(pdev, pci_select_bars(pdev, IORESOURCE_MEM));

	return -1;
}

// Handles tear down for pci removal
static void pci_blinkDriver_remove(struct pci_dev *pdev) {
	printk(KERN_INFO "Blink Driver PCI remove called\n");

	iounmap(myPci.hw_addr);
	pci_release_selected_regions(pdev, pci_select_bars(pdev, IORESOURCE_MEM));
}

// Called when the module is 'opened'
static int example5_open(struct inode *inode, struct file *file) {
	mydev.syscall_val = blink_rate;
	mydev.LED = 0x00;

	printk(KERN_INFO "HW5 file Opened--------------------\n");
	tm = current_kernel_time();
	printk("Current num secs (from init): %ld", tm.tv_sec);

	// Prohibiting multiple creations from multiple open calls
	if (mydev.openTracker == 0) {
		// Setting up the timer and designating it's callback function
		timer_setup(&my_timer, my_callback, 0);
		// Starting the timer and setting it in seconds at a 50% rate
		mod_timer(&my_timer, jiffies + msecs_to_jiffies((blink_rate * 1000) / 2));
		mydev.openTracker = 1;
	}

	return 0;
}

// Called when something tries to read from the module
static ssize_t example5_read(struct file *file, char __user *buf, size_t len, loff_t *offset) {
	/* Get a local kernel buffer set aside */
	int ret;

	if (*offset >= sizeof(int))
		return 0;

	/* Make sure our user wasn't bad... */
	if (!buf) {
		ret = -EINVAL;
		goto out;
	}

	// Copying to userspace the current blink_rate
	if (copy_to_user(buf, &blink_rate, sizeof(int))) {
		ret = -EFAULT;
		goto out;
	}
	ret = sizeof(int);
	*offset += len;

	// Printing to dmesg the current value of syscall_val
	printk(KERN_INFO "User read() from us %d\n", blink_rate);

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
	printk(KERN_INFO "ECE373 HW5 loading... blink_rate = %d\n", blink_rate);
	mydev.openTracker = 0;

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

	return -1;

unreg_dev_create:
    device_destroy(mydev.myClass, mydev_node);

destroy_class:
    class_destroy(mydev.myClass);

	return -1;
}

// Just a standard module cleanup function after a "sudo rmmod NAME"
static void __exit example5_exit(void) {
	// Deletes the timer
	del_timer(&my_timer);
	pci_unregister_driver(&pci_blinkDriver);

	/* destroy the cdev */
	cdev_del(&mydev.cdev);

	/* clean up the devices */
	unregister_chrdev_region(mydev_node, DEVCNT);
	device_destroy(mydev.myClass, mydev_node);
	class_destroy(mydev.myClass);

	printk(KERN_INFO "ECE373 HW5 module unloaded!\n");
}

MODULE_AUTHOR("Bliss Brass");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
module_init(blinkDriver_init);
module_exit(example5_exit);