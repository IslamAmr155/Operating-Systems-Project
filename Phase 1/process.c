#include "headers.h"

/* Modify this file as needed*/
int shm_id;

int main(int agrc, char * argv[])
{
    initClk();
    printf("Process starting at time: %d\n", getClk());
    
    key_t key_id = ftok("keyfile", 77);
    // Create/Get shared memory for one integer variable 4 bytes
    shm_id = shmget(key_id, 4, IPC_CREAT | 0644);
    if ((long)shm_id == -1)
    {
        perror("Error in creating shm!");
        exit(-1);
    }

    printf("Process attached at shm_id: %d\n", shm_id);
    
    int * shmAddr = (int *) shmat(shm_id, (void *)0, 0);
    if ((long)shmAddr == -1)
    {
        perror("Error in attaching the shm in process!");
        exit(-1);
    }
    
    //TODO it needs to get the remaining time from somewhere

    while (*shmAddr > 0)
    {
        printf("Process %d: Remaining time = %d\n", getpid(), *shmAddr);
        int prev = getClk();
        while(prev == getClk());
        (*shmAddr)--;
    }
    
    destroyClk(false);

    // deattach from shared memory
    shmdt(shmaddr);
    
    return 0;
}
