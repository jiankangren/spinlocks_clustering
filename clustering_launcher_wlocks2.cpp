// Argument: the name of the taskset/schedule file:

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <vector>
#include <string>
#include <math.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include "single_use_barrier.h"
#include "spinlocks_util.h"
#include "statistics.h"

enum rt_gomp_clustering_launcher_error_codes
{ 
	RT_GOMP_CLUSTERING_LAUNCHER_SUCCESS,
	RT_GOMP_CLUSTERING_LAUNCHER_FILE_OPEN_ERROR,
	RT_GOMP_CLUSTERING_LAUNCHER_FILE_PARSE_ERROR,
	RT_GOMP_CLUSTERING_LAUNCHER_UNSCHEDULABLE_ERROR,
	RT_GOMP_CLUSTERING_LAUNCHER_FORK_EXECV_ERROR,
	RT_GOMP_CLUSTERING_LAUNCHER_BARRIER_INITIALIZATION_ERROR,
	RT_GOMP_CLUSTERING_LAUNCHER_ARGUMENT_ERROR,
	RT_GOMP_CLUSTERING_LAUNCHER_LOCKS_INITIALIZATION_ERROR,
	RT_GOMP_CLUSTERING_LAUNCHER_STAT_INITIALIZATION_ERROR
};

// Son (04Oct2015) Program usage:
// ./clustering_launcher_wlocks <full_path_to_rtps_file>
// The second argument is the path to the .rtps file, without ".rtps" trailer.
// For example: ./clustering_launcher_wlocks tasksets/taskset1_fifo
// will read file taskset1_fifo.rtps in folder tasksets, and store the output of the task set
// to folder tasksets/taskset1_output

int main(int argc, char *argv[])
{
	// Define the total number of timing parameters that should appear on the second line for each task
	const unsigned num_timing_params = 11;
	
	// Define the number of timing parameters to skip on the second line for each task
	const unsigned num_skipped_timing_params = 4;
	
	// Define the number of partition parameters that should appear on the third line for each task
	const unsigned num_partition_params = 3;
	
	// Define the name of the barrier used for synchronizing tasks after creation
	const std::string barrier_name = "RT_GOMP_CLUSTERING_BARRIER_2";

	// Name of the shared memory segment contains lock objects
	const std::string locks_name = "RT_GOMP_CLUSTERING_SPINLOCKS_2";

    // Name of the shared memory region contains statistics data
    const std::string stat_name = "RT_GOMP_CLUSTERING_STAT_2";
	
	// Verify the number of arguments
	if (argc != 2 && argc != 3 && argc != 4)
	{
		fprintf(stderr, "ERROR: The program must receive a single argument which is the taskset/schedule filename without any extension.");
		return RT_GOMP_CLUSTERING_LAUNCHER_ARGUMENT_ERROR;
	}
	
	// Determine the taskset (.rtpt) and schedule (.rtps) filenames from the program argument
	std::string taskset_filename(argv[1]);
	taskset_filename += ".rtpt";
	std::string schedule_filename(argv[1]);
	schedule_filename += ".rtps";

	// Son (04Oct2015):
	// In this implementation, it is guaranteed that the rtps always exists and updated.
	// Thus we can get rid of the part below (which calls python script to convert rtpt file)

	/*
	// Check for an up to date schedule (.rtps) file. If not, create one from a taskset (.rtpt) file.
	struct stat taskset_stat, schedule_stat;
	int taskset_ret_val = stat(taskset_filename.c_str(), &taskset_stat);
	int schedule_ret_val = stat(schedule_filename.c_str(), &schedule_stat);
	if (schedule_ret_val == -1 || (taskset_ret_val == 0 && taskset_stat.st_mtime > schedule_stat.st_mtime))
	{
		if (taskset_ret_val == -1)
		{
			fprintf(stderr, "ERROR: Cannot open taskset file: %s", taskset_filename.c_str());
			return RT_GOMP_CLUSTERING_LAUNCHER_FILE_OPEN_ERROR;
		}
		
		fprintf(stderr, "Scheduling taskset %s ...\n", argv[1]);
		
		// We will call a python scheduler script and pass the taskset filename without the extension
		std::vector<const char *> scheduler_script_argv;
		scheduler_script_argv.push_back("python");
		scheduler_script_argv.push_back("cluster.py");
		scheduler_script_argv.push_back(argv[1]);
		if (argc >= 3)
		{
		    scheduler_script_argv.push_back(argv[2]);
		}
		// NULL terminate the argument vector
		scheduler_script_argv.push_back(NULL);
		
		// Fork and execv the scheduler script
		pid_t pid = fork();
		if (pid == 0)
		{
			// Const cast is necessary for type compatibility. Since the strings are
			// not shared, there is no danger in removing the const modifier.
			execvp("python", const_cast<char **>(&scheduler_script_argv[0]));
			
			// Error if execv returns
			perror("Execv-ing scheduler script failed");
			return RT_GOMP_CLUSTERING_LAUNCHER_FORK_EXECV_ERROR;
		}
		else if (pid == -1)
		{
			perror("Forking a new process for scheduler script failed");
			return RT_GOMP_CLUSTERING_LAUNCHER_FORK_EXECV_ERROR;
		}
		
		// Wait until the child process has terminated
		while (!(wait(NULL) == -1 && errno == ECHILD));	
	}
	*/
	
	// Open the schedule (.rtps) file
	std::ifstream ifs(schedule_filename.c_str());
	if (!ifs.is_open())
	{
		fprintf(stderr, "ERROR: Cannot open schedule file");
		return RT_GOMP_CLUSTERING_LAUNCHER_FILE_OPEN_ERROR;
	}
	
	// Son (21Sep2015): adding one line to .rtps file:
	// the additional third line contains the number of shared resources

	// Count the number of tasks
	unsigned num_lines = 0;
	std::string line;
	while (getline(ifs, line)) { num_lines += 1; }
	unsigned num_tasks = (num_lines - 3) / 3;
	if (!(num_lines >= 3 && (num_lines - 3) % 3 == 0))
	{
		fprintf(stderr, "ERROR: Invalid number of lines in schedule file");
		return RT_GOMP_CLUSTERING_LAUNCHER_FILE_PARSE_ERROR;
	}
	
	// Seek back to the beginning of the file
	ifs.clear();
	ifs.seekg(0, std::ios::beg);
	
	// Check if the taskset is schedulable
	std::string schedulability_line;
	if (std::getline(ifs, schedulability_line))
	{
		unsigned schedulability;
		std::istringstream schedulability_stream(schedulability_line);
		if (schedulability_stream >> schedulability)
		{
			// Son (04Oct2015): don't need this part

			/*
		    // Output the schedulability to a machine readable file if specified
		    if (argc == 4)
		    {
		        std::string log_file(argv[3]);
	            log_file += "/schedulability.txt";
	            FILE * fp = fopen(log_file.c_str(), "w");
			    if (fp != NULL)
                {
                    fprintf(fp, "%d\n", schedulability);
                }
                else
                {
                    perror("Opening output file failed.");
                }
                
                if (schedulability == 2)
                {
                    std::string log_file2(argv[3]);
	                log_file2 += "/task0_result.txt";
	                FILE * fp2 = fopen(log_file2.c_str(), "w");
			        if (fp2 != NULL)
                    {
                        fprintf(fp2, "1 0 0");
                    }
                    else
                    {
                        perror("Opening output file failed.");
                    }
                }
            }
			*/
			
			if (schedulability == 0)
			{
				fprintf(stderr, "Taskset is schedulable: %s\n", argv[1]);
			}
			else if (schedulability == 1)
			{
				fprintf(stderr, "WARNING: Taskset may not be schedulable: %s\n", argv[1]);
			}
			else
			{
				fprintf(stderr, "ERROR: Taskset NOT schedulable: %s", argv[1]);
				return RT_GOMP_CLUSTERING_LAUNCHER_UNSCHEDULABLE_ERROR;
			}
		}
		else
		{
			fprintf(stderr, "ERROR: Schedulability improperly specified");
			return RT_GOMP_CLUSTERING_LAUNCHER_FILE_PARSE_ERROR;
		}
	}
	else
	{
		fprintf(stderr, "ERROR: Schedulability improperly specified");
		return RT_GOMP_CLUSTERING_LAUNCHER_FILE_PARSE_ERROR;
	}
	
	// Extract the core range line from the file; currently not used
	std::string core_range_line;
	if (!std::getline(ifs, core_range_line))
	{
		fprintf(stderr, "ERROR: Missing system first and last cores line");
		return RT_GOMP_CLUSTERING_LAUNCHER_FILE_PARSE_ERROR;
	}
	
	// Initialize a barrier to synchronize the tasks after creation
	int ret_val = init_single_use_barrier(barrier_name.c_str(), num_tasks);
	if (ret_val != 0)
	{
		fprintf(stderr, "ERROR: Failed to initialize barrier");
		return RT_GOMP_CLUSTERING_LAUNCHER_BARRIER_INITIALIZATION_ERROR;
	}

	// Extract the number of shared resources of the task set
	std::string resources_num;
	std::string lock_type_str;
	std::string resources_line;
	if (std::getline(ifs, resources_line)) {
		std::istringstream resources_stream(resources_line);
		if (!(resources_stream >> resources_num &&
			  resources_stream >> lock_type_str)) {
			fprintf(stderr, "ERROR: Shared resources number improperly specified");
			return RT_GOMP_CLUSTERING_LAUNCHER_FILE_PARSE_ERROR;
		}
	} else {
		fprintf(stderr, "ERROR: Shared resources number improperly specified");
		return RT_GOMP_CLUSTERING_LAUNCHER_FILE_PARSE_ERROR;
	}

	if (lock_type_str != "fifo" && lock_type_str != "prio") {
		fprintf(stderr, "ERROR: Lock type improperly specified");
		return RT_GOMP_CLUSTERING_LAUNCHER_FILE_PARSE_ERROR;
	}

	// Initialize lock objects in a shared memory segment
	ret_val = init_spinlocks(locks_name.c_str(), (unsigned)stoi(resources_num), lock_type_str);
	if (ret_val != SPINLOCKS_UTIL_SUCCESS) {
		fprintf(stderr, "ERROR: Failed to initialize spin locks");
		return RT_GOMP_CLUSTERING_LAUNCHER_LOCKS_INITIALIZATION_ERROR;
	}

	// Initialize a shared memory region for recording statistics data
	ret_val = init_stat_mem(stat_name.c_str(), (unsigned) stoi(resources_num));
	if (ret_val != STATISTICS_SUCCESS) {
		fprintf(stderr, "ERROR: Failed to initialize statistics mem");
		return RT_GOMP_CLUSTERING_LAUNCHER_STAT_INITIALIZATION_ERROR;
	}

	// Iterate over the tasks and fork and execv each one
	std::string task_command_line, task_timing_line, task_partition_line;
	for (unsigned t = 1; t <= num_tasks; ++t)
	{
		if (
		    std::getline(ifs, task_command_line) && 
		    std::getline(ifs, task_timing_line) && 
		    std::getline(ifs, task_partition_line)
	    )
		{
			std::istringstream task_command_stream(task_command_line);
			std::istringstream task_timing_stream(task_timing_line);
			std::istringstream task_partition_stream(task_partition_line);
			
			// Add arguments to this vector of strings. This vector will be transformed into
			// a vector of char * before the call to execv by calling c_str() on each string,
			// but storing the strings in a vector is necessary to ensure that the arguments
			// have different memory addresses. If the char * vector is created directly, by
			// reading the arguments into a string and and adding the c_str() to a vector, 
			// then each new argument could overwrite the previous argument since they might
			// be using the same memory address. Using a vector of strings ensures that each
			// argument is copied to its own memory before the next argument is read.
			std::vector<std::string> task_manager_argvector;
			
			// Add the task program name to the argument vector
			std::string program_name;
			if (task_command_stream >> program_name)
			{
				task_manager_argvector.push_back(program_name);
			}
			else
			{
				fprintf(stderr, "ERROR: Program name not provided for task");
				kill(0, SIGTERM);
				return RT_GOMP_CLUSTERING_LAUNCHER_FILE_PARSE_ERROR;
			}
			
			// Add the partition parameters to the argument vector
			std::string partition_param;
			for (unsigned i = 0; i < num_partition_params; ++i)
			{
				if (task_partition_stream >> partition_param)
				{
					task_manager_argvector.push_back(partition_param);
				}
				else
				{
					fprintf(stderr, "ERROR: Too few partition parameters were provided for task %s", program_name.c_str());
					kill(0, SIGTERM);
					return RT_GOMP_CLUSTERING_LAUNCHER_FILE_PARSE_ERROR;
				}
			}
			
			// Check for extra partition parameters
			if (task_partition_stream >> partition_param)
			{
				fprintf(stderr, "ERROR: Too many partition parameters were provided for task %s", program_name.c_str());
				kill(0, SIGTERM);
				return RT_GOMP_CLUSTERING_LAUNCHER_FILE_PARSE_ERROR;
			}
			
			// Skip the first few timing parameters that were only needed by the scheduler
			std::string timing_param;
			for (unsigned i = 0; i < num_skipped_timing_params; ++i)
			{
				if (!(task_timing_stream >> timing_param))
				{
					fprintf(stderr, "ERROR: Too few timing parameters were provided for task %s", program_name.c_str());
					kill(0, SIGTERM);
					return RT_GOMP_CLUSTERING_LAUNCHER_FILE_PARSE_ERROR;
				}
			}
			
			// Add the timing parameters to the argument vector
			for (unsigned i = num_skipped_timing_params; i < num_timing_params; ++i)
			{
				if (task_timing_stream >> timing_param)
				{
					task_manager_argvector.push_back(timing_param);
				}
				else
				{
					fprintf(stderr, "ERROR: Too few timing parameters were provided for task %s", program_name.c_str());
					kill(0, SIGTERM);
					return RT_GOMP_CLUSTERING_LAUNCHER_FILE_PARSE_ERROR;
				}
			}
			
			// Check for extra timing parameters
			if (task_timing_stream >> timing_param)
			{
				fprintf(stderr, "ERROR: Too many timing parameters were provided for task %s", program_name.c_str());
				kill(0, SIGTERM);
				return RT_GOMP_CLUSTERING_LAUNCHER_FILE_PARSE_ERROR;
			}
			
			// Add the barrier name to the argument vector
			task_manager_argvector.push_back(barrier_name);

			// Son (21Sep2015): add locks name & number of shared resources to the arguments vector
			// Son (27Sep2015): add lock type ("fifo" or "prio") after that to the arguments vector
			task_manager_argvector.push_back(locks_name);
			task_manager_argvector.push_back(resources_num);
			task_manager_argvector.push_back(lock_type_str);

			// Son (08Feb2016): add name of statistics shared memory
			task_manager_argvector.push_back(stat_name);
			
			// Add the task arguments to the argument vector
			task_manager_argvector.push_back(program_name);
			
			std::string task_arg;
			while (task_command_stream >> task_arg)
			{
				task_manager_argvector.push_back(task_arg);
			}
			
			// Create a vector of char * arguments from the vector of string arguments
			std::vector<const char *> task_manager_argv;
			for (std::vector<std::string>::iterator i = task_manager_argvector.begin(); i != task_manager_argvector.end(); ++i)
			{
				task_manager_argv.push_back(i->c_str());
			}

			// NULL terminate the argument vector
			task_manager_argv.push_back(NULL);
			
			fprintf(stderr, "Forking and execv-ing task %s\n", program_name.c_str());
			
			// Fork and execv the task program
			pid_t pid = fork();
			if (pid == 0)
			{
			    // Redirect STDOUT to a file if specified
				std::ostringstream log_file;
				// TODO change "task" to program_name after running experiments
				std::string in_file(argv[1]);
				std::string out_folder(argv[1], 0, strlen(argv[1])-4);
				out_folder += "output";

				// Create a new folder to store output if it is not exist
				// BUG found on Oct 05, 2015: this step creates a race between 
				// processes. All processes read as the folder not exist and 
				// tries to create a new folder, but only first process can create it.
				// The other processes fail and return. Then since the first process
				// wait at the barrier for the other processes, it cannot proceed anymore.
				// FIX: Create all output folders before writing the output files.

				/*
				struct stat sb;
				if (stat(out_folder.c_str(), &sb) != 0) {
					if (mkdir(out_folder.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
						//fprintf(stderr, "ERROR: Cannot make output directory");
						perror("ERROR: Cannot make output directory.");
						return -1;
					}
				}
				*/

				// Extract type of lock from the name of the rtps file. Either "fifo" or "prio"
				std::string lock_type(argv[1], strlen(argv[1])-4, 4);
				//std::cout << "Lock type: " << lock_type << std::endl;
				log_file << out_folder << "/" << "task" << t << "_" << lock_type << ".txt";
				//std::cout << "Log file: " << log_file.str() << std::endl;
				int fd = open(log_file.str().c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
				if (fd != -1) {
					dup2(fd, STDOUT_FILENO);
				} else {
					perror("Redirecting STDOUT failed.");
				}
                
				// Const cast is necessary for type compatibility. Since the strings are
				// not shared, there is no danger in removing the const modifier.
				execv(program_name.c_str(), const_cast<char **>(&task_manager_argv[0]));
				
				// Error if execv returns
				perror("Execv-ing a new task failed");
				kill(0, SIGTERM);
				return RT_GOMP_CLUSTERING_LAUNCHER_FORK_EXECV_ERROR;
			}
			else if (pid == -1)
			{
				perror("Forking a new process for task failed");
				kill(0, SIGTERM);
				return RT_GOMP_CLUSTERING_LAUNCHER_FORK_EXECV_ERROR;
			}	
		}
		else
		{
			fprintf(stderr, "ERROR: Provide three lines for each task in the schedule (.rtps) file");
			kill(0, SIGTERM);
			return RT_GOMP_CLUSTERING_LAUNCHER_FILE_PARSE_ERROR;
		}
	}
	
	// Close the file
	ifs.close();
	
	fprintf(stderr, "All tasks started\n");
	
	// Wait until all child processes have terminated
	while (!(wait(NULL) == -1 && errno == ECHILD));

	// Son (21Sep2015): destroy the shared memory segment of locks
	ret_val = destroy_spinlocks(locks_name.c_str());
	if (ret_val != SPINLOCKS_UTIL_SUCCESS) {
		fprintf(stderr, "WARNING: Destroy shared memory failed");
	}
	
	// Process statistics data before destroying the shared mem.
	// For now, just print it out.
	unsigned resources_number = (unsigned) stoi(resources_num);
	size_t size = sizeof(struct waiting_requests_stat) * resources_number;
	int error_flag;
	volatile struct waiting_requests_stat* stats = get_stat_mem(stat_name.c_str(), size, &error_flag);
	if (stats == NULL) {
		fprintf(stderr, "ERROR: Cannot get the statistics data");
	} else {
		//		for (unsigned i=0; i<resources_number; i++) {
		//			volatile struct waiting_requests_stat* stat = stats+i;
		//			printf("Number of recorded events: %d\n", stat->next_event);
		//			for (unsigned j=0; j<stat->next_event; j++) {
		//				printf("%d ", stat->data[j]);
		//			}
		//		}
	}

	std::ostringstream stat_file;
	std::string in_file(argv[1]);
	std::string out_folder(argv[1], 0, strlen(argv[1])-4);
	out_folder += "output";
	
	// Extract type of lock from the name of the rtps file. Either "fifo" or "prio"
	std::string lock_type(argv[1], strlen(argv[1])-4, 4);
	stat_file << out_folder << "/" << "stat_" << lock_type;

	// Write statistics data to a file
	std::ofstream out_file;
	out_file.open(stat_file.str().c_str());
	if (!out_file) {
		fprintf(stderr, "Statistic file open failed.");
	} else {
		for (unsigned i=0; i<resources_number; i++) {
			out_file << (i+1);
			unsigned num_events = (stats+i)->next_event;
			out_file << " " << num_events << "\n";
			//if (!out_file.write((const char*)(stats+i)->data, num_events)) {
			//	fprintf(stderr, "Write statistics data to file failed.\n");
			//
			//}
			(stats+i)->data[num_events] = '\0';
			std::string events((const char*)(stats+i)->data, num_events);
			//std::cout << "Events: " << (stats+i)->data << std::endl;
			//out_file << (stats+i)->data;
			out_file << events;
			out_file << "\n";
		}
	}
	out_file.close();
	
	// Unmap the shared memory before destroying it
	unmap_stat_mem(stats, resources_number);	

	// Destroy the shared memory region of statistics data (after processing)
	ret_val = destroy_stat_mem(stat_name.c_str(), (unsigned) stoi(resources_num));
	if (ret_val != STATISTICS_SUCCESS) {
		fprintf(stderr, "WARNING: Destroy shared memory failed");
	}

	fprintf(stderr, "All tasks finished\n");
	return 0;
}

