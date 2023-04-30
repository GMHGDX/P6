// Name: Gabrielle Hieken
// Class: 4760 Operating Systems
// Date: 4/28/2023

#include <stdio.h>
#include <getopt.h> //Needed for optarg function
#include <stdlib.h> //EXIT_FAILURE
#include <unistd.h> //for pid_t and exec
#include <sys/types.h>
#include <time.h> //to create system time
#include <sys/shm.h> //Shared memory
#include <stdbool.h> //bool values
#include <sys/wait.h> //wait
#include <string.h> //remedy exit warning
#include <sys/msg.h> //message queues
#include "oss.h"

//msgbuffer for message queue
typedef struct queue {
    int pid;
    int resources[10];
} queue;

void printTable(int resourceTable[][10]);

#define MAX 40

queue blockedQueue[MAX];
int front = 0;
int rear = -1;
int itemCount = 0;

queue peek() { return blockedQueue[front]; }
bool isEmpty() { return itemCount == 0; }
bool isFull() { return itemCount == MAX; }
int size() { return itemCount; }  


void insert(queue data) {
   if(!isFull()) {
      if(rear == MAX-1) {
        rear = -1;            
      }
      rear++;
      int i;
      for(i=0;i<10;i++){
        blockedQueue[rear].resources[i] = data.resources[i];
      }
      blockedQueue[rear].pid = data.pid;
      itemCount++;
   }
}

queue removeData() {
   queue data = blockedQueue[front++];
   if(front == MAX) {
      front = 0;
   }
   itemCount--;
   return data;  
}

typedef struct pidstruct {
    pid_t realpid;
    int simpid;
} pidstruct;

int main(int argc, char *argv[]){
    int i;
    int j;

    queue toInsert; //insert numbers to blocked queue struct
    char* logFile = "logfile"; //logfile declaration
    FILE *fileLogging; //for the file 
    pid_t childpid = 0; //child process ID 
    int resourceTable[18][10]; //Initialize resource table
    int resourcesLeft[10];
    pidstruct mypidstruct[50];

    for(i=0;i<50;i++){ mypidstruct[i].simpid = -1;} //initialize wsimulated PID

    srand(time(0)); //Seed the random number generator

    //variables for our system clock
    struct timespec start, stop;
    double sec;
    double nano;
    int milliLim = 500; //time limit for random forked processes in milliseconds

    //Create shared memory key and message buffer for message queue
    const int sh_key = 3147550;     
    key_t msqkey;
    int msqid;
    msgbuffer buf;

    //Parse through command line options
	char opt;
    while((opt = getopt(argc, argv, "hf:")) != -1 )
    {
        switch (opt)
        {
        case 'h': //help message
			printf("To run this project: \n\n");
            printf("run the command: ./oss -n num -s num -t num\n\n");
                    printf("\t-f = name of logfile you wish to write the process table in oss.c to\n\n"); 
                    printf("If you leave out '-f' in the command line prompt it will default to 'logfile'\n\n");
                    printf("Have fun :)\n\n");

                    exit(0);
            break;
        case 'f': //logfile
            logFile = optarg; 
            break;
        default:
            printf ("Invalid option %c \n", optopt);
            return (EXIT_FAILURE);
        }
    }

    fileLogging = fopen(logFile, "w+"); //Open the log file before input begins 

    //Initialize empty resources left (all zeros)
    for(i = 0; i < 10; i++){
        resourcesLeft[i] = 20;
    }

    //Initialize empty resource table (all zeros)
    for(i = 0; i < 18; i++){
        for(j = 0; j < 10; j++){
            resourceTable[i][j] = 0;
        }
    }
    printTable(resourceTable);

    //Create message queue
    if((msqkey = ftok("oss.h", 'a')) == (key_t) -1){ perror("IPC error: ftok"); exit(1); } //Create key using ftok() for more uniquenes
    if ((msqid = msgget(msqkey, PERMS | IPC_CREAT)) == -1) { perror("Failed to create new private message queue"); exit(1); } //open an existing message queue or create a new one

    //create shared memory
    int shm_id = shmget(sh_key, sizeof(double), IPC_CREAT | 0666);
    if(shm_id <= 0) {fprintf(stderr,"ERROR: Failed to get shared memory, shared memory id = %i\n", shm_id); exit(1); }

    //attatch memory we allocated to our process and point pointer to it 
    double *shm_ptr = (double*) (shmat(shm_id, NULL, 0));
    if (shm_ptr <= 0) { fprintf(stderr,"Shared memory attach failed\n"); exit(1); }

    //start the simulated system clock
    if( clock_gettime( CLOCK_REALTIME, &start) == -1 ) { perror( "clock gettime" ); return EXIT_FAILURE; }

    //intialize values for use in while loop
    double currentTime; //time going into shared memory
    double limitReach = 0; //random time next child is forked 
    double writeToMem;
    int numofchild = 0; //DELETEEEEEEEEE
    int milliSec = 0; //milliseconds used in time limit
    int resourcesUsed[10]; //resources in an array
    char* text; //used to seperate message recieved by whitespace 
    int simpidofsender;
    bool notenoughresources = false;
    int checkWhatToDo = -1;
    bool allResourcesFree = false;

    while(1) {
        //stop simulated system clock and get seconds and nanoseconds
        if( clock_gettime( CLOCK_REALTIME, &stop) == -1 ) { perror( "clock gettime" ); return EXIT_FAILURE; }
        sec = (stop.tv_sec - start.tv_sec); 
        nano = (double)( stop.tv_nsec - start.tv_nsec);

        //if start time nanosecond is greater than stop, carry the one to get positive nanosecond
        if(start.tv_nsec > stop.tv_nsec){
            sec = (stop.tv_sec - start.tv_sec) - 1;
            nano = (double)( stop.tv_nsec - start.tv_nsec) + ((double)(1)*BILLION);
        }
        currentTime = sec + nano/BILLION;

        //Write the current time to memory for children to read
        writeToMem = currentTime;
        *shm_ptr = writeToMem;

        if(limitReach <= currentTime && numofchild < 10 && currentTime <= 5){
            numofchild++;
            milliSec = randomNumberGenerator(milliLim); //create random number for next child to fork at 
            limitReach = sec + (double)(milliSec/1000) + (double)(nano/BILLION); //combine sec, millisec, and nanosec as one decimal to get new time to fork process

            childpid = fork(); //fork child

            if (childpid == -1) { perror("Failed to fork"); return 1; }
            if (childpid == 0){  //send shared memory key to user_proc for children to use 
                char sh_key_string[50];
                snprintf(sh_key_string, sizeof(sh_key_string), "%i", sh_key);

                char *args[] = {"user_proc", sh_key_string, NULL};
                execvp("./user_proc", args); //exec function to send children to user_proc along with our shared memory key

                return 1;
            }
            if(childpid != 0 ){               
                i = 0;  //Finds the smallest positoin to put the new child into (cannot be over 17 or it will break our code)
                while(mypidstruct[i].simpid != -1){
                    i++;
                }
                if(i>17){
                    printf(" "); //should not get here..
                }
                mypidstruct[i].simpid = i;
                mypidstruct[i].realpid = childpid;
            }
        }

        buf.intData = 0;
        strcpy(buf.strData, "-1"); //Clear the message string back to nothing before we check for a msgrcv
        checkWhatToDo = -1; //Return checkwaht todo back to "do nothing"

        msgrcv(msqid, &buf, sizeof(msgbuffer), getpid(), IPC_NOWAIT); // receive a message from user_proc, but only one for our PID
        checkWhatToDo = atoi(buf.strData);  //If 0, means a process has died, if greater than 0, meana we got some reacourses to alloacte

        if(checkWhatToDo == 0){//-----------------------------------------------------------------------------------------------------------------
            printf("Dealloacting\n"); //deallocating resources from the process
            fprintf(fileLogging, "Dealloacting\n");
            i = 0;
            while(i < 18){
                if(mypidstruct[i].realpid == buf.intData){ 
                    simpidofsender = mypidstruct[i].simpid;

                    mypidstruct[i].realpid = 0; //Clear out the position in mypidstruct for reuse
                    mypidstruct[i].simpid = -1;

                    printf("Resources deallocated: ");
                    fprintf(fileLogging, "Resources deallocated: ");
                    
                    for (i=0;i<10;i++){ //Update resource table with new values
                        printf(" %i",  resourceTable[simpidofsender][i]);
                        fprintf(fileLogging, " %i",  resourceTable[simpidofsender][i]);
                        resourcesLeft[i] += resourceTable[simpidofsender][i];
                        resourceTable[simpidofsender][i] = 0;
                    }
                    printf("\n");
                    fprintf(fileLogging, "\n");
                    i = 20;
                }
                i++;
            }
            printTable(resourceTable);
            }
        }
        if(checkWhatToDo > 0){ //------------------------------------------------------------------------------------------------------------------
            printf("Child %d is requesting the following resourceses: %s\n",buf.intData, buf.strData);
            fprintf(fileLogging, "Child %d is requesting the following resourceses: %s\n",buf.intData, buf.strData); 

            text = strtok(buf.strData, " ");
            for (i=0;i<10;i++){
                if(text == NULL){
                    break;
                }
                resourcesUsed[i] = atoi(text);
                text = strtok(NULL, " ");
            }
            i = 0;
            while(i < 18){
                if(mypidstruct[i].realpid == buf.intData){  //Will thius crash if mypidstruct[i].realpid is not set to anything (unitalized)
                    simpidofsender = mypidstruct[i].simpid;
                    break;
                }
                i++;
            }

            for(i=0;i<10;i++){ //Check if we have enough resources for this process
                if((resourcesLeft[i] - resourcesUsed[i]) < 0){
                    notenoughresources = true;
                }
            }

            if(!notenoughresources){
                strcpy(buf.strData, "1"); //send message back to child that there are enough resources
                buf.mtype = buf.intData;
                printf("There are enough resources for child  %i\n", buf.intData); 
                fprintf(fileLogging, "There are enough resources for child  %i\n", buf.intData); 
                if (msgsnd(msqid, &buf, sizeof(msgbuffer)-sizeof(long), 0) == -1) { perror("msgsnd to child 1 failed\n"); exit(1); } 

                for (i=0;i<10;i++){ //Update resource table with new values
                    resourceTable[simpidofsender][i] = resourcesUsed[i];
                    resourcesLeft[i] -= resourcesUsed[i];
                }

                printTable(resourceTable);
                }
            }else{  //If not enough is true
                //send to blocked queue, should hold the pid of the process that is blocked and the rescoruces it is requesting, first in first out
                printf("There are not enough resources for child %d, sent to blocked queue\n", buf.intData);
                fprintf(fileLogging, "There are not enough resources for child %d, sent to blocked queue\n", buf.intData);
                toInsert.pid = buf.intData;
                for(i=0;i<10;i++){
                    toInsert.resources[i] = resourcesUsed[i];
                }
                insert(toInsert);
            } 
            notenoughresources = false;
        }

        if(!isEmpty()){ //Is blocked queue empty? -------------------------------------------------------------------------------------------------
            toInsert = peek();
            for (i=0;i<10;i++){
                resourcesUsed[i] = toInsert.resources[i];
            }

            if(!notenoughresources){
                printf("There are now enough resources for %i to come out of the blocked queue\n", toInsert.pid);
                fprintf(fileLogging, "There are now enough resources for %i to come out of the blocked queue\n", toInsert.pid);
                removeData(); //Delete process from front of queue
                strcpy(buf.strData, "1"); //send message back to child that there are enough resources
                buf.mtype = toInsert.pid;
                buf.intData = toInsert.pid;
                printf("Child %d is granted the following resources: %s\n", buf.intData, buf.strData); //TESTING
                fprintf(fileLogging, "Child %d is granted the following resources: %s\n", buf.intData, buf.strData); //TESTING
                if (msgsnd(msqid, &buf, sizeof(msgbuffer)-sizeof(long), 0) == -1) { perror("msgsnd to child 1 failed\n"); exit(1); } 

                for (i=0;i<10;i++){ //Update resource table with new values
                    resourceTable[simpidofsender][i] = resourcesUsed[i];
                    resourcesLeft[i] -= resourcesUsed[i];
                }
                printTable(resourceTable); //print resource table
            }
            notenoughresources = false;
        }
        
        for(i=0;i<10;i++){ //detecting if all resources are deallocated for all processes
            if(resourcesLeft[i] != 20){
                allResourcesFree = false;
                break; 
            }
            if(i == 9 && resourcesLeft[i] == 20){
                allResourcesFree = true;
            }
        }
        
        //Ending if passed 5 seconds and hit maximum children
        if((allResourcesFree && isEmpty()) && (numofchild > 40 || currentTime > 5)){
            printf("Ending Program!");
            fprintf(fileLogging, "Ending Program!");
            break; //end program
        }else{
            allResourcesFree = false;
        }
    }  //-----------------------------------------------------------------------------------------------------------------------------------------
    //-----------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------


    printf(" Waiting for the child to end its own life\n");
    fprintf(fileLogging, " Waiting for the child to end its own life\n");
    wait(0); //wait for child to finish in user_proc

    printTable(resourceTable);

    printf("RescouresLeft: ");
    fprintf(fileLogging, "RescouresLeft: ");
    for(i=0;i<10;i++){
        printf(" %i", resourcesLeft[i]);
        fprintf(fileLogging, " %i", resourcesLeft[i]);
    }
    printf("\n");
    fprintf(fileLogging, "\n");

    printf("deleting memory");
    fprintf(fileLogging, "deleting memory");
    shmdt( shm_ptr ); // Detach from the shared memory segment
    shmctl( shm_id, IPC_RMID, NULL ); // Free shared memory segment shm_id 

    //Removes the message queue immediately
    if (msgctl(msqid, IPC_RMID, NULL) == -1) { perror("msgctl"); return EXIT_FAILURE; }

    fclose(fileLogging); //close the log file

    return 0;
}

void printTable(int resourceTable[][10]){
    int i,j;
    printf("------------------------------------------------------------");
    printf("\t");
    //fprintf(fileLogging, "\t");
    for(i=0;i<10;i++){
        printf("3R%i\t", i);
        //fprintf(fileLogging, "R%i\t", i);
    }
    printf("\n");
    //fprintf(fileLogging, "\n");
    for(i = 0; i < 18; i++){ //Print resource table and max processes on the side
        printf("P%i\t", i);
        //fprintf(fileLogging, "P%i\t", i);
        for(j = 0; j < 10; j++){
            printf("%i\t", resourceTable[i][j]);
            //fprintf(fileLogging, "%i\t", resourceTable[i][j]);
        }
        printf("\n");
        //fprintf(fileLogging, "\n");
    }
}