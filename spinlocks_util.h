#ifndef SPINLOCKS_INIT_H
#define SPINLOCKS_INIT_H

#include <string>
#include "atomic.h"

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
//volatile ticketlock* get_spinlocks(const char* name, unsigned resource_num);
//void unmap_spinlocks(volatile ticketlock* locks, unsigned resource_num);
volatile void* get_spinlocks(const char* name, unsigned resource_num, const std::string& lock_type);
void unmap_spinlocks(volatile void* locks, unsigned resource_num, const std::string& lock_type);


#endif
