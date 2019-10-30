#include <cmath>
#include <iostream>
#include <algorithm>
#include <climits>
#include <string>
#include "blocking_analysis.h"


// Type of priority assignment
extern std::string prio_assign;

// Update number of processors allocated
unsigned int BlockingAnalysis::alloc_proc(unsigned long wcet, unsigned long span, unsigned long deadline, 
										  unsigned long whole_blk, unsigned long single_blk) {
	if (deadline < span + single_blk) {
		cout << "Denominator of equation for n_i is negative !!!" << endl;
	} else if (deadline == span + single_blk) {
		cout << "Denominator of equation for n_i is zero !!!" << endl;
	}

	return ceil( (double)((wcet+whole_blk) - (span+single_blk)) / (deadline - (span+single_blk)) );
}

// Update bound of response time using blocking time, #processors
unsigned long BlockingAnalysis::response_time(unsigned long wcet, unsigned long span, unsigned int proc_num, 
									   unsigned long whole_blk, unsigned long single_blk) {
	//    unsigned long response_time = (wcet+whole_blk-span-single_blk)/proc_num + span + single_blk;
	unsigned long response_time = span + single_blk + (wcet + whole_blk)/proc_num;
	return response_time; // in nanosecond
}


// Calculate delay-per-request for priority-ordered locks with unordered tie break
bool BlockingAnalysis::delay_per_request(Task *task, TaskSet *taskset, ResourceID rid, SpinlockType lock_type, unsigned long *ret) {
	if (lock_type != PRIO_UNORDERED && lock_type != PRIO_FIFO) {
		cout << "Wrong lock type in calculation of delay-per-request !!!" << endl;
		return false;
	}

	TaskID my_id = task->task_id_;
	map<TaskID, Task*> &tset = taskset->tasks_;
	vector<TaskID> &lp_tasks = taskset->lower_prio_tasks_[my_id];
	vector<TaskID> &hp_tasks = taskset->higher_prio_tasks_[my_id];
	vector<TaskID> &ep_tasks = taskset->equal_prio_tasks_[my_id];

	// List of interfering tasks w.r.t this resource
	vector<TaskID> *interfering_tasks = task->interferences_[rid];

	// Collect lower, higher, equal locking-priority tasks w.r.t this resource
	vector<TaskID> lp_tasks_for_resource;
	vector<TaskID> hp_tasks_for_resource;
	vector<TaskID> ep_tasks_for_resource;
	for (unsigned i=0; i<interfering_tasks->size(); i++) {
		TaskID his_id = interfering_tasks->at(i);
		if (find(lp_tasks.begin(), lp_tasks.end(), his_id) != lp_tasks.end()) {
			lp_tasks_for_resource.push_back(his_id);
		} else if (find(hp_tasks.begin(), hp_tasks.end(), his_id) != hp_tasks.end()) {
			hp_tasks_for_resource.push_back(his_id);
		} else if (find(ep_tasks.begin(), ep_tasks.end(), his_id) != ep_tasks.end()) {
			ep_tasks_for_resource.push_back(his_id);
		} else {
			cout << "SOMETHING WRONG!!!" << endl;
		}
	}

	// Find the maximum length of a critical section among
	// lower locking-priority tasks (w.r.t this resource)
	unsigned long max_cslen_lp = 0;
	for (unsigned int i=0; i<lp_tasks_for_resource.size(); i++) {
		TaskID his_id = lp_tasks_for_resource[i];
		unsigned long his_cs_len = tset[his_id]->my_resources_[rid]->critical_section_len;
		if (his_cs_len > max_cslen_lp)
			max_cslen_lp = his_cs_len;
	}

	/*
	// Add 1 to avoid the case when max_cslen_lp equals 0
	unsigned long dpr = max_cslen_lp + 1;

	// Start
	// SonDN (May 28): try to add self-blocking to the 
	// initial value of dpr
	unsigned int proc_num = task->proc_num_;
	unsigned int req_num = task->my_resources_[rid]->request_num;
	double cs_len = task->my_resources_[rid]->critical_section_len;
	dpr += min(proc_num-1, req_num-1) * cs_len;
	// End
	*/
	
	unsigned long dpr = 0;

	// Debugging variable to count the number of iterations
	//	int count = 0;

	bool converged = false;
	while (!converged) {
		//		count++;
		// Init tmp (interferences from LOWER locking-priority requests)
		//unsigned long tmp = max_cslen_lp + 1;
		unsigned long tmp = max_cslen_lp;
		
		// Add interferences from HIGHER locking-priority requests
		for (unsigned int i=0; i<hp_tasks_for_resource.size(); i++) {
			TaskID his_id = hp_tasks_for_resource[i];
			unsigned long his_cs_len = tset[his_id]->my_resources_[rid]->critical_section_len;
			unsigned int his_req_num = tset[his_id]->my_resources_[rid]->request_num;
			tmp += njobs(tset[his_id], dpr) * his_req_num * his_cs_len;
		}

		// Add interferences from EQUAL locking-priority requests
		if (lock_type == PRIO_UNORDERED) {
			for (unsigned int i=0; i<ep_tasks_for_resource.size(); i++) {
				TaskID his_id = ep_tasks_for_resource[i];
				unsigned long his_cs_len = tset[his_id]->my_resources_[rid]->critical_section_len;
				unsigned int his_req_num = tset[his_id]->my_resources_[rid]->request_num;
				tmp += njobs(tset[his_id], dpr) * his_req_num * his_cs_len;
			}
			unsigned long my_cs_len = task->my_resources_[rid]->critical_section_len;
			unsigned int my_req_num = task->my_resources_[rid]->request_num;
			tmp += (my_req_num - 1)*my_cs_len;
		} else if (lock_type == PRIO_FIFO) {
			for (unsigned int i=0; i<ep_tasks_for_resource.size(); i++) {
				TaskID his_task_id = ep_tasks_for_resource[i];
				unsigned long his_cs_len = tset[his_task_id]->my_resources_[rid]->critical_section_len;
				unsigned int his_proc_num = tset[his_task_id]->proc_num_;
				unsigned int his_req_num = njobs(tset[his_task_id], task->response_time_) * tset[his_task_id]->my_resources_[rid]->request_num;
				tmp += min(his_proc_num, his_req_num) * his_cs_len;
			}
			unsigned int my_proc_num = task->proc_num_;
			unsigned int my_req_num = task->my_resources_[rid]->request_num;
			double my_cs_len = task->my_resources_[rid]->critical_section_len;
			tmp += min(my_proc_num-1, my_req_num-1) * my_cs_len;
		}

		// Set flag to true if dpr is converged
		if (tmp == dpr)
			converged = true;
		
		// Update value for dpr either way
		dpr = tmp;

		if (dpr > task->period_) {
			//cout << "Delay-per-request happens to be larger than period !!!" << endl;			
			//dpr = task->period_;
			//converged = true;
			return false;
		}
	}

	*ret = dpr;
	return true;

	//	return dpr;
}


// Calculate maximum blocking for FIFO locks using numerical approach
unsigned long BlockingAnalysis::critical_path_blocking_fifo(Task *task, map<ResourceID, map<TaskID, CSData> > &x_data) {
	unsigned int my_proc_num = task->proc_num_;
	unsigned long my_blocking = 0;
	map<ResourceID, map<TaskID, CSData> >::iterator it = x_data.begin();
	for (; it != x_data.end(); it++) {
		ResourceID res_id = it->first;
		map<TaskID, CSData> &interfere = it->second;
		unsigned int my_req_num = task->my_resources_[res_id]->request_num;
		unsigned long cs_len = task->my_resources_[res_id]->critical_section_len;

		vector<unsigned long> blks;
		for (unsigned int i=0; i<=my_req_num; i++) {
			// Add up blocking inside the task
			unsigned long tmp = min((my_proc_num-1)*i, my_req_num-i)*cs_len;

			// Add up blocking from other tasks
			for (map<TaskID, CSData>::iterator inner_it=interfere.begin(); inner_it!=interfere.end(); inner_it++) {
				//TaskID his_task_id = inner_it->first;
				unsigned int his_proc_num = inner_it->second.proc_num;
				unsigned int his_req_num = inner_it->second.request_num;
				unsigned long his_cs_len = inner_it->second.cs_len;
				tmp += min(his_proc_num*i, his_req_num)*his_cs_len;
				
				//LJ
				//tmp += min(min(his_proc_num, tset[his_task_id]->myResources[res_id]->requestNum)*i, his_req_num)*his_cs_len;
			}

			blks.push_back(tmp);
		}
		my_blocking += *max_element(blks.begin(), blks.end());
	}
	return my_blocking;
}

// Sort the list of interference tasks by their critical section length 
// @input: interfere - map of all interference tasks and their cs data
// @output: results - sorted cs data of lower priority tasks by decreasing cs length
void BlockingAnalysis::sort_by_cslen(Task *task, TaskSet *taskset, map<TaskID, CSData> &interfere, vector<CSData> &results) {
	vector<TaskID> &my_lp_tasks = taskset->lower_prio_tasks_[task->task_id_];

	for (map<TaskID, CSData>::iterator it = interfere.begin(); it != interfere.end(); it++) {
		// Only consider lower priotiry tasks
		if (find(my_lp_tasks.begin(), my_lp_tasks.end(), it->first) == my_lp_tasks.end())
			continue;

		// Find the correct insert position
		unsigned long current_cslen = it->second.cs_len;
		vector<CSData>::iterator insert_pos = results.begin();
		for (; insert_pos!=results.end(); insert_pos++) {
			if (current_cslen < insert_pos->cs_len) {
				continue;
			} else {
				break;
			}
		}

		// Insert to the result list
		results.insert(insert_pos, it->second);
	}
}

// Maximum blocking for Priority-ordered locks using numerical method
unsigned long BlockingAnalysis::critical_path_blocking_prio(Task *task, TaskSet *taskset, map<ResourceID, map<TaskID, CSData> > &x_data,
												  map<ResourceID, unsigned long> &dpr, SpinlockType lock_type) {
	map<TaskID, Task*> &tset = taskset->tasks_;
	//	vector<TaskID> &my_lp_tasks = taskset->lower_prio_tasks_[task->task_id_];
	vector<TaskID> &my_ep_tasks = taskset->equal_prio_tasks_[task->task_id_];
	vector<TaskID> &my_hp_tasks = taskset->higher_prio_tasks_[task->task_id_];
	unsigned int my_proc_num = task->proc_num_;
	unsigned long my_blocking = 0;

	map<ResourceID, map<TaskID, CSData> >::iterator it = x_data.begin();
	for (; it != x_data.end(); it++) {
		ResourceID res_id = it->first;
		map<TaskID, CSData> &interfere = it->second;
		unsigned int my_req_num = task->my_resources_[res_id]->request_num;
		unsigned long cs_len = task->my_resources_[res_id]->critical_section_len;

		// Sort interference tasks' data by cs length (decreasing order)
		vector<CSData> sorted_by_cslen;
		sort_by_cslen(task, taskset, interfere, sorted_by_cslen);
		
		vector<unsigned long> blks;
		for (unsigned int i=0; i<=my_req_num; i++) {
			unsigned long tmp_blocking = 0;
			// Add up blocking inside the task
			if (lock_type == PRIO_UNORDERED) {
				if (i > 0)
					tmp_blocking += (my_req_num-i)*cs_len;
			} else if (lock_type == PRIO_FIFO) {
				tmp_blocking += min((my_proc_num-1)*i, my_req_num-i)*cs_len;
			}

			// Add up blocking from lower locking-priority tasks
			unsigned int remain_req = i;
			for (unsigned int j=0; j<sorted_by_cslen.size(); j++) {
				if (sorted_by_cslen[j].request_num >= remain_req) {
					tmp_blocking += remain_req * sorted_by_cslen[j].cs_len;
					break;
				} else {
					tmp_blocking += sorted_by_cslen[j].request_num * sorted_by_cslen[j].cs_len;
					remain_req -= sorted_by_cslen[j].request_num;
				}
			}

			if (lock_type == PRIO_UNORDERED) {
				map<TaskID, CSData>::iterator inter_it = interfere.begin();
				
				for (; inter_it != interfere.end(); inter_it++) {
					TaskID his_task_id = inter_it->first;

					if (find(my_ep_tasks.begin(), my_ep_tasks.end(), his_task_id) != my_ep_tasks.end() ||
						find(my_hp_tasks.begin(), my_hp_tasks.end(), his_task_id) != my_hp_tasks.end() ) {
						// Add blocking from equal & higher priority tasks
						unsigned int his_req_num = inter_it->second.request_num;
						unsigned long his_cs_len = inter_it->second.cs_len;
						unsigned long dpr_for_curr_res = dpr[res_id];
						unsigned int req_inside_dpr = njobs(tset[his_task_id], dpr_for_curr_res)*tset[his_task_id]->my_resources_[res_id]->request_num;
						tmp_blocking += min(req_inside_dpr*i, his_req_num) * his_cs_len;
					}
				}

			} else if (lock_type == PRIO_FIFO) {
				map<TaskID, CSData>::iterator inter_it = interfere.begin();

				for (; inter_it != interfere.end(); inter_it++) {
					TaskID his_task_id = inter_it->first;
					
					if (find(my_ep_tasks.begin(), my_ep_tasks.end(), his_task_id) != my_ep_tasks.end() ) {
						// Add blocking from equal priority tasks
						unsigned int his_req_num = inter_it->second.request_num;
						unsigned int his_proc_num = inter_it->second.proc_num;
						unsigned long his_cs_len = inter_it->second.cs_len;

						tmp_blocking += min(his_proc_num*i, his_req_num)*his_cs_len;
					} else if (find(my_hp_tasks.begin(), my_hp_tasks.end(), his_task_id) != my_hp_tasks.end() ) {
						// Add blocking from higher priority tasks
						unsigned int his_req_num = inter_it->second.request_num;
						unsigned long his_cs_len = inter_it->second.cs_len;
						unsigned long dpr_for_curr_res = dpr[res_id];
						unsigned int req_inside_dpr = njobs(tset[his_task_id], dpr_for_curr_res)*tset[his_task_id]->my_resources_[res_id]->request_num;
						tmp_blocking += min(req_inside_dpr*i, his_req_num) * his_cs_len;
					}
				}

			} else {
				cerr << "WRONG PRIORITY LOCK TYPE !!!" << endl;
				exit(-1);
			}

			blks.push_back(tmp_blocking);
		}

		my_blocking += *max_element(blks.begin(), blks.end());
	}

	return my_blocking;
}

// Using numerical method instead of optimization to bound blocking on a single processor of a task
void BlockingAnalysis::critical_path_blocking(Task *task, TaskSet *taskset, unsigned int m, unsigned long *blocking, map<ResourceID, map<TaskID, CSData>>& x_data, map<ResourceID, unsigned long>& dpr, SpinlockType lock_type) {
	/*
	TaskID my_id = task->task_id_;
	double r_i = task->response_time_;
	map<ResourceID, vector<TaskID>*> &interferences = task->interferences_;
	map<ResourceID, vector<TaskID>*>::iterator rit = interferences.begin();
	map<TaskID, Task*> &tset = taskset->tasks_;

	// Gather information from requests of tau_x which 
	// interfere with my requests
	map<ResourceID, map<TaskID, CSData> > x_data;
	for (; rit != interferences.end(); rit++) {
		ResourceID r_id = rit->first;
		vector<TaskID>* vec = rit->second;
		for (unsigned int i=0; i<vec->size(); i++) {
			TaskID t_id = vec->at(i);
			// Abort if this is me, but this is impossible
			if (t_id == my_id)
				continue;
			Task* tau_x = tset[t_id];
			unsigned int proc_num = tau_x->proc_num_;
			map<ResourceID, Resource*> &tau_x_resources = tau_x->my_resources_;
			unsigned int n_x_q = tau_x_resources[r_id]->request_num;
			unsigned long cs_len = tau_x_resources[r_id]->critical_section_len;
			unsigned int request_num = njobs(tau_x, (unsigned long)r_i)*n_x_q;
			map<TaskID, CSData> &inner_map = x_data[r_id];
			CSData data = {proc_num, request_num, cs_len};
			inner_map.insert(std::pair<TaskID, CSData> (t_id, data));
		}
	}
	*/

	if (lock_type == FIFO) {
		// Maximum blocking for FIFO locks
		*blocking = critical_path_blocking_fifo(task, x_data);
	} else if (lock_type == PRIO_UNORDERED || lock_type == PRIO_FIFO) {
		/*
		// Calculate delay-per-request if lock type is priority-ordered
		map<ResourceID, unsigned long> dpr;
		map<ResourceID, Resource*> &my_resources = task->my_resources_;
		for (map<ResourceID, Resource*>::iterator it = my_resources.begin(); it != my_resources.end(); it++) {
			ResourceID res_id = it->first;
		    unsigned long delay = delay_per_request(task, taskset, res_id, lock_type);
			dpr[res_id] = delay;
		}
		*/

		// Maximum blocking for Priority locks
		*blocking = critical_path_blocking_prio(task, taskset, x_data, dpr, lock_type);
	}
}


// Max blocking for a whole job with FIFO locks
unsigned long BlockingAnalysis::work_blocking_fifo(Task *task, map<ResourceID, map<TaskID, CSData> > &x_data) {

	unsigned int my_proc_num = task->proc_num_;
	
	unsigned long blocking = 0;
	map<ResourceID, map<TaskID, CSData> >::iterator it = x_data.begin();
	for (; it != x_data.end(); it++) {
		ResourceID res_id = it->first;
		map<TaskID, CSData> &interfere = it->second;
		unsigned int my_req_num = task->my_resources_[res_id]->request_num;
		unsigned long my_cs_len = task->my_resources_[res_id]->critical_section_len;
		
		// Add up self blocking
		if (my_req_num < my_proc_num) {
			blocking += (my_req_num*(my_req_num-1)/2) * my_cs_len;
		} else {
			blocking += ((my_proc_num-1)*(my_req_num-my_proc_num) + my_proc_num*(my_proc_num-1)/2) * my_cs_len;
		}

		// Add up blocking from other tasks
		for (map<TaskID, CSData>::iterator inter_it=interfere.begin(); inter_it!=interfere.end(); inter_it++) {
			//TaskID his_task_id = inter_it->first;
			unsigned int his_req_num = inter_it->second.request_num;
			unsigned long his_cs_len = inter_it->second.cs_len;
			unsigned int his_proc_num = inter_it->second.proc_num;

			//LJ
			//unsigned int his_proc_num = min(inter_it->second.procNum,tset[his_task_id]->myResources[res_id]->requestNum);
			blocking += min(my_proc_num*his_req_num, his_proc_num*my_req_num) * his_cs_len;
		}
	}

	return blocking;
}

// Max blocking for the whole job with Priority lock
unsigned long BlockingAnalysis::work_blocking_prio(Task *task, TaskSet *taskset, 
												   map<ResourceID, map<TaskID, CSData> > &x_data, map<ResourceID, unsigned long> &dpr) {
	unsigned int my_proc_num = task->proc_num_;

	map<TaskID, Task*> &tset = taskset->tasks_;
	//	vector<TaskID> &my_lp_tasks = taskset->lower_prio_tasks_[task->task_id_];
	vector<TaskID> &my_ep_tasks = taskset->equal_prio_tasks_[task->task_id_];
	vector<TaskID> &my_hp_tasks = taskset->higher_prio_tasks_[task->task_id_];

	unsigned long blocking = 0;
	map<ResourceID, map<TaskID, CSData> >::iterator it = x_data.begin();
	for (; it != x_data.end(); it++) {
		ResourceID res_id = it->first;
		map<TaskID, CSData> &interfere = it->second;
		unsigned int my_req_num = task->my_resources_[res_id]->request_num;
		unsigned long my_cs_len = task->my_resources_[res_id]->critical_section_len;

		// Add up self blocking
		if (my_req_num < my_proc_num) {
			blocking += (my_req_num*(my_req_num-1)/2) * my_cs_len;
		} else {
			blocking += ((my_proc_num-1)*(my_req_num-my_proc_num) + my_proc_num*(my_proc_num-1)/2) * my_cs_len;
		}

		//copy from single prio for other tasks, only FIFO tie break
		// Sort interference tasks' data by cs length (decreasing order)
		vector<CSData> sorted_by_cslen;
		sort_by_cslen(task, taskset, interfere, sorted_by_cslen);

		// Add up blocking from lower locking-priority tasks
		unsigned int remain_req = my_req_num;
		for (unsigned int i=0; i<sorted_by_cslen.size(); i++) {
			if (sorted_by_cslen[i].request_num >= remain_req) {
				blocking += remain_req * sorted_by_cslen[i].cs_len;
				break;
			} else {
				blocking += sorted_by_cslen[i].request_num * sorted_by_cslen[i].cs_len;
				remain_req -= sorted_by_cslen[i].request_num;
			}
		}

		map<TaskID, CSData>::iterator inter_it = interfere.begin();
		for (; inter_it != interfere.end(); inter_it++) {
			TaskID his_task_id = inter_it->first;

			//equal priority
			if (find(my_ep_tasks.begin(), my_ep_tasks.end(), his_task_id) != my_ep_tasks.end() ) {
				// Add blocking from equal priority tasks
				unsigned int his_req_num = inter_it->second.request_num;
				unsigned int his_proc_num = inter_it->second.proc_num;
				unsigned long his_cs_len = inter_it->second.cs_len;
				blocking += min(his_proc_num*my_req_num, his_req_num*my_proc_num)*his_cs_len;
				cout <<"need to fix min(his_proc_num, per_job_request)"<<endl;
			//high priority
			} else if (find(my_hp_tasks.begin(), my_hp_tasks.end(), his_task_id) != my_hp_tasks.end() ) {
				// Add blocking from higher priority tasks
				unsigned int his_req_num = inter_it->second.request_num;
				unsigned long his_cs_len = inter_it->second.cs_len;
				unsigned long dpr_for_curr_res = dpr[res_id];
				unsigned int req_inside_dpr = njobs(tset[his_task_id], dpr_for_curr_res)*tset[his_task_id]->my_resources_[res_id]->request_num;
				blocking += min(req_inside_dpr*my_req_num, his_req_num*my_proc_num) * his_cs_len;

				//LJ
				//cout<<"\t"<< inter_it->second.procNum << " " << req_inside_dpr <<" "<< tset[his_task_id]->myResources[res_id]->requestNum <<endl;
			}
		}
	}

	return blocking;
}


// Blocking bound of a whole job
void BlockingAnalysis::work_blocking(Task *task, TaskSet *taskset, unsigned int m, unsigned long *blocking, map<ResourceID, map<TaskID, CSData>>& x_data, map<ResourceID, unsigned long>& dpr, SpinlockType lock_type) {
	/*
	TaskID my_id = task->task_id_;
	double r_i = task->response_time_;
	map<ResourceID, vector<TaskID>*> &interferences = task->interferences_;
	map<ResourceID, vector<TaskID>*>::iterator rit = interferences.begin();
	map<TaskID, Task*> &tset = taskset->tasks_;

	// Gather information from requests of tau_x which 
	// interfere with my requests
	map<ResourceID, map<TaskID, CSData> > x_data;
	for (; rit != interferences.end(); rit++) {
		ResourceID r_id = rit->first;
		vector<TaskID>* vec = rit->second;
		for (unsigned int i=0; i<vec->size(); i++) {
			TaskID t_id = vec->at(i);
			// Abort if this is me, but this is impossible
			if (t_id == my_id)
				continue;
			Task* tau_x = tset[t_id];
			unsigned int proc_num = tau_x->proc_num_;
			map<ResourceID, Resource*> &tau_x_resources = tau_x->my_resources_;
			unsigned int n_x_q = tau_x_resources[r_id]->request_num;
			unsigned long cs_len = tau_x_resources[r_id]->critical_section_len;
			unsigned int request_num = njobs(tau_x, (unsigned long)r_i)*n_x_q;
			map<TaskID, CSData> &inner_map = x_data[r_id];
			CSData data = {proc_num, request_num, cs_len};
			inner_map.insert(std::pair<TaskID, CSData> (t_id, data));
		}
	}
	*/

	if (lock_type == FIFO) {
		*blocking = work_blocking_fifo(task, x_data);
	} else if (lock_type == PRIO_UNORDERED) {
		// Not supported yet
	} else if (lock_type == PRIO_FIFO) {
		/*
		map<ResourceID, unsigned long> dpr;
		map<ResourceID, Resource*> &my_resources = task->my_resources_;
		for (map<ResourceID, Resource*>::iterator it = my_resources.begin(); it != my_resources.end(); it++) {
			ResourceID res_id = it->first;
		    unsigned long delay = delay_per_request(task, taskset, res_id, lock_type);
			dpr[res_id] = delay;
		}
		*/

		//LJ
		*blocking = work_blocking_prio(task, taskset, x_data, dpr);
	}
}


// Gather the information of interfering tasks for each resource 
// of the input task
map<ResourceID, map<TaskID, CSData>> BlockingAnalysis::calc_interfere(Task *task, TaskSet *taskset) {
	TaskID my_id = task->task_id_;
	double r_i = task->response_time_;
	map<ResourceID, vector<TaskID>*> &interferences = task->interferences_;
	map<ResourceID, vector<TaskID>*>::iterator rit = interferences.begin();
	map<TaskID, Task*> &tset = taskset->tasks_;

	// Gather information from requests of tau_x which 
	// interfere with my requests
	map<ResourceID, map<TaskID, CSData> > x_data;
	for (; rit != interferences.end(); rit++) {
		ResourceID r_id = rit->first;
		vector<TaskID>* vec = rit->second;
		for (unsigned int i=0; i<vec->size(); i++) {
			TaskID t_id = vec->at(i);
			// Abort if this is me, but this is impossible
			if (t_id == my_id)
				continue;
			Task* tau_x = tset[t_id];
			unsigned int proc_num = tau_x->proc_num_;
			map<ResourceID, Resource*> &tau_x_resources = tau_x->my_resources_;
			unsigned int n_x_q = tau_x_resources[r_id]->request_num;
			unsigned long cs_len = tau_x_resources[r_id]->critical_section_len;
			unsigned int request_num = njobs(tau_x, (unsigned long)r_i)*n_x_q;
			map<TaskID, CSData> &inner_map = x_data[r_id];
			CSData data = {proc_num, request_num, cs_len};
			inner_map.insert(std::pair<TaskID, CSData> (t_id, data));
		}
	}
	
	return x_data;
}

// Calculate the delay_per_request quantities for a task
// (w.r.t its resources)
bool BlockingAnalysis::calc_dpr_task(Task *task, TaskSet *taskset, SpinlockType lock_type, map<ResourceID, unsigned long>& dpr) {
	//	map<ResourceID, unsigned long> dpr;
	map<ResourceID, Resource*> &my_resources = task->my_resources_;
	for (map<ResourceID, Resource*>::iterator it = my_resources.begin(); it != my_resources.end(); it++) {
		ResourceID res_id = it->first;
		//		unsigned long delay = delay_per_request(task, taskset, res_id, lock_type);
		unsigned long delay;
		bool ret = delay_per_request(task, taskset, res_id, lock_type, &delay);

		// If delay_per_request is larger than the task's relative deadline, return unschedulable
		if (ret == false)
			return false;

		dpr[res_id] = delay;
	}
	
	return true;
	//	return dpr;
}

// Schedulability test using the following procedure:
// In each iteration, update blocking bound, then update number of processors 
// allocated for tasks. For each task, if the new number of processors allocated 
// to it decreases, keep it as the previous iteration. Otherwise, if it increases,
// update it. The test terminates when the total number of processors > m (unschedulable)
// OR processor allocations for all tasks do not change (schedulable)
// For now, we consider FIFO and PRIO_FIFO spin locks
/*
bool BlockingAnalysis::is_schedulable(TaskSet *tset, unsigned int m, SpinlockType lock_type) {
	map<TaskID, Task*> &taskset = tset->tasks_;
	map<TaskID, Task*>::iterator it = taskset.begin();

	unsigned total_assigned_cores = 0;
	for (; it != taskset.end(); it++) {
		total_assigned_cores += it->second->proc_num_;
	}

	if (total_assigned_cores > m) {
		tset->schedulability_status = 2;
		return false;
	}

	// For all tasks, set their response times to relative deadline
	for (it = taskset.begin(); it != taskset.end(); it++) {
		it->second->response_time_ = it->second->period_;
	}

	// Copy values for new_proc_num_ fields
	for (it = taskset.begin(); it != taskset.end(); it++) {
		it->second->new_proc_num_ = it->second->proc_num_;
	}

	while (true) {
		unsigned int total_proc_num = 0;
		unsigned long blocking;
		
		// Copy the new number of cores for each task
		for (it = taskset.begin(); it != taskset.end(); it++) {
			if (it->second->converged_ == false)
				it->second->proc_num_ = it->second->new_proc_num_;
		}
		
		// Calculate new processors allocations for tasks
		for (it = taskset.begin(); it != taskset.end(); it++) {
			Task * task = it->second;

			// Blocking bound on a single processor using numerical method
			critical_path_blocking(task, tset, m, &blocking, lock_type);
			
			// If D < L + B1, this task is deemed to be unschedulable
			if (task->period_ < task->span_ + blocking) {
				//cout << "Task " << task->task_id_ << " blocking: " << blocking << " => more than span" << endl;
				tset->schedulability_status = 1;
				return false;
			}

			// Calculate blocking bound for the whole job
			unsigned long blocking_whole_job;
			if (lock_type == FIFO || lock_type == PRIO_FIFO) {
				work_blocking(task, tset, m, &blocking_whole_job, lock_type);
			} else if (lock_type == PRIO_UNORDERED) {
				blocking_whole_job = task->proc_num_ * blocking;
			}

			//cout << "Task " << task->task_id_ << " blockings: <" << blocking << ", " << blocking_whole_job << ">" << endl;
			
			unsigned int proc_num = alloc_proc(task->wcet_, task->span_, task->period_, blocking_whole_job, blocking);
			
			// If the new processors allocated is larger than the old one, update it,
			// otherwise, do not update it
			if (proc_num > task->proc_num_) {
				task->new_proc_num_ = proc_num;
				task->converged_ = false;
			} else {
				task->converged_ = true;
			}
			
			// Update task's blocking bound, total number of processors allocated so far
			task->blocking_on_single_ = blocking;
			task->blocking_whole_job_ = blocking_whole_job;
			total_proc_num += task->new_proc_num_;
		}

		// If total processors used is larger than m, the taskset is unschedulable
		if (total_proc_num > m) {
			//cout << "Total core required: " << total_proc_num << " => more than m" << endl;
			tset->schedulability_status = 1;
			return false;
		}

		// If processor allocations for all tasks do not change from the previous iteration
		// then the taskset is considered as converged
		bool global_converged = true;
		for (it = taskset.begin(); it != taskset.end(); it++) {
			global_converged &= it->second->converged_;
		}

		if (global_converged == true) {
			tset->schedulability_status = 0;
			return true;
		}
	}
	
}
*/

bool BlockingAnalysis::is_schedulable(TaskSet *tset, unsigned int m, SpinlockType lock_type) {
	map<TaskID, Task*> &taskset = tset->tasks_;
	map<TaskID, Task*>::iterator it = taskset.begin();

	unsigned total_assigned_cores = 0;
	for (; it != taskset.end(); it++) {
		total_assigned_cores += it->second->proc_num_;
	}

	if (total_assigned_cores > m) {
		tset->schedulability_status = 2;
		return false;
	}

	// Only support FIFO-ordered and priority-ordered with FIFO tiebreaking spin locks now
	if (lock_type == FIFO) {
		return is_schedulable_fifo(tset, m);
	} else if (lock_type == PRIO_FIFO) {
		if (prio_assign == "dm") {
			// Compute the schedulability with the locking-priorities assigned in Deadline Monotonic manner
			// (the locking-priorities of the tasks are set this way when the task set is read from file)
			return is_schedulable_common(tset, m, PRIO_FIFO);
		} else if (prio_assign == "opt") {
			// Compute the schedulability with the optimal locking-priority assignment
			return is_schedulable_prio(tset, m);
		} else if (prio_assign == "sim") {
			// Compute the schedulability for priority-ordered locks with simulated annealing
			return is_schedulable_prio_anneal(tset, m);
		} else {
			cerr << "ERROR: Wrong type of locking-priority assignment!" << endl;
			return false;
		}
	} else {
		cout << "Only support FIFO and PRIO_FIFO spin locks now !!!" << endl;
		return false;
	}	
}


// This is a common schedulability test method for a task set with a given lock type,
// and a priority assignment for its tasks (in case of priority-ordered locks)
// It assumes that the caller has already checked that there exists a partition for 
// the task set supposing there is no blocking
bool BlockingAnalysis::is_schedulable_common(TaskSet *tset, unsigned int m, SpinlockType lock_type) {
	map<TaskID, Task*> &taskset = tset->tasks_;
	map<TaskID, Task*>::iterator it = taskset.begin();

	// For all tasks, set their response times to relative deadline
	for (it = taskset.begin(); it != taskset.end(); it++) {
		it->second->response_time_ = it->second->period_;
	}

	// Copy values for new_proc_num_ fields
	for (it = taskset.begin(); it != taskset.end(); it++) {
		it->second->new_proc_num_ = it->second->proc_num_;
	}

	while (true) {
		unsigned int total_proc_num = 0;
		unsigned long blocking;
		
		// Copy the new number of cores for each task
		for (it = taskset.begin(); it != taskset.end(); it++) {
			if (it->second->converged_ == false)
				it->second->proc_num_ = it->second->new_proc_num_;
		}
		
		// Calculate new processors allocations for tasks
		for (it = taskset.begin(); it != taskset.end(); it++) {
			Task * task = it->second;

			// Calculate the interference data
			map<ResourceID, map<TaskID, CSData>> x_data = calc_interfere(task, tset);

			// Calculate delay-per-request quantities if it is priority-ordered spin locks
			map<ResourceID, unsigned long> dpr;
			if (lock_type == PRIO_FIFO) {
				bool ret = calc_dpr_task(task, tset, lock_type, dpr);
				
				// If for any resource, delay-per-request is larger than D_i,
				// we return the task set is unschedulable
				if (ret == false) {
					tset->schedulability_status = 1;
					return false;
				}
			}

			// Blocking bound on a single processor using numerical method
			critical_path_blocking(task, tset, m, &blocking, x_data, dpr, lock_type);
			
			// If D < L + B1, this task is deemed to be unschedulable
			if (task->period_ < task->span_ + blocking) {
				//cout << "Task " << task->task_id_ << " blocking: " << blocking << " => more than span" << endl;
				tset->schedulability_status = 1;
				return false;
			}

			// Calculate blocking bound for the whole job
			unsigned long blocking_whole_job;
			work_blocking(task, tset, m, &blocking_whole_job, x_data, dpr, lock_type);

			//cout << "Task " << task->task_id_ << " blockings: <" << blocking << ", " << blocking_whole_job << ">" << endl;
			
			unsigned int proc_num = alloc_proc(task->wcet_, task->span_, task->period_, blocking_whole_job, blocking);
			
			// If the new processors allocated is larger than the old one, update it,
			// otherwise, do not update it
			if (proc_num > task->proc_num_) {
				task->new_proc_num_ = proc_num;
				task->converged_ = false;
			} else {
				task->converged_ = true;
			}
			
			// Update task's blocking bound, total number of processors allocated so far
			task->blocking_on_single_ = blocking;
			task->blocking_whole_job_ = blocking_whole_job;
			total_proc_num += task->new_proc_num_;
		}

		// If total processors used is larger than m, the taskset is unschedulable
		if (total_proc_num > m) {
			//cout << "Total core required: " << total_proc_num << " => more than m" << endl;
			tset->schedulability_status = 1;
			return false;
		}

		// If processor allocations for all tasks do not change from the previous iteration
		// then the taskset is considered as converged
		bool global_converged = true;
		for (it = taskset.begin(); it != taskset.end(); it++) {
			global_converged &= it->second->converged_;
		}

		if (global_converged == true) {
			tset->schedulability_status = 0;
			return true;
		}
	}
}

bool BlockingAnalysis::is_schedulable_fifo(TaskSet *tset, unsigned int m) {
	return is_schedulable_common(tset, m, FIFO);
}


// Reset the information for each task of the task set and 
// the list of higher, equal, lower priority tasks before 
// performing the schedulability test for priority-ordered locks.
// The inputs include the task set and the list of priorities for its tasks
void BlockingAnalysis::reset_taskset(TaskSet *tset, vector<unsigned int>& priorities) {
	map<TaskID, Task*>& taskset = tset->tasks_;
	map<TaskID, Task*>::iterator it = taskset.begin();

	// Reset the priority assignments
	unsigned int index = 0;
	for (; it!=taskset.end(); it++) {
		it->second->priority_ = priorities[index];
		index++;
	}

	vector<Task*> tasks_array;
	for (it=taskset.begin(); it!=taskset.end(); it++) {
		tasks_array.push_back(it->second);
	}

	// Reset the list of higher, lower, and equal priority tasks
	tset->higher_prio_tasks_.clear();
	tset->lower_prio_tasks_.clear();
	tset->equal_prio_tasks_.clear();
	for (it=taskset.begin(); it!=taskset.end(); it++) {
		TaskID task_id = it->first;
		vector<TaskID> &hp_tasks = tset->higher_prio_tasks_[task_id];
		vector<TaskID> &lp_tasks = tset->lower_prio_tasks_[task_id];
		vector<TaskID> &ep_tasks = tset->equal_prio_tasks_[task_id];

		for (unsigned int i=0; i<tasks_array.size(); i++) {
			if (tasks_array[i]->task_id_ == task_id)
				continue;

			if (tasks_array[i]->priority_ < it->second->priority_) {
				hp_tasks.push_back(tasks_array[i]->task_id_);
			} else if(tasks_array[i]->priority_ > it->second->priority_) {
				lp_tasks.push_back(tasks_array[i]->task_id_);
			} else {
				ep_tasks.push_back(tasks_array[i]->task_id_);
			}
		}
	}

	// Reset the information for each task in the task set
	for (it=taskset.begin(); it!=taskset.end(); it++) {
		it->second->blocking_on_single_ = 0;
		it->second->blocking_whole_job_ = 0;
		it->second->proc_num_ = alloc_proc(it->second->wcet_, 
										   it->second->span_, 
										   it->second->period_,
										   0, 0);
		it->second->response_time_ = response_time(it->second->wcet_,
												   it->second->span_,
												   it->second->proc_num_, 
												   0, 0);
		it->second->converged_ = false;
	}
	
}

bool BlockingAnalysis::is_schedulable_prio(TaskSet *tset, unsigned int m) {
	cout << "Go here" << endl;

	map<TaskID, Task*> &taskset = tset->tasks_;	
	
	// Priorities start from 1 (highest)
	vector<unsigned int> priorities;
	for (unsigned int i=0; i<taskset.size(); i++) {
		priorities.push_back(i+1);
	}

	// Important: must sort the list of priorities otherwise we may
	// miss some permutations of priority assignment. This is because 
	// the next_permutation() returns the next lexicographically greater 
	// permutation of the input set.
	sort(priorities.begin(), priorities.end());

	bool schedulable = false;
	
	int count = 0;
	do {
		count++;

		// Need to reset the data of the tasks and recalculate the 
		// list of higher, lower, equal priority tasks for each permutation
		reset_taskset(tset, priorities);
		schedulable = is_schedulable_common(tset, m, PRIO_FIFO);
		if (schedulable == true)
			break;

	} while(next_permutation(priorities.begin(), priorities.end()));

	cout << "Number of permutation: " << count << endl;
	
	return schedulable;
}


// Apply simulated annealing to find a good locking-priority 
// assignment for priority-ordered locks. The cost function 
// is defined to be the total number of cores required 
// by a locking-priority assignment (i.e., smaller is better). 
// The algorithm keep searching until it finds an assignment 
// that makes the task set schedulable, or after it finishes 
// all iterations. The initial solution is chosen to be the 
// one using deadline monotonic fashion.
// Note that the task set after saved by saved_taskset already 
// had locking-priorities assigned based on relative deadlines.
bool BlockingAnalysis::is_schedulable_prio_anneal(TaskSet *tset, unsigned int m) {
	double temperature = 1.0;
	double temperature_min = 0.00001;
	double alpha = 0.9;
	
	// Calculate the number of cores required when 
	// assign locking-priority using relative deadlines
	unsigned int old_cost = annealing_cost(tset, m);

	// If the deadline-based locking-priority makes the 
	// task set schedulable, return right away.
	if (old_cost <= m) {
		tset->schedulability_status = 0;
		return true;
	}

	// Get the initial locking-priority assignment, 
	// which is based on relative deadlines.
	vector<unsigned int> old_solution;
	map<TaskID, Task*> tasks = tset->tasks_;
	unsigned int num_tasks = tasks.size();
	for (unsigned int i=1; i <= num_tasks; i++) {
		unsigned int prio = tasks[i]->priority_;
		old_solution.push_back(prio);
	}

	while (temperature > temperature_min) {
		int i = 1;
		while (i <= 100) {
			// Get a "neighbor" priority assignment
			vector<unsigned int> neighbor_solution = neighbor(old_solution);

			// Reset task set with the new priority assignment
			reset_taskset(tset, neighbor_solution);

			// Get the cost of the neighbor (i.e., its number of cores)
			unsigned int neighbor_cost = annealing_cost(tset, m);

			// Check if the neighbor solution is good enough to make 
			// the task set schedulable. If so, we return.
			if (neighbor_cost <= m) {
				tset->schedulability_status = 0;
				return true;
			}

			double prob = acceptance_probability(old_cost, neighbor_cost, temperature);
			if (prob > random()) {
				// Assign solution to the new solution
				old_solution = neighbor_solution;
				old_cost = neighbor_cost;
			}
			i++;
		}
		temperature = temperature * alpha;
	}

	tset->schedulability_status = 1;
	return false;
}

// The cost function for simulated annealing is the number of cores 
// required by the input locking-priority assignment. Note that 
// it runs until it gets a converged value for the number of cores,
// even if that value is larger than m. This allows to compare 
// different solutions (locking-priority assignment).
unsigned int BlockingAnalysis::annealing_cost(TaskSet *tset, unsigned int m) {
	map<TaskID, Task*> &taskset = tset->tasks_;
	map<TaskID, Task*>::iterator it = taskset.begin();

	// For all tasks, set their response times to relative deadline
	for (it = taskset.begin(); it != taskset.end(); it++) {
		it->second->response_time_ = it->second->period_;
	}

	// Copy values for new_proc_num_ fields
	for (it = taskset.begin(); it != taskset.end(); it++) {
		it->second->new_proc_num_ = it->second->proc_num_;
	}

	while (true) {
		unsigned int total_proc_num = 0;
		unsigned long blocking;
		
		// Copy the new number of cores for each task
		for (it = taskset.begin(); it != taskset.end(); it++) {
			if (it->second->converged_ == false)
				it->second->proc_num_ = it->second->new_proc_num_;
		}
		
		// Calculate new processors allocations for tasks
		for (it = taskset.begin(); it != taskset.end(); it++) {
			Task * task = it->second;

			// Calculate the interference data
			map<ResourceID, map<TaskID, CSData>> x_data = calc_interfere(task, tset);

			// Calculate delay-per-request quantities if it is priority-ordered spin locks
			map<ResourceID, unsigned long> dpr;
			//if (lock_type == PRIO_FIFO) {
			bool ret = calc_dpr_task(task, tset, PRIO_FIFO, dpr);
				
			// If for any resource, delay-per-request is larger than D_i,
			// we return the task set is unschedulable
			if (ret == false)
				return UINT_MAX;
			//}

			// Blocking bound on a single processor using numerical method
			critical_path_blocking(task, tset, m, &blocking, x_data, dpr, PRIO_FIFO);
			
			// If D < L + B1, this task is deemed to be unschedulable
			if (task->period_ < task->span_ + blocking) {
				//cout << "Task " << task->task_id_ << " blocking: " << blocking << " => more than span" << endl;
				//tset->schedulability_status = 1;
				return UINT_MAX;
			}

			// Calculate blocking bound for the whole job
			unsigned long blocking_whole_job;
			work_blocking(task, tset, m, &blocking_whole_job, x_data, dpr, PRIO_FIFO);

			//cout << "Task " << task->task_id_ << " blockings: <" << blocking << ", " << blocking_whole_job << ">" << endl;
			
			unsigned int proc_num = alloc_proc(task->wcet_, task->span_, task->period_, blocking_whole_job, blocking);
			
			// If the new processors allocated is larger than the old one, update it,
			// otherwise, do not update it
			if (proc_num > task->proc_num_) {
				task->new_proc_num_ = proc_num;
				task->converged_ = false;
			} else {
				task->converged_ = true;
			}
			
			// Update task's blocking bound, total number of processors allocated so far
			task->blocking_on_single_ = blocking;
			task->blocking_whole_job_ = blocking_whole_job;
			total_proc_num += task->new_proc_num_;
		}

		/*
		// If total processors used is larger than m, the taskset is unschedulable
		if (total_proc_num > m) {
			//cout << "Total core required: " << total_proc_num << " => more than m" << endl;
			tset->schedulability_status = 1;
			return false;
		}
		*/

		// If processor allocations for all tasks do not change from the previous iteration
		// then the taskset is considered as converged
		bool global_converged = true;
		for (it = taskset.begin(); it != taskset.end(); it++) {
			global_converged &= it->second->converged_;
		}

		if (global_converged == true) {
			//tset->schedulability_status = 0;
			//return true;
			return total_proc_num;
		}
	}
}

// Get a neighbor of the current priorities assignment.
// It simply swaps 2 consecutive priority values.
vector<unsigned int> BlockingAnalysis::neighbor(vector<unsigned int> old_priorities) {
	unsigned int size = old_priorities.size();
	srand(time(NULL));
	unsigned int position = rand() % size;

	// If it is the last element, swap it with the first element
	if (position == (size-1)) {
		unsigned int tmp = old_priorities[0];
		old_priorities[0] = old_priorities[position];
		old_priorities[position] = tmp;
	} else {
		// Otherwise, swap it with the next element
		unsigned int tmp = old_priorities[position];
		old_priorities[position] = old_priorities[position+1];
		old_priorities[position+1] = tmp;
	}

	return old_priorities;
}


// Implementation of the acceptance probability function
double BlockingAnalysis::acceptance_probability(unsigned int old_cost, unsigned int neighbor_cost, double temperature) {
	double power = (old_cost - neighbor_cost)/temperature;
	double a = exp(power);

	if (a > 1.0)
		return 1.0;
	else 
		return a;
}


// Return number of tasks tau_x in interval length t
unsigned int BlockingAnalysis::njobs(Task* tau_x, unsigned long t) {
	unsigned long tau_x_period = tau_x->period_;
	unsigned long tau_x_response_time = tau_x->response_time_;	
	return ceil( (t + tau_x_response_time) / tau_x_period);
}

#if 0
// This is the formula Jing suggested
unsigned int BlockingAnalysis::njobs(Task *tau_x, unsigned long t) {
	return (floor(t/tau_x->period_) + 1);
}
#endif
