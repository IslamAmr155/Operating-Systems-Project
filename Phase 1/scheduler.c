#include "headers.h"

int capacity, msgq_id, shmid, *shm_addr;
struct msgbuff message;
CC_PQueue *cc_pq;
FILE *pFile;

int compare_priority(const void *a, const void *b){
    struct pcb *p1 = (struct pcb *)a;
    struct pcb *p2 = (struct pcb *)b;
    return p1->priority < p2->priority ? 1 : -1;
}

bool compare_remaining_time(void *a, void *b){
    struct pcb *p1 = (struct pcb *)a;
    struct pcb *p2 = (struct pcb *)b;
    return p1->remainingtime < p2->remainingtime ? 1 : -1;
}

void hpf(CC_PQueue *cc_pq){
    int processes = 0;
    int counter = 0;
    struct pcb process_memory[capacity];
    while(processes < capacity){
        while(receiveMsg(msgq_id, &message, false) != -1)
        {
            process_memory[message.process.id - 1] = message.process;
            printf("Process received -> P_ID = %d, P_arr: %d, P_run: %d, P_pri: %d at %d\n", message.process.id, message.process.arrivaltime, message.process.runningtime, message.process.priority, getClk());
            cc_pqueue_push(cc_pq, &(process_memory[message.process.id - 1]));
            counter++;
        }
        if(counter == 0)
            continue;
        void *p;
        cc_pqueue_pop(cc_pq, &p);
        counter--;
        struct pcb *current = (struct pcb *)p;
        *shm_addr = current->remainingtime;
        int pid = fork();
        if (pid == 0)
        {
            char *args[] = {"./process.o", NULL};
            execv(args[0], args);
        }
        // printf("Process started -> P_ID = %d, P_arr: %d, P_run: %d, P_pri: %d at %d\n", current->id, current->arrivaltime, current->runningtime, current->priority, getClk());
        fprintf(pFile, "At time %d process %d %s arr %d total %d remain %d wait %d\n", getClk(), current->id, "started", current->arrivaltime, current->runningtime, current->remainingtime, getClk() - current->arrivaltime);
        processes++;
        // current->starttime = getClk();
        // int prev_clk = current->starttime;
        // while(current->remainingtime > 0){
            // printf("Current time: %d - Remaining time: %d\n", getClk(), current->remainingtime);
            // while(getClk() == prev_clk);
            // prev_clk = getClk();
            // current->remainingtime--;
        // }
        // current->finishtime = getClk();
        waitpid(pid, NULL, 0);
        // printf("Process finished -> P_ID = %d, P_arr: %d, P_run: %d, P_pri: %d at %d\n", current->id, current->arrivaltime, current->runningtime, current->priority, getClk());
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

void rr(CC_PQueue pq){
    
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

    // Shared memory that contains the remaining time for the running process
    key_id = ftok("keyfile", 77);
    shmid = shmget(key_id, 4, IPC_CREAT | 0644);
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
        cc_pqueue_new(&cc_pq, compare_priority);
        hpf(cc_pq);
        break;
    case 2:
        // pq = pq_new_queue(capacity, compare_remaining_time, status);
        break;
    default:
        // cq = (struct circularQueue *)malloc(sizeof(struct circularQueue));
        // head = &cq->front;
        break;
    }
    fclose(pFile);
    destroyClk(true);
    return 0;
}
