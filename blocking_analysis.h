#ifndef __BLOCKING_ANALYSIS_H__
#define __BLOCKING_ANALYSIS_H__

#include "saved_taskset.h"

// Structure to store information of requests 
// from tau_x to l_q which intefere with tau_i
// - requestNum: #requests of tau_x to l_q while tau_i pending
// - csLen: CS length of request from tau_x to l_q (nanosecond)
typedef struct {
	unsigned int proc_num;
	unsigned int request_num;
	unsigned long cs_len;
} CSData;


class BlockingAnalysis {
 private:
	BlockingAnalysis() {}

 public:
	static BlockingAnalysis& get_instance() {
		static BlockingAnalysis instance;
		return instance;
	}
	
 public:
	// Compute the number of processors allocated to a task
	static unsigned int alloc_proc(unsigned long wcet, unsigned long span, 
								   unsigned long deadline, unsigned long whole_blk, unsigned long single_blk);

	// Compute the response time bound for a task given its work, span, and number of processors
	static unsigned long response_time(unsigned long wcet, unsigned long span, 
									   unsigned int proc_num, unsigned long whole_blk, unsigned long single_blk);

 private:

	// Maximum number of jobs of a task can be pending in an interval of length t
	unsigned int njobs(Task *task, unsigned long t);

	// Compute delay_per_request quantity, i.e., maximum waiting time of a single request in priority-order locks
	bool delay_per_request(Task *task, TaskSet *taskset, ResourceID rid, SpinlockType lock_type, unsigned long *dpr);

	// Calculate critical-path blocking bound for FIFO-ordered spin locks
	unsigned long critical_path_blocking_fifo(Task *task, map<ResourceID, map<TaskID, CSData> > &x_data);

	void sort_by_cslen(Task *task, TaskSet *taskset, map<TaskID, CSData> &interfere, vector<CSData> &results);

	// Calculate critical-path blocking bound for priority-ordered spin locks
	unsigned long critical_path_blocking_prio(Task *task, TaskSet *taskset, map<ResourceID, map<TaskID, CSData> > &x_data,
									map<ResourceID, unsigned long> &dpr, SpinlockType lock_type);

	// Calculate critical-path blocking bound for a particular type of spin locks
	void critical_path_blocking(Task *task, TaskSet *taskset, unsigned int m, unsigned long *blocking, map<ResourceID, map<TaskID, CSData>>& x_data, map<ResourceID, unsigned long>& dpr, SpinlockType lock_type);

	// Calculate work blocking bound for FIFO-ordered spin locks
	unsigned long work_blocking_fifo(Task *task, map<ResourceID, map<TaskID, CSData> > &x_data);

	// Calculate work blocking bound for priority-ordered spin locks
	unsigned long work_blocking_prio(Task *task, TaskSet *taskset, 
											  map<ResourceID, map<TaskID, CSData> > &x_data, map<ResourceID, unsigned long> &dpr);

	// Calculate work blocking bound for a particular type of spin locks
	void work_blocking(Task *task, TaskSet *taskset, unsigned int m, unsigned long *blocking, map<ResourceID, map<TaskID, CSData>>& x_data, map<ResourceID, unsigned long>& dpr, SpinlockType lock_type);

	map<ResourceID, map<TaskID, CSData>> calc_interfere(Task *task, TaskSet *taskset);
	bool calc_dpr_task(Task *task, TaskSet *taskset, SpinlockType lock_type, map<ResourceID, unsigned long>& dpr);

	bool is_schedulable_common(TaskSet *tset, unsigned int m, SpinlockType lock_type);
	
	bool is_schedulable_fifo(TaskSet *tset, unsigned int m);
	bool is_schedulable_prio(TaskSet *tset, unsigned int m);
	void reset_taskset(TaskSet *test, vector<unsigned int>& priorities);

	unsigned int annealing_cost(TaskSet *tset, unsigned int m);
	vector<unsigned int> neighbor(vector<unsigned int> old_priorities);
	double acceptance_probability(unsigned int old_cost, unsigned int new_cost, double temperature);

 public:
	// Schedulability test method
	bool is_schedulable(TaskSet *tset, unsigned int m, SpinlockType lock_type);

	// Schedulability test for priority-ordered locks with 
	// simulated annealing as heuristics to search for locking-priority
	bool is_schedulable_prio_anneal(TaskSet *test, unsigned int m);
};


#endif
