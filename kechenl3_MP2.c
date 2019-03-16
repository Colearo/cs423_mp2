#define LINUX

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <asm/uaccess.h>
#include "mp2_given.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kechen Lu");
MODULE_DESCRIPTION("CS-423 MP2 RMS scheduler");

// #define DEBUG 0

// Declare all needed global variables for the proc fs initializaiton
// The mp1 directory
#define MP2_DIR "mp2"
struct proc_dir_entry * mp2_dir;
// The mp1/status entry
#define MP2_STAT "status"
struct proc_dir_entry * mp2_status;

// Define the structure to persist the PID and its attributes
struct registration_block {
    int pid;
    unsigned long period;
    unsigned long computation_period;
};


// Parser helper function to get next phrase between comma
char* parse_next_phrase(char **str) {
    char *res, *p;
    res = kmalloc(128 * sizeof(char), GFP_KERNEL);
    memset(res, 0, 128 * sizeof(char));
    p = res;

    if (*str == NULL) {
	kfree(res);
	return NULL;
    }

    while(*str != NULL) {
	*(p++) = *((*str)++);
	if (*(*str) == ',') {
	    (*str)++;
	    break;
	}
    }

    return res;
}

#define MAX_BUF_SIZE 4096

// Decalre the callback functions for proc read and write
// Write callback function for user space to write pid to the 
// /proc/mp2/status file
ssize_t write_call(struct file *file, 
	const char __user *usr_buf, 
	size_t n, 
	loff_t *ppos) {
    // Local variable to store the result copied from user buffer
    char *kern_buf, *token, *head;
    // Variables used in the kstrtoul()
    unsigned long pid_val;
    int ret = 0;
    // Structures for pid info
    struct registration_block *rb;

    // Using vmalloc() to allocate buffer for kernel space
    kern_buf = (char *)kmalloc(MAX_BUF_SIZE * sizeof(char), GFP_KERNEL);
    if (!kern_buf) 
	return -ENOMEM;
    memset(kern_buf, 0, MAX_BUF_SIZE * sizeof(char));
    head = kern_buf;

    // If the input str is larger than buffer, return error
    if (n > MAX_BUF_SIZE || *ppos > 0) {
	ret = -EFAULT;
	goto RET;
    }
    if (copy_from_user(kern_buf, usr_buf, n)) {
	ret = -EFAULT;
	goto RET;
    }

    kern_buf[n] = 0;
    printk(KERN_DEBUG "ECHO %s", kern_buf); 

    if (n < 3) {
	printk(KERN_ALERT "Incorrect format to have the commands\n");
	ret = -EFAULT;
	goto RET;
    }

    switch(kern_buf[0]) {
	case 'R' :
	    printk(KERN_DEBUG "REGISTRATION\n");
	    rb = kmalloc(sizeof(struct registration_block), GFP_KERNEL);

	    kern_buf += 2;
	    token = parse_next_phrase(&kern_buf);
	    // Convert the pid string to the integer type
	    ret = kstrtol(token, 10, &pid_val);
	    if (ret != 0) {
		kfree(rb);
		goto RET;
	    }
	    printk(KERN_DEBUG "PID: [%d]\n", (int)pid_val);
	    rb->pid = (int)pid_val;

	    token = parse_next_phrase(&kern_buf);
	    ret = kstrtoul(token, 10, &rb->period);
	    if (ret != 0) {
		kfree(rb);
		goto RET;
	    }
	    printk(KERN_DEBUG "PERIOD: [%lu]\n", rb->period);

	    token = parse_next_phrase(&kern_buf);
	    ret = kstrtoul(token, 10, &rb->computation_period);
	    if (ret != 0) {
		kfree(rb);
		goto RET;
	    }
	    printk(KERN_DEBUG "COMPUTATION PERIOD: [%lu]\n", rb->computation_period);

	    break;

	case 'Y' :
	    printk(KERN_DEBUG "YIELD\n");

	    kern_buf += 2;
	    token = parse_next_phrase(&kern_buf);
	    // Convert the pid string to the integer type
	    ret = kstrtol(token, 10, &pid_val);
	    if (ret != 0) 
		goto RET;
	    printk(KERN_DEBUG "PID: [%d]\n", (int)pid_val);
	    break;

	case 'D' :
	    printk(KERN_DEBUG "DE-REGISTRATION\n");

	    kern_buf += 2;
	    token = parse_next_phrase(&kern_buf);
	    // Convert the pid string to the integer type
	    ret = kstrtol(token, 10, &pid_val);
	    if (ret != 0) 
		goto RET;
	    printk(KERN_DEBUG "PID: [%d]\n", (int)pid_val);
	    break;

	default :
	    printk(KERN_DEBUG "NO MATCHED\n");
	    ret = -EFAULT;
	    goto RET;
    }


RET: kfree(head);
     return ret;
}


// Read callback function for user space to read the proc file in
// /proc/mp1/status
ssize_t read_call(struct file *file, 
	char __user *usr_buf, 
	size_t n, 
	loff_t *ppos) {
    // Local variable to store the data would copy to user buffer
    char* kern_buf;
    int length = 0;

    // Using kmalloc() to allocate buffer for kernel space
    kern_buf = (char *)kmalloc(MAX_BUF_SIZE * sizeof(char), GFP_KERNEL);
    if (!kern_buf) 
	return -ENOMEM;
    memset(kern_buf, 0, MAX_BUF_SIZE * sizeof(char));

    // If the input str is larger than buffer or 
    // someone has read it to let offset pointer is not to 0, return zero
    if (n < MAX_BUF_SIZE || *ppos > 0) {
	kfree(kern_buf);
	return 0;
    }

    // If no pid registered in list
    if (length == 0) {
	kfree(kern_buf);
	length = sprintf(kern_buf, "No PID registered\n");
    }

    printk(KERN_DEBUG "Read this proc file %d\n", length);
    kern_buf[length] = 0;

    // Copy returned data from kernel space to user space
    if (copy_to_user(usr_buf, (const void *)kern_buf, length)) {
	kfree(kern_buf);
	return -EFAULT;
    }
    *ppos = length;

    kfree(kern_buf);
    return length;
}

// File operations callback functions, overload read, write, open, and 
// so forth a series of ops
static const struct file_operations mp2_proc_fops = {
    .owner = THIS_MODULE,
    .write = write_call,
    .read = read_call,
};


// mp1_init - Called when module is loaded
int __init mp2_init(void) {
   #ifdef DEBUG
   printk(KERN_ALERT "MP2 MODULE LOADING\n");
   #endif
   // Make a new proc dir /proc/mp1
   mp2_dir = proc_mkdir(MP2_DIR, NULL); 
   // Make a new proc entry /proc/mp1/status
   mp2_status = proc_create(MP2_STAT, 0666, mp2_dir, &mp2_proc_fops); 

   printk(KERN_ALERT "MP2 MODULE LOADED\n");
   return 0;   
}

// mp2_exit - Called when module is unloaded
void __exit mp2_exit(void) {
   #ifdef DEBUG
   printk(KERN_ALERT "MP1 MODULE UNLOADING\n");
   #endif
   // Remove all the proc file entry and dir we created before
   proc_remove(mp2_status);
   proc_remove(mp2_dir);

   printk(KERN_ALERT "MP2 MODULE UNLOADED\n");
}

// Register init and exit funtions
module_init(mp2_init);
module_exit(mp2_exit);
