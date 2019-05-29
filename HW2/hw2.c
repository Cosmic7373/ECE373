// Bliss Brass
// HW2 ECE 373 4-21-19
// Loading a Linux module that can read/write from userspace

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define DEVCNT 5
#define DEVNAME "HW2_WOOOOW"

static struct mydev_dev {
	struct cdev cdev;
	/* more stuff will go in here later... */
	//int sys_int;
	int syscall_val;
} mydev;

static dev_t mydev_node;

// This shows up under /sys/modules/hw2/parameters
// With "hw2" seemingly coming from the hw2.c filename
// Setting it's default to 15 and giving it permisions with the S_
static int testPara = 15;
module_param(testPara, int, S_IRUSR | S_IWUSR);

// Currently unused but here for example purposes
/* this doesn't appear in /sys/modules */
static int exam_nosysfs = 15;
module_param(exam_nosysfs, int, 0);

static int example5_open(struct inode *inode, struct file *file) {
	printk(KERN_INFO "Successfully opened!\n");
	//mydev.sys_int = 23;
	mydev.syscall_val = 40;

	return 0;
}

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

	if (copy_to_user(buf, &mydev.syscall_val, sizeof(int))) {
		ret = -EFAULT;
		goto out;
	}
	ret = sizeof(int);
	*offset += len;

	// Printing to dmesg the current value of syscall_val
	printk(KERN_INFO "User got from us %d\n", mydev.syscall_val);
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

	//printk(KERN_INFO "kbus addr: %d\n", kern_buf);
	//printk(KERN_INFO "kbus val: %d\n", *(int*)kern_buf);
	// Assigning whats in the buffer to syscall_val
	mydev.syscall_val = *(int*) kern_buf;
	ret = len;

	printk(KERN_INFO "syscall_val is \"%d\"\n", mydev.syscall_val);
	/* print what userspace gave us */
	//printk(KERN_INFO "Userspace wrote \"%d\" to us\n", kern_buf);

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
static int __init example5_init(void) {
	printk(KERN_INFO "ECE373 HW2 module loading... Test Parameter = %d\n", testPara);

	if (alloc_chrdev_region(&mydev_node, 0, DEVCNT, DEVNAME)) {
		printk(KERN_ERR "alloc_chrdev_region() failed!\n");
		return -1;
	}

	printk(KERN_INFO "Allocated %d devices at major: %d\n", DEVCNT,
	       MAJOR(mydev_node));

	/* Initialize the character device and add it to the kernel */
	cdev_init(&mydev.cdev, &mydev_fops);
	mydev.cdev.owner = THIS_MODULE;

	if (cdev_add(&mydev.cdev, mydev_node, DEVCNT)) {
		printk(KERN_ERR "cdev_add() failed!\n");
		/* clean up chrdev allocation */
		unregister_chrdev_region(mydev_node, DEVCNT);

		return -1;
	}

	return 0;
}

// Just a standard module cleanup function after a "sudo rmmod NAME"
static void __exit example5_exit(void) {
	/* destroy the cdev */
	cdev_del(&mydev.cdev);

	/* clean up the devices */
	unregister_chrdev_region(mydev_node, DEVCNT);

	printk(KERN_INFO "ECE373 HW2 module unloaded!\n");
}

MODULE_AUTHOR("Bliss Brass");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
module_init(example5_init);
module_exit(example5_exit);