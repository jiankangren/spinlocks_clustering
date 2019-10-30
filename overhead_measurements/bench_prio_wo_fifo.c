#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>
#include <pthread.h>
#include <errno.h>
#include "atomic.h"

const unsigned ITERS_PER_THR = 100000;

extern void plock_lock(plock *p);
extern void plock_unlock(plock *p);
extern void init_prio(int priority);

unsigned long long to_ns(struct timespec *t) {
	return (t->tv_sec * 1000000000 + t->tv_nsec);
}

pthread_barrier_t barrier;

// Each thread has a local struct which stores its priority
extern __thread prio my_prio;

struct ThreadData {
    unsigned core_id; // start from 0
	plock *lock;
	unsigned long long *diff;
};


void *thr_run(void *input) {
	struct timespec stime, etime;
	struct ThreadData *data = (struct ThreadData*) input;
	
	// Read core id (also priority of the thread) this thread bound to
	unsigned core = data->core_id;
	plock* shared_lock = data->lock;
	unsigned long long *diff = data->diff;
	
	pthread_t thread_id = pthread_self();

	// Bind this thread to the core id tid
	cpu_set_t my_set;
	CPU_ZERO(&my_set);
	CPU_SET(core, &my_set);
	if (pthread_setaffinity_np(thread_id, sizeof(cpu_set_t), &my_set)) {
		perror("ERROR: Could not set affinity.");
		return (void*)-1;
	}

	// Set thread priority
	unsigned priority = core;
	init_prio(priority);

	// Wait for other threads
	int ret = pthread_barrier_wait(&barrier);
	if (ret != PTHREAD_BARRIER_SERIAL_THREAD && ret != 0) {
		printf("ERROR: Barrier wait failed");
		return (void*)-1;
	}

	// Every threads arrive, now all go
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &stime);
	for (unsigned i=0; i<ITERS_PER_THR; i++) {
		plock_lock(shared_lock);
		barrier();
		plock_unlock(shared_lock);
	}
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &etime);

	// Store elapsed time
	*diff = to_ns(&etime) - to_ns(&stime);
	printf("Total time on thread %d: %llu ns\n", core, *diff);
	return (void*)0;
}

int main(int argc, char *argv[]) {
	// Default number of threads
	unsigned NUM_THREADS = 8;

	if (argc == 2) {
		NUM_THREADS = atoi(argv[1]);
	} else if (argc == 1) {
		printf("Default number of threads: %d\n", NUM_THREADS);
	} else {
		printf("Usage: %s number_of_threads\n", argv[0]);
		return 0;
	}
	
	unsigned long long diffs[NUM_THREADS];
	
	plock shared_lock = {0, 0};
	struct timespec stime, etime;
	
	// Time it take to run on a single processor
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &stime);
	for (unsigned i=0; i<NUM_THREADS; i++) {
		for (unsigned j=0; j<ITERS_PER_THR; j++) {
			barrier();
		}
	}
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &etime);

	unsigned long long diff = to_ns(&etime) - to_ns(&stime);
	printf("Time to run on a single CPU: %llu ns\n", diff);

	// Bind process to a set of processors
	cpu_set_t my_set;
	CPU_ZERO(&my_set);
	for (unsigned i=0; i<NUM_THREADS; i++) {
		CPU_SET(i, &my_set);
	}
	if (sched_setaffinity(getpid(), sizeof(cpu_set_t), &my_set)) {
		perror("ERROR: Could not set affinity.");
		return -1;
	}
	
	// Run on multiple threads
	pthread_t threads[NUM_THREADS];
	// Thread data
	struct ThreadData data[NUM_THREADS];
	
	if (pthread_barrier_init(&barrier, NULL, NUM_THREADS)) {
		perror("ERROR: Create barrier failed");
		return -1;
	}

	for (unsigned i=0; i<NUM_THREADS; i++) {
		data[i].core_id = i;
		data[i].lock = &shared_lock;
		data[i].diff = &diffs[i];

		if (pthread_create(&threads[i], NULL, thr_run, &data[i])) {
			perror("ERROR: Create a new pthread failed.");
			return -1;
		}
	}

	for (unsigned int i=0; i<NUM_THREADS; i++) {
		if (pthread_join(threads[i], NULL)) {
			perror("WARNING: Join pthread failed");
		}
	}

	pthread_barrier_destroy(&barrier);
	
	// Process elapsed times of all threads
	unsigned long long total=0, mean=0;
	for (unsigned i=0; i<NUM_THREADS; i++) {
		total += diffs[i];
	}
	
	// Overhead of each acquire/release pair
	mean = (total-diff)/(NUM_THREADS * ITERS_PER_THR);
	printf("Time to run on multiple threads: %llu ns\n", total);
	printf("Average overhead per critical section: %llu ns\n", mean);

	return 0;
}


/*
void *dump_thread(void *input) {
	ticketlock *lock = (ticketlock*) input;
	struct timespec start, end;
	unsigned long long max, min, mean, total;
	min = ULONG_MAX;
	max = 0;
	mean = 0;
	total = 0;

	int ret = pthread_barrier_wait(&barrier);
	if (ret != PTHREAD_BARRIER_SERIAL_THREAD && ret != 0) {
		printf("ERROR: Barrier wait failed");
		return (void*)-1;
	}

	for (unsigned i=0; i<num_runs; i++) {
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
		ticket_lock(lock);
		ticket_unlock(lock);
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
		
		unsigned long long diff = to_ns(&end) - to_ns(&start);
		total += diff;

		if (diff > max)
			max = diff;
		if (diff < min)
			min = diff;
	}
	
	mean = total/num_runs;
	
	printf(" --- START --- \n");
	printf("Maximum overhead: %llu ns\n", max);
	printf("Minimum overhead: %llu ns\n", min);
	printf("Mean overhead: %llu ns\n", mean);	
	return NULL;
}


void main() {
	ticketlock lock = {.u = 0};
	pthread_t threads[num_threads];
	
	if (pthread_barrier_init(&barrier, NULL, num_threads)) {
		perror("ERROR: Create barrier failed");
		return;
	}
	
	for (unsigned i=0; i<num_threads; i++) {
		if (pthread_create(&threads[i], NULL, dump_thread, &lock)) {
			perror("ERROR: Create a new pthread failed.");
			return;
		}
	}

	for (unsigned int i=0; i<num_threads; i++) {
		if (pthread_join(threads[i], NULL)) {
			perror("WARNING: Join pthread failed");
		}
	}

	pthread_barrier_destroy(&barrier);

}
*/
