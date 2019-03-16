#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_BUF_SIZE 4096
// Factorial computation
static unsigned long factor(unsigned n);
// Register the pid to the linked list
static void pid_register();
// Get the pid list from reading the /proc/mp1/status
static void pid_list();
