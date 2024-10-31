# [PingPongOS](https://wiki.inf.ufpr.br/maziero/doku.php?id=so:pingpongos)
This is a didactic operating system incrementally developed along with the Operating Systems Course at UFPR in C.

## Developed subprojects include:
* [Circular doubly linked list](https://www.geeksforgeeks.org/introduction-to-circular-doubly-linked-list/)
* Context switching
  * Implemented [getcontext()](https://man7.org/linux/man-pages/man3/getcontext.3.html), [makecontext()](https://man7.org/linux/man-pages/man3/makecontext.3.html), [swapcontext()](https://man7.org/linux/man-pages/man3/makecontext.3.html)
* Management of Tasks
  * Initialization, switching and exiting
* Task scheduler and dispatcher
    * Initially implements FCFS
* Priority Scheduling
    * Tasks now have priority (+-20), to avoid starvation when scheduling [aging](https://en.wikipedia.org/wiki/Aging_(scheduling)) was implemented.
* Time preemption
    * Implemented interrupt handler, clock ticks and quantum (20 ticks only on user tasks)
* Task telemetry
    * Counts total execution time, processor time and number of activations
* Suspending tasks
    * Implemented waiting for other tasks similar [linux's](https://man7.org/linux/man-pages/man2/wait.2.html) implementation. Also suspending and awaking tasks.
* Tasks sleeping
    * Suspends tasks for a certain time (in milliseconds).
* Semaphores
    * Implemented creating, using and destroying semaphores. Used [atomic memory access](https://gcc.gnu.org/onlinedocs/gcc-4.1.2/gcc/Atomic-Builtins.html) to avoid race conditions when accessing them.
* Tasks communication
    * Implemented the ability for tasks to add and remove messages from queues (producer and consumer).
