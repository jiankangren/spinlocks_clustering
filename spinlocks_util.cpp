#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "spinlocks_util.h"
#include "atomic.h"


// Create a new or get an existing shared memory region of ticket lock objects
static volatile ticketlock* get_ticketlocks(const char* name, size_t size, int* error_flag) {
	int fd = shm_open(name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		perror("ERROR: spinlocks_util call to shm_open failed");
		*error_flag = SPINLOCKS_UTIL_SHM_OPEN_FAILED_ERROR;
		return NULL;
	}

	int ret_val = ftruncate(fd, size);
	if (ret_val == -1) {
		perror("ERROR: spinlocks_util call to ftruncate failed");
		*error_flag = SPINLOCKS_UTIL_FTRUNCATE_FAILED_ERROR;
		return NULL;
	}

	volatile ticketlock* locks = (ticketlock*) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (locks == MAP_FAILED) {
		perror("ERROR: spinlocks_util call to mmap failed");
		*error_flag = SPINLOCKS_UTIL_MMAP_FAILED_ERROR;
		return NULL;
	}

	ret_val = close(fd);
	if (ret_val == -1) {
		perror("WARNING: spinlocks_util close file descriptor failed");
	}

	*error_flag = SPINLOCKS_UTIL_SUCCESS;
	return locks;
}

// Create a new or get an existing shared memory region of priority lock objects
static volatile plock* get_plocks(const char* name, size_t size, int* error_flag) {
	int fd = shm_open(name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		perror("ERROR: spinlocks_util call to shm_open failed");
		*error_flag = SPINLOCKS_UTIL_SHM_OPEN_FAILED_ERROR;
		return NULL;
	}

	int ret_val = ftruncate(fd, size);
	if (ret_val == -1) {
		perror("ERROR: spinlocks_util call to ftruncate failed");
		*error_flag = SPINLOCKS_UTIL_FTRUNCATE_FAILED_ERROR;
		return NULL;
	}

	volatile plock* locks = (plock*) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (locks == MAP_FAILED) {
		perror("ERROR: spinlocks_util call to mmap failed");
		*error_flag = SPINLOCKS_UTIL_MMAP_FAILED_ERROR;
		return NULL;
	}

	ret_val = close(fd);
	if (ret_val == -1) {
		perror("WARNING: spinlocks_util close file descriptor failed");
	}

	*error_flag = SPINLOCKS_UTIL_SUCCESS;
	return locks;
}


// This method should be called in clustering_launcher.cpp to init the 
// shared memory segment to store lock objects. This is done before 
// any children tasks is forked.
int init_spinlocks(const char* name, unsigned resource_num, const std::string& lock_type) {
	if (resource_num == 0) {
		fprintf(stderr, "WARNING: No shared resource");
		return SPINLOCKS_UTIL_INVALID_INPUT_ERROR;
	}

	size_t size;
	if (lock_type == "fifo") {
		size = sizeof(ticketlock) * resource_num;
	} else if (lock_type == "prio") {
		size = sizeof(plock) * resource_num;
	} else {
		fprintf(stderr, "ERROR: Wrong lock type");
		return SPINLOCKS_UTIL_INVALID_INPUT_ERROR;
	}

	int error_flag;

	if (lock_type == "fifo") {
		volatile ticketlock* locks = get_ticketlocks(name, size, &error_flag);
		if (error_flag == SPINLOCKS_UTIL_SUCCESS) {
			for (unsigned i = 0; i < resource_num; i++) {
				locks[i].u = 0;
			}
			
			int ret_val = munmap((void*)locks, size);
			if (ret_val == -1) {
				perror("WARNING: spinlocks_util unmap memory failed");
			}
		}
	} else if (lock_type == "prio") {
		volatile plock* locks = get_plocks(name, size, &error_flag);
		if (error_flag == SPINLOCKS_UTIL_SUCCESS) {
			for (unsigned i = 0; i < resource_num; i++) {
				locks[i].owner = NULL;
				locks[i].waiters = 0;
			}
			
			int ret_val = munmap((void*)locks, size);
			if (ret_val == -1) {
				perror("WARNING: spinlocks_util unmap memory failed");
			}
		}
	}

	return error_flag;
}


// This method should be called in clustering_launcher.cpp, after every 
// children have terminated, to destroy the shared memory region.
int destroy_spinlocks(const char* name) {
	int ret_val = shm_unlink(name);
	if (ret_val == -1) {
		perror("ERROR: spinlocks_util destroy shared memory failed");
		return SPINLOCKS_UTIL_SHM_UNLINK_FAILED_ERROR;
	}
	
	return SPINLOCKS_UTIL_SUCCESS;
}

// This method should be called by tasks of a task set to obtain the
// pointer to a lock object of a resource it is trying to access.
// Ideally, it should be called by init() of the task. Note that it
// requires the name of the shared memory, hence in task_manager.cpp, 
// this name must be passed to init() defined by the specific task.
volatile void* get_spinlocks(const char* name, unsigned resource_num, const std::string& lock_type) {
	size_t size;
	volatile void* locks;
	int error_flag;

	if (lock_type == "fifo") {
		size = sizeof(ticketlock) * resource_num;
		locks = (void*) get_ticketlocks(name, size, &error_flag);
	} else if (lock_type == "prio") {
		size = sizeof(plock) * resource_num;
		locks = (void*) get_plocks(name, size, &error_flag);
	} else {
		fprintf(stderr, "ERROR: Wrong lock type");
		return NULL;
	}

	if (error_flag != SPINLOCKS_UTIL_SUCCESS)
		return NULL;
	
	return locks;
}

/*
volatile ticketlock* get_spinlocks(const char* name, unsigned resource_num) {
	size_t size = sizeof(ticketlock) * resource_num;
	int error_flag;

	volatile ticketlock* locks = get_ticketlocks(name, size, &error_flag);
	if (error_flag != SPINLOCKS_UTIL_SUCCESS)
		return NULL;
	
	return locks;
}
*/

// This should be called by finalize() of each task, to unmap 
// the shared memory region which contains spinlocks.
void unmap_spinlocks(volatile void* locks, unsigned resource_num, const std::string& lock_type) {
	size_t size;
	int ret_val;

	if (lock_type == "fifo") {
		size = sizeof(ticketlock) * resource_num;
		ret_val = munmap((void*) locks, size);
	} else if (lock_type == "prio") {
		size = sizeof(plock) * resource_num;
		ret_val = munmap((void*) locks, size);
	} else {
		fprintf(stderr, "WARNING: Wrong lock type");
		return;
	}

	if (ret_val == -1) {
		perror("WARNING: spinlocks_util unmap memory failed");
	}
}

/*
void unmap_spinlocks(volatile ticketlock* locks, unsigned resource_num) {
	size_t size = sizeof(ticketlock) * resource_num;
	int ret_val = munmap((void*) locks, size);

	if (ret_val == -1) {
		perror("WARNING: spinlocks_util unmap memory failed");
	}
}
*/
