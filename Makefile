# This Makefile is copied from the original makefile to add spinlocks to synthetic tasks
# Son changed, Sep 16 2015


#CC = g++
CC = /usr/local/bin/g++-5.1.0
FLAGS = -Wall -std=c++0x
LIBS = -L. -lrt -lpthread -lm -lclustering
CLUSTERING_OBJECTS = single_use_barrier.o timespec_functions.o

all: clustering_distribution synthetic_task_wlocks scheduler analysis_only_sched

scheduler: partition_random.cpp saved_taskset.cpp blocking_analysis.cpp
	$(CC) $(FLAGS) partition_random.cpp saved_taskset.cpp blocking_analysis.cpp -o scheduler $(LIBS)

analysis_only_sched: partition_random_analysis_only.cpp saved_taskset.cpp blocking_analysis.cpp
	$(CC) $(FLAGS) partition_random_analysis_only.cpp saved_taskset.cpp blocking_analysis.cpp -o analysis_only_sched $(LIBS)

synthetic_task_wlocks: synthetic_task.cpp spinlocks_util.cpp ticket_locks.c priority_locks.c statistics.cpp task_manager.cpp
	$(CC) $(FLAGS) -fopenmp synthetic_task.cpp spinlocks_util.cpp ticket_locks.c priority_locks.c statistics.cpp task_manager.o -o synthetic_task_wlocks $(LIBS)

#synthetic_task_wlocks: synthetic_task_wlocks.cpp spinlocks_util.cpp ticket_locks.c priority_locks.c task_manager.cpp
#	$(CC) $(FLAGS) -fopenmp synthetic_task_wlocks.cpp spinlocks_util.cpp ticket_locks.c priority_locks.c task_manager.o -o synthetic_task_wlocks $(LIBS)

#synthetic_task: synthetic_task.cpp
#	$(CC) $(FLAGS) -fopenmp synthetic_task.cpp task_manager.o -o synthetic_task $(LIBS)
#	
#synthetic_task_utilization: synthetic_task.cpp
#	$(CC) $(FLAGS) -fopenmp synthetic_task.cpp utilization_calculator.o -o synthetic_task_utilization $(LIBS)

#simple_task: simple_task.cpp
#	$(CC) $(FLAGS) -fopenmp simple_task.cpp task_manager.o -o simple_task $(LIBS)
#	
#simple_task_utilization: simple_task.cpp
#	$(CC) $(FLAGS) -fopenmp simple_task.cpp utilization_calculator.o -o simple_task_utilization $(LIBS)
#	
clustering_distribution: libclustering.a utilization_calculator.o task_manager.o clustering_launcher_wlocks clustering_launcher_wlocks2 clustering_launcher_wlocks3

clustering_launcher_wlocks: clustering_launcher_wlocks.cpp spinlocks_util.cpp statistics.cpp
	$(CC) $(FLAGS) clustering_launcher_wlocks.cpp spinlocks_util.cpp statistics.cpp -o clustering_launcher_wlocks $(LIBS)

clustering_launcher_wlocks2: clustering_launcher_wlocks2.cpp spinlocks_util.cpp statistics.cpp
	$(CC) $(FLAGS) clustering_launcher_wlocks2.cpp spinlocks_util.cpp statistics.cpp -o clustering_launcher_wlocks2 $(LIBS)

clustering_launcher_wlocks3: clustering_launcher_wlocks3.cpp spinlocks_util.cpp statistics.cpp
	$(CC) $(FLAGS) clustering_launcher_wlocks3.cpp spinlocks_util.cpp statistics.cpp -o clustering_launcher_wlocks3 $(LIBS)

libclustering.a: $(CLUSTERING_OBJECTS)
	ar rcsf libclustering.a $(CLUSTERING_OBJECTS)

utilization_calculator.o: utilization_calculator.cpp
	$(CC) $(FLAGS) -c utilization_calculator.cpp

task_manager.o: task_manager.cpp
	$(CC) $(FLAGS) -c task_manager.cpp

single_use_barrier.o: single_use_barrier.cpp
	$(CC) $(FLAGS) -c single_use_barrier.cpp

timespec_functions.o: timespec_functions.cpp
	$(CC) $(FLAGS) -c timespec_functions.cpp

clean:
	rm -f *.o *.rtps *.pyc libclustering.a clustering_launcher_wlocks clustering_launcher_wlocks2 clustering_launcher_wlocks3 synthetic_task_wlocks scheduler analysis_only_sched