#!/usr/bin/python
import os
from collections import OrderedDict

# Define parameters of the task sets we want to gather
num_cores = 12
cs_type = 'short'
util=0.75
num_tasks = 5

# A flag to switch between varying number of requests and varying number of resources
varying_requests = True

# In case varying number requests per resource
# List of numbers of requests for 1 shared resources
num_requests_list = [128, 256, 384, 512, 640, 768, 896, 1024]
num_resources = 1

# List of numbers of requests for 4 shared resources
#num_requests_list = [32, 64, 96, 128, 160, 192, 224, 256]

# In case varying number of shared resources
num_resources_list = [1, 2, 3, 4, 5, 6, 7, 8]

# Number of requests per shared resource
num_requests = 128

# Number of task sets we want to gather
num_tasksets = 1000

# Record the number of schedulable task sets for each setting
results_analysis_fifo = OrderedDict()
results_analysis_dm_prio = OrderedDict()
results_analysis_opt_prio = OrderedDict()
results_analysis_sim_prio = OrderedDict()


# Gather analysis data for either type of locks
# @folder: path to the folder that contains result files.
# priority-ordered locks are used.
# Return: @number of schedulable task sets with FIFO locks (task sets with 
# schedulability status of 0).
#         @number of schedulable task sets with DM-based priority locks
#         @number of schedulable task sets with optimal priority locks
#         @number of schedulable task sets with simulated annealing priority locks
def gather_analysis_data(folder):
    # Count number of analytically schedulable task sets
    fifo_count = 0
    dm_prio_count = 0
    opt_prio_count = 0
    sim_prio_count = 0
    
    for i in range(1, num_tasksets+1):
        fifo_file_path = folder + '/taskset' + str(i) + '_fifo.rtps'
        dm_file_path = folder + '/taskset' + str(i) + '_prio_dm.rtps'
        opt_file_path = folder + '/taskset' + str(i) + '_prio_opt.rtps'
        sim_file_path = folder + '/taskset' + str(i) + '_prio_sim.rtps'

        fifo_file = open(fifo_file_path, 'r')
        dm_file = open(dm_file_path, 'r')
        opt_file = open(opt_file_path, 'r')
        sim_file = open(sim_file_path, 'r')

        line = fifo_file.readline()
        if int(line) == 0:
            fifo_count += 1

        line = dm_file.readline()
        if int(line) == 0:
            dm_prio_count += 1

        line = opt_file.readline()
        if int(line) == 0:
            opt_prio_count += 1

        line = sim_file.readline()
        if int(line) == 0:
            sim_prio_count += 1

        fifo_file.close()
        dm_file.close()
        opt_file.close()
        sim_file.close()

    return fifo_count, dm_prio_count, opt_prio_count, sim_prio_count


# Gather all analysis and empirical data for FIFO, DM-based, optimal locking priority
def gather_all_data():
    if varying_requests == True:
        for requests in num_requests_list:
            folder = 'data_analysis_tpds/core='+str(num_cores)+'n='+str(num_tasks)+'util='+str(util)+'resources='+str(num_resources)+'requests='+str(requests)+'type='+cs_type
        
            fifo, dm, opt, sim = gather_analysis_data(folder)
            
            # Store the percentage of schedulable task sets
            results_analysis_fifo[requests] = float(fifo)/num_tasksets
            results_analysis_dm_prio[requests] = float(dm)/num_tasksets
            results_analysis_opt_prio[requests] = float(opt)/num_tasksets
            results_analysis_sim_prio[requests] = float(sim)/num_tasksets

    else:
        # In case we varying the number of shared resources
        for resources in num_resources_list:
            folder = 'data_analysis_tpds/varying_resources/core='+str(num_cores)+'n='+str(num_tasks)+'util='+str(util)+'resources='+str(resources)+'requests='+str(num_requests)+'type='+cs_type

            fifo, dm, opt, sim = gather_analysis_data(folder)
            
            results_analysis_fifo[resources] = float(fifo)/num_tasksets
            results_analysis_dm_prio[resources] = float(dm)/num_tasksets
            results_analysis_opt_prio[resources] = float(opt)/num_tasksets
            results_analysis_sim_prio[resources] = float(sim)/num_tasksets

def print_data():
    #print "#Requests\tFIFO\tDM Prio\tOpt Prio\tSim Prio"
    for key, fifo in results_analysis_fifo.iteritems():
        dm = results_analysis_dm_prio[key]
        opt = results_analysis_opt_prio[key]
        sim = results_analysis_sim_prio[key]
        print key, '\t', fifo, '\t', dm, '\t', opt, '\t', sim


def main():
    gather_all_data()
    print_data()

main()
