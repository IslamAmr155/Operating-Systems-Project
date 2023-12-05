#include "headers.h"

int capacity, msgq_id, *shm_addr, msgqIdProcess, quanta, Qtemp = 0;
uint64_t current = 0, received_processes = 0;
struct msgbuff message;
FILE *pFile;
struct pcb **processTable;


int compare_priority(const void *a, const void *b){
    struct pcb **p1 = (struct pcb **)a;
    struct pcb **p2 = (struct pcb **)b;
    return (*p1)->priority < (*p2)->priority ? 1 : -1;
}

int compare_remaining_time(const void *a, const void *b){
    struct pcb **p1 = (struct pcb **)a;
    struct pcb **p2 = (struct pcb **)b;
    return (*p1)->remainingtime < (*p2)->remainingtime ? 1 : -1;
}

int getWait(){
    return getClk() - processTable[current]->arrivaltime - (processTable[current]->runningtime - processTable[current]->remainingtime);
}

void logging(char *event) {
    fprintf(pFile, "At time %d process %d %s arr %d total %d remain %d wait %d", getClk(), processTable[current]->id, event, processTable[current]->arrivaltime, processTable[current]->runningtime, processTable[current]->remainingtime, getWait());            
    if (event != "finished")
        fprintf(pFile, "\n");
    else
        fprintf(pFile, " TA %d WTA %.2f\n", getClk() - processTable[current]->arrivaltime, (float)(getClk() - processTable[current]->arrivaltime) / processTable[current]->runningtime);
        
}

// Synchronizing the scheduler with the clock; if the process is over, it will be terminated
void sync(){
    // If no process is running, return
    if(!current)
        return;

    // Synchronizing the scheduler with each process, so that each process receives its specific message
    message.mtype = processTable[current]->pid;
    message.process = *processTable[current];
    sendMsg(msgqIdProcess, &message, true);
    receiveSpecificMsg(msgqIdProcess, &message, true, getpid());
    *processTable[current] = message.process;
}

void checkFinished(){
    // If the process is over, terminate it
    if(!processTable[current]->remainingtime)
    {
        int status;
        int finished = wait(&status);
        status = status >> 8;
        printf("Process %d is finished!\n", status);
        logging("finished");
        Qtemp = 0;
        // Delete the process from the process table
        free(processTable[current]);
        processTable[current] = NULL;
        current = 0;
    }
}

int receive(){
    // receive all incoming processes
    int count = 0;
    while(receiveMsg(msgq_id, &message, false) != -1){
        printf("Process received -> P_ID = %d, P_arr: %d, P_run: %d, P_pri: %d at %d\n", message.process.id, message.process.arrivaltime, message.process.runningtime, message.process.priority, getClk());
        received_processes++;
        processTable[received_processes] = malloc(sizeof(struct pcb));
        *processTable[received_processes] = message.process;
        printf("Process ID: %d is received\n", processTable[received_processes]->id);
        count++;
    }
    return count;
}

void run() {
    // If there is no pid saved in the process, it did not run before
    if(processTable[current]->pid == -1){
        logging("started");
        processTable[current]->pid = fork();
        if (processTable[current]->pid == 0)
        {
            char str[100];
            printf("forking process %d\n", processTable[current]->id);
            sprintf(str, "%d", processTable[current]->id);
            char *args[] = {"./process.out", str, NULL};
            execv(args[0], args);
        }
    } else {
        logging("resumed");
    }
}

void hpf(CC_PQueue *cc_pq){
    int count_received;
    while(received_processes < capacity || cc_pq->size || current != 0)
    {
        // Receiving new processes
        count_received = receive();

        // Enqueueing the received processes
        while(count_received--)
        {
            printf("New process %ld is being created \n", received_processes - count_received);
            cc_pqueue_push(cc_pq, &(processTable[received_processes - count_received]));
        }
        
        // Context Switching
        printf("size of pq now: %ld\n", cc_pq->size);
        if(cc_pq->size && !current){

            // Schedule a new process to run
            void *p;
            cc_pqueue_pop(cc_pq, &p);
            current = (*(struct pcb **)p)->id;

            // Fork a new process to run
            run();
        }

        // Synchronizing the process with the scheduler.
        sync();

        // Synchronizing the scheduler with the clock
        down(semclk);

        // Check if the current process is finished
        checkFinished();
    }
}

void srtn(CC_PQueue *cc_pq){
    int count_received;
    while(received_processes < capacity || cc_pq->size || current != 0)
    {
        // Receiving new processes
        count_received = receive();

        // If there is a new process, then I need to check if I need to context switch
        // If there is no new process, then I continue only if there is a process running as I am sure it has the least remaining time
        // or there are no processes in the queue so I need to check if there are new processes to receive
        if(count_received || (!current && cc_pq->size))
        {
            // If there is a process running, then I need to check if I need to context switch
            if (current != 0)
                cc_pqueue_push(cc_pq, &(processTable[current]));

            // Enqueueing the received processes
            while(count_received--)
            {
                printf("New process %ld is being created \n", received_processes - count_received);
                cc_pqueue_push(cc_pq, &(processTable[received_processes - count_received]));
            }

            void *p;
            cc_pqueue_pop(cc_pq, &p);

            // If the process is the same as the current process, then I do not need to context switch
            if(current != (*(struct pcb **)p)->id)
            {
                // If the process is different from the current process, then I need to context switch, so log stop on the current process
                if(current != 0)            
                    logging("stopped");
                
                current = (*(struct pcb **)p)->id;
                printf("Current: %ld\n", current);

                // Fork or resume a new process to run
                run();
            }
        }

        // Synchronizing the process with the scheduler.
        sync();

        // Synchronizing the scheduler with the clock
        down(semclk);
        
        // Check if the current process is finished
        checkFinished();
    } 
}

void rr(CC_Rbuf *rbuf){
    int count_received;
    while(received_processes < capacity || !cc_rbuf_is_empty(rbuf) || current != 0)
    {   
        // Receiving new processes
        count_received = receive();

        // Enqueueing the received processes
        while(count_received--)
        {
            printf("New process %ld is being created\n", received_processes - count_received);
            cc_rbuf_enqueue(rbuf, received_processes - count_received); 
        }
        
        // context switching
        if(!cc_rbuf_is_empty(rbuf) && Qtemp == 0){

            // Suspend current process
            if(current != 0){
                logging("stopped");
                cc_rbuf_enqueue(rbuf, current);
                current = 0;
            }

            // Schedule a new process to run
            cc_rbuf_dequeue(rbuf, &current);

            // fork or resume
            run();
            
            // Reset the quanta
            Qtemp = quanta;
        }

        // Synchronizing the process with the scheduler.
        sync();

        // Decrementing the quanta (capped at 0)
        Qtemp = Qtemp ? Qtemp - 1 : 0;

        // Synchronizing the scheduler with the clock
        down(semclk);

        // Check if the current process is finished
        checkFinished();
    }
}

int main(int argc, char * argv[])
{
    initClk();
    
    //TODO implement the scheduler :)
    //upon termination release the clock resources.

    msgq_id = msgget(GENKEY, 0666 | IPC_CREAT);
    int algorithm = atoi(argv[1]);
    capacity = atoi(argv[2]);
    processTable = malloc((capacity+1) * sizeof(struct pcb *));
    quanta = atoi(argv[3]);

    semclk = semget(SEMKEY, 1, 0666);

    msgqIdProcess = msgget(MSGKEY, 0666 | IPC_CREAT);
    if(msgqIdProcess == -1)
    {
        perror("Error in creating message queue of processes");
        exit(-1);
    }

    struct msgbuff message;

    pFile = fopen("scheduler.log", "w");
    if(pFile == NULL)
    {
        perror("Error in opening scheduler.log");
        exit(-1);
    }

    CC_PQueue *cc_pq;
    switch (algorithm)
    {
    case 1:
        cc_pqueue_new(&cc_pq, compare_priority);
        hpf(cc_pq);
        break;
    case 2:
        cc_pqueue_new(&cc_pq, compare_remaining_time);
        srtn(cc_pq);
        break;
    default:
        CC_Rbuf *cc_rbuf;
        quanta = atoi(argv[3]);
        cc_rbuf_new(&cc_rbuf);
        rr(cc_rbuf);
        break;
    }

    fclose(pFile);

    

    destroyClk(true);
    return 0;
}
