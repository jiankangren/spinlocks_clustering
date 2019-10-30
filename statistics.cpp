#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "statistics.h"


// Open a new shared mem region or get an existing one for statistics data
volatile struct waiting_requests_stat* get_stat_mem(const char* name, unsigned size, int* error_flag) {
	int fd = shm_open(name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		perror("ERROR: statistics call to shm_open failed");
		*error_flag = STATISTICS_SHM_OPEN_FAILED_ERROR;
		return NULL;
	}

	int ret_val = ftruncate(fd, size);
	if (ret_val == -1) {
		perror("ERROR: statistics call to ftruncate failed");
		*error_flag = STATISTICS_FTRUNCATE_FAILED_ERROR;
		return NULL;
	}

	volatile struct waiting_requests_stat* stat = (struct waiting_requests_stat*) mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (stat == MAP_FAILED) {
		perror("ERROR: statistics call to mmap failed");
		*error_flag = STATISTICS_MMAP_FAILED_ERROR;
		return NULL;
	}

	ret_val = close(fd);
	if (ret_val == -1) {
		perror("WARNING: statistics close file descriptor failed");
	}
	
	*error_flag = STATISTICS_SUCCESS;
	return stat;
}


// This method should be called in clustering_launcher.cpp to init the 
// shared memory segment to store statistics data. This is done before 
// any children tasks is forked.
int init_stat_mem(const char* name, unsigned resources_num) {
	if (resources_num == 0) {
		perror("WARNING: No shared resource");
		return STATISTICS_INVALID_INPUT_ERROR;
	}

	size_t size = sizeof(struct waiting_requests_stat) * resources_num;
	int error_flag;

	volatile struct waiting_requests_stat* stats = get_stat_mem(name, size, &error_flag);
	if (error_flag == STATISTICS_SUCCESS) {
		for (unsigned i=0; i<resources_num; i++) {
			volatile struct waiting_requests_stat* stat = stats+i;
			memset((void*) stat, 0, sizeof(struct waiting_requests_stat));
			if (pthread_spin_init(&(stat->spinlock), PTHREAD_PROCESS_SHARED) != 0) {
				perror("ERROR: statistics pthread_spin_init failed");
				return STATISTICS_PTHREAD_SPIN_INIT_ERROR;
			}
		}

		int ret_val = munmap((void*) stats, size);
		if (ret_val == -1) {
			perror("WARNING: statistics unmap memory failed");
		}
	}

	return error_flag;
}

// This method should be called in clustering_launcher.cpp, after every 
// children have terminated, to destroy the shared memory region.
int destroy_stat_mem(const char* name, unsigned resources_num) {
	int error_flag;
	size_t size = sizeof(struct waiting_requests_stat) * resources_num;
	volatile struct waiting_requests_stat* stats = get_stat_mem(name, size, &error_flag);
	
	if (error_flag != STATISTICS_SUCCESS) {
		perror("WARNING: destroy pthread spin locks failed");
	} else {
		for (unsigned i=0; i<resources_num ; i++) {
			volatile struct waiting_requests_stat* stat = stats+i;
			if (pthread_spin_destroy(&(stat->spinlock)) != 0) {
				perror("WARNING: pthread_spin_destroy failed");
			}
		}
	}

	int ret_val = shm_unlink(name);
	if (ret_val == -1) {
		perror("ERROR: statistics destroy shared memory failed");
		return STATISTICS_SHM_UNLINK_FAILED_ERROR;
	}

	return STATISTICS_SUCCESS;
}

// This should be called by finalize() of each task, to unmap 
// the shared memory region which contains statistics data.
int unmap_stat_mem(volatile void* stats, unsigned resources_num) {
	size_t size = sizeof(struct waiting_requests_stat) * resources_num;
	int ret_val = munmap((void*) stats, size);

	if (ret_val == -1) {
		perror("WARNING: statistics unmap memory failed");
	}

	return ret_val;
}
