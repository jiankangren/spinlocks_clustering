#include <iostream>
#include <cmath>
#include <random>
#include <map>
#include <algorithm>
#include <cassert>
#include <errno.h>
#include <string.h>
#include "saved_taskset.h"
#include "blocking_analysis.h"

using namespace std;

extern int g_proc_num;
extern int g_resource_num;
extern int g_max_request_num;
extern string g_cs_type;


Task::Task() : my_resources_(), interferences_() {
	// Valid task id starts from 1
	task_id_ = 0;
	period_ = 0;
	span_ = 0;
	utilization_ = 0;
	wcet_ = 0;

	// Intermediate data while doing schedulability test
	blocking_on_single_ = 0;
	blocking_whole_job_ = 0;
	proc_num_ = 0;
	response_time_ = 0;
	new_proc_num_ = 0;
	converged_ = false;

	// Valid priority starts from 1
	priority_ = 0;
}

Task &Task::operator= (const Task& other) {
	if (this != &other) {
		task_id_ = other.task_id_;
		period_ = other.period_;
		span_ = other.span_;
		utilization_ = other.utilization_;
		wcet_ = other.wcet_;
		
		blocking_on_single_ = other.blocking_on_single_;
		blocking_whole_job_ = other.blocking_whole_job_;
		proc_num_ = other.proc_num_;
		response_time_ = other.response_time_;
		priority_ = other.priority_;
			
		map<ResourceID, Resource*>::const_iterator res_it;
		for (res_it = other.my_resources_.begin(); res_it != other.my_resources_.end(); res_it++) {
			Resource *my_res = new Resource();
			*my_res = *(res_it->second);
			my_resources_.insert( std::pair<ResourceID, Resource*>(res_it->first, my_res) );
		}
		
		map<ResourceID, vector<TaskID>*>::const_iterator inter_it;
		for (inter_it = other.interferences_.begin(); inter_it!=other.interferences_.end(); inter_it++) {
			vector<TaskID>* my_vec = new vector<TaskID>;
			*my_vec = *(inter_it->second);
			interferences_.insert( std::pair<ResourceID, vector<TaskID>*>(inter_it->first, my_vec) );
		}
	}
	
	return *this;
};


Task::~Task() {
	for (map<ResourceID, Resource*>::iterator it=my_resources_.begin(); it!=my_resources_.end(); it++) {
		delete it->second;
	}
	
	for (map<ResourceID, vector<TaskID>*>::iterator it=interferences_.begin(); it!=interferences_.end(); it++) {
		delete it->second;
	}
}


TaskSet::TaskSet() : schedulability_status(0), tasks_(), higher_prio_tasks_(),
					 lower_prio_tasks_(), equal_prio_tasks_() 
{}

// Copy assignment operator to clone a task set
TaskSet &TaskSet::operator= (const TaskSet &other) {
	if (this != &other) {
		// Copy list of tasks
		map<TaskID, Task*>::const_iterator task_it;
		for (task_it = other.tasks_.begin(); task_it!=other.tasks_.end(); task_it++) {
			Task *my_task = new Task();
			*my_task = *(task_it->second);
			tasks_.insert( std::pair<TaskID, Task*>(task_it->first, my_task) );
		}
		
			// Copy list of higher, lower, equal priority tasks
		higher_prio_tasks_ = other.higher_prio_tasks_;
		lower_prio_tasks_ = other.lower_prio_tasks_;
		equal_prio_tasks_ = other.equal_prio_tasks_;
	}
	
		return *this;
};

TaskSet::~TaskSet() {
	for (map<TaskID, Task*>::iterator it=tasks_.begin(); it!=tasks_.end(); it++) {
		delete it->second;
	}
}


// Sort tasks by increasing (implicit) relative deadline
bool compare_period(const Task* first, const Task* second) {
	return (first->period_ < second->period_);
}


// This method initializes priorities of tasks in the task set AND
// computes the set of higher, lower, equal priority tasks for each task
static void init_taskset(TaskSet *tset) {
	vector<Task*> tasks_array;
	for (map<TaskID, Task*>::iterator it=tset->tasks_.begin(); it!=tset->tasks_.end(); it++) {
		tasks_array.push_back(it->second);
	}

	sort(tasks_array.begin(), tasks_array.end(), compare_period);

	// Higher number means lower priority (1 is the highest)
	for (unsigned int i=0; i<tasks_array.size(); i++) {
		// As Kunal said, all tasks have distinct priorities
		tasks_array[i]->priority_ = i + 1;
	}

	for (map<TaskID, Task*>::iterator it=tset->tasks_.begin(); it!=tset->tasks_.end(); it++) {
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

}


// Initialize blocking time, #processors, response time 
// Also, update "interferences" for each task in task set
// Return: true, if taskset are OK after initiating
//         false, otherwise
// (OK means total processors allocated to tasks < m AND 
//  there is no task with Response time > Deadline)
static bool init_iteration(TaskSet* taskset, unsigned int m) {
	map<TaskID, Task*> &tset = taskset->tasks_;

	for (map<TaskID, Task*>::iterator it=tset.begin(); it != tset.end(); it++) {
		Task* task = it->second;
		task->blocking_on_single_ = 0;
		task->blocking_whole_job_ = 0;
		task->proc_num_ = BlockingAnalysis::get_instance().alloc_proc(task->wcet_, task->span_, task->period_, 0, 0);
		//cout << "Initial proc num: " << task->proc_num_ << endl;
		task->response_time_ = BlockingAnalysis::get_instance().response_time(task->wcet_, task->span_, task->proc_num_, 0, 0);
		task->converged_ = false;
		
		// Update interferences for this task
		map<ResourceID, vector<TaskID>*> &interferences = task->interferences_;
		map<ResourceID, Resource*> &my_resources = task->my_resources_;
		map<TaskID, Task*>::iterator task_iter;
		map<ResourceID, Resource*>::iterator res_iter;

		for (res_iter = my_resources.begin(); res_iter!=my_resources.end(); res_iter++) {
			ResourceID rid = res_iter->first;
			interferences.insert(std::pair<ResourceID, vector<TaskID>*> (rid, new vector<TaskID>()));
			for (task_iter = tset.begin(); task_iter!=tset.end(); task_iter++) {
				// Abort if this is me
				if (task_iter->second->task_id_ == task->task_id_)
					continue;

				// Add tasks access to the same resource
				if (task_iter->second->my_resources_.find(rid) != 
					task_iter->second->my_resources_.end()) {
					interferences.find(rid)->second->push_back(task_iter->first);
				}
			}
		}
	}

	return true;
}


void startup_taskset(TaskSet *taskset, unsigned int num_cores) {
	init_taskset(taskset);
	init_iteration(taskset, num_cores);
}
