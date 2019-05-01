// Bliss Brass
// HW3 ECE 373 4-30-19
// Loading a Linux module that can read/write from userspace

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
//#include <limits.h>

#define DEVCNT 1
#define DEVNAME "BINKY"

// Prototypes
static int pci_blinkDriver_probe(struct pci_dev *pdev, const struct pci_device_id *ent);
static void pci_blinkDriver_remove(struct pci_dev *pdev);

char blinkDriver_name[] = "blinkDriver";

static struct mydev_dev {
	struct cdev cdev;
	long syscall_val;
	long led_initial_val;
	long led_current_val;
} mydev;

static struct myPCI {
	struct pci_dev *pdev;
	void *hw_addr;
} myPci;

static const struct pci_device_id pci_blinkDriverTable[] = {
	// Numbers specific to Intel and the ethernet driver
	{ PCI_DEVICE(0x8086, 0x100e) },
	{},
};

static struct pci_driver pci_blinkDriver = {
	.name = "Blink Driver",
	.id_table = pci_blinkDriverTable,
	.probe = pci_blinkDriver_probe,
	.remove = pci_blinkDriver_remove,
};

static dev_t mydev_node;

// This shows up under /sys/modules/hw3/parameters
// With "hw3" seemingly coming from the hw3.c filename
// Setting it's default is 15 and giving it permisions with the S_
static int testPara = 15;
module_param(testPara, int, S_IRUSR | S_IWUSR);

// Currently unused but here for example purposes
// this doesn't appear in /sys/modules
static int exam_nosysfs = 15;
module_param(exam_nosysfs, int, 0);

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
	printk(KERN_INFO "/dev node successfully opened!\n");
	mydev.syscall_val = 40;

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
	mydev.led_current_val = readl(myPci.hw_addr + 0xE00);

	// Taking the base address that got stored in .hw_addr and adding the
	// offset of the LED control register
	if (copy_to_user(buf, &mydev.led_current_val, sizeof(int))) {
		ret = -EFAULT;
		goto out;
	}
	ret = sizeof(int);
	*offset += len;

	// Printing to dmesg the current value of syscall_val
	printk(KERN_INFO "User read() from us %ld\n", mydev.led_current_val);
	//printk(KERN_INFO "buf is: %d\n", buf);

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

	/* Get some memory to copy into... */
	// kernel version of calloc()
	kern_buf = kzalloc(sizeof(int), GFP_KERNEL);
	//kern_buf = kmalloc(sizeof(int), GFP_KERNEL);

	/* ...and make sure it's good to go */
	if (!kern_buf) {
		ret = -ENOMEM;
		goto out;
	}

	/* Copy from the user-provided buffer */
	if (copy_from_user(kern_buf, buf, len)) {
	//if (copy_from_user(mydev.sys_int, buf, len)) {
		/* uh-oh... */
		ret = -EFAULT;
		goto mem_out;
	}

	// Assigning whats in the buffer to syscall_val
	mydev.led_current_val = *(int*) kern_buf;
	ret = len;

	mydev.syscall_val = mydev.led_current_val;
	//printk(KERN_INFO "Test: %lx", INT_MAX);
	if ((mydev.syscall_val < -4294967296) || (mydev.syscall_val > 4294967296)) {
		printk(KERN_INFO "Overflow error, value is greater than a 2^32: %lx", mydev.syscall_val);
		return -1;
	}

	writel(mydev.led_current_val, myPci.hw_addr + 0xE00);

	// print what userspace gave us
	printk(KERN_INFO "led_current_val is \"%ld\"\n", mydev.led_current_val);

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

// Most of this is just standard replicant code, DEVCNT sets number of minor numbers
static int __init blinkDriver_init(void) {
	printk(KERN_INFO "ECE373 HW3 loading... Test Parameter = %d\n", testPara);

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

	// Successful init
	return 0;

// Seems to be just a label?
unreg_pci_driver:
	pci_unregister_driver(&pci_blinkDriver);
//exit:
	return -1;
}

// Just a standard module cleanup function after a "sudo rmmod NAME"
static void __exit example5_exit(void) {
	pci_unregister_driver(&pci_blinkDriver);

	/* destroy the cdev */
	cdev_del(&mydev.cdev);

	/* clean up the devices */
	unregister_chrdev_region(mydev_node, DEVCNT);

	printk(KERN_INFO "ECE373 HW3 module unloaded!\n");
}

MODULE_AUTHOR("Bliss Brass");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
module_init(blinkDriver_init);
module_exit(example5_exit);