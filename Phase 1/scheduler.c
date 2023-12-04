#include "headers.h"

int capacity, msgq_id, *shm_addr, msgqIdProcess, quanta, Qtemp = 0;
uint64_t current = 0;
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

void rrHandler(int signum)
{
    signal(SIGUSR1, rrHandler);
    printf("Process %ld is finished!\n", current);
    fprintf(pFile, "At time %d process %d %s arr %d total %d remain %d wait %d\n", getClk(), processTable[current].id, "finished", processTable[current].arrivaltime, processTable[current].runningtime, 0, getClk() - processTable[current].arrivaltime);
    // free(&processTable[current]);
    Qtemp = 0;
    current = 0;
}

void clearResourcesOfScheduler(int signum)
{
    //TODO Clears all resources in case of interruption
    destroyClk(true);
    msgctl(msgqIdProcess, IPC_RMID, (struct msqid_ds *)0);
    // delete semaphores
    semctl(semclk, 0, IPC_RMID, NULL);
    fclose(pFile);
    exit(0);
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
    uint64_t processes = 1;
    signal(SIGUSR1, rrHandler);
    while(processes <= capacity || !cc_rbuf_is_empty(rbuf) || current != 0)
    {
        // printf("%ld out of %d, cond1 = %d, cond2 = %d\n", processes, capacity, !cc_rbuf_is_empty(rbuf), current != 0);
        printf("-------------------------------------------------\n");
        if(current){
            printf("SENDING to %d...\n", processTable[current].id);
            message.mtype = processTable[current].id;
            sendMsg(msgqIdProcess, &message, true);
        }
        
        down(semclk);

        Qtemp = Qtemp ? Qtemp - 1 : 0;
        
        // wait for message if empty or check for new messages
        if(receiveMsg(msgq_id, &message, false) != -1)
            printf("Process received -> P_ID = %d, P_arr: %d, P_run: %d, P_pri: %d at %d\n", message.process.id, message.process.arrivaltime, message.process.runningtime, message.process.priority, getClk());

        if(message.mtype == processes)
        {
            printf("New process %ld is being created\n", processes);
            cc_rbuf_enqueue(rbuf, processes);
            processTable[processes] = message.process;
            processes++;
        }

        // context switching
        if(!cc_rbuf_is_empty(rbuf) && Qtemp == 0){

            // Suspend current process
            if(current != 0){
                printf("Current %ld is going to sleep now!\n", current);
                processTable[current].remainingtime = *shm_addr;
                fprintf(pFile, "At time %d process %d %s arr %d total %d remain %d wait %d\n", getClk(), processTable[current].id, "stopped", processTable[current].arrivaltime, processTable[current].runningtime, processTable[current].remainingtime, getClk() - processTable[current].arrivaltime);
                if (*(shm_addr) != 0)
                    cc_rbuf_enqueue(rbuf, current);
                current = 0;
            }

            // Schedule a new process to run
            cc_rbuf_dequeue(rbuf, &current);
            printf("Current = %ld is dequeued!\n", current);
            *shm_addr = processTable[current].remainingtime;

             // fork or resume
            if(processTable[current].pid == -1){
                fprintf(pFile, "At time %d process %d %s arr %d total %d remain %d wait %d\n", getClk(), processTable[current].id, "started", processTable[current].arrivaltime, processTable[current].runningtime, processTable[current].remainingtime, getClk() - processTable[current].arrivaltime);
                processTable[current].pid = fork();
                if (processTable[current].pid == 0)
                {
                    char str[100];
                    sprintf(str, "%d", processTable[current].id);
                    char *args[] = {"./process.out", str, NULL};
                    execv(args[0], args);
                }
            } else {
                fprintf(pFile, "At time %d process %d %s arr %d total %d remain %d wait %d\n", getClk(), processTable[current].id, "resumed", processTable[current].arrivaltime, processTable[current].runningtime, processTable[current].remainingtime, getClk() - processTable[current].arrivaltime);
            }
            Qtemp = quanta;
        }
    }
}

int main(int argc, char * argv[])
{
    initClk();
    signal(SIGINT, clearResourcesOfScheduler);
    
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

    key_t key = ftok("keyfile", 40);
    msgqIdProcess = msgget(key, 0666 | IPC_CREAT);
    if(msgqIdProcess == -1)
    {
        perror("Error in creating message queue of processes");
        exit(-1);
    }

    struct msgbuff message;

    // Shared memory that contains the remaining time for the running process
    key_id = ftok("keyfile", 77);
    int shmid = shmget(key_id, 4, IPC_CREAT | 0644);
    if ((long)shmid == -1)
    {
        perror("Error in creating shm!");
        exit(-1);
    }
    
    shm_addr = (int *) shmat(shmid, (void *)0, 0);
    if ((long)shmaddr == -1)
    {
        perror("Error in attaching the shm in process!");
        exit(-1);
    }

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
