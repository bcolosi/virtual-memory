#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <signal.h>
#include "473_mm.h"

#define FIFO ((int) 1)
#define TCR ((int) 2)

#define READ ((int) 0)
#define WRITE ((int) 1)
#define NRW ((int) 2)


// GLOBALS **************************************************
void* vmSTART = NULL;
int vmSIZE = 0;
int max_phys_pages = 0;
int pageSIZE = 0;
int policy_type = 0;
int frame_number_counter = 1;

int current_pages = 0;

// STRUCTS **************************************************
struct queue{
    int page_num;
    int write_back;
    int phys_addr;
    int reference;
    int third_chance; 
    int frame;
    struct queue* next;
} queue;

struct queue* PM_queue = NULL;
struct queue* evict_page = NULL;

// HELPER FUNCTIONS *****************************************

// can call append for TCR if no evictions
void append(int page_num, int phys_addr, int frame){
    if(policy_type == FIFO){
        if(PM_queue == NULL){
            PM_queue = (struct queue*)malloc(sizeof(struct queue));
            PM_queue->page_num = page_num;
            PM_queue->write_back = 0;
            PM_queue->frame = frame;
            PM_queue->phys_addr = phys_addr;
            PM_queue->next = NULL;
        }
        else{
            struct queue* tmp = PM_queue;
            while(tmp->next != NULL){
                tmp = tmp->next;
            }
            tmp->next = (struct queue*)malloc(sizeof(struct queue));
            tmp = tmp->next;
            tmp->page_num = page_num;
            tmp->write_back = 0;
            tmp->phys_addr = phys_addr;
            tmp->frame = frame;
            tmp->next = NULL;
        }
    }
    else if(policy_type == TCR){
        if(current_pages > max_phys_pages){
            printf("Error: Can't append page!\n");
        }
        if(PM_queue == NULL){
            PM_queue = (struct queue*)malloc(sizeof(struct queue));
            PM_queue->page_num = page_num;
            PM_queue->write_back = 0;
            PM_queue->phys_addr = phys_addr;
            PM_queue->reference = 1;
            PM_queue->third_chance = 0;
            PM_queue->next = PM_queue;
            PM_queue->frame = frame;
            evict_page = PM_queue;
        }
        else{
            struct queue* tmp = PM_queue;
            while(tmp->next != PM_queue){
                tmp = tmp->next;
            }
            tmp->next = (struct queue*)malloc(sizeof(struct queue));
            tmp = tmp->next;
            tmp->page_num = page_num;
            tmp->write_back = 0;
            tmp->phys_addr = phys_addr;
            tmp->reference = 1;
            tmp->third_chance = 0;
            tmp->next = PM_queue;
            tmp->frame = frame;
            evict_page = PM_queue;
        }
    }
    else{
        printf("Invalid policy_type: %d\n", policy_type);
    }

    return;
}

void fifo_remove(){
    if(PM_queue != NULL){
        struct queue* tmp = PM_queue->next;
        free(PM_queue);
        PM_queue = tmp;
    } 
    else{
        printf("ERROR: Queue is already empty!\n");
    }
    return;
}

void remove_append(int page_num, int phys_addr){
    if(PM_queue != NULL){
        struct queue *tmp = evict_page;
        tmp->page_num = page_num;
        tmp->write_back = 0;
        tmp->phys_addr = phys_addr;
        tmp->reference = 1;
        tmp->third_chance = 0;

        return;
    }

    else{
        printf("ERROR: Queue is already empty!\n");
    }
    return;
}

void handler(int sig, siginfo_t* info, void* ucontext){
    struct queue* tmp = PM_queue;
    int page = (int)(info->si_addr - vmSTART) / pageSIZE;
    int cause = -1;
    int evicted_page = -1;
    int write_back = 0;
    int offset = ((int)info->si_addr - ((int)vmSTART + page * pageSIZE));
    int phys_addr = -1;
    int frame_number = -1;
    printf("\nOffset: %x\n\n", offset);
    printf("Entering Handler for Page %d!\n", page);

    if(policy_type == 1){

        printf("Inside handler with FIFO policy!\n");

        if(tmp != NULL){
            printf("tmp not NULL in FIFO Policy!\n");
            while(tmp->page_num != page && tmp->next != NULL){
                printf("Finding page!\n");
                tmp = tmp->next;
            }
        
            if(tmp->page_num == page){
                printf("Paged found in FIFO policy!\n");
                tmp->write_back = 1;
                frame_number = tmp->frame;
                mprotect(page*pageSIZE + vmSTART, pageSIZE, PROT_READ | PROT_WRITE);
                cause = WRITE;
            }
            else{
                current_pages++;
                if(current_pages > max_phys_pages){  
                    printf("Evicting page in Policy!\n");
                    evicted_page = PM_queue->page_num;
                    frame_number = PM_queue->frame;
                    write_back = PM_queue->write_back;
                    if(PM_queue != NULL){
                        mprotect(vmSTART + (evicted_page * pageSIZE), pageSIZE, PROT_NONE);
                    }
                    fifo_remove();
                    current_pages--;
                    printf("Page evicted in FIFO policy!\n");
                    append(page, phys_addr, frame_number);
                }
                else{
                    frame_number = frame_number_counter;
                    append(page, phys_addr, frame_number);
                    frame_number_counter++;
                }

                if(mprotect(vmSTART + (page * pageSIZE), pageSIZE, PROT_READ) == 0){
                    cause = READ;
                }
            }
        }
        else{
            printf("tmp is null!\n");
            current_pages++;
            frame_number = 0;
            append(page, phys_addr, frame_number);
            
            if(mprotect(vmSTART + (page * pageSIZE), pageSIZE, PROT_READ) == 0){
                cause = READ;
            }
        }

        phys_addr = (frame_number * pageSIZE) + offset;
    }
    else if(policy_type == 2){

        printf("Inside handler with THR policy!\n");

        if(tmp != NULL){

            printf("\n");
            printf("Page #: %d     %d      %d      %d\n",PM_queue->page_num, PM_queue->next->page_num, PM_queue->next->next->page_num, PM_queue->next->next->next->page_num);
            printf("Ref  #: %d     %d      %d      %d\n",PM_queue->reference, PM_queue->next->reference, PM_queue->next->next->reference, PM_queue->next->next->next->reference);
            printf("TCR  #: %d     %d      %d      %d\n",PM_queue->third_chance, PM_queue->next->third_chance, PM_queue->next->next->third_chance, PM_queue->next->next->next->third_chance);
            printf("\n");

            while(tmp->next != PM_queue){
                mprotect(tmp->page_num*pageSIZE + vmSTART, pageSIZE, PROT_NONE);
                tmp = tmp->next;
            }
            tmp = PM_queue;
            printf("tmp not NULL in TCR Policy!\n");
            while(tmp->page_num != page && tmp->next != PM_queue){
                printf("Finding page!\n");
                tmp = tmp->next;
            }
        
            if(tmp->page_num == page){
                printf("Paged found in TCR policy!\n");
                frame_number = tmp->frame;
                if(tmp->reference == 1){
                    tmp->write_back = 1;
                    tmp->third_chance = 1;
                    mprotect(page*pageSIZE + vmSTART, pageSIZE, PROT_READ | PROT_WRITE);
                    cause = WRITE;
                }
                else{
                    tmp->reference = 1;
                    if(tmp->third_chance == 1){
                        mprotect(page*pageSIZE + vmSTART, pageSIZE, PROT_READ);
                        cause = WRITE;
                    }
                    else{
                        tmp->write_back = 1;
                        tmp->third_chance = 1;
                        mprotect(page*pageSIZE + vmSTART, pageSIZE, PROT_READ | PROT_WRITE);
                        cause = NRW;
                    }
                }
            }
            else{
                current_pages++;
                if(current_pages > max_phys_pages){  
                    printf("Evicting page in Policy!\n");

                    while(evict_page->reference != 0 || evict_page->third_chance != 0){
                        if(evict_page->reference == 0){
                            printf("\nFinding page to evict, third chance set to 0\n\n");
                            evict_page->third_chance = 0;
                        }
                        evict_page->reference = 0;
                        if(evict_page->third_chance == 1){
                            mprotect(vmSTART + (evict_page->page_num * pageSIZE), pageSIZE, PROT_NONE);
                        }
                        evict_page = evict_page->next;
                    }                    

                    evicted_page = evict_page->page_num;
                    frame_number = evict_page->frame;
                    write_back = evict_page->write_back;
                    
                    if(PM_queue != NULL){
                        mprotect(vmSTART + (evicted_page * pageSIZE), pageSIZE, PROT_NONE);
                    }
                    remove_append(page, phys_addr);
                    current_pages--;
                    evict_page = evict_page->next;
                    printf("Page evicted in TCR policy!\n");
                }
                else{
                    printf("\n");
                    printf("current pages: %d\n", current_pages);
                    printf("\n");
                    frame_number = frame_number_counter;
                    append(page, phys_addr, frame_number);
                    frame_number_counter ++;
                }

                if(mprotect(vmSTART + (page * pageSIZE), pageSIZE, PROT_READ) == 0){
                    cause = READ;
                }
            }
        }
        else{
            printf("tmp is null!\n");
            current_pages++;
            frame_number = 0;
            append(page, phys_addr, frame_number);
            
            if(mprotect(vmSTART + (page * pageSIZE), pageSIZE, PROT_READ) == 0){
                cause = READ;
            }
        }
        phys_addr = (frame_number * pageSIZE) + offset;
    }
    else{
        printf("ERROR: Invalid policy type!\n");
    }


    mm_logger(page, cause, evicted_page, write_back, phys_addr);
    return;
}

// mm_init ***************************************************
void mm_init(void* vm, int vm_size, int n_frames, int page_size, int policy)
{
    if(policy != FIFO && policy != TCR){
        printf("ERROR: Invalid policy type.\n");
        printf("Policy value: %d\n", policy);

        return;
    }

    vmSTART = vm;
    vmSIZE = vm_size;
    max_phys_pages = n_frames;
    pageSIZE = page_size;
    policy_type = policy;

    struct sigaction action;
    action.sa_flags = SA_SIGINFO;
    action.sa_sigaction = handler;

    sigaction(SIGSEGV, &action, NULL);
    mprotect(vm, vm_size, PROT_NONE);
}
