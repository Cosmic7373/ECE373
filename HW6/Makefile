KERNEL_DIR = /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

obj-m += hw6.o

# Gap
# makefile for ledmon (duh)
#

#CFLAGS=-Wall

ledmon: ledmon.o
	$(CC) -o $@  $<

cha:  
	gcc -g -o cha user.c

# Gap
default:
	$(MAKE) -C $(KERNEL_DIR) SUBDIRS=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) SUBDIRS=$(PWD) clean
	rm ledmon
