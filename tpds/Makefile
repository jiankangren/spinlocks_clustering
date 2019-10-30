# Compile clustering launcher and synthetic task using new lock implementations 
# MCS for FIFO locks and a new queue-based priority locks

CC = /usr/local/bin/g++-5.1.0
FLAGS = -Wall -std=c++0x #-g
LIBS = -L. -lrt -lpthread -lm

all: clustering_launcher_wlocks synthetic_task_wlocks clustering_launcher_fast synthetic_task_fast

synthetic_task_wlocks: synthetic_task.cpp spinlocks_util.cpp mcs_locks.c priority_queue_locks.c ../statistics.cpp ../single_use_barrier.cpp ../timespec_functions.cpp task_manager.cpp 
	$(CC) $(FLAGS) -fopenmp synthetic_task.cpp spinlocks_util.cpp mcs_locks.c priority_queue_locks.c ../statistics.cpp ../single_use_barrier.cpp ../timespec_functions.cpp task_manager.cpp -o synthetic_task_wlocks $(LIBS)

clustering_launcher_wlocks: clustering_launcher_wlocks.cpp spinlocks_util.cpp ../statistics.cpp  ../single_use_barrier.cpp
	$(CC) $(FLAGS) clustering_launcher_wlocks.cpp spinlocks_util.cpp ../statistics.cpp ../single_use_barrier.cpp -o clustering_launcher_wlocks $(LIBS)

# Compile a version where the cluster will terminate immediately after a job of any task misses deadline.
# The schedulability result of the task set is written to a file with .sched extension.
synthetic_task_fast: synthetic_task.cpp spinlocks_util.cpp mcs_locks.c priority_queue_locks.c ../statistics.cpp ../single_use_barrier.cpp ../timespec_functions.cpp task_manager_fast.cpp 
	$(CC) $(FLAGS) -fopenmp synthetic_task.cpp spinlocks_util.cpp mcs_locks.c priority_queue_locks.c ../statistics.cpp ../single_use_barrier.cpp ../timespec_functions.cpp task_manager_fast.cpp -o synthetic_task_fast $(LIBS)

clustering_launcher_fast: clustering_launcher_wlocks_fast.cpp spinlocks_util.cpp ../statistics.cpp  ../single_use_barrier.cpp
	$(CC) $(FLAGS) clustering_launcher_wlocks_fast.cpp spinlocks_util.cpp ../statistics.cpp ../single_use_barrier.cpp -o clustering_launcher_fast $(LIBS)

clean:
	rm -f *.o *.pyc clustering_launcher_wlocks synthetic_task_wlocks clustering_launcher_fast synthetic_task_fast