// Bliss Brass HW1 ECE 373 4-13-19

#include <linux/init.h>
#include <linux/module.h>

MODULE_LICENSE("Dual BSD/GPS");

static int __init hello_init(void) {
  // KERN_INFO acts as a less verbose mode
  printk(KERN_INFO "Hello, kernel\n");
  return 0;
}

static void __exit hello_exit(void) {
  printk(KERN_INFO "Goodbye, kernel\n");
}

module_init(hello_init);
module_exit(hello_exit);
