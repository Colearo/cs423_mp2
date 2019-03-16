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
#include "mp1_given.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kechen Lu");
MODULE_DESCRIPTION("CS-423 MP1");

// #define DEBUG 0

// Declare all needed global variables for the proc fs initializaiton
// The mp1 directory
#define MP1_DIR "mp1"
struct proc_dir_entry * mp1_dir;
// The mp1/status entry
#define MP1_STAT "status"
struct proc_dir_entry * mp1_status;

// Define the structure to persist the PID and its CPU use time
struct registration_block {
    int pid;
    unsigned long used_time;
    struct list_head next;
};

// Declare a linked list to store the registered process information
// Initialize the empty linked list
LIST_HEAD(registration_list);

// Declare the timer callback function
static void timer_handler(unsigned long data);
// Declare the staic variable to timeout five seconds
static unsigned long five_sec;
// Declare the global kernel timer
DEFINE_TIMER(update_timer, timer_handler, 0, 0);

// Declare the workqueue
static struct workqueue_struct *wq;
// Define the workqueue name
#define UPDATE_WQ "update_wq"
// Declare work handler
static void work_handler(struct work_struct *work_arg);

// Decalre a mutex lock
struct mutex access_lock;

#define MAX_BUF_SIZE 4096
// Register the pid, which means to return error if has repeated,
// and return 0 if the adding pid to linked list successfully.
// This function is wraping function without lock
static int __register_pid(int pid_val, unsigned long used_time) {
    // Linked list entry
    struct registration_block* new_block_ptr;
    struct registration_block* cur, * temp;

    // Iterate the whole linked list to prevent insert repeated one of pid
    list_for_each_entry_safe(cur, temp, &registration_list, next) {
       // Access the entry and determine if the pid has been in the list
       if (cur->pid == pid_val) {
	   printk(KERN_ALERT "Repeated pid number\n");
	   return -EFAULT;
       }
    }

    // Allocate a new block to store the result
    new_block_ptr = kmalloc(sizeof(*new_block_ptr), GFP_KERNEL);
    new_block_ptr->pid = pid_val;
    new_block_ptr->used_time = used_time;
    INIT_LIST_HEAD(&new_block_ptr->next);
    // Add the entry to the linked list to register
    list_add(&new_block_ptr->next, &registration_list);

    return 0;
}

// Decalre the callback functions for proc read and write
// Write callback function for user space to write pid to the 
// /proc/mp1/status file
ssize_t register_pid(struct file *file, 
	const char __user *usr_buf, 
	size_t n, 
	loff_t *ppos) {
    // Local variable to store the result copied from user buffer
    char* kern_buf;
    // Variables used in the kstrtol()
    unsigned long pid_val, used_time;
    int ret;

    // Using vmalloc() to allocate buffer for kernel space
    kern_buf = (char *)kmalloc(MAX_BUF_SIZE * sizeof(char), GFP_KERNEL);
    if (!kern_buf) 
	return -ENOMEM;
    memset(kern_buf, 0, MAX_BUF_SIZE * sizeof(char));

    // If the input str is larger than buffer, return error
    if (n > MAX_BUF_SIZE || *ppos > 0) {
	kfree(kern_buf);
	return -EFAULT;
    }
    if (copy_from_user(kern_buf, usr_buf, n)) {
	kfree(kern_buf);
	return -EFAULT;
    }

    kern_buf[n] = 0;
    printk(KERN_DEBUG "ECHO %s", kern_buf); 

    // Convert the pid string to the integer type
    ret = kstrtoul(kern_buf, 10, &pid_val);
    if (ret != 0 || pid_val >= ((1<<16)-1)) {
	kfree(kern_buf);
	printk(KERN_ALERT "Unrecognized pid number");
	return -EFAULT;
    }

    printk(KERN_DEBUG "PID %d", (int)pid_val); 

    // Through the helper function to get the cpu used time
    ret = get_cpu_use((int)pid_val, &used_time); 
    if (ret != 0) {
	kfree(kern_buf);
	printk(KERN_ALERT "Unrecognized or invalid pid number\n");
	return -EFAULT;
    }

    // Lock
    mutex_lock(&access_lock);
    // Wraper function for registration
    if (__register_pid((int)pid_val, used_time) != 0) {
	// Unlock
	mutex_unlock(&access_lock);
	kfree(kern_buf);
	return -EFAULT;
    }
    // Unlock
    mutex_unlock(&access_lock);

    kfree(kern_buf);
    return n;
}

// Wraper function for reading callback to access the registration
// linked list
static void __get_status(int* length, char* kern_buf) {
    // Linked list entry
    struct registration_block* cur, * temp;
    // Iterate the whole linked list to output to the user space read buf
    list_for_each_entry_safe(cur, temp, &registration_list, next) {
	*length += sprintf(kern_buf + *length, "PID[%d]: %lu\n", 
				cur->pid, cur->used_time);
    }
}

// Read callback function for user space to read the proc file in
// /proc/mp1/status
ssize_t get_status(struct file *file, 
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

    // Lock
    mutex_lock(&access_lock);
    // Wrapper func to access the registration list
    __get_status(&length, kern_buf);
    // Unlock
    mutex_unlock(&access_lock);

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
static const struct file_operations mp1_proc_fops = {
    .owner = THIS_MODULE,
    .write = register_pid,
    .read = get_status,
};

// Timer updating handler
static void timer_handler(unsigned long data) {
    /*unsigned long j = jiffies;*/
    /*printk(KERN_DEBUG "time handling here %lu\n", j/HZ);*/

    // Allocate the work content
    struct work_struct *update_work;
    update_work = (struct work_struct *)kmalloc(sizeof(struct work_struct),
	    		GFP_KERNEL);
    // Declare the work content binded with the work function
    INIT_WORK(update_work, work_handler);
    queue_work(wq, update_work);

    // Update the timer expires
    mod_timer(&update_timer, jiffies + five_sec);
}

// Wrapper for work function without lock
static void __work_handler(void) {
    struct registration_block* cur, * temp;
    int ret;
    // Iterate the whole linked list to update each registered process 
    // used cpu time
    list_for_each_entry_safe(cur, temp, &registration_list, next) {
	// Through the helper function to get the cpu used time
	ret = get_cpu_use(cur->pid, &cur->used_time); 
	if (ret != 0) {
	   list_del(&cur->next);
	   kfree(cur);
	}
    }
}

// Work function for workqueue to be scheduled
static void work_handler(struct work_struct *work_arg) {
    // Lock
    mutex_lock(&access_lock);
    // Wraper func
    __work_handler();
    // Unlock
    mutex_unlock(&access_lock);

    // Delocate the pointer to work content space
    kfree(work_arg);

    #ifdef DEBUG
    printk(KERN_DEBUG "Workqueue worker completed\n");
    #endif 
}

// mp1_init - Called when module is loaded
int __init mp1_init(void) {
   #ifdef DEBUG
   printk(KERN_ALERT "MP1 MODULE LOADING\n");
   #endif
   // Make a new proc dir /proc/mp1
   mp1_dir = proc_mkdir(MP1_DIR, NULL); 
   // Make a new proc entry /proc/mp1/status
   mp1_status = proc_create(MP1_STAT, 0666, mp1_dir, &mp1_proc_fops); 
   // Initialize the timeout delay 
   five_sec = msecs_to_jiffies(5000 * 1);
   mod_timer(&update_timer, jiffies + five_sec);
   // Initialize a new workqueue
   wq = alloc_workqueue(UPDATE_WQ, WQ_MEM_RECLAIM, 0);
   // Initialize a new mutex lock
   mutex_init(&access_lock);

   printk(KERN_ALERT "MP1 MODULE LOADED\n");
   return 0;   
}

// Wrapper func for mp1_exit to delete and deallocate all resources
static void __mp1_exit(void) {
    struct registration_block* cur, * temp;
    // Remove all entries of the registration list
    list_for_each_entry_safe(cur, temp, &registration_list, next) {
       // Access the entry and delete/free memory space
       printk(KERN_DEBUG "PID %d: [%lu]\n", cur->pid, cur->used_time);
       list_del(&cur->next);
       kfree(cur);
    }
}

// mp1_exit - Called when module is unloaded
void __exit mp1_exit(void) {
   #ifdef DEBUG
   printk(KERN_ALERT "MP1 MODULE UNLOADING\n");
   #endif
   // Remove all the proc file entry and dir we created before
   proc_remove(mp1_status);
   proc_remove(mp1_dir);
   // Delete the kernel timer
   del_timer(&update_timer);
   // Destroy the workqueue
   if (wq != NULL) {
       flush_workqueue(wq);
       destroy_workqueue(wq);
   }

   // Lock
   mutex_lock(&access_lock);
   // Deallocate all needed resources
   __mp1_exit();
   // Unlock
   mutex_unlock(&access_lock);

   printk(KERN_ALERT "MP1 MODULE UNLOADED\n");
}

// Register init and exit funtions
module_init(mp1_init);
module_exit(mp1_exit);
