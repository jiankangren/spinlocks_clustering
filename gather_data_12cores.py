#!/usr/bin/python
import os
from collections import OrderedDict

# Define parameters of the task sets we want to gather
num_cores = 12
num_resources = 1
cs_type = 'mod'
util=0.625
num_requests_list = [128, 256, 384, 512, 640, 768, 896, 1024]

# Number of task sets we want to gather
num_tasksets = 100

# Record the number of schedulable task sets for each setting
results_analysis_fifo = OrderedDict()
results_analysis_prio = OrderedDict()
results_exper_fifo = OrderedDict()
results_exper_prio = OrderedDict()

def gather_data():
    for num_requests in num_requests_list:
        # Keep track number of schedulable task sets
        analysis_fifo_count = 0
        analysis_prio_count = 0
        exper_fifo_count = 0
        exper_prio_count = 0

        folder_path = 'ecrts_data/core='+str(num_cores)+'util='+str(util)+'resources='+str(num_resources)+'requests='+str(num_requests)+'type='+cs_type
        #print folder_path
        
        # Go through all task sets
        num_valid_tasksets = num_tasksets
        for i in range(1, num_tasksets+1):
            # Read the schedulability for FIFO case
            fifo_rtps_path = folder_path + '/taskset' + str(i) + '_fifo.rtps'
            fifo_file = open(fifo_rtps_path, 'r')
            if int(fifo_file.readline()) == 0:
                analysis_fifo_count += 1
            fifo_file.close()
                
            # Read the schedulability for PRIO case
            prio_rtps_path = folder_path + '/taskset' + str(i) + '_prio.rtps'
            prio_file = open(prio_rtps_path, 'r')
            if int(prio_file.readline()) == 0:
                analysis_prio_count += 1            
            prio_file.close()
                
            # Read the schedulability of the experiments
            exper_result_folder = folder_path + '/taskset' + str(i) + '_output'
            num_tasks = (len(os.listdir(exper_result_folder)) - 2)/2

            # While reading task output files, track whether the task set is schedulable
            fifo_schedulable = True
            prio_schedulable = True
            for j in range(1, num_tasks+1):
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


def gather_analysis_data():
    for num_requests in num_requests_list:
        # Keep track number of schedulable task sets
        analysis_fifo_count = 0
        analysis_prio_count = 0

        folder_path = 'ecrts_data/core='+str(num_cores)+'util='+str(util)+'resources='+str(num_resources)+'requests='+str(num_requests)+'type='+cs_type
        #print folder_path
        
        # Go through all task sets
        num_valid_tasksets = num_tasksets
        for i in range(1, num_tasksets+1):
            # Read the schedulability for FIFO case
            fifo_rtps_path = folder_path + '/taskset' + str(i) + '_fifo.rtps'
            fifo_file = open(fifo_rtps_path, 'r')
            if int(fifo_file.readline()) == 0:
                analysis_fifo_count += 1
            fifo_file.close()
                
            # Read the schedulability for PRIO case
            prio_rtps_path = folder_path + '/taskset' + str(i) + '_prio.rtps'
            prio_file = open(prio_rtps_path, 'r')
            if int(prio_file.readline()) == 0:
                analysis_prio_count += 1            
            prio_file.close()

        print "#requests: ", num_requests, "Number of valid task sets: ", num_valid_tasksets
        results_analysis_fifo[num_requests] = float(analysis_fifo_count)/num_valid_tasksets
        results_analysis_prio[num_requests] = float(analysis_prio_count)/num_valid_tasksets


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

def print_all_at_once():
    print "Data: "
    for key, ana_fifo_val in results_analysis_fifo.iteritems():
        ana_prio_val = results_analysis_prio[key]
        exp_fifo_val = results_exper_fifo[key]
        exp_prio_val = results_exper_prio[key]
        print key, '\t', ana_fifo_val, '\t', ana_prio_val, '\t', exp_fifo_val, '\t', exp_prio_val

def print_analysis_data():
    print "Data: "
    for key, ana_fifo_val in results_analysis_fifo.iteritems():
        ana_prio_val = results_analysis_prio[key]
        print key, '\t', ana_fifo_val, '\t', ana_prio_val



def main():
    #gather_data()
    #print_all_at_once()
    gather_analysis_data()
    print_analysis_data()

main()
