#ifndef SPINLOCKS_INIT_H
#define SPINLOCKS_INIT_H

#include <string>
#include "../atomic.h"

// Default page size is 4096 bytes
#define DEFAULT_PAGE_SIZE 4096

// Align the mapping address of the spinlocks to a multiple of page size
#define PAGE_SIZE_ALIGN 2

enum spinlocks_util_error_codes {
	SPINLOCKS_UTIL_SUCCESS = 0,
	SPINLOCKS_UTIL_INVALID_INPUT_ERROR,
	SPINLOCKS_UTIL_SHM_OPEN_FAILED_ERROR,
	SPINLOCKS_UTIL_FTRUNCATE_FAILED_ERROR,
	SPINLOCKS_UTIL_MMAP_FAILED_ERROR,
	SPINLOCKS_UTIL_SHM_UNLINK_FAILED_ERROR
};

// Functions intended to be called by clustering_launcher.cpp
int init_spinlocks(const char* name, unsigned resource_num, const std::string& lock_type);

int destroy_spinlocks(const char* name);

// Functions intended to be called by functions init() and finalize() of tasks
void get_fifo_locks(const char *name, unsigned resource_num, volatile mcs_lock **tails, volatile mcs_lock_t **elems);

void get_priority_locks(const char *name, unsigned resource_num, volatile prio_lock **locks, volatile mcs_lock_t **elems);

void unmap_spinlocks(volatile void* locks, unsigned resource_num, const std::string& lock_type);


#endif
