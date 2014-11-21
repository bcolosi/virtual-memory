#include "473_mm.h"
#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/mman.h>

#include <stdio.h>
#include <stdarg.h>
#define DEBUG_LEVEL 0b11111111

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

// pool of resident memory.
static struct rmem *pool;
static int num_frames;

static void clock_handler(int v, siginfo_t *si);

void mm_init(void* vm, int vm_size, int n_frames, int page_size, int policy) {
	debug(1, "mm_init called: vm %p, vm_size: 0x%x, n_frames: %d, page_size: 0x%x, policy: %d\n", vm, vm_size, n_frames, page_size, policy);
	switch(policy) {
		case POLICY_FIFO:

		break;
		case POLICY_CLOCK:

		// Allocate memory to hold all the resident frames.
		pool = malloc(sizeof(struct rmem) * n_frames);
		{
			int i;
			for (i = 0; i < n_frames - 1; i++) {
				pool[i].frame = -1;
				pool[i].accessed = 0;
				pool[i].dirty = 0;
				pool[i].next = &pool[i+1 % n_frames];
			}
		}

		struct sigaction action;
		action.sa_handler = clock_handler;
		action.sa_flags = 0;
		sigaction(SIGSEGV, &action, NULL);

		//mprotect(vm, vm_size, PROT_NONE);

		break;
	}
}

static void clock_handler(int v, siginfo_t *si) {
	debug(2, "SIGSEGV received: %p\n", si);
}

unsigned long mm_report_npage_faults() {
	return 0;
}

unsigned long mm_report_nwrite_backs() {
	return 0;
}
