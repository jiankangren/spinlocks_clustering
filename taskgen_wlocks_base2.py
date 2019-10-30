#!/usr/bin/python
# ====== VERSION 1 ======
# This file is a modification of taskgenerate.py to incorporate shared resources.
# The implementation in this file assumes that each strand of a segment of a task
# can have at most one critical section, and thus it may only be used for testing 
# purpose. The algorithm is as follow:
# - We generate a task set without resources information. The task set is a list of
#   tasks [ [period, program, utilization], ... ] with each of the inner list is for
#   a task. For each task, the 'program' element of its list is itself a list of 
#   segments in this format: [ [seg_id, #strands, length], ... ]; each for a strand.
# - Then, this task set is passed to a function (currently named resources_generate)
#   to add information about resources to the task set. The tasks information after 
#   that is: [ [period, program, utilization, resources_dict], ... ]; where each
#   resources_dict = { res_id:[#requests, cs_len], ... }.
# - Then, distribute_resources_taskset() is called to distribute critical sections
#   to strands of each task. HOWEVER, the current implementation DOES NOT guarantee
#   that all resources of a task are assigned to strands. There can be resources 
#   of a task that was generated but not assigned to any strand of that task.
#
# ====== VERSION 2 ======
# In this version, each strand
# - can have more than 1 critical sections
# - critical sections start at different offset from the beginning
# - critical sections could be for different resources
#
# ====== VERSION 3 (Feb 05, 2016) ======
# This version generates a basic task set (i.e., period, tasks' structure, and
# their utilizations). Then for each input number of critical sections, it distributes
# the critical sections to the tasks. The result is a list of task sets that derive 
# from the same base task sets but have different number of critical sections for
# each resource.

import sys
import string
import os
import math
import random
import copy


#minperiod = 11
#maxperiod = 16

# One second is a billion nanoseconds
nsec_per_sec = 1000000000

# One millisecond is a million nanoseconds
nsec_per_msec = 1000000

# Min and max period in milliseconds
period_min = 10
period_max = 1000

# Min and max exponent of period (base 2)
# The obtained period's unit is microsecond
period_min_expo = 13
period_max_expo = 20

# One microsecond is a thousand nanoseconds
nsec_per_usec = 1000

# Min and max length of critical sections in microseconds
short_cs_min = 1
short_cs_max = 15
mod_cs_min = 1
mod_cs_max = 100

# Number of hyper-period we want the task set to run
num_hyper_period = 100

#excpercent = 1/2.0
excpercent = 0.25 
#excpercent = 0.4

#excpercent = 0.2
choice = [0.5, 0.5, 0.5, 0.5, 0.65, 0.65, 0.65, 0.75, 0.75, 1]
#choice = [0.4,0.4,0.4,0.4,0.5,0.5,0.5,0.7,0.7,1]
#choice = [0.1, 0.15, 0.3, 0.5, 0.75, 1]

resource_sharing_factor = [0.25, 0.25, 0.5, 0.5, 0.65, 0.65, 0.65, 0.75, 0.75, 1]

# List of number of critical sections per resource
critical_sections_per_resource = 128

# List of numbers of shared resources
num_shared_resources = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]

# Generate ratio between span and period (span/period)
def excp_generate():
	return excpercent*(random.choice(choice))

# Generate period in nanosecond
def period_generate():
	period_us = int(math.pow(2, random.randint(period_min_expo, period_max_expo)))
	return int(period_us * nsec_per_usec)
'''
def period_generate():
	period_ms = math.pow(2,  random.uniform(math.log(period_min, 2), math.log(period_max, 2)) )
	period_ms = math.ceil(period_ms)
	return int(period_ms * nsec_per_msec)
	#return int(math.pow(2, random.randint(minperiod, maxperiod)))
'''

# Generate number of strands (in a segment)
def numthread_generate():
	#LJ#r = int(math.floor(random.lognormvariate(1, 0.7)+1))
	#r = 10*int(math.floor(random.lognormvariate(1, 0.7)+1))
	r = 3*int(math.floor(random.lognormvariate(1, 0.7)+1))
	return r

# Generate segment length in nanosecond
# We want to generate segment length relative to span
def threadtype_generate(span):
	temp = int(math.floor(random.lognormvariate(0.6, 3)+5))
	#if span < temp*1000:
	#	print "INVALID! Span: ", span, ". temp: ", temp
	#	exit()
	return int(1000 * math.floor(span/(temp*1000)) )

#def threadtype_generate(max):
#	r = int(math.floor(2000*(random.lognormvariate(0.6, 0.8)+1)))
#	return r * 1000

# Generate short critical section length in nanosecond
def short_cs_generate():
	cs_in_us = random.uniform(short_cs_min, short_cs_max)
	return int(cs_in_us * nsec_per_usec)

# Generate moderate critical section length in nanosecond
def mod_cs_generate():
	cs_in_us = random.uniform(mod_cs_min, mod_cs_max)
	return int(cs_in_us * nsec_per_usec)


# Generate number of requests given the maximum number of requests
def num_requests_generate(max_requests_num):
	return random.randint(1, max_requests_num)


#generate a program
#program = list of subtasks: ['segid', subtasknum, runtime]
#return period, program, total utility
def program_generate():
	period = period_generate()
	exctime = excp_generate()*period
	sumtime = 0 # Track the sum of lengths of generated segments (span)
	sumworkload = 0 # Track the total work that generated so far (wcet)
	segid = 0
	#list of subtasks: [segid, subtasknum, runtime]
	program = []

	# Keep generating segments. The sumtime after the loop ends always <= span
	while sumtime <= exctime:
		seglength = threadtype_generate(exctime)
		while ((sumtime + seglength) > exctime and sumtime == 0) or seglength == 0:
			#print "Generated segment length: ", seglength, ". Span: ", exctime
			seglength = threadtype_generate(exctime)
		if (sumtime + seglength) > exctime:
			break
		sub = [] # Each segment contains [segment id, #strands, segment length]
		segid += 1
		sub.append(segid)
		segnum = numthread_generate()
		sub.append(segnum)
		sub.append(seglength)
		sumtime += seglength
		sumworkload += seglength*segnum
		program.append(sub)
	sumutil = 1.0*sumworkload/period # Utilization of the task
	return period, program, sumutil

# Supplementary function for sorting the list of segments by lengths
def getKey(item):
	return item[1]

# Generate program with a given utilization
# Output keeps the same, i.e., the program structure:
# list of segments [segid, #strands, segment length]
def program_generate2(util):
	period = period_generate()
	expected_span = excp_generate() * period
	expected_work = (int)(period * util)
	sumtime = 0
	sumwork = 0
	segid = 0
	program = []
	segments = [] # list of segment [segid, segment length]
	
	# First, generate a list of segment lengths that makes up the span
	while sumtime < expected_span:
		seglength = threadtype_generate(expected_span)
		while ((sumtime + seglength) > expected_span and sumtime == 0) or seglength == 0:
			seglength = threadtype_generate(expected_span)
		if (sumtime + seglength) > expected_span:
			break
		sub = [] # [segid, segment length]
		segid += 1
		sumtime += seglength
		sub.append(segid)
		sub.append(seglength)
		segments.append(sub)

	# Initiate the program structure
	for segment in segments:
		sub = []
		sub.append(segment[0])
		sub.append(1)
		sub.append(segment[1])
		program.append(sub)
		
		# Also initiate the work
		sumwork += segment[1]
	
	# Make a copy of the list of segments
	# Sort it by increasing length of the segments
	segments_copy = copy.copy(segments)
	segments_copy = sorted(segments_copy, key=getKey)
	
	# Then iteratively add strands until it adds up to work (approximately)
	while sumwork < expected_work:
		seg = random.choice(segments)
		if (sumwork + seg[1]) <= expected_work:
			program[seg[0]-1][1] += 1
			sumwork += seg[1]
		else:
			# Find the last strand that can be added to the program
			last_segid = -1
			for segment in segments_copy:
				if (sumwork + segment[1]) <= expected_work:
					last_segid = segment[0]
				else:
					break
			if last_segid != -1:
				program[last_segid-1][1] += 1
				sumwork += program[last_segid-1][2]
			break
	
	#print "Final program: ", program
	#check_work = 0
	#for segment in program:
	#	check_work += segment[1]*segment[2]

	#if check_work != sumwork:
	#	print "Something wrong !"
	#print "Sumwork: ", sumwork, ". Check work: ", check_work

	actual_util = 1.0*sumwork/period
	#print "Util: ", util, ". Actual util: ", actual_util

	return period, program, actual_util

# A function to test how close is the generated utilization to the expected utilization
def test_program_generate():
	count = 0
	for i in range(1, 1000):
		period, program, actual_util = program_generate2(1.25) #program_generate2(math.sqrt(12))
		if 1.25 - actual_util >= 0.01:
		#if math.sqrt(12) - actual_util >= 0.01:
			count += 1
			print "Actual util: ", actual_util, ". Expected util: ", 1.25 #math.sqrt(12)
	print "Count: ", count

#test_program_generate()

# Return the number of cores required for input task
# Assume task is implicit deadline
def num_cores(period, program, sumutil):
	span = 0
	work = 0
	for segment in program:
		span += segment[2]
		work += segment[2] * segment[1]
	return int(math.ceil(1.0 * (work - span)/(period - span)))

# Generate task set
# Input is the total number of cores of the system
# Keep generating task and add it to the task set until the 
# total number of cores required reaches m
def taskset_generate(m):
	# Return a list of tasks
	# Each is a list [period, program, util]
	taskset = []
	assigned_cores = 0
	num_tries = 0
	sum_util = 0

	while assigned_cores < m:
		num_tries += 1
		period, program, util = program_generate()
		if not (float(util) >= 1.25 and float(util) <= math.sqrt(float(m))):
			continue
		cores = num_cores(period, program, util)
		if (assigned_cores + cores) > m:
			break
		assigned_cores += cores
		sum_util += util
		task = [period, program, util]
		taskset.append(task)
		#print "Add task to the task set"

	#print "Number of tries: ", num_tries
	print "Total utilization: ", sum_util, ". Cores assigned: ", assigned_cores
	return taskset

# Look and see how diverse the generated total utilizations are
# (without controlling the total util explicitly)
def test_total_util_diversity():
	for i in range(1, 30):
		taskset_generate(24)
	
#test_total_util_diversity()

# Generate utiliations for tasks in the taskset with a given total util (m*fraction)
# Return a list of utilizations
def generate_tasks_utils(m, fraction):
	total_util = fraction*m
	if total_util < 1.25:
		print "Total utilization is too small !"
		return 
	
	utils_list = []
	remain = total_util
	while True:
		util = random.uniform(1.25, math.sqrt(m))
		remain -= util
		if remain > math.sqrt(m):
			utils_list.append(util)
		elif remain >= 1.25 and remain <= math.sqrt(m):
			utils_list.append(util)
			utils_list.append(remain)
			return utils_list
		else: # remain < 1.25 thus too small for another task
			remain += util

def test_tasks_utils():
	for i in range (1, 1000):
		utils = generate_tasks_utils(12, 0.5)
		print "Utilizations: ", utils

#test_tasks_utils()

# Generate task set from a list of generated utilizations for its tasks
def taskset_generate2(m, fraction):
	utils = generate_tasks_utils(m, fraction)
	taskset = []
	actual_total_util = 0
	total_util = 0

	for util in utils:
		period, program, actual_util = program_generate2(util)
		task = [period, program, actual_util]
		taskset.append(task)

		actual_total_util += actual_util
		total_util += util

	#print "Actual total util: ", actual_total_util, ". Total util: ", total_util
	return taskset

def test_taskset_generate2():
	taskset = taskset_generate2(12, 0.75)
	
#test_taskset_generate2()

# Return the utilization of a task based on standard deviation
# Task utilization is generated with normal distribution with 
# a fixed mean value (mean of 1.25 and sqrt(m))
def get_task_util(m, std_dev_factor):
	mean = (1.25 + math.sqrt(m))/2
	std_dev = std_dev_factor * m
	while True:
		util = random.normalvariate(mean, std_dev)
		if util >= 1.25 and util <= math.sqrt(m):
			return util
	

def test_util_gen():
	util = get_task_util(12, 0.1)
	print "Generated util: ", util


# Utility function to print out the generated task set
def taskset_print(taskset):
	count = 1
	for task in taskset:
		print "Task ", count
		print "Period: ", task[0], ". Util: ", task[2]
		for segment in task[1]:
			print "Segment ", segment[0], ": ", segment
		#	print "Segment id: ", segment[0], ". #strands: ", segment[1], ". Len: ", segment[2],
		#	". #requests: ", segment[3], ". Resource id: ", segment[4], ". cs_len: ", segment[5]
		count += 1

		# Print resource information
		my_res = task[3]
		for res_id in my_res:
			print "Resource id: ", res_id, ". #requests: ", my_res[res_id][0], ", cs_len: ", my_res[res_id][1]

	return




# Generate resource information to the task set
# For each task in the taskset list, append a resource dictionary for that task
# i.e. input taskset =  [ [period, program, util], ... ]
#      output taskset = [ [period, program, util, resources_dict], ... ]; where
#      resources_dict = { res_id : [#requests, cs_len], ... }
def resources_generate(taskset, num_resources, max_requests, cs_type):
	num_tasks = len(taskset)
	out_taskset = taskset
	for task in out_taskset:
		task.append(dict())

	for i in range(num_resources):
		num_shared_tasks = int(math.ceil(num_tasks * random.choice(resource_sharing_factor)))
		if num_shared_tasks <= 1:
			num_shared_tasks = 2
		shared_tasks = [] # Contain id of tasks share this resources
		count = 0
		while count < num_shared_tasks:
			task_id = random.randint(1, num_tasks)
			if task_id in shared_tasks:
				continue
			shared_tasks.append(task_id)
			count += 1

		for task_id in shared_tasks:
			task = out_taskset[task_id-1]
			my_res = task[3]
			num_requests = num_requests_generate(max_requests)
			if cs_type == "short":
				cs_len = short_cs_generate()
			elif cs_type == "mod":
				cs_len = mod_cs_generate()
			else:
				print "Incorrect critical section type"
				exit()
			# Resource id starts from 1
			my_res[i+1] = [num_requests, cs_len]
			
	return out_taskset

# Given the maximum number of requests to a resource,
# return an actual request number
def get_actual_requests_num(max_requests):
	return max_requests

# Generate resources information for the task set given the 
# maximum number requests for each resources
def resources_generate_random(taskset, num_resources, max_requests, cs_type, critsec_lengths):
	num_requests = get_actual_requests_num(max_requests)
	num_tasks = len(taskset)

	for task in taskset:
		task.append(dict())

	# Add number of critical sections for each resource
	# a task access
	for i in range(num_resources):
		for j in range(num_requests):
			task_id = random.randint(1, num_tasks)
			task = taskset[task_id-1]
			my_res = task[3]
			res_id = i+1 # Resource id starts from 1
			if res_id not in my_res:
				my_res[res_id] = [1]
			else:
				res_info = my_res[res_id]
				res_info[0] += 1

	# Add critical section length for each resource accessed
	task_idx = 0
	for task in taskset:
		my_res = task[3]
		for res_id in my_res:
			'''
			if cs_type == "short":
				cs_len = short_cs_generate()
			elif cs_type == "mod":
				cs_len = mod_cs_generate()
			else:
				print "Incorrect critical section type"
				exit()
			'''
			cs_len = critsec_lengths[task_idx][res_id - 1]
			res_info = my_res[res_id]
			res_info.append(cs_len)
		task_idx += 1

# Assign resources to strands of the task
def distribute_resources_task_sync(task):
	resources_copy = copy.deepcopy(task[3])
	resources_list = []
	for res_id in resources_copy:
		resources_list.append(res_id)

	segments_list = task[1]
	for segment in segments_list:
		# All requests are assigned, no request left for this segment
		if len(resources_list) <= 0:
			segment.extend([0, 0, 0]);
			continue
		
		# Otherwise, pick a resource and assign its requests
		# to this segment
		res_id = random.choice(resources_list)
		if resources_copy[res_id][0] <= segment[1]:
			num_requests = resources_copy[res_id][0]
			resources_list.remove(res_id)
		else:
			num_requests = segment[1]
			resources_copy[res_id][0] -= segment[1]

		cs_len = resources_copy[res_id][1]
		segment.append(num_requests)
		segment.append(res_id)
		segment.append(cs_len)
		#print "Segment: ", segment[0], ". #requests: ", num_requests, ". Res_id: ", res_id
		#print "Segment ", segment[0], ": ", segment
	return

# Assign resources to strands of each task
# Input: taskset which is already added resources information
def distribute_resources_sync(taskset):
	print "Size of task set: ", len(taskset)
	count = 1
	for task in taskset:
		print "Distributing resources for task ", count
		distribute_resources_task_sync(task)
		count += 1
	return

# Convert a time duration in nanoseconds to a pair (seconds, nanoseconds)
def convert_nsec_to_timespec(length):
	if length > nsec_per_sec:
		len_sec = length/nsec_per_sec
		len_nsec = length - nsec_per_sec*len_sec
	else:
		len_sec = 0
		len_nsec = length

	return len_sec, len_nsec

# Generate number of time a task is released
def num_iterations_generate():
	return 100

# Get hyper-period of the task set
# Since periods are multiple of each other by factor of 2,
# the hyper-period is just the maximum period among tasks
def get_taskset_hyperperiod(taskset):
	periods = []
	for task in taskset:
		periods.append(task[0])
	return max(periods)

# Return the number of times a task will be released
def get_num_iterations(task, hyper_period):	
	period = task[0]
	times = hyper_period / period
	return (num_hyper_period * times)

def write_to_rtpt_sync(taskset, m, num_resources):
	f = open("test_taskset1.rtpt", 'w')
	lines = str(0) + ' ' + str(m-1) + '\n'
	lines += str(num_resources) + '\n'

	for task in taskset:
		num_segments = len(task[1])
		# A line for command line arguments
		line = "synthetic_task_wlocks " + str(num_segments) + ' '
		work = 0
		span = 0
		for segment in task[1]:
			#print "Segment items: ", len(segment)
			span += segment[2]
			work += segment[2] * segment[1]
			if segment[2] > nsec_per_sec:
				len_sec = segment[2]/nsec_per_sec
				len_nsec = segment[2] - nsec_per_sec*len_sec
			else:
				len_sec = 0
				len_nsec = segment[2]

			line += str(segment[1]) + ' ' + str(len_sec) + ' ' + str(len_nsec) + ' '
			line += str(segment[3]) + ' ' + str(segment[4]) + ' ' + str(segment[5]) + ' '
		line += '\n'
		lines += line

		work_sec, work_nsec = convert_nsec_to_timespec(work)
		span_sec, span_nsec = convert_nsec_to_timespec(span)
		period_sec, period_nsec = convert_nsec_to_timespec(task[0])
		num_iters = num_iterations_generate()

		# A line for timing parameters
		line2 = str(work_sec)+' '+str(work_nsec)+' '+str(span_sec)+' '+str(span_nsec)+' '+\
		    str(period_sec)+' '+str(period_nsec)+' '+str(period_sec)+' '+str(period_nsec)+' 0 0 '+str(num_iters)+'\n'
		lines += line2
		

	f.write(lines)
	
	return

# Expected arguments: 
# 1. Total number of processors
# 2. Number of shared resources of the task set
# 3. Maximum number of requests per resource per job
# 4. Type of critical section: short ([1, 15] us) or mod ([1, 100] us)
def main_sync():
	if len(sys.argv) != 5:
		print "Usage: ", sys.argv[0], " <num_cores> <num_resources> <max_num_requests> <cs_type>"
		exit()
	# Must cast to integer. Otherwise, m is a string!
	m = int(sys.argv[1])
	num_resources = int(sys.argv[2])
	max_requests = int(sys.argv[3])
	cs_type = sys.argv[4]
	
	if not (cs_type == "short" or cs_type == "mod"):
		print "Incorrect critical section type"
		exit()

	taskset = taskset_generate(m)
	resources_generate(taskset, num_resources, max_requests, cs_type)
	distribute_resources_sync(taskset)
	write_to_rtpt_sync(taskset, m, num_resources)
	#taskset_print(taskset)

# Input parameter @length in nanoseconds
def get_max_requests(length, cs_type):
	if cs_type == "short":
		return int(length/(short_cs_max*nsec_per_usec))
	elif cs_type == "mod":
		return int(length/(mod_cs_max*nsec_per_usec))
	else:
		print "Incorrect type for critical section length"
		return -1

# Actually function to distribute critical sections to strands of the task
# Input is a task: [period, program, util, resources_dict]
def distribute_resources_task_random(task, cs_type):
	segment_list = task[1]
	for segment in segment_list:
		strands_info = []
		num_strands = segment[1]
		max_requests_per_strand = get_max_requests(segment[2], cs_type)
		for i in range(num_strands):
			info = []
			info.append(max_requests_per_strand)
			strands_info.append(info)

		segment.append(strands_info)
	#print segment_list

	# Calculate the alignment distance between critical sections
	# of a strand
	if cs_type == "short":
		align_distance = short_cs_max * nsec_per_usec
	elif cs_type == "mod":
		align_distance = mod_cs_max * nsec_per_usec

	# Initialize a list of indices of available segments
	# (which are segments that can be assigned requests to)
	# This is just a meta-info to keep track which segments
	# still have space to assign requests to, while we try
	# to distribute requests.
	# avail_segments = [ [seg_idx, [strand_idx, ...]], ... ]
	avail_segments = []
	for i in range(len(segment_list)):
		# Ignore segments that too short to contain a request
		if get_max_requests(segment_list[i][2], cs_type) <= 0:
			continue
		num_strands = segment_list[i][1]
		avail_strands = []
		for j in range(num_strands):
			avail_strands.append(j)
		avail_segments.append([i, avail_strands])

	# For each request, randomly assign it to a strand
	resources_dict = task[3]
	for res_id, res_info in resources_dict.iteritems():
		if len(avail_segments) == 0:
			#print "WARNING: Run out of space to assign critical section!"
			break
		num_requests = res_info[0]
		request_len = res_info[1]
		for i in range(num_requests):
			if len(avail_segments) == 0:
				break
			# Choose a segment and a corresponding strand
			chosen_segment = random.choice(avail_segments)
			seg_idx = chosen_segment[0]
			strand_idx = random.choice(chosen_segment[1])
			segment = segment_list[seg_idx]
			strand_info = segment[3][strand_idx]

			# Add this request to this strand
			offset = align_distance * (len(strand_info)-1)
			strand_info.append([offset, res_id, request_len])

			# If the number of assigned requests of this strand
			# larger than the maximum requests allowed, remove
			# this strand from the available list.
			if (len(strand_info)-1) >= strand_info[0]:
				chosen_segment[1].remove(strand_idx)
				if len(chosen_segment[1]) <= 0:
					avail_segments.remove(chosen_segment)
				

	return

# Distribute generated resources to tasks so that
# each tasks' strands can have more than 1 critical sections
# and each critical section start at some offset.
def distribute_resources_random(taskset, cs_type):
	#print "Size of task set: ", len(taskset)
	count = 1
	for task in taskset:
		#print "Distributing resources for task ", count
		distribute_resources_task_random(task, cs_type)
		count += 1
	return

# Return the next number for a rtpt file
def get_rtpt_file_number(directory):
	list_dir = os.listdir(directory)
	count = 0
	for f in list_dir:
		if f.endswith('.rtpt'):
			count += 1
	return (count+1)

# Write the generated task set to a rtpt file
def write_to_rtpt_random(taskset, m, num_resources, directory):
	hyper_period = get_taskset_hyperperiod(taskset)
	#print "Hyper period: ", hyper_period
	#f = open("test_taskset_random1.rtpt", 'w')
	f_num = get_rtpt_file_number(directory)
	f = open(str(directory)+'/taskset'+str(f_num)+'.rtpt', 'w')
	lines = str(0) + ' ' + str(m-1) + '\n'
	lines += str(num_resources) + '\n'

	for task in taskset:
		num_segments = len(task[1])
		# A line for command line arguments
		line = "synthetic_task_wlocks " + str(num_segments) + ' '
		work = 0
		span = 0
		for segment in task[1]:
			#print "Segment items: ", len(segment)
			span += segment[2]
			work += segment[2] * segment[1]
			if segment[2] > nsec_per_sec:
				len_sec = segment[2]/nsec_per_sec
				len_nsec = segment[2] - nsec_per_sec*len_sec
			else:
				len_sec = 0
				len_nsec = segment[2]

			line += str(segment[1]) + ' ' + str(len_sec) + ' ' + str(len_nsec) + ' '
			# Now write down critical sections information for each strand
			strands_info = segment[3]			
			for strand in strands_info:
				num_requests = len(strand) - 1
				line += str(num_requests) + ' '
				for i in range(1, len(strand)):
					request = strand[i]
					line += str(request[0])+' '+str(request[1])+' '+str(request[2])+' '
			
		line += '\n'
		lines += line

		work_sec, work_nsec = convert_nsec_to_timespec(work)
		span_sec, span_nsec = convert_nsec_to_timespec(span)
		period_sec, period_nsec = convert_nsec_to_timespec(task[0])
		num_iters = get_num_iterations(task, hyper_period)
		#num_iters = num_iterations_generate()

		# A line for timing parameters
		line2 = str(work_sec)+' '+str(work_nsec)+' '+str(span_sec)+' '+str(span_nsec)+' '+\
		    str(period_sec)+' '+str(period_nsec)+' '+str(period_sec)+' '+str(period_nsec)+' 0 0 '+str(num_iters)+'\n'
		lines += line2
			
	f.write(lines)
	return 

def write_to_rtpt_random_2(taskset, sys_first_core, sys_last_core, num_resources, directory):
	m = sys_last_core - sys_first_core + 1
	hyper_period = get_taskset_hyperperiod(taskset)
	#print "Hyper period: ", hyper_period
	#f = open("test_taskset_random1.rtpt", 'w')
	f_num = get_rtpt_file_number(directory)
	f = open(str(directory)+'/taskset'+str(f_num)+'.rtpt', 'w')
	#lines = str(0) + ' ' + str(m-1) + '\n'
	lines = str(sys_first_core) + ' ' + str(sys_last_core) + '\n'
	lines += str(num_resources) + '\n'

	for task in taskset:
		num_segments = len(task[1])
		# A line for command line arguments
		line = "synthetic_task_wlocks " + str(num_segments) + ' '
		work = 0
		span = 0
		for segment in task[1]:
			#print "Segment items: ", len(segment)
			span += segment[2]
			work += segment[2] * segment[1]
			if segment[2] > nsec_per_sec:
				len_sec = segment[2]/nsec_per_sec
				len_nsec = segment[2] - nsec_per_sec*len_sec
			else:
				len_sec = 0
				len_nsec = segment[2]

			line += str(segment[1]) + ' ' + str(len_sec) + ' ' + str(len_nsec) + ' '
			# Now write down critical sections information for each strand
			strands_info = segment[3]			
			for strand in strands_info:
				num_requests = len(strand) - 1
				line += str(num_requests) + ' '
				for i in range(1, len(strand)):
					request = strand[i]
					line += str(request[0])+' '+str(request[1])+' '+str(request[2])+' '
			
		line += '\n'
		lines += line

		work_sec, work_nsec = convert_nsec_to_timespec(work)
		span_sec, span_nsec = convert_nsec_to_timespec(span)
		period_sec, period_nsec = convert_nsec_to_timespec(task[0])
		num_iters = get_num_iterations(task, hyper_period)
		#num_iters = num_iterations_generate()

		# A line for timing parameters
		line2 = str(work_sec)+' '+str(work_nsec)+' '+str(span_sec)+' '+str(span_nsec)+' '+\
		    str(period_sec)+' '+str(period_nsec)+' '+str(period_sec)+' '+str(period_nsec)+' 0 0 '+str(num_iters)+'\n'
		lines += line2
			
	f.write(lines)
	return 


# Expected arguments:
# 1. Total number of processors
# 2. Number of shared resources of the task set
# 3. Number of requests per resource (among all tasks)
# 4. Type of critical sections: short or mod
def main_random():
	if len(sys.argv) != 5:
		print "Usage: ", sys.argv[0], " <num_cores> <num_resources> <max_num_requests> <cs_type>"
		exit()
	
	m = int(sys.argv[1])
	num_resources = int(sys.argv[2])
	max_requests = int(sys.argv[3])
	cs_type = sys.argv[4]

	if not (cs_type == "short" or cs_type == "mod"):
		print "Incorrect critical section type"
		exit()

	taskset = taskset_generate(m)
	resources_generate_random(taskset, num_resources, max_requests, cs_type)
	distribute_resources_random(taskset, cs_type)
	directory = 'core='+str(m)+'resources='+str(num_resources)+'requests='+str(max_requests)+'type='+cs_type
	if os.path.isdir(directory) == False:
		os.system('mkdir ' + directory)
	write_to_rtpt_random(taskset, m, num_resources, directory)
	#taskset_print(taskset)

def main_random_2():
	if len(sys.argv) != 6:
		print "Usage: ", sys.argv[0], " <sys_first_core> <sys_last_core> <num_resources> <max_num_requests> <cs_type>"
		exit()
	
	#m = int(sys.argv[1])
	sys_first_core = int(sys.argv[1])
	sys_last_core = int(sys.argv[2])
	if sys_last_core < sys_first_core:
		print "Incorrect system core range!"
		exit()
	
	num_resources = int(sys.argv[3])
	max_requests = int(sys.argv[4])
	cs_type = sys.argv[5]
	m = sys_last_core - sys_first_core + 1

	if not (cs_type == "short" or cs_type == "mod"):
		print "Incorrect critical section type"
		exit()

	taskset = taskset_generate(m)
	resources_generate_random(taskset, num_resources, max_requests, cs_type)
	distribute_resources_random(taskset, cs_type)
	directory = 'core='+str(m)+'resources='+str(num_resources)+'requests='+str(max_requests)+'type='+cs_type
	if os.path.isdir(directory) == False:
		os.system('mkdir ' + directory)
	#write_to_rtpt_random_2(taskset, sys_first_core, sys_last_core, num_resources, directory)

# Add the total utilization as a controlled parameter
def main_random_3():
	if len(sys.argv) != 7:
		print "Usage: ", sys.argv[0], " <sys_first_core> <sys_last_core> <num_resources> <max_num_requests> <cs_type> <total_util_frac>"
		exit()
	
	#m = int(sys.argv[1])
	sys_first_core = int(sys.argv[1])
	sys_last_core = int(sys.argv[2])
	if sys_last_core < sys_first_core:
		print "Incorrect system core range!"
		exit()
	
	num_resources = int(sys.argv[3])
	max_requests = int(sys.argv[4])
	cs_type = sys.argv[5]
	fraction = float(sys.argv[6])

	m = sys_last_core - sys_first_core + 1

	if not (cs_type == "short" or cs_type == "mod"):
		print "Incorrect critical section type"
		exit()

	# Generate task set with the total utilization is (fraction * m)
	taskset = taskset_generate2(m, fraction)
	resources_generate_random(taskset, num_resources, max_requests, cs_type)
	distribute_resources_random(taskset, cs_type)

	ecrts_folder = 'ecrts_data/'
	directory = ecrts_folder + 'core='+str(m)+'util='+str(fraction)+'resources='+str(num_resources)+'requests='+str(max_requests)+'type='+cs_type
	if os.path.isdir(directory) == False:
		os.system('mkdir ' + directory)
	write_to_rtpt_random_2(taskset, sys_first_core, sys_last_core, num_resources, directory)


# Generate a single base task set. Then for each option of the number of shared resources, 
# create a copy of the base task set and add these critical sections to the new task set. 
# The argument <requests_per_resource> is the number of critical sections for each resource.
def main_random_base():
	if len(sys.argv) != 6:
		print "Usage: ", sys.argv[0], " <sys_first_core> <sys_last_core> <requests_per_resource> <cs_type> <total_util_frac>"
		exit()
	
	sys_first_core = int(sys.argv[1])
	sys_last_core = int(sys.argv[2])
	if sys_last_core < sys_first_core:
		print "Incorrect system core range!"
		exit()
	
	requests_per_res = int(sys.argv[3])
	cs_type = sys.argv[4]
	fraction = float(sys.argv[5])

	m = sys_last_core - sys_first_core + 1

	if not (cs_type == "short" or cs_type == "mod"):
		print "Incorrect critical section type"
		exit()

	# Generate a base task set with the total utilization is (fraction * m)
	taskset = taskset_generate2(m, fraction)
	
	# Generate critical section lengths of all resources for all tasks
	critsec_lengths = []
	for i in range(len(taskset)):
		resources = []
		for j in range(len(num_shared_resources)):
			if cs_type == "short":
				cs_len = short_cs_generate()
			elif cs_type == "mod":
				cs_len = mod_cs_generate()
			resources.append(cs_len)
		critsec_lengths.append(resources)
	
	# For each value of the number of shared resources, create 
	# a copy of the base task set and distribute the critical sections to it
	for res_num in num_shared_resources:
		taskset_clone = copy.deepcopy(taskset)
		resources_generate_random(taskset_clone, res_num, requests_per_res, cs_type, critsec_lengths)
		distribute_resources_random(taskset_clone, cs_type)
		
		# Write the clone task set to a .rtpt file
		ecrts_folder = 'ecrts_data_base/varying_num_requests/'
		directory = ecrts_folder + 'core='+str(m)+'util='+str(fraction)+'resources='+str(res_num)+'requests='+str(requests_per_res)+'type='+cs_type
		if os.path.isdir(directory) == False:
			os.system('mkdir ' + directory)
		write_to_rtpt_random_2(taskset_clone, sys_first_core, sys_last_core, res_num, directory)
		

main_random_base()
