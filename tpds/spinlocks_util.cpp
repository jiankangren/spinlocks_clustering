#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "spinlocks_util.h"


// Create a new or get an existing shared memory region for FIFO-ordered locks
static volatile void *shm_fifo_locks(const char *name, size_t size, int *error_flag) {
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
	
    long page_size = sysconf(_SC_PAGE_SIZE);
	if (page_size == -1) {
		//		fprintf(stderr, "Get page size failed!\n");
		page_size = DEFAULT_PAGE_SIZE; // Use default page size
	}
	
	// Map the shared memory of the MCS-locks to a fixed address
	volatile void *locks = mmap((void*)(PAGE_SIZE_ALIGN*page_size), size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);

	//	volatile void *locks = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
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

// Create a new or get an existing shared memory region for Priority-ordered locks
static volatile void *shm_priority_locks(const char *name, size_t size, int *error_flag) {
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

	volatile void *locks = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
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

	// Get the number of processors online in the machine
	long num_procs = sysconf(_SC_NPROCESSORS_ONLN);
	//	printf("Number of online processors: %li\n", num_procs);

	// Each processor is reserved space for a MCS structure in the shared 
	// memory region. Since a processor can send only 1 request at a time 
	// (i.e., no nested accesses), it can reuse 1 MCS structure for all 
	// shared resources.
	size_t size;
	if (lock_type == "fifo") {
		size = sizeof(mcs_lock) * resource_num + sizeof(mcs_lock_t) * num_procs;
	} else if (lock_type == "prio") {
		size = sizeof(prio_lock) * resource_num + sizeof(mcs_lock_t) * num_procs;
	} else {
		fprintf(stderr, "ERROR: Wrong lock type");
		return SPINLOCKS_UTIL_INVALID_INPUT_ERROR;
	}

	int error_flag;

	if (lock_type == "fifo") {
		volatile void *locks = shm_fifo_locks(name, size, &error_flag);
		if (error_flag == SPINLOCKS_UTIL_SUCCESS) {
			// memset() does not work with volatile pointer, thus 
			// we initialize the memory manually
			for (unsigned i=0; i<size; i++) {
				*((char*)locks+i) = 0;
			}

			int ret_val = munmap((void*)locks, size);
			if (ret_val == -1) {
				perror("WARNING: spinlocks_util unmap memory failed");
			}
		}
	} else if (lock_type == "prio") {
		volatile void *locks = shm_priority_locks(name, size, &error_flag);
		if (error_flag == SPINLOCKS_UTIL_SUCCESS) {
			// Initialize the shared memory object
			for (unsigned i=0; i<size; i++) {
				*((char*)locks+i) = 0;
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

// These 2 methods should be called by tasks of a task set to obtain the
// pointer to a lock object of a resource it is trying to access.
// Ideally, it should be called by init() of the task. Note that it
// requires the name of the shared memory, hence in task_manager.cpp, 
// this name must be passed to init() defined by the specific task.
void get_fifo_locks(const char *name, unsigned resource_num, volatile mcs_lock **tails, volatile mcs_lock_t **elems) {
	size_t size;
	int error_flag;
	volatile void *ptr;

	// Get the number of processors online in the machine
	long num_procs = sysconf(_SC_NPROCESSORS_ONLN);

	size = sizeof(mcs_lock) * resource_num + sizeof(mcs_lock_t) * num_procs;
	ptr = shm_fifo_locks(name, size, &error_flag);

	if (error_flag != SPINLOCKS_UTIL_SUCCESS) {
		*tails = NULL;
		*elems = NULL;
		return;
	}

	*tails = (volatile mcs_lock*)ptr;
	*elems = (volatile mcs_lock_t*) ((char*)ptr + sizeof(mcs_lock) * resource_num);
}


void get_priority_locks(const char *name, unsigned resource_num, volatile prio_lock **locks, volatile mcs_lock_t **elems) {
	size_t size;
	int error_flag;
	volatile void *ptr;

	// Get the number of processors online in the machine
	long num_procs = sysconf(_SC_NPROCESSORS_ONLN);

	size = sizeof(prio_lock) * resource_num + sizeof(mcs_lock_t) * num_procs;
	ptr = shm_priority_locks(name, size, &error_flag);

	if (error_flag != SPINLOCKS_UTIL_SUCCESS) {
		*locks = NULL;
		*elems = NULL;
		return;
	}

	*locks = (volatile prio_lock*)ptr;
	*elems = (volatile mcs_lock_t*) ((char*)ptr + sizeof(prio_lock) * resource_num);
}


// This should be called by finalize() of each task, to unmap 
// the shared memory region which contains spinlocks.
void unmap_spinlocks(volatile void *locks, unsigned resource_num, const std::string& lock_type) {
	size_t size;
	int ret_val;

	long num_procs = sysconf(_SC_NPROCESSORS_ONLN);

	if (lock_type == "fifo") {
		size = sizeof(mcs_lock) * resource_num + sizeof(mcs_lock_t) * num_procs;
		ret_val = munmap((void*) locks, size);
	} else if (lock_type == "prio") {
		size = sizeof(prio_lock) * resource_num + sizeof(mcs_lock_t) * num_procs;
		ret_val = munmap((void*) locks, size);
	} else {
		fprintf(stderr, "WARNING: Wrong lock type!");
		return;
	}

	if (ret_val == -1) {
		perror("WARNING: spinlocks_util unmap memory failed");
	}
}
