#ifndef __SAVED_TASKSET_H__
#define __SAVED_TASKSET_H__

#include <map>
#include <vector>
#include "defines.h"

using namespace std;

// Structure for storing resource data under a particular task
typedef struct Resource {
	ResourceID resource_id;
	unsigned long critical_section_len; // in nanosecond
	unsigned int request_num;
} Resource;

// Implicit deadline parallel task
class Task { 
 public:
	Task();
	~Task();
	Task &operator=(const Task& other);

 public:
	TaskID task_id_;
	unsigned long period_; // period, in nanosecond
	unsigned long span_; // critical path length, in nanosecond
	double utilization_; // task's utiliation
	unsigned long wcet_; // Worst-case-execution-time, in nanosecond
	
	map<ResourceID, Resource*> my_resources_;

	// Stored data for fixed-point iteration
	double blocking_on_single_; // worst-case blocking time on 1 processor, in nanosecond
	double blocking_whole_job_; // worst-case blocking time of the whole job, in nanosecond
	unsigned int proc_num_; // number of processors allocated
	double response_time_; // response time, in nanosecond

	unsigned int new_proc_num_; // new number of processors allocated in the next iteration
	bool converged_; // true if the task parameters have converged
	
	// Other tasks access to the same resources
	map<ResourceID, vector<TaskID>*> interferences_;

	// Higher number means lower priority (highest priority has value 1).
	// NOTE: the value of this priority is only used in this analysis code.
	// When a task's priority is set using sched_setscheduler(), the higher
	// value indicates the higher priority. Hence, when writing the task's priority
	// to rtps file, we must convert to a correct priority's value.
	unsigned int priority_;
};


// Task set comprised of parallel tasks
class TaskSet {
 public:
	TaskSet();
	~TaskSet();
	TaskSet &operator=(const TaskSet& other);

 public:
	// This stores whether this task set is deemed schedulable
	// by the method BlockingAnalysis::is_schedulable()
	// Convention: 
	// 0 if the task set is schedulable
	// 1 if there is a partition available when we don't account the blocking
	// 2 if there is no partition available even at the beginning of the call
	int schedulability_status;
	
	map<TaskID, Task*> tasks_;
	map<TaskID, vector<TaskID> > higher_prio_tasks_;
	map<TaskID, vector<TaskID> > lower_prio_tasks_;
	map<TaskID, vector<TaskID> > equal_prio_tasks_;
};

void startup_taskset(TaskSet *taskset, unsigned int num_cores);


#endif // __SAVED_TASKSET_H__
