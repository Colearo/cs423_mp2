#include "userapp.h"

// Process pid register when starting running
static void pid_register() {
    // Define the buffer
    char * buf = malloc(MAX_BUF_SIZE * sizeof(char));
    int len;
    memset(buf, 0, MAX_BUF_SIZE * sizeof(char));

    // Open the proc file
    int fd = open("/proc/mp1/status", O_RDWR);
    
    // Get the pid of the current process
    pid_t pid = getpid();
    // Write pid to the proc file /proc/mp1/status
    len = sprintf(buf, "%d", pid);
    write(fd, buf, len);
    printf("PID[%d]", pid);
    free(buf);
}

// Read from the proc file to learn the pid numbers and its
// coresponding cpu used time
static void pid_list() {
    // Define the buffer
    char * buf = malloc(MAX_BUF_SIZE * sizeof(char));
    int len;
    memset(buf, 0, MAX_BUF_SIZE * sizeof(char));

    // Open the proc file
    int fd = open("/proc/mp1/status", O_RDWR);

    // Read the content of pid list from the proc file
    len = read(fd, buf, MAX_BUF_SIZE * sizeof(char));
    // Print the buffer we read
    printf("%s", buf);
    free(buf);
}

// Factorial computation
static unsigned long factor(unsigned n) {
    if (n <= 1) 
	return 1;
    return factor(n - 1) * (unsigned long)n;
}

int main(int argc, char* argv[])
{
    unsigned i, j;
    // Register the pid
    pid_register();
    // Do the factorial computation repeatedly
    for (i = 1; i < 200000; i++) {
	for (j = 1; j < 16; j++)
	    printf("Factor of %d: %lu\n", j, factor(j));
    }
    // Print the pid list
    pid_list();
    return 0;
}
