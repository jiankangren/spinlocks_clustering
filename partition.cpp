// This file reads an input .rtpt file, performs a schedulability test
// for the task set defined in the file. Then it writes the partition 
// to the output .rtps file (for each task, the partition determines 
// a set of cores it is assigned to).
// NOTE: that this code only works with task sets of synthetic_tasks.

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <cassert>
#include "saved_taskset.h"
#include "blocking_analysis.h"

using namespace std;

const unsigned long kNsecInSec = 1000000000;

int main(int argc, char *argv[]) {
	if (argc != 2 && argc != 3) {
		cout << "Usage: " << argv[0] << " <path_to_rtpt_file> [lock_type=fifo]" << endl;
		return -1;
	}

	SpinlockType lock_type = FIFO;
	if (argc == 2) {
		// Default spin locks is FIFO-Ordered
		lock_type = FIFO;
	} else if (argc == 3) {
		string type(argv[2]);
		if (type == "fifo") {
			lock_type = FIFO;
		} else if (type == "prio") {
			lock_type = PRIO_FIFO;
		} else {
			cerr << "ERROR: Wrong lock type. Should be either \"fifo\" or \"prio\"." << endl;
			return -1;
		}
	}
	
	ifstream ifs(argv[1]);
	if (!ifs.is_open()) {
		cerr << "ERROR: Cannot open rtpt file" << endl;
		return -1;
	}

	// Calculate the number of tasks
    unsigned num_lines = 0;
	vector<string> lines;
	string line;
	while (getline(ifs, line)) { 
		num_lines += 1; 
		lines.push_back(line);
	}
	unsigned num_tasks = (num_lines-2)/2;
	if ( !(num_lines > 2 && (num_lines % 2) == 0) ) {
		cerr << "ERROR: Incorrect number of lines" << endl;
		return -1;
	}

	// Go to the beginning of the file
	ifs.clear();
	ifs.seekg(0, ios::beg);

	// Read the system core range
	string system_cores_line;
	if (!getline(ifs, system_cores_line)) {
		cerr << "ERROR: Cannot read the system cores line" << endl;
		return -1;
	}

	istringstream system_cores_stream(system_cores_line);
	unsigned system_first_core, system_last_core;
	if ( !(system_cores_stream >> system_first_core &&
		   system_cores_stream >> system_last_core) ) {
		cerr << "ERROR: Cannot read system core range" << endl;
		return -1;
	}
	
	// The total number of processors in the system (a.k.a m)
	unsigned num_cores = system_last_core - system_first_core + 1;

	// Read the number of shared resources
	string resources_num_line;
	if (!getline(ifs, resources_num_line)) {
		cerr << "ERROR: Cannot read the resources line" << endl;
		return -1;
	}

	unsigned resources_num;
	istringstream resources_num_stream(resources_num_line);
	if ( !(resources_num_stream >> resources_num) ) {
		cerr << "ERROR: Cannot read the number of resources" << endl;
		return -1;
	}

	
	// Save information for the task set to a TaskSet object
	TaskSet *taskset = new TaskSet();
	map<TaskID, Task*> &tasks = taskset->tasks_;

	string task_param_line, task_timing_line;
	for (unsigned i=1; i<=num_tasks; i++) {
		if ( getline(ifs, task_param_line) &&
			 getline(ifs, task_timing_line) ) {
			
			// Read each task's command line arguments
			istringstream task_param_stream(task_param_line);
			string program_name;
			unsigned num_segments;
			if ( !(task_param_stream >> program_name &&
				   task_param_stream >> num_segments) )  {
				cerr << "ERROR: Task command line improperly provided" << endl;
				return -1;
			}
			
			Task *task = new Task();
			map<ResourceID, Resource*> &resources = task->my_resources_;
			
			unsigned num_strands, len_sec;
			unsigned long len_ns, cslen_ns;
			unsigned num_requests, resource_id;
			for (unsigned j=0; j<num_segments; j++) {
				if ( !(task_param_stream >> num_strands &&
					   task_param_stream >> len_sec &&
					   task_param_stream >> len_ns &&
					   task_param_stream >> num_requests &&
					   task_param_stream >> resource_id &&
					   task_param_stream >> cslen_ns) ) {
					cerr << "ERROR: Task command line (strands) improperly provided" << endl;
					return -1;
				}

				// Add new resource or update existing resource information
				if (num_requests > 0) {
					map<ResourceID, Resource*>::iterator it = resources.find(resource_id);
					if (it != resources.end()) {
						it->second->request_num += num_requests;
						it->second->critical_section_len = cslen_ns;
					} else {
						Resource *resource = new Resource();
						resource->resource_id = resource_id;
						resource->critical_section_len = cslen_ns;
						resource->request_num = num_requests;
						resources.insert( std::pair<ResourceID, Resource*> (resource_id, resource) );
					}
				}

			}
			

			// Read each task's timing parameters
			istringstream task_timing_stream(task_timing_line);
			unsigned work_sec, span_sec, period_sec, deadline_sec, release_sec;
			unsigned long work_ns, span_ns, period_ns, deadline_ns, release_ns;
			unsigned num_iters;
			if ( !(task_timing_stream >> work_sec &&
				   task_timing_stream >> work_ns &&
				   task_timing_stream >> span_sec &&
				   task_timing_stream >> span_ns &&
				   task_timing_stream >> period_sec &&
				   task_timing_stream >> period_ns &&
				   task_timing_stream >> deadline_sec &&
				   task_timing_stream >> deadline_ns &&
				   task_timing_stream >> release_sec &&
				   task_timing_stream >> release_ns &&
				   task_timing_stream >> num_iters) ) {
				
				cerr << "ERROR: Task timing parameter improperly provided" << endl;
				return -1;
			}

			task->task_id_ = i;
			task->period_ = period_sec * kNsecInSec + period_ns;
			task->wcet_ = work_sec * kNsecInSec + work_ns;
			task->span_ = span_sec * kNsecInSec + span_ns;
			task->utilization_ = (double) task->wcet_/task->period_;

			// Insert task to the task set
			tasks.insert( std::pair<TaskID, Task*> (i, task) );

		} else {
			cerr << "ERROR: Cannot read task parameters" << endl;
			return -1;
		}
	}
	ifs.close();
	
	// Fill up every other data of the task set object & tasks' objects
	startup_taskset(taskset, num_cores);

	BlockingAnalysis::get_instance().is_schedulable(taskset, num_cores, lock_type);

	// Open .rtps file to write to
	string rtps_file_name = string(argv[1], 0, strlen(argv[1]) - 1);
	rtps_file_name += "s";
	std::ofstream ofs(rtps_file_name.c_str());
	if ( !ofs.is_open() ) {
		cerr << "ERROR: Cannot open rtps file to write" << endl;
		return -1;
	}

	const char *ltype;
	if (argc == 2) {
		ltype = "fifo";
	} else if (argc == 3) {
		ltype = argv[2];
	}

	// Write partition result
	ofs << taskset->schedulability_status << "\n";
	ofs << lines[0].c_str() << "\n";
	ofs << lines[1].c_str() << " " << ltype << "\n";

	// DEBUG
	//	for (unsigned i=0; i<lines.size(); i++) {
	//		cout << lines[i] << endl;
	//	}
	// END DEBUG


	unsigned current_first_core = system_first_core;
	for (unsigned i=0; i<num_tasks; i++) {
		// Write the task arguments
		ofs << lines[2+2*i].c_str() << "\n";

		// Write the task timing parameters
		ofs << lines[3+2*i].c_str() << "\n";

		unsigned current_last_core = current_first_core + tasks[i+1]->proc_num_ - 1;
		// Just for sure that all assigned cores are always in the range
		// This should never fire
		//assert (current_last_core <= system_last_core);
		unsigned priority = tasks[i+1]->priority_;

		// Write the task partition & priority
		ofs << current_first_core << " " << current_last_core << " " << priority << "\n";
		current_first_core = current_last_core + 1;
	}

	ofs.close();

	return 0;
}
