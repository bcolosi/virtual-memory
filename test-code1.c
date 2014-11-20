#include "473_mm.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <signal.h>
#include <malloc.h>
#include <errno.h>
#include <sys/mman.h>

//#define PAGE_SIZE 4096
void mm_log(FILE *);

int main ()
{
	int* vm_ptr;
	int PAGE_SIZE = sysconf(_SC_PAGE_SIZE);
	//printf("%d\n", PAGE_SIZE);
	int vm_size = 16*PAGE_SIZE;
	int temp;
	FILE* f1 = fopen("results.txt", "w");

	vm_ptr=memalign(PAGE_SIZE, vm_size);
	if(vm_ptr==NULL)
	{
		printf("FAILURE in virtual memory allocation\n");	
		return 0;
	}

	mm_init((void*)vm_ptr, vm_size, 4, PAGE_SIZE, 1);
	mm_log(f1);	

	/* virtual memory access starts */
	
	temp = vm_ptr[8];					// Read virtual page 1
	mm_log(f1);												 
	vm_ptr[8 + ((int)((1*PAGE_SIZE)/sizeof(int)))] = 72; 	// Write virtual page 2
	mm_log(f1);												 
	vm_ptr[16] = 12;					// Write virtual page 1 
	mm_log(f1);												 
	temp = vm_ptr[8 + ((int)((2*PAGE_SIZE)/sizeof(int)))];	// Read virtual page 3
	mm_log(f1);												 
	temp = vm_ptr[8 + ((int)((3*PAGE_SIZE)/sizeof(int)))];	// Read virtual page 4
	mm_log(f1);												 
	temp = vm_ptr[24];					// Read virtual page 1 
	mm_log(f1);												 
	temp = vm_ptr[8 + ((int)((4*PAGE_SIZE)/sizeof(int)))];	// Read virtual page 5
	mm_log(f1);												 
	vm_ptr[32] = 64;					// Write virtual page 1  
	mm_log(f1);												 
	temp = vm_ptr[16 + ((int)((1*PAGE_SIZE)/sizeof(int)))]; // Read virtual page 2
	mm_log(f1);												 

	/* virtual memory access ends */

	free(vm_ptr);
	fclose(f1);
	return 0;
}

void mm_log(FILE *f1)
{
	fprintf(f1, "%ld %ld\n", mm_report_npage_faults(), mm_report_nwrite_backs());	
	printf("%ld %ld\n", mm_report_npage_faults(), mm_report_nwrite_backs());	
}
