#!/usr/bin/python
import os
from collections import OrderedDict

# Define parameters of the task sets we want to gather
num_cores = 36
cs_type = 'mod'
util=0.75

# For fixed number of tasks in each task set
num_tasks = 8

# For 1 resource
num_resources = 1
num_requests_list = [128, 256, 384, 512, 640, 768, 896, 1024]

# For 4 resources
#num_resources = 4
#num_requests_list = [32, 64, 96, 128, 160, 192, 224, 256]

# Number of task sets we want to gather
num_tasksets = 100

# Record the number of schedulable task sets for each setting
results_analysis_fifo = OrderedDict()
results_analysis_prio = OrderedDict()
results_analysis_opt_prio = OrderedDict()
results_exper_fifo = OrderedDict()
results_exper_prio = OrderedDict()
results_exper_opt_prio = OrderedDict()


# Gather analysis data for either type of locks
# @lock_type: either "fifo" or "prio"
# @folder: path to the folder that contains result files.
# This folder must directly contain rtps files under it.
# Return: @number of schedulable task sets
#         @number of valid task sets
def gather_analysis_data(lock_type, folder):
    if lock_type != "fifo" and lock_type != "prio":
        print "ERROR: Lock type must be fifo or prio!"
        return

    # Count number of analytically schedulable task sets
    analysis_count = 0
    
    num_valid_tsets = num_tasksets
    for i in range(1, num_tasksets+1):
        file_path = folder + '/taskset' + str(i) + '_' + lock_type + '.rtps'
        rtps_file = open(file_path, 'r')
        line = rtps_file.readline()
        if int(line) == 2:
            #print 'ALERT: Taskset ', i, ' is unschedulable(2)!'
            num_valid_tsets -= 1
            continue
        
        if int(line) == 0:
            analysis_count += 1
        
        rtps_file.close()

    return analysis_count, num_valid_tsets

# Gather empirical data for either type of locks.
# @lock_type: "fifo" or "prio"
# @folder: the folder contains outputs for the task sets
# Return: @number of successfully scheduled task sets
#         @number of valid task sets
def gather_exper_data(lock_type, folder):
    if lock_type != "fifo" and lock_type != "prio":
        print "ERROR: Lock type must be fifo or prio!"
        return 

    # Count number of successfully scheduled task sets
    exper_count = 0
    num_valid_tsets = num_tasksets
    for i in range(1, num_tasksets+1):
        # If the task set is unschedulable analytically, then we abort it
        file_path = folder + '/taskset' + str(i) + '_' + lock_type + '.rtps'
        rtps_file = open(file_path, 'r')
        line = rtps_file.readline()
        if int(line) == 2:
            #print 'ALERT: Taskset ', i, ' is unschedulable(2)!'
            num_valid_tsets -= 1
            continue

        # Read the schedulability of the experiments
        exper_result_folder = folder + '/taskset' + str(i) + '_output'
        num_tasks = (len(os.listdir(exper_result_folder)) - 2)/2

        # Track whether the task set is successfully scheduled
        successful = True

        # While reading task output files, track whether the task set is schedulable
        for j in range(1, num_tasks+1):
            task_path = exper_result_folder + '/task' + str(j) + '_' + lock_type + '.txt'
            task_file = open(task_path, 'r')
            first_line = task_file.readline()
            if first_line == "Binding failed !":
                print 'ALERT: Task set ', i, ' failed to bind!'
                successful = False
                num_valid_tasksets -= 1
                task_file.close()
                break

            if first_line:
                fraction = first_line.rsplit(' ', 1)[1]
                num_fails = fraction.rsplit('/', 1)[0]
                if int(num_fails) > 0:
                    successful = False
                    task_file.close()
                    break

        if successful == True:
            #print 'Task set ', i, ' is successfully scheduled'
            exper_count += 1

    return exper_count, num_valid_tsets

# Gather all analysis and empirical data for FIFO, DM-based, optimal locking priority
def gather_all_data():
    for num_requests in num_requests_list:
        opt_folder = 'data_tpds/core='+str(num_cores)+'n='+str(num_tasks)+'util='+str(util)+'resources='+str(num_resources)+'requests='+str(num_requests)+'type='+cs_type
        fifo_folder = 'data_tpds/core='+str(num_cores)+'n='+str(num_tasks)+'util='+str(util)+'resources='+str(num_resources)+'requests='+str(num_requests)+'type='+cs_type+'/dm_prio_assignment'
        
        # Collect analysis data
        ana_fifo, ana_fifo_valid = gather_analysis_data('fifo', fifo_folder)
        ana_dm_prio, ana_dm_prio_valid = gather_analysis_data('prio', fifo_folder)
        ana_opt_prio, ana_opt_prio_valid = gather_analysis_data('prio', opt_folder)
        
        # Collect empirical data
        exper_fifo, exper_fifo_valid = gather_exper_data('fifo', fifo_folder)
        exper_dm_prio, exper_dm_prio_valid = gather_exper_data('prio', fifo_folder)
        exper_opt_prio, exper_opt_prio_valid = gather_exper_data('prio', opt_folder)

        # Store the percentage of schedulable task sets
        results_analysis_fifo[num_requests] = float(ana_fifo)/ana_fifo_valid
        results_analysis_prio[num_requests] = float(ana_dm_prio)/ana_dm_prio_valid
        results_analysis_opt_prio[num_requests] = float(ana_opt_prio)/ana_opt_prio_valid

        # Store the percentage of successfully scheduled task sets
        results_exper_fifo[num_requests] = float(exper_fifo)/exper_fifo_valid
        results_exper_prio[num_requests] = float(exper_dm_prio)/exper_dm_prio_valid
        results_exper_opt_prio[num_requests] = float(exper_opt_prio)/exper_opt_prio_valid


# Gather all data, from analysis to empirical for FIFO, DM-based priority, and optimal priority
def gather_data():
    '''
    # Gather empirical data for optimal locking priority with "fast" cluster implementation
    for num_requests in num_requests_list:
        exper_opt_prio_count = 0
        num_valid_tasksets = num_tasksets

        folder = 'data_tpds/core='+str(num_cores)+'util='+str(util)+'resources='+str(num_resources)+'requests='+str(num_requests)+'type='+cs_type
        
        for i in range(1, num_tasksets+1):
            # Must check for the task sets with schedulability status of 2
            opt_prio_path = folder + '/taskset' + str(i) + '_prio.rtps'
            opt_prio_file = open(opt_prio_path, 'r')
            line = opt_prio_file.readline()
            if int(line) == 2:
                print 'Taskset unschedulable(2). #requests: ', num_requests, ', taskset: ', i
                #num_valid_tasksets -= 1
                continue

            opt_prio_path = folder + '/taskset' + str(i) + '_prio.sched'
            opt_prio_file = open(opt_prio_path, 'r')
            line = opt_prio_file.readline()
            if line == 'schedulable':
                exper_opt_prio_count += 1

        results_exper_opt_prio[num_requests] = float(exper_opt_prio_count)/num_valid_tasksets
    '''

    for num_requests in num_requests_list:
        # Number of schedulable task sets analytically
        analysis_fifo_count = 0
        analysis_prio_count = 0
        analysis_opt_prio_count = 0

        # Number of task sets successfully scheduled empirically
        exper_fifo_count = 0
        exper_prio_count = 0
        exper_opt_prio_count = 0

        # Folder for FIFO and DM-based locking priority
        fifo_path = 'data_tpds/core='+str(num_cores)+'util='+str(util)+'resources='+str(num_resources)+'requests='+str(num_requests)+'type='+cs_type+'/dm_prio_assignment'

        # Folder for optimal locking priority
        opt_path = 'data_tpds/core='+str(num_cores)+'util='+str(util)+'resources='+str(num_resources)+'requests='+str(num_requests)+'type='+cs_type
        
        # A task set is invalid if a task in it failed to execute (probably caused by not binding to assigned cores).
        # A task set is also invalid if its schedulability status in its rtps file is 2, meaning there is no 
        # valid partion for this task set even if blocking is not accounted for.
        # TODO: Right now, task set 94 is invalid due to the second case. We need to generate a new task set to replace it.
        num_valid_tasksets = num_tasksets
        for i in range(1, num_tasksets+1):
            fifo_schedulable = True
            prio_schedulable = True
            '''
            # Read the schedulability for FIFO case
            fifo_rtps_path = folder_path + '/taskset' + str(i) + '_fifo.rtps'
            fifo_file = open(fifo_rtps_path, 'r')
            first_line = fifo_file.readline()
            if int(first_line) == 0:
                analysis_fifo_count += 1
            if int(first_line) == 2:
                fifo_schedulable = False
            fifo_file.close()
            '''

            # Read the schedulability for PRIO case
            prio_rtps_path = folder_path + '/taskset' + str(i) + '_prio.rtps'
            prio_file = open(prio_rtps_path, 'r')
            first_line = prio_file.readline()
            if int(first_line) == 0:
                analysis_prio_count += 1
            if int(first_line) == 2:
                prio_schedulable = False
            prio_file.close()
    
            # Read the schedulability of the experiments
            exper_result_folder = folder_path + '/taskset' + str(i) + '_output'
            num_tasks = (len(os.listdir(exper_result_folder)) - 2)/2

            # While reading task output files, track whether the task set is schedulable
            for j in range(1, num_tasks+1):
                '''
                # Check tasks with FIFO locks
                fifo_f_name = exper_result_folder + '/task' + str(j) + '_fifo.txt'
                fifo_f = open(fifo_f_name, 'r')
                first_line = fifo_f.readline()

                # Debugging
                #print 'Taskset: ', i, ', task: ', j, '. First line : ', first_line
                # End debugging

                # For now, if we see any output file with empty first line or a line 
                # indicating the core assignment is impossible (>47), we discard this task set.
                if first_line == "Binding failed !":
                    print "Discard task set ", i
                    fifo_schedulable = False
                    prio_schedulable = False
                    num_valid_tasksets -= 1
                    fifo_f.close()
                    break
                
                # Read the number of time it missed deadline if not empty
                if first_line:
                    fraction = first_line.rsplit(' ', 1)[1]
                    num_fails = fraction.rsplit('/', 1)[0]
                    if int(num_fails) > 0:
                        fifo_schedulable = False
                fifo_f.close()
                '''

                # Check tasks with Prio locks
                prio_f_name = exper_result_folder + '/task' + str(j) + '_prio.txt'
                prio_f = open(prio_f_name, 'r')
                first_line = prio_f.readline()
                if first_line == "Binding failed !":
                    print "Discard task set ", i
                    fifo_schedulable = False
                    prio_schedulable = False
                    num_valid_tasksets -= 1
                    prio_f.close()
                    break

                if first_line:
                    fraction = first_line.rsplit(' ', 1)[1]
                    num_fails = fraction.rsplit('/', 1)[0]
                    if int(num_fails) > 0:
                        prio_schedulable = False
                prio_f.close()

            if fifo_schedulable == True:
                exper_fifo_count += 1
            if prio_schedulable == True:
                exper_prio_count += 1

        print "#requests: ", num_requests, "Number of valid task sets: ", num_valid_tasksets
        results_analysis_fifo[num_requests] = float(analysis_fifo_count)/num_valid_tasksets
        results_analysis_prio[num_requests] = float(analysis_prio_count)/num_valid_tasksets
        results_exper_fifo[num_requests] = float(exper_fifo_count)/num_valid_tasksets
        results_exper_prio[num_requests] = float(exper_prio_count)/num_valid_tasksets


def print_data():
    print "Data for FIFO analysis:"
    for key,value in results_analysis_fifo.iteritems():
        print key, '\t', value

    print "Data for PRIO analysis:"
    for key,value in results_analysis_prio.iteritems():
        print key, '\t', value

    print "Data for FIFO experiment:"
    for key,value in results_exper_fifo.iteritems():
        print key, '\t', value

    print "Data for PRIO experiment:"
    for key,value in results_exper_prio.iteritems():
        print key, '\t', value

# Print all analysis and empirical data
def print_all_data():
    print '#requests\tFIFO(ana)\tPrio(ana)\tOptPrio(ana)\tFIFO(exp)\tPrio(exp)\tOptPrio(exp)'
    for key, ana_fifo_val in results_analysis_fifo.iteritems():
        ana_prio_val = results_analysis_prio[key]
        ana_opt_prio_val = results_analysis_opt_prio[key]
        exp_fifo_val = results_exper_fifo[key]
        exp_prio_val = results_exper_prio[key]
        exp_opt_prio_val = results_exper_opt_prio[key]
        print key, '\t', ana_fifo_val, '\t', ana_prio_val, '\t', ana_opt_prio_val, '\t', exp_fifo_val, '\t', exp_prio_val, '\t', exp_opt_prio_val

# Print empirical data
def print_exper_data():
    print '#requests\tFIFO(exp)\tPrio(exp)\tOptPrio(exp)'
    for key, exp_fifo_val in results_exper_fifo.iteritems():
        exp_prio_val = results_exper_prio[key]
        exp_opt_prio_val = results_exper_opt_prio[key]
        print key, '\t', exp_fifo_val, '\t', exp_prio_val, '\t', exp_opt_prio_val


def print_analysis_data():
    print "Data: "
    for key, ana_fifo_val in results_analysis_fifo.iteritems():
        ana_prio_val = results_analysis_prio[key]
        print key, '\t', ana_fifo_val, '\t', ana_prio_val


def main():
    gather_all_data()
    #print_all_data()
    print_exper_data()

main()
