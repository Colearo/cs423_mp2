# cs423_mp2
CS423 Machine Problem 2 - RM Scheduler

# Introduce

This the README document of the CS423 MP2, writen by Kechen Lu (kechenl3), is for introducing How-to and the basic implementation ideas of this machine problem.

This module is compiled in the Linux Kernel v4.4 which is running over the VM the MP0 has done that. Basically, this module runs for scheduling the running processes registered according to the Rate Monotonic Scheduling algorithm, and each time we have the highest priority (the one with shortest period) task to run. For each job of each process, we should not miss each deadline and thus have the addmission control to 

And during the running process, it could read from the proc file to get all registered processes and its scheduling states. This module used the proc/ filesystem, the kernel timer, kthread and kernle sched APIs.

# Module Design



# How-to

## How-to Compile

```bash
$ make all
```

Running make all to compile the user app and kernel modules.

## How-to Install

```bash
$ sudo insmod kechenl3_MP2.ko
```

After installing the module, we will see the log through dmesg, implying the module has been loaded. 

```bash
$ dmesg
[  229.829534] MP2 MODULE LOADING
[  229.830191] MP2 MODULE LOADED
```

## How-to Run

We can use the user app to test the scheduler kernel module working correctly. By running the cmd below to concurrentlly run the user application. Below is the example to run the userapp with period 4000 ms and processing time is 1000 ms with number of jobs is 8 times.

```bash
./userapp [period] [computation time] [number of jobs]

kechenl3@sp19-cs423-027:~/MP2$ ./userapp 4000 1000 8
REGISTER PROCESS[22133]
PID[22133]: STATE(3) NEXT_PERIOD(4315259473)
22133 3 4315259473
[PID 22133] Jobs started at 1553372334.699168
YIELD PROCESS[22133]
[PID 22133] 0-th job started at 1553372338.707573
Complete the job
[PID 22133] 0-th job ended at 1553372339.707631
[PID 22133] 0-th job ProcessTime is 1000 ms
YIELD PROCESS[22133]
[PID 22133] 1-th job started at 1553372342.699560
Complete the job
[PID 22133] 1-th job ended at 1553372343.699604
[PID 22133] 1-th job ProcessTime is 1000 ms
YIELD PROCESS[22133]
[PID 22133] 2-th job started at 1553372346.699550
Complete the job
[PID 22133] 2-th job ended at 1553372347.699584
[PID 22133] 2-th job ProcessTime is 1000 ms
YIELD PROCESS[22133]
[PID 22133] 3-th job started at 1553372350.699561
Complete the job
[PID 22133] 3-th job ended at 1553372351.699602
[PID 22133] 3-th job ProcessTime is 1000 ms
YIELD PROCESS[22133]
[PID 22133] 4-th job started at 1553372354.699748
Complete the job
[PID 22133] 4-th job ended at 1553372355.699783
[PID 22133] 4-th job ProcessTime is 1000 ms
YIELD PROCESS[22133]
[PID 22133] 5-th job started at 1553372358.699556
Complete the job
[PID 22133] 5-th job ended at 1553372359.699601
[PID 22133] 5-th job ProcessTime is 1000 ms
YIELD PROCESS[22133]
[PID 22133] 6-th job started at 1553372362.699552
Complete the job
[PID 22133] 6-th job ended at 1553372363.699589
[PID 22133] 6-th job ProcessTime is 1000 ms
YIELD PROCESS[22133]
[PID 22133] 7-th job started at 1553372366.699553
Complete the job
[PID 22133] 7-th job ended at 1553372367.699598
[PID 22133] 7-th job ProcessTime is 1000 ms
YIELD PROCESS[22133]
DE-REGISTER PROCESS[22133]
```

While running the two processes simultaneously, like the example below, we have two processes registered first and yield. Then we will see the process with PID 22151 Period 4000 run first as its higher priority, and then after it completed its job, then the process 22150 with period 4500 run. After 4000 ms, the process 22151 runs again with job finished in 1100 ms, and so on and so forth. The two processes run correctly and does not miss any deadline in this RM scheduler for the real time system.

```bash
kechenl3@sp19-cs423-027:~/MP2$ ./userapp 4500 1500 3& ./userapp 4000 1100 3&
[1] 22150
[2] 22151
kechenl3@sp19-cs423-027:~/MP2$ REGISTER PROCESS[22150]
PID[22150]: STATE(3) NEXT_PERIOD(4315439361)
22150 3 4315439361
[PID 22150] Jobs started at 1553373054.249700
REGISTER PROCESS[22151]
PID[22151]: STATE(3) NEXT_PERIOD(4315439361)
PID[22150]: STATE(3) NEXT_PERIOD(4315440486)
22151 3 4315439361
[PID 22151] Jobs started at 1553373054.250043
YIELD PROCESS[22151]
[PID 22151] 0-th job started at 1553373058.259550
Complete the job
[PID 22151] 0-th job ended at 1553373059.359602
[PID 22151] 0-th job ProcessTime is 1100 ms
YIELD PROCESS[22150]
[PID 22150] 0-th job started at 1553373059.359688
Complete the job
[PID 22150] 0-th job ended at 1553373060.859740
[PID 22150] 0-th job ProcessTime is 1500 ms
YIELD PROCESS[22151]
[PID 22151] 1-th job started at 1553373062.251550
Complete the job
[PID 22151] 1-th job ended at 1553373063.351597
[PID 22151] 1-th job ProcessTime is 1100 ms
YIELD PROCESS[22150]
[PID 22150] 1-th job started at 1553373063.351713
Complete the job
[PID 22150] 1-th job ended at 1553373064.851760
[PID 22150] 1-th job ProcessTime is 1500 ms
YIELD PROCESS[22151]
[PID 22151] 2-th job started at 1553373066.251562
Complete the job
[PID 22151] 2-th job ended at 1553373067.351605
[PID 22151] 2-th job ProcessTime is 1100 ms
YIELD PROCESS[22150]
[PID 22150] 2-th job started at 1553373067.755537
Complete the job
[PID 22150] 2-th job ended at 1553373069.255580
[PID 22150] 2-th job ProcessTime is 1500 ms
YIELD PROCESS[22151]
DE-REGISTER PROCESS[22151]
YIELD PROCESS[22150]
DE-REGISTER PROCESS[22150]

[1]-  Done                    ./userapp 4500 1500 3
[2]+  Done                    ./userapp 4000 1100 3
```

And below is another example, simply the two processes with same period 4000 ms, so we can find that the scheduler would pick one of the two since they would be put in READY state at almost same time. So after one of the process starting, the other may preempt it and thus the processing time would be its own processing time plus the other job's processing time, that is to say, the job need to wait another preempting job completion and so that it could start again.

```bash
kechenl3@sp19-cs423-027:~/MP2$ ./userapp 4000 1000 3& ./userapp 4000 1100 3&
[1] 22144
[2] 22145
kechenl3@sp19-cs423-027:~/MP2$ REGISTER PROCESS[22144]
REGISTER PROCESS[22145]
PID[22145]: STATE(3) NEXT_PERIOD(4315389282)
PID[22144]: STATE(3) NEXT_PERIOD(4315389282)
22145 3 4315389282
[PID 22145] Jobs started at 1553372853.933991
PID[22145]: STATE(3) NEXT_PERIOD(4315390282)
PID[22144]: STATE(3) NEXT_PERIOD(4315389282)
22145 3 4315390282
22144 3 4315389282
[PID 22144] Jobs started at 1553372853.934074
YIELD PROCESS[22145]
[PID 22145] 0-th job started at 1553372857.939535
Complete the job
[PID 22145] 0-th job ended at 1553372859.039616
[PID 22145] 0-th job ProcessTime is 1100 ms
YIELD PROCESS[22144]
[PID 22144] 0-th job started at 1553372859.039770
Complete the job
[PID 22144] 0-th job ended at 1553372860.039832
[PID 22144] 0-th job ProcessTime is 1000 ms
YIELD PROCESS[22144]
[PID 22144] 1-th job started at 1553372861.935566
YIELD PROCESS[22145]
[PID 22145] 1-th job started at 1553372861.939519
Complete the job
[PID 22145] 1-th job ended at 1553372863.039583
[PID 22145] 1-th job ProcessTime is 1100 ms
Complete the job
[PID 22144] 1-th job ended at 1553372864.035756
[PID 22144] 1-th job ProcessTime is 2100 ms
YIELD PROCESS[22144]
[PID 22144] 2-th job started at 1553372865.935561
YIELD PROCESS[22145]
[PID 22145] 2-th job started at 1553372865.939489
Complete the job
[PID 22145] 2-th job ended at 1553372867.039551
[PID 22145] 2-th job ProcessTime is 1100 ms
Complete the job
[PID 22144] 2-th job ended at 1553372868.035785
[PID 22144] 2-th job ProcessTime is 2100 ms
YIELD PROCESS[22144]
DE-REGISTER PROCESS[22144]
YIELD PROCESS[22145]
DE-REGISTER PROCESS[22145]

[1]-  Done                    ./userapp 4000 1000 3
[2]+  Done                    ./userapp 4000 1100 3
```

## How-to Remove

```bash
$ sudo rmmod kechenl3_MP2
$ dmesg
[ 1604.079952] MP2 MODULE UNLOADING
[ 1604.080006] MP2 MODULE UNLOADED
```

# Screenshot 

![Screenshot for two processes running under the RM scheduler](./ScreenShot_for_two_process_running.png)