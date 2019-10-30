#define _GNU_SOURCE
#include <sched.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <pthread.h>
#include <errno.h>
#include "atomic.h"

const unsigned num_runs = 1000000;

extern void lock_mcs(mcs_lock *lock, mcs_lock_t *me);
extern void unlock_mcs(mcs_lock *lock, mcs_lock_t *me);

unsigned long long to_ns(struct timespec *t) {
	return (t->tv_sec * 1000000000 + t->tv_nsec);
}

void main() {

	mcs_lock lock = 0;
	mcs_lock_t me;

	struct timespec start, end;
	unsigned long long max, min, mean, total;
	min = ULONG_MAX;
	max = 0;
	mean = 0;
	total = 0;

	// Bind process to CPU 2
	cpu_set_t my_set;
	CPU_ZERO(&my_set);
	CPU_SET(2, &my_set);
	if (sched_setaffinity(getpid(), sizeof(cpu_set_t), &my_set)) {
		perror("ERROR: Could not set affinity.");
		return;
	}

	struct sched_param sp;
	sp.sched_priority = 98;
    if (sched_setscheduler(getpid(), SCHED_FIFO, &sp)) {
		perror("ERROR: Could not set process scheduler/priority");
		return;
	}
	
	for (unsigned i=0; i<num_runs; i++) {
		//clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
		clock_gettime(CLOCK_MONOTONIC, &start);
		lock_mcs(&lock, &me);
		unlock_mcs(&lock, &me);
		clock_gettime(CLOCK_MONOTONIC, &end);
		//clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);

		unsigned long long diff = to_ns(&end) - to_ns(&start);
		total += diff;

		if (diff > max)
			max = diff;
		if (diff < min)
			min = diff;
	}

	mean = total/num_runs;
	
	printf("Maximum overhead: %llu ns\n", max);
	printf("Minimum overhead: %llu ns\n", min);
	printf("Mean overhead: %llu ns\n", mean);	
	return NULL;
}
