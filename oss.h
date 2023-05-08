//What needs to be done:
//FIFO (clock) paage replacement algo, circular queue
//allocate shared memory for data structures/page table
//create fixed size arrays for page table assuming processes require less than 32k mem and each page is 1k
//make sure process don't access memory outside the page limit 
//have child runs in loop constantly until terminated
//oss check if there is a page fault and block process if there is (increment by 100 nano if no fault)
//Each request for disk read/write takes 14 ms to fulfill
//Child request random meory space 1 - 32 then multipy it by 1024 and add a random offset between 0 - 1023 to get actual memory addree
//Once child request memory space randomly choose if it will have read or write(with more incline to read) then increment the clock
//check if process should terminate at random time and return all memory to oss

//try to documment the following: 
// Number of memory accesses per second
// Number of page faults per memory access
// Average memory access speed
//terminate after 100 processes/2 real second

//What this program should have:
//system memory has 256k
//frame table
//page table
//18 processes running at a time (use define macro as a hard limit)

// Print Example:
// Master: P2 requesting read of address 25237 at time xxx:xxx
// Master: Address 25237 in frame 13, giving data to P2 at time xxx:xxx
// Master: P5 requesting write of address 12345 at time xxx:xxx
// Master: Address 12345 in frame 203, writing data to frame at time xxx:xxx
// Master: P2 requesting write of address 03456 at time xxx:xxx
// Master: Address 12345 is not in a frame, pagefault
// Master: Clearing frame 107 and swapping in p2 page 3
// Master: Dirty bit of frame 107 set, adding additional time to the clock
// Master: Indicating to P2 that write has happened to address 03456

// Current memory layout at time xxx:xxx is:
        // Occupied DirtyBit HeadOfFIFO
// Frame 0: No      0
// Frame 1: Yes     1
// Frame 2: Yes     0        *
// Frame 3: Yes     1

// where Occupied indicates if we have a page in that frame, the * indicates that the next page will be replaced there,
// and the dirty bit indicates if the frame has been written to.

#include <stdio.h>
#include <unistd.h> //for pid_t and exec

#define BILLION 1000000000L
#define PERMS 0644

//20 instances of each resource 
int R0, R1, R2, R3, R4, R5, R6, R7, R8, R9 = 20;

double currentTime;

//Create random second and nanosecond in bound of user input
int randomNumberGenerator(int limit)
{
    int sec;
    sec = (rand() % (limit)) + 1;
    return sec;
}
struct Table{
    int pageTable[33];
    int currentTime;
};

//msgbuffer for message queue
typedef struct msgbuffer {
    long mtype;
    char strData[100];
    int intData;
} msgbuffer;