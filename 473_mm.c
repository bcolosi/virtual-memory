#include "473_mm.h"
#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h> // potentially unnecessary. I'll check later.

#include <stdio.h>
#include <stdarg.h>
#define DEBUG_LEVEL 0b11011000

static void debug(int level, const char *fmt, ...) {
	if (!(level & DEBUG_LEVEL))
		return;
	va_list args;
	va_start(args, fmt);

	// Provide color coding.
	int k = 0;
	while (level > 0)
		k++, level >>= 1;
	fprintf(stderr, "\033[%dm", 30 + k);
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\033[00m");
	va_end(args);
}

// Struct denoting a page which is resident in memory.
struct rmem {
	unsigned int frame;
	int accessed : 1;
	int dirty : 1;
	// gap of 30 bits here on a 64 bit machine.
	struct rmem *next;
};

// Frame struct used in FIFO implementation
struct fifo_frame {
	int frame;
	int accessed;
	int dirty;
	struct fifo_frame *next;
};


// pool of resident memory.
static struct rmem *pool;
static struct fifo_frame *fifo_pool;
static int num_frames;
static void* mem_start;
static int PAGE_SIZE;
static int page_faults = 0, write_backs = 0;

static void clock_handler(int, siginfo_t*, void*);
static void fifo_handler(int v, siginfo_t *si, void *context);

void mm_init(void* vm, int vm_size, int n_frames, int page_size, int policy) {
	debug(1, "mm_init called: vm %p, vm_size: 0x%x, n_frames: %d, page_size: 0x%x, policy: %d\n", vm, vm_size, n_frames, page_size, policy);
	switch(policy) {
		case POLICY_FIFO:

		fifo_pool = malloc(sizeof(struct fifo_frame) * n_frames);
		
		int i;
		for(i = 0; i < n_frames; i++)
		{
			fifo_pool[i].frame = -1;
			fifo_pool[i].accessed = 0;
			fifo_pool[i].dirty = 0;
			fifo_pool[i].next = &fifo_pool[i+1];
		}
		fifo_pool[i-1].next = NULL;
		//for (i = 0; i < n_frames; i++) {
		//	printf("pool[%d] addr: %p; next: %p\n", i, &fifo_pool[i], fifo_pool[i].next);
		//}

		struct sigaction fifo_action;
		fifo_action.sa_sigaction = fifo_handler;
		fifo_action.sa_flags = SA_SIGINFO;
		sigaction(SIGSEGV, &fifo_action, NULL);

		mprotect(vm, vm_size, PROT_NONE);
		mem_start = vm;
		PAGE_SIZE = page_size;
		num_frames = n_frames;	
	
		break;
		case POLICY_CLOCK:

		// Allocate memory to hold all the resident frames.
		pool = malloc(sizeof(struct rmem) * n_frames);
		{
			int i;
			for (i = 0; i < n_frames; i++) {
				pool[i].frame = -1;
				pool[i].accessed = 0;
				pool[i].dirty = 0;
				pool[i].next = &pool[(i+1) % n_frames];
			}
			for (i = 0; i < n_frames; i++) {
				debug(2, "pool[%d] addr: %p; next: %p\n", i, &pool[i], pool[i].next);
			}
		}

		struct sigaction action;
		action.sa_sigaction = clock_handler;
		action.sa_flags = SA_SIGINFO;
		sigaction(SIGSEGV, &action, NULL);

		mprotect(vm, vm_size, PROT_NONE);
		mem_start = vm;
		PAGE_SIZE = page_size;

		break;
	}
}

static void dump() {
	struct rmem *trace = pool;
	do {
		debug(32, "%p; frame: %d, accessed: %d, dirty: %d, next: %p\n", trace, trace->frame, trace->accessed, trace->dirty, trace->next);
		trace = trace->next;
	} while (trace != pool);
}

static void clock_handler(int v, siginfo_t *si, void *context) {
	int page = (int) (si->si_addr - mem_start) / PAGE_SIZE;
	debug(2, "SIGSEGV received: addr: %p\n", si->si_addr);
	struct rmem *trace = pool;
	int i;
	dump();

	debug(4, "page: %d", page);
	// Check to see if the memory is already resident.
	// If so, we should set its accessed bit or dirty bit,
	// depending on the style of modification.
	do {
		if (trace->frame == page) {
			debug(4, "; in-memory: %p", trace);
			if (trace->accessed) {
				debug(4, "; writing\n");
				trace->dirty = 1;
				mprotect(page*PAGE_SIZE + mem_start, PAGE_SIZE, PROT_READ | PROT_WRITE);
			} else {
				debug(4, "; reading\n");
				trace->accessed = 1;
				if (trace->dirty) {
					mprotect(page*PAGE_SIZE + mem_start, PAGE_SIZE, PROT_READ | PROT_WRITE);
				} else {
					mprotect(page*PAGE_SIZE + mem_start, PAGE_SIZE, PROT_READ);
				}
			}
			dump();
			debug(2, "Exiting signal handler.\n");
			return;
		}
		trace = trace->next;
	} while (trace != pool);

	page_faults++;
	debug(4, "; not in memory [%d]", page_faults);

	// If the memory is not resident, we should evict the next bad element.
	while (pool->accessed) {
		pool->accessed = 0;
		// still dirty, but we should prevent reading so that we know if it gets accessed again.
		mprotect(pool->frame*PAGE_SIZE + mem_start, PAGE_SIZE, PROT_NONE);
		pool = pool->next;
	}
	debug(4, "; evicting: %d", pool->frame);
	if (pool->dirty) {
		debug(4, "; dirty");
		write_backs++;
	}
	debug(4, "; emplace: %p\n", pool);
	pool->frame = page;
	pool->accessed = 1;
	mprotect(page*PAGE_SIZE + mem_start, PAGE_SIZE, PROT_READ);
	pool->dirty = 0;

	pool = pool->next;
	dump();
	debug(2, "Exiting signal handler.\n");
}

static void fifo_handler(int v, siginfo_t *si, void *context) {
	int page = (int) (si->si_addr - mem_start) / PAGE_SIZE;
	
	//check if page is present if physical memory
	//struct fifo_frame *trace = fifo_pool;
	int i;
	for(i = 0; i < num_frames; i++)
	{
		if(fifo_pool[i].frame == page)
		{
			if(fifo_pool[i].accessed)
			{
				fifo_pool[i].dirty = 1;
				mprotect(page*PAGE_SIZE + mem_start, PAGE_SIZE, PROT_READ | PROT_WRITE);
			}
			else
			{
			  fifo_pool[i].accessed = 1;
			  if (fifo_pool[i].dirty == 1)
			  {
			  	mprotect(page*PAGE_SIZE + mem_start, PAGE_SIZE, PROT_READ | PROT_WRITE);
			  }
			  else
			  {
			  	mprotect(page*PAGE_SIZE + mem_start, PAGE_SIZE, PROT_READ);
			  }
			}
			//for (i = 0; i < num_frames; i++) {
			//	printf("pool[%d] page: %d; dirty: %d\n", i, fifo_pool[i].frame, fifo_pool[i].dirty);
			//}
			return;
		}
	}

	//If we reach this point, page is not in memory
	page_faults++;
	
	int evicted_page = fifo_pool[0].frame;

	if(fifo_pool[0].dirty == 1)
	{
		write_backs++;
	}
	for(i = 0; i < num_frames - 1; i++)
	{
		fifo_pool[i].frame = fifo_pool[i+1].frame;
		fifo_pool[i].accessed = fifo_pool[i+1].accessed;
		fifo_pool[i].dirty = fifo_pool[i+1].dirty;
	}
	fifo_pool[i].frame = page;
	fifo_pool[i].accessed = 0;
	fifo_pool[i].dirty = 0;

	if(evicted_page != -1)
	{
		mprotect(evicted_page*PAGE_SIZE + mem_start, PAGE_SIZE, PROT_NONE);
	}

	//for (i = 0; i < num_frames; i++) {
	//	printf("pool[%d] page: %d; dirty: %d\n", i, fifo_pool[i].frame, fifo_pool[i].dirty);
	//}
}

static void replace_oldest (struct fifo_frame *to_push)
{
	fifo_pool = fifo_pool->next;
}

unsigned long mm_report_npage_faults() {
	return page_faults;
}

unsigned long mm_report_nwrite_backs() {
	return write_backs;
}
