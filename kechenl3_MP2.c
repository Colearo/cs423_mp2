#define LINUX

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <asm/uaccess.h>
#include "mp2_given.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kechen Lu");
MODULE_DESCRIPTION("CS-423 MP2 RMS scheduler");

#define DEBUG 0

// Declare all needed global variables for the proc fs initializaiton
// The mp1 directory
#define MP2_DIR "mp2"
struct proc_dir_entry * mp2_dir;
// The mp1/status entry
#define MP2_STAT "status"
struct proc_dir_entry * mp2_status;

// Kernel slab cache allocator
struct kmem_cache *tasks_cache;

// Kernel dispatcher name
#define RMS_DISPATCHER "rms_dispatcher"

// Spinlock
spinlock_t sl;

// Define the structure to persist the PID and its attributes
struct registration_block {
    unsigned int pid;
    unsigned long period;
    unsigned long computation_period;
};

// Define the dispatching thread
struct task_struct* dispatcher;

// Define the customized task struct
struct mp2_task_struct {
    struct task_struct* linux_task;
    struct timer_list wakeup_timer;
    struct registration_block rb;
    struct list_head next;

#define RUNNING  0x01
#define READY    0x02
#define SLEEPING 0x03
    unsigned int state;
    uint64_t next_release;
};

// Define the current task running
struct mp2_task_struct* curr_mp2_task;

// Define the tasks list
LIST_HEAD(reg_task_list);

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

    while(*(*str) != 0) {
	*(p++) = *((*str)++);
	if (*(*str) == ',') {
	    (*str)++;
	    break;
	}
    }

    return res;
}

#define MAX_BUF_SIZE 4096

// Timer callback 
static void timer_callback(unsigned long data) {
    struct mp2_task_struct *task_ptr = (struct mp2_task_struct*)data;
    unsigned long flags;

    // Spinlock lock
    spin_lock_irqsave(&sl, flags);
    printk(KERN_DEBUG "Timer callback %d [state %d]\n", task_ptr->rb.pid, task_ptr->state);
    // Change the state of the new period task to READY
    task_ptr->state = READY;
    // Spinlock unlock
    spin_unlock_irqrestore(&sl, flags);

    // Wake up the scheduler
    wake_up_process(dispatcher);

    printk(KERN_DEBUG "Timer after waking up dispatcher\n");
}

// Registration func for process to register
static ssize_t registration(unsigned int pid, unsigned long period, unsigned long computation_period) {
    struct mp2_task_struct *task_ptr;
    unsigned long flags;

    // Allocate the corresponding struct using slab cache allocator
    task_ptr = kmem_cache_alloc(tasks_cache, GFP_KERNEL);

    // Get the task PCB by PID
    task_ptr->linux_task = find_task_by_pid(pid);
    if (task_ptr->linux_task == NULL) {
	kmem_cache_free(tasks_cache, task_ptr);
	return -EFAULT;
    }

    // Assign the pid and other information of this task to the registration_block
    task_ptr->rb.pid = pid;
    task_ptr->rb.period = period;
    task_ptr->rb.computation_period = computation_period;

    // Initialize the timer
    setup_timer(&task_ptr->wakeup_timer, timer_callback, (unsigned long)task_ptr);
    task_ptr->next_release = jiffies;
    /*mod_timer(&task_ptr->wakeup_timer, jiffies + msecs_to_jiffies(5000));*/

    // Initialize the state of the task to SLEEPING
    task_ptr->state = SLEEPING;

    // Linked list entry
    INIT_LIST_HEAD(&task_ptr->next);

    // Spinlock lock
    spin_lock_irqsave(&sl, flags);
    // Add the task to the registartion task list
    list_add(&task_ptr->next, &reg_task_list);
    // Spinlock unlock
    spin_unlock_irqrestore(&sl, flags);

    return 0;
}

// De-registration for the tasks by pid
static ssize_t deregistration(unsigned int pid) {
    struct mp2_task_struct *cur, *temp;
    int flag = 0;
    unsigned long flags;

    // Spinlock lock
    spin_lock_irqsave(&sl, flags);
    //Iterate the whole linked list to delete the PID equals this pid
    list_for_each_entry_safe(cur, temp, &reg_task_list, next) {
	// If the current task's pid is the one we want, delete it
	if (cur->rb.pid == pid) {
	    flag = 1;
	    if (curr_mp2_task->rb.pid == pid) 
		curr_mp2_task = NULL;
	    del_timer(&cur->wakeup_timer);
	    list_del(&cur->next);
	    kmem_cache_free(tasks_cache, cur);
	}
    }
    // Spinlock unlock
    spin_unlock_irqrestore(&sl, flags);

    // Wake up the scheduler
    wake_up_process(dispatcher);

    if (flag == 0) 
	return -EFAULT;

    return 0;
}

// YEILD function to relinquish the CPU 
static ssize_t yielding(unsigned int pid) {
    struct mp2_task_struct *cur, *temp;
    int flag = 0;
    unsigned long flags;

    // Spinlock lock
    spin_lock_irqsave(&sl, flags);
    //Iterate the whole linked list to find the pid
    list_for_each_entry_safe(cur, temp, &reg_task_list, next) {
	// If the current task's pid is what we want
	if (cur->rb.pid == pid) {
	    flag = 1;
	    cur->state = SLEEPING;
	    set_task_state(cur->linux_task, TASK_UNINTERRUPTIBLE);

	    // Precceding the timer
	    if (cur->next_release + msecs_to_jiffies(cur->rb.period) > jiffies) {
		cur->next_release += msecs_to_jiffies(cur->rb.period);
		mod_timer(&cur->wakeup_timer, cur->next_release);
	    } else {
		return -EINVAL;
	    }

	    break;
	}
    }

    // Spinlock unlock
    spin_unlock_irqrestore(&sl, flags);

    // Wake up the scheduler
    wake_up_process(dispatcher);
    schedule();

    if (flag == 0) 
	return -EFAULT;

    return 0;
}

// Helper function to get the highest priority 
struct mp2_task_struct* get_highest_task(void) {
    struct mp2_task_struct *cur, *temp, *res;
    unsigned long shortest = -1;
    res = NULL;

    //Iterate the whole linked list to found the one with shortest period
    list_for_each_entry_safe(cur, temp, &reg_task_list, next) {
	// If the current task's period is shorter than the found
	if (cur->state == READY && cur->rb.period < shortest) {
	    shortest = cur->rb.period;
	    res = cur;
	}
    }

    return res;
}

// Dispatcher thread to schedule the tasks
static int dispatching(void *data) {
    struct mp2_task_struct *highest;
    struct sched_param sparam;
    unsigned long flags;

    while(!kthread_should_stop()) {
	printk(KERN_DEBUG "Dispatching: Start\n");
	spin_lock_irqsave(&sl, flags);
	if (curr_mp2_task && curr_mp2_task->state == RUNNING) {
		curr_mp2_task->state = READY;
		curr_mp2_task = NULL;
		printk(KERN_DEBUG "Old Task %d Running to Ready\n", curr_mp2_task->rb.pid);
	} else if (curr_mp2_task) {
	    sparam.sched_priority = 0;
	    sched_setscheduler(curr_mp2_task->linux_task, SCHED_NORMAL, &sparam);
	    printk(KERN_DEBUG "Old Task %d Not Running\n", curr_mp2_task->rb.pid);
	}

	highest = get_highest_task();
	if (highest == NULL) {
	    spin_unlock_irqrestore(&sl, flags);
	    goto SLEEP;
	}
	highest->state = RUNNING;
	spin_unlock_irqrestore(&sl, flags);
	
	printk(KERN_DEBUG "New Task %d Running\n", highest->rb.pid);
	// set_task_state(highest->linux_task, TASK_RUNNING)
	// Wake up the highest task
	sparam.sched_priority = 99;
	sched_setscheduler(highest->linux_task, SCHED_FIFO, &sparam);
	curr_mp2_task = highest;
	wake_up_process(highest->linux_task);

SLEEP:	// Sleep
	set_current_state(TASK_INTERRUPTIBLE);
	schedule();
    }

    return 0;
}

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
    unsigned long pid_val, period, computation_period;
    int ret = 0;

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

	    kern_buf += 2;
	    token = parse_next_phrase(&kern_buf);
	    // Convert the pid string to the integer type
	    ret = kstrtoul(token, 10, &pid_val);
	    kfree(token);
	    if (ret != 0) 
		goto RET;
	    printk(KERN_DEBUG "PID: [%d]\n", (unsigned int)pid_val);

	    token = parse_next_phrase(&kern_buf);
	    ret = kstrtoul(token, 10, &period);
	    kfree(token);
	    if (ret != 0) 
		goto RET;
	    printk(KERN_DEBUG "PERIOD: [%lu]\n", period);

	    token = parse_next_phrase(&kern_buf);
	    ret = kstrtoul(token, 10, &computation_period);
	    kfree(token);
	    if (ret != 0) 
		goto RET;
	    printk(KERN_DEBUG "COMPUTATION PERIOD: [%lu]\n", computation_period);

	    // REGISTRATION for the new task
	    ret = registration(pid_val, period, computation_period);
	    if (ret != 0)
		goto RET;

	    break;

	case 'Y' :
	    printk(KERN_DEBUG "YIELD\n");

	    kern_buf += 2;
	    token = parse_next_phrase(&kern_buf);
	    // Convert the pid string to the integer type
	    ret = kstrtol(token, 10, &pid_val);
	    kfree(token);
	    if (ret != 0) 
		goto RET;
	    printk(KERN_DEBUG "PID: [%d]\n", (int)pid_val);

	    ret = yielding((unsigned int)pid_val);
	    if (ret != 0)
		goto RET;

	    break;

	case 'D' :
	    printk(KERN_DEBUG "DE-REGISTRATION\n");

	    kern_buf += 2;
	    token = parse_next_phrase(&kern_buf);
	    // Convert the pid string to the integer type
	    ret = kstrtol(token, 10, &pid_val);
	    kfree(token);
	    if (ret != 0) 
		goto RET;
	    printk(KERN_DEBUG "PID: [%d]\n", (int)pid_val);

	    ret = deregistration(pid_val);
	    if (ret != 0)
		goto RET;
	    break;

	default :
	    printk(KERN_DEBUG "NO MATCHED\n");
	    ret = -EFAULT;
	    goto RET;
    }

    ret = n;

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
    unsigned long flags;
    struct mp2_task_struct *cur, *temp;

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

    spin_lock_irqsave(&sl, flags);
    list_for_each_entry_safe(cur, temp, &reg_task_list, next) {
	// Iterate the list to cat the information of each managed task
	length += sprintf(kern_buf + length, "PID[%u]: STATE(%u) NEXT_PERIOD(%llu)\n", cur->rb.pid, cur->state, cur->next_release);
    }
    spin_unlock_irqrestore(&sl, flags);

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

   // Make a new slab cache
   tasks_cache = KMEM_CACHE(mp2_task_struct, SLAB_HWCACHE_ALIGN|SLAB_PANIC);

   // Make a new kernel thread for RM scheduler
   dispatcher = kthread_create(dispatching, NULL, RMS_DISPATCHER);

   // Make a new spinlock for sychronization
   spin_lock_init(&sl);

   printk(KERN_ALERT "MP2 MODULE LOADED\n");
   return 0;   
}

// mp2_exit - Called when module is unloaded
void __exit mp2_exit(void) {
   struct mp2_task_struct *cur, *temp;

   #ifdef DEBUG
   printk(KERN_ALERT "MP1 MODULE UNLOADING\n");
   #endif

   // Remove all the proc file entry and dir we created before
   proc_remove(mp2_status);
   proc_remove(mp2_dir);

   //Iterate the whole linked list to delete the PID equals this pid
   list_for_each_entry_safe(cur, temp, &reg_task_list, next) {
	    del_timer(&cur->wakeup_timer);
	    list_del(&cur->next);
	    kmem_cache_free(tasks_cache, cur);
   }

   kmem_cache_destroy(tasks_cache);

   kthread_stop(dispatcher);

   printk(KERN_ALERT "MP2 MODULE UNLOADED\n");
}

// Register init and exit funtions
module_init(mp2_init);
module_exit(mp2_exit);
