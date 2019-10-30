#ifndef STATISTICS_H
#define STATISTICS_H

#include <string.h>
#include <pthread.h>

const unsigned int MAX_EVENTS = 16 * (1 << 20); // 16M events

// Data structure to record the number of requests in resource queue.
// For each event occurs (a request arrives, a request finishes), 
// the number of requests in the queue is updated.
struct waiting_requests_stat {
	pthread_spinlock_t spinlock;
	unsigned int next_event;
	char data[MAX_EVENTS+1];
};

enum statistics_error_codes {
	STATISTICS_SUCCESS = 0,
	STATISTICS_INVALID_INPUT_ERROR,
	STATISTICS_SHM_OPEN_FAILED_ERROR,
	STATISTICS_FTRUNCATE_FAILED_ERROR,
	STATISTICS_MMAP_FAILED_ERROR,
	STATISTICS_SHM_UNLINK_FAILED_ERROR,
	STATISTICS_PTHREAD_SPIN_INIT_ERROR
};

// Functions intended to be called clustering_launcher.cpp
int init_stat_mem(const char* name, unsigned resources_num);
int destroy_stat_mem(const char* name, unsigned resources_num);

// Functions intended to be called by init() and finalize() of tasks
volatile struct waiting_requests_stat* get_stat_mem(const char* name, unsigned size, int* error_flag);
int unmap_stat_mem(volatile void* stats, unsigned resources_num);

#endif
