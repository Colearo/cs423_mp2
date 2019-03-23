#include "userapp.h"

// Process pid register when starting running
static void task_register(unsigned long period, unsigned long c_period) {
    // Define the buffer
    char * buf = malloc(MAX_BUF_SIZE * sizeof(char));
    int len;
    memset(buf, 0, MAX_BUF_SIZE * sizeof(char));

    // Open the proc file
    int fd = open("/proc/mp2/status", O_RDWR);
    
    // Get the pid of the current process
    pid_t pid = getpid();
    // Write R,PID,PERIOD,PROCESSING_TIME to the proc file /proc/mp2/status
    len = sprintf(buf, "R,%d,%lu,%lu", pid, period, c_period);
    write(fd, buf, len);
    printf("REGISTER PROCESS[%d]\n", pid);

    free(buf);
}

// Write to the proc to have the deregistration
static void task_deregister() {
    // Define the buffer
    char * buf = malloc(MAX_BUF_SIZE * sizeof(char));
    int len;
    memset(buf, 0, MAX_BUF_SIZE * sizeof(char));

    // Open the proc file
    int fd = open("/proc/mp2/status", O_RDWR);
    
    // Get the pid of the current process
    pid_t pid = getpid();
    // Write D,PID to the proc file /proc/mp2/status
    len = sprintf(buf, "D,%d", pid);
    write(fd, buf, len);
    printf("DE-REGISTER PROCESS[%d]\n", pid);

    free(buf);
}

// Write to the proc to yield and give up the control right of this CPU
static void task_yield() {
    // Define the buffer
    char * buf = malloc(MAX_BUF_SIZE * sizeof(char));
    int len;
    memset(buf, 0, MAX_BUF_SIZE * sizeof(char));

    // Open the proc file
    int fd = open("/proc/mp2/status", O_RDWR);
    
    // Get the pid of the current process
    pid_t pid = getpid();
    // Write Y,PID to the proc file /proc/mp2/status
    len = sprintf(buf, "Y,%d", pid);
    write(fd, buf, len);
    printf("YIELD PROCESS[%d]\n", pid);

    free(buf);
}

// Read from the proc file to learn the pid numbers and its
// whether in the registered pid list
static bool has_pid_in_list() {
    // Define the buffer
    char * buf = malloc(MAX_BUF_SIZE * sizeof(char));
    char * temp = buf;
    int len;
    unsigned int pid, s;
    unsigned long long next_period;
    bool flag = false;
    // Get the pid of the current process
    pid_t cur_pid = getpid();

    memset(buf, 0, MAX_BUF_SIZE * sizeof(char));

    // Open the proc file
    int fd = open("/proc/mp2/status", O_RDWR);

    // Read the content of pid list from the proc file
    len = read(fd, buf, MAX_BUF_SIZE * sizeof(char));
    buf[len] = 0;

    // Print the buffer we read
    printf("%s", buf);

    len = 0;
    // Parse the read buffer to a list 
    while(1) {
	len = sscanf(buf, "PID[%u]: STATE(%u) NEXT_PERIOD(%llu)\n", &pid, &s, &next_period);
	if (len == EOF || len < 3) 
	    break;
	printf("%u %u %llu\n", pid, s, next_period);
	len = sprintf(temp_buf, "PID[%u]: STATE(%u) NEXT_PERIOD(%llu)\n", pid, s, next_period);
	buf += len;
	memset(temp_buf, 0, MAX_BUF_SIZE);
	if (cur_pid == pid) {
	    flag = true;
	    break;
	}
    }

    free(temp);
    return flag;
}

// Do the job to have the exactly accurate computation time
static void do_job(unsigned long comput) {
    unsigned long i, j;
    clock_t start, end;
    start = clock();
    end = start + comput * CLOCKS_PER_SEC / 1000;
    // Do the factorial computation repeatedly
    for (i = 0; i < comput * 100; i++) {
	for (j = 0; j < 180; j++)
	    factor(16);
	if (clock() > end)
	    break;
    }
    printf("Complete the job\n");
}

// Factorial computation
static unsigned long factor(unsigned long n) {
    if (n <= 1) 
	return 1;
    return factor(n - 1) * (unsigned long)n;
}

int main(int argc, char* argv[])
{
    unsigned i;
    unsigned long period, c_period, job_num;
    char *ptr;
    struct timeval t0, wakeup, giveup;
    pid_t pid = getpid();

    if (argc != 4) {
	printf("Incompatible number of arguments\n");
	return 0;
    }

    // Parse the argv to get the two parameters
    period = strtoul(argv[1], &ptr, 10);
    c_period = strtoul(argv[2], &ptr, 10);
    job_num = strtoul(argv[3], &ptr, 10);
    if (period & c_period & job_num == 0) {
	printf("Invalid arguments, please use ./userapp period computation_period number of jobs\n");
	return 0;
    }

    // REGISTER the pid
    task_register(period, c_period);
    
    // Check the pid list
    if (!has_pid_in_list()) {
	printf("REGISTER Failed\n");
	return -1;
    }

    // Get the first starting time at the first YEILD
    gettimeofday(&t0, NULL);
    printf("[PID %d] Jobs started at %ld.%.6ld\n", pid, t0.tv_sec, t0.tv_usec);

    // YIELD
    task_yield();

    // Do the periodic jobs of job_num times
    for (i = 0; i < job_num; i++) {
	memset(&wakeup, 0, sizeof(struct timeval));
	gettimeofday(&wakeup, NULL);
	printf("[PID %d] %d-th job started at %ld.%.6ld\n", pid, i, wakeup.tv_sec, wakeup.tv_usec);

        // Do the job
	do_job(c_period);

	memset(&giveup, 0, sizeof(struct timeval));
	gettimeofday(&giveup, NULL);
	printf("[PID %d] %d-th job ended at %ld.%.6ld\n", pid, i, giveup.tv_sec, giveup.tv_usec);
	printf("[PID %d] %d-th job ProcessTime is %llu ms\n", pid, i, (((unsigned long long) giveup.tv_sec * 1000000 + giveup.tv_usec)-((unsigned long long)wakeup.tv_sec * 1000000 + wakeup.tv_usec))/1000);

        // YIELD
	task_yield();
    }

    // DE-REGISTRATION
    task_deregister();

    return 0;
}
