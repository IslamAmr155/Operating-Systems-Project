#include "headers.h"

int capacity, msgq_id, *shm_addr, msgqIdProcess, quanta, Qtemp = 0;
uint64_t current = 0, received_processes = 0;
struct msgbuff message;
FILE *pFile;
struct pcb *processTable;


int compare_priority(const void *a, const void *b){
    struct pcb *p1 = (struct pcb *)a;
    struct pcb *p2 = (struct pcb *)b;
    return p1->priority < p2->priority ? 1 : -1;
}

int compare_remaining_time(void *a, void *b){
    struct pcb *p1 = (struct pcb *)a;
    struct pcb *p2 = (struct pcb *)b;
    return p1->remainingtime < p2->remainingtime ? 1 : -1;
}

int getWait(){
    return getClk() - processTable[current].arrivaltime - (processTable[current].runningtime - processTable[current].remainingtime);
}

void logging(char *event) {
    fprintf(pFile, "At time %d process %d %s arr %d total %d remain %d wait %d\n", getClk(), processTable[current].id, event, processTable[current].arrivaltime, processTable[current].runningtime, processTable[current].remainingtime, getWait());            
}

// Synchronizing the scheduler with the clock; if the process is over, it will be terminated
void sync(){
    // If no process is running, return
    if(!current)
        return;

    // Synchronizing the scheduler with each process, so that each process receives its specific message
    message.mtype = processTable[current].id;
    --processTable[current].remainingtime;
    message.process = processTable[current];
    sendMsg(msgqIdProcess, &message, true);

    // If the process is over, terminate it
    if(!processTable[current].remainingtime)
    {
        int status;
        int finished = wait(&status);
        status = status >> 8;
        printf("Process %d is finished!\n", status);
        logging("finished");
        Qtemp = 0;
        current = 0;
    }
}

int receive(){
    // receive all incoming processes
    int count = 0;
    while(receiveMsg(msgq_id, &message, false) != -1){
        printf("Process received -> P_ID = %d, P_arr: %d, P_run: %d, P_pri: %d at %d\n", message.process.id, message.process.arrivaltime, message.process.runningtime, message.process.priority, getClk());
        received_processes++;
        processTable[received_processes] = message.process;
        count++;
    }
    return count;
}

void run() {
    // If there is no pid saved in the process, it did not run before
    if(processTable[current].pid == -1){
        logging("started");
        processTable[current].pid = fork();
        if (processTable[current].pid == 0)
        {
            char str[100];
            printf("forking process %d\n", processTable[current].id);
            sprintf(str, "%d", processTable[current].id);
            char *args[] = {"./process.out", str, NULL};
            execv(args[0], args);
        }
    } else {
        logging("resumed");
    }
}

void hpf(CC_PQueue *cc_pq){
    int processes = 0;
    int counter = 0;
    while(processes < capacity){

        while(receiveMsg(msgq_id, &message, false) != -1)
        {
            processTable[message.process.id - 1] = message.process;
            printf("Process received -> P_ID = %d, P_arr: %d, P_run: %d, P_pri: %d at %d\n", message.process.id, message.process.arrivaltime, message.process.runningtime, message.process.priority, getClk());
            cc_pqueue_push(cc_pq, &(processTable[message.process.id - 1]));
            counter++;
        }

        if(counter == 0)
            continue;
        
        void *p;
        cc_pqueue_pop(cc_pq, &p);
        counter--;
        struct pcb *current = (struct pcb *)p;
        *shm_addr = current->remainingtime;
        current->pid = fork();
        if (current->pid == 0)
        {
            char *args[] = {"./process.out", NULL};
            execv(args[0], args);
        }
        fprintf(pFile, "At time %d process %d %s arr %d total %d remain %d wait %d\n", getClk(), current->id, "started", current->arrivaltime, current->runningtime, current->remainingtime, getClk() - current->arrivaltime);
        processes++;

        waitpid(current->pid, NULL, 0);

        int time = getClk();
        current->finishtime = time;
        int pID = current->id;
        int pArr = current->arrivaltime;
        int pRun = current->runningtime;
        int pRemain = *shm_addr;
        int pTA = current->finishtime - pArr;
        int pWait = pTA - current->runningtime;
        float pWTA = (float)pTA / pRun;
        fprintf(pFile, "At time %d process %d %s arr %d total %d remain %d wait %d TA %d WTA %.2f\n", time, pID, "finished", pArr, pRun, pRemain, pWait, pTA, pWTA);
    }
}

void srtn(CC_PQueue pq){
    
}

void rr(CC_Rbuf *rbuf){
    int count_received;
    while(received_processes < capacity || !cc_rbuf_is_empty(rbuf) || current != 0)
    {
        // Synchronizing the scheduler with the clock
        down(semclk);

        // Synchronizing the process with the scheduler.
        sync();

        // Decrementing the quanta (capped at 0)
        Qtemp = Qtemp ? Qtemp - 1 : 0;
        
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
    }
}

int main(int argc, char * argv[])
{
    initClk();
    
    //TODO implement the scheduler :)
    //upon termination release the clock resources.
    key_t key_id;

    key_id = ftok("keyfile", 65);
    msgq_id = msgget(key_id, 0666 | IPC_CREAT);
    int algorithm = atoi(argv[1]);
    capacity = atoi(argv[2]);
    processTable = malloc((capacity+1) * sizeof(struct pcb));
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

    switch (algorithm)
    {
    case 1:
        CC_PQueue *cc_pq;
        cc_pqueue_new(&cc_pq, compare_priority);
        hpf(cc_pq);
        break;
    case 2:
        // pq = pq_new_queue(capacity, compare_remaining_time, status);
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
