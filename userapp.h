#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#define MAX_BUF_SIZE 4096
char temp_buf[MAX_BUF_SIZE];

// Factorial computation
static unsigned long factor(unsigned long n);
// Register the process to the scheduler
static void task_register(unsigned long period, unsigned long computation_period);
// De-register the process to the scheduler
static void task_deregister();
// Yeild to relinquish the control right of CPU
static void task_yeild();
// Get the pid list from reading the /proc/mp1/status
static bool has_pid_in_list();
// To do the job for the certain time of computation
static void do_job(unsigned long computation_period);
