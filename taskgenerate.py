#!/usr/bin/python

import sys
import string
import os
import math
import random
import copy

#sys.path.append('/home/research1/lij/openmp')
from lib_deadline_calculation import lorh, assign
from lib_scheduling import assign_priority, sortreldead, simulation, sortprog
from lib_assigncore import *
from lib_cluster import *
from lib_gedf import *

#parameters
#number of cores
#LJ#
corenum = 14
#corenum = 36
#corenum = 6
#resource augmentation
#resourceaug = 5
#range of the period: 2^n
minperiod = 11
maxperiod = 16
#length of thread
#minlength = 100
#maxlength = 1000
#excusion time percentage of period
#LJ#
#excpercent = 1/2.0
excpercent = 0.25 
#excpercent = 0.2
choice = [0.4,0.4,0.4,0.4,0.5,0.5,0.5,0.7,0.7,1]
#choice = [0.1, 0.15, 0.3, 0.5, 0.75, 1]

#print the task sets or not
doprint = 2
#strdir = 'core'+str(corenum)+'aug2util05-90'+'/'
strdir = 'core'+str(corenum)+'aug2avg3exp1arbispeedup'+'/'
os.system('mkdir '+strdir)

# Generate ratio between span and period (span/period)
def excp_generate():
	return excpercent*(random.choice(choice))

# Generate period
def period_generate():
	return int(math.pow(2, random.randint(minperiod, maxperiod)))

# Generate number of strands (in a segment)
def numthread_generate():
	#LJ#r = int(math.floor(random.lognormvariate(1, 0.7)+1))
	#r = 10*int(math.floor(random.lognormvariate(1, 0.7)+1))
	r = int(math.floor(random.lognormvariate(1, 0.7)+1))
	return r

# Generate segment length (input is the maximum length possible, but
# it seems not to be used here)
def threadtype_generate(max):
	#r = 0
	#while r < 100 or r > 1000:
	r = int(math.floor(100*(random.lognormvariate(0.6, 0.8)+1)))
	return r

#generate a program
#program = list of subtasks: ['segid', subtasknum, runtime]
#return period, program, total utility
def program_generate():
	period = period_generate()
	#print period
	exctime = excp_generate()*period
	sumtime = 0 # Track the sum of lengths of generated segments (span)
	sumworkload = 0 # Track the total work that generated so far (wcet)
	segid = 0
	#list of subtasks: [segid, subtasknum, runtime]
	program = []

	# Keep generating segments. The sumtime after the loop ends always <= span
	while sumtime <= exctime:
		seglength = threadtype_generate(exctime)
		while (sumtime + seglength) > exctime and sumtime == 0:
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
		#print sub
	sumutil = 1.0*sumworkload/period # Utilization of the task
	#print program, sumtime, sumutil
	return period, program, sumutil

#change the program's sumutil from oldutil to newutil
def change_program_sumutil(program, oldutil, newutil):
	newprogram = []
	workload = 0
	for sub in program:
		newrun = int(math.floor(newutil*sub[2]/oldutil))
		if newrun < 100:
			if newrun*sub[1] < 100:
				threadnum = 1
			else:
				threadnum = newrun*sub[1]/100
			newrun = 100
		else:
			threadnum = sub[1]
		newsub = [sub[0]]+[threadnum]+[newrun]
		newprogram.append(newsub)
		workload += threadnum*newrun
	#print workload, newprogram
	return newprogram, workload

#add the real worstcase excution time into a program => task
#calculate the min excution time and total workload
#task = list of subtasks: ['segid', subtasknum, runtime, worstcase]
def task_generate(program, resourceaug):
	sumtime = 0
	sumwork = 0
	task = []
	for segment in program:
		seglength = segment[2]
		worstcase = resourceaug*seglength
		sub = segment+[worstcase]
		sumtime += worstcase
		sumwork += worstcase*segment[1]
		task.append(sub)
	#load = [mintime, workload]
	load = [sumtime, sumwork]
	#print task, load
	return task, load

#calculate program deadline (according to certain resourceaug, shown by worstcase)
##list of subtasks: ['segid', subtasknum, runtime, worstcase, release, reladead]
def program_deadline(period, task, load):
	hlfraction = []
	release = []
	reladead = []
	#calculate the slack
	slack = period - 0.5*load[0]
	#print 'slack:', slack
	if slack <= 0:
		print 'Not Schedulable!'
		return 0
	bound = 0.5*load[1]/slack
	#print 'bound:', bound
	#distinguish heavy and light segments
	#liload = [light mintime, light workload]
	#hlflag: heavy or light segment flag
	(liload, hlflag) = lorh(task, bound)
	#print 'light load:', liload
	#assign release time and relative deadline
	assign(period, task, load, liload, hlflag, slack)
	#print task

#delete the worstcase from task and take runtime as worstcase (for schedule)
#add the program_id and period to the task
##list of subtasks: [programid, period, segid, subtasknum(=sub_count), runtime(=worst_case), (worstcase,) release(=rel_tm), reladead(=rel_dead)]
def del_worst_add_name(task, programid, period):
	program = []
	for sub in task:
		tmp = [programid, period]+sub[0:3]+sub[4:6]
		program.append(tmp)
	#print program
	return program

def addprogram(totalutil, resourceaug):
	#(prog_id, period, seg_id, sub_count, worst_case, rel_tm, rel_dead, *priority, *[core])
	util = 0
	programid = 0
	taskset = []
	flag = 0
	while flag == 0:
		#generate a program
		programid += 1
		(period, program, sumutil) = program_generate()
		if util >= totalutil-0.02:
		#if (util+sumutil/corenum) >= totalutil:
		#	print 'stop:', util+sumutil/corenum
			#newutil = (totalutil - util)*corenum
			#(program, newload) = change_program_sumutil(program, sumutil, newutil)
			#sumutil = 1.0*newload/period
			flag = 1
			load = [0, 0]
		elif (util+sumutil/corenum) >= totalutil:
			(period, program, sumutil) = program_generate()
			while (util+sumutil/corenum) >= totalutil:
				(period, program, sumutil) = program_generate()
			(task, load) = task_generate(program, resourceaug)	
		else:
		#calculate real worstcase according to resourceaug
			(task, load) = task_generate(program, resourceaug)
		if load[0] == 0:
			programid -= 1
		else:
			#add release time and deadline
			program_deadline(period, task, load)
			#delete real worstcase
			program = del_worst_add_name(task, programid, period)
			#add program into taskset and update programid and util
			taskset += program
			#print util
			util += sumutil/corenum
	#print util
	#print taskset
	return programid, taskset, util

#resourceaug should be a list
#use different resourceaug to calculate deadline
def addprogram_diffaug(totalutil, resourceauglist):
	#(prog_id, period, seg_id, sub_count, worst_case, rel_tm, rel_dead, *priority, *[core])
	util = 0
	programid = 0
	taskset = []
	flag = 0
	for i in range(0, len(resourceauglist)):
		taskset.append([])
	while flag == 0:
		#generate a program
		programid += 1
		(period, program, sumutil) = program_generate()
		if (util+sumutil/corenum) >= totalutil:
			newutil = (totalutil - util)*corenum
			(program, newload) = change_program_sumutil(program, sumutil, newutil)
			sumutil = 1.0*newload/period
			flag = 1
		#calculate real worstcase according to resourceaug
		for i in range(0, len(resourceauglist)):
			(task,load) = task_generate(program, resourceauglist[i])
			if load[0] == 0:
				programid -= 1
				break
			else:
				#add release time and deadline
				program_deadline(period, task, load)
				#delete real worstcase
				newprogram = del_worst_add_name(task, programid, period)
				#add program into taskset and update programid and util
				taskset[i] += newprogram
		#print util
		util += sumutil/corenum
	#for item in taskset:
	#	print item
	#print util
	return programid, taskset		

def addoneprogram(programid, taskset, util, resourceaug):
	(period, program, sumutil) = program_generate()
	(task, load) = task_generate(program, resourceaug)
	if load[0] == 0:
		return programid, taskset, util
	newprogramid = programid + 1
	program_deadline(period, task, load)
	program = del_worst_add_name(task, newprogramid, period)
	newtaskset = taskset + program
	newutil = util + sumutil/corenum
	#print '\t', newtaskset
	return newprogramid, newtaskset, newutil

def print_task(dir, progid, task, period, segcount):
	outfile = open(dir+'/task'+str(progid)+'.txt', 'w')
	lines = str(period)+' '+str(segcount)+'\n'
	for seg in task:
		lines += str(seg[3])+' '+str(seg[4])+' '+str(seg[7])+' '+str(seg[5])+'\n'
		num = len(seg[8])
		for i in range(0, num-1):
			lines += str(seg[8][i])+' '
		lines += str(seg[8][num-1])+'\n'
	outfile.write(lines)
	outfile.close()

def print_taskset(dir, taskset, sched):
	progid = 0
	period = 0
	task = []
	segcount = 0
	for sub in taskset:
		if progid != sub[0]:
			if progid != 0:
				print_task(dir, progid, task, period, segcount)
			segcount = 0
			progid = sub[0]
			period = sub[1]
			task = []
		segcount += 1
		task.append(sub)
	print_task(dir, progid, task, period, segcount)
	outfile = open(dir+'/info', 'w')
	outfile.write(str(progid)+'\n'+str(sched)+'\n')
	outfile.close()
				
#0: ffit
#1: scan
#2: disff
#3: best
#4: boundff
#5: clustered
def utility_test(number, utility, resourceaug):
	cansche = 0
	cansche1 = 0
	cansche2 = 0
	cansche3 = 0
	cansche4 = 0
	cansche5 = 0
	cansche6 = 0
	matrix = [[0,0],[0,0]]
	matrix1 = [[0,0],[0,0]]
	matrix2 = [[0,0],[0,0]]
	matrix3 = [[0,0],[0,0]]
	matrix4 = [[0,0],[0,0]]
	matrix5 = [[0,0],[0,0]]
	matrix6 = [[0,0],[0,0]]
	notone = 0
	progcount = 0
	meanutil = 0
	i = 0
	#for 36#
	canfit = 0
	cannotfit = 101
	cannotsche = 0
	if doprint == 1:
		strutil = str(int(round(utility,5)*100))
		dir = strdir+'ffitaug5util'+strutil
		os.system('mkdir '+dir)
		dir = strdir+'scanaug5util'+strutil
		os.system('mkdir '+dir)
		dir = strdir+'disffaug5util'+strutil
		os.system('mkdir '+dir)
		dir = strdir+'bestaug5util'+strutil
		os.system('mkdir '+dir)
		dir = strdir+'boundffaug5util'+strutil
		os.system('mkdir '+dir)
	if doprint == 2:
		strutil = str(int(round(utility,5)*100))
		dir = strdir+'bestaug5util'+strutil
		os.system('mkdir '+dir)
	while i < number:
		#generate a taskset
		(prognum, taskset, util) = addprogram(utility, resourceaug)
		taskset5 = copy.deepcopy(taskset)
		taskset6 = copy.deepcopy(taskset)
		#assign priority
		taskset.sort(sortreldead)
		assign_priority(taskset)
		#assign core and schedule
#		taskset1 = copy.deepcopy(taskset)
#		taskset2 = copy.deepcopy(taskset)
		taskset3 = copy.deepcopy(taskset)
#		taskset4 = copy.deepcopy(taskset)
	#	(sche, corestr) = ffit_assign_core(taskset, prognum, corenum)
#		(sche1, corestr1) = scan_assign_core(taskset1, prognum, corenum)
#		(sche2, corestr2) = assign_core(taskset2, prognum, corenum)
		(sche3, corestr3) = best_assign_core(taskset3, prognum, corenum)
#		(sche4, corestr4) = bound_assign_core(taskset4, prognum, corenum)
		(sche5, corestr5, outinfo) = cluster_assign_core(taskset5, prognum, corenum, 5)
#		(sche6) = gedf_simple_test(taskset6, prognum, corenum)
		(sche6, corestr6, outinfo6) = cluster_assign_core(taskset6, prognum, corenum, 6)
		#pnum = 0
	#	for core in corestr:
	#		pnum = 0
	#		for prog in core:
	#			if prog != []:
	#				pnum+=1
	#		if pnum != 0 and pnum != 1:
	#			notone += 1
	#			break
	#	if pnum != 0 and pnum != 1:
			#number+=1
	#		continue
		#simulation
		progcount += prognum
		meanutil += util
	#	simu = simulation(corestr)
#		simu1 = simulation(corestr1)
#		simu2 = simulation(corestr2)
		simu3 = simulation(corestr3)
#		simu4 = simulation(corestr4)
		simu5 = cluster_simulation(corestr5, taskset5)
#		simu6 = gedf_simulation()
		simu6 = cluster_simulation(corestr6, taskset6)
	#	if sche == 0:
	#		cansche += 1
	#		if simu == 0:
	#			matrix[0][0]+=1
	#		else:
	#			matrix[0][1]+=1
	#	else:
	#		if simu == 0:
	#			matrix[1][0]+=1
	#		else:
	#			matrix[1][1]+=1
#		if sche1 == 0:
#			cansche1 += 1
#			if simu1 == 0:
#				matrix1[0][0]+=1
#			else:
#				matrix1[0][1]+=1
#		else:
#			if simu1 == 0:
#				matrix1[1][0]+=1
#			else:
#				matrix1[1][1]+=1
#		if sche2 == 0:
#			cansche2 += 1
#			if simu2 == 0:
#				matrix2[0][0]+=1
#			else:
#				matrix2[0][1]+=1
#		else:
#			if simu2 == 0:
#				matrix2[1][0]+=1
#			else:
#				matrix2[1][1]+=1
		if sche3 == 0:
			cansche3 += 1
			if simu3 == 0:
				matrix3[0][0]+=1
			else:
				matrix3[0][1]+=1
		else:
			if simu3 == 0:
				matrix3[1][0]+=1
			else:
				matrix3[1][1]+=1
#		if sche4 == 0:
#			cansche4 += 1
#			if simu4 == 0:
#				matrix4[0][0]+=1
#			else:
#				matrix4[0][1]+=1
#		else:
#			if simu4 == 0:
#				matrix4[1][0]+=1
#			else:
#				matrix4[1][1]+=1
		if sche5 == 0:
			cansche5 += 1
			if simu5 == 0:
				matrix5[0][0]+=1
			else:
				matrix5[0][1]+=1
		else:
		#elif sche5 == -1:
			if simu5 == 0:
				matrix5[1][0]+=1
			else:
				matrix5[1][1]+=1
#		if sche6 == 0:
#			cansche6 += 1
#			if simu6 == 0:
#				matrix6[0][0]+=1
#			else:
#				matrix6[0][1]+=1
#		else:
#			if simu6 == 0:
#				matrix6[1][0]+=1
#			else:
#				matrix6[1][1]+=1
		#for 36#
		if sche6 == 0 or sche6 == -10:
			cansche6 += 1
			if simu6 == 0:
			#if sche6 == -10:
				matrix6[0][0]+=1
			else:
				matrix6[0][1]+=1
		else:
			if simu6 == 0:
			#if sche6 == -11:
				matrix6[1][0]+=1
			else:
			#elif sche6 != 2:
				matrix6[1][1]+=1
		#else:
		#	print taskset, util
		#output taskset
		#for 36#
		if doprint == 2:
			if sche6 == -10 or sche6 == -11:
				canfit += 1
				taskset3.sort(sortprog)
				dir3 = strdir+'bestaug5util'+strutil+'/taskset'+str(canfit)
				os.system('mkdir '+dir3)
				print_taskset(dir3, taskset3, sche3)
			else:
				if sche6 == 2:
					cannotsche += 1
				cannotfit -= 1
				taskset3.sort(sortprog)
				dir3 = strdir+'bestaug5util'+strutil+'/taskset'+str(cannotfit)
				os.system('mkdir '+dir3)
				print_taskset(dir3, taskset3, sche3)

		if doprint == 1:
			taskset.sort(sortprog)
			dir = strdir+'ffitaug5util'+strutil+'/taskset'+str(i+1)
			os.system('mkdir '+dir)
			print_taskset(dir, taskset, sche)
			taskset1.sort(sortprog)
			dir1 = strdir+'scanaug5util'+strutil+'/taskset'+str(i+1)
			os.system('mkdir '+dir1)
			print_taskset(dir1, taskset1, sche1)
			taskset2.sort(sortprog)
			dir2 = strdir+'disffaug5util'+strutil+'/taskset'+str(i+1)
			os.system('mkdir '+dir2)
			print_taskset(dir2, taskset2, sche2)
			taskset3.sort(sortprog)
			dir3 = strdir+'bestaug5util'+strutil+'/taskset'+str(i+1)
			os.system('mkdir '+dir3)
			print_taskset(dir3, taskset3, sche3)
			taskset4.sort(sortprog)
			dir4 = strdir+'boundffaug5util'+strutil+'/taskset'+str(i+1)
			os.system('mkdir '+dir4)
			print_taskset(dir4, taskset4, sche4)
		i += 1
	#for 36#
	print matrix6, canfit, cannotfit, cannotsche
	if matrix[0][1]!=0 or matrix1[0][1]!=0 or matrix2[0][1]!=0 or matrix3[0][1]!=0 or matrix4[0][1]!=0 or matrix5[0][1]!=0 or matrix6[0][1]!=0:
		print '\twhat!!!!!!!!', matrix[0][1], matrix1[0][1], matrix2[0][1], matrix3[0][1], matrix4[0][1], matrix5[0][1], matrix6[0][1]
	print 100.0*cansche/number, 100.0*cansche1/number, 100.0*cansche2/number, 100.0*cansche3/number, 100.0*cansche4/number, 100.0*cansche5/number, 100.0*cansche6/number, 1.0*progcount/number, meanutil/number, corenum*meanutil/progcount
	#print matrix, notone
	print 100.0*(matrix[0][0]+matrix[1][0])/number, 100.0*(matrix1[0][0]+matrix1[1][0])/number, 100.0*(matrix2[0][0]+matrix2[1][0])/number, 100.0*(matrix3[0][0]+matrix3[1][0])/number, 100.0*(matrix4[0][0]+matrix4[1][0])/number, 100.0*(matrix5[0][0]+matrix5[1][0])/number, 100.0*(matrix6[0][0]+matrix6[1][0])/number
	return 100.0*cansche/number, 100.0*(matrix[0][0]+matrix[1][0])/number, 100.0*cansche1/number, 100.0*(matrix1[0][0]+matrix1[1][0])/number, 100.0*cansche2/number, 100.0*(matrix2[0][0]+matrix2[1][0])/number, 100.0*cansche3/number, 100.0*(matrix3[0][0]+matrix3[1][0])/number, 100.0*cansche4/number, 100.0*(matrix4[0][0]+matrix4[1][0])/number, 100.0*cansche5/number, 100.0*(matrix5[0][0]+matrix5[1][0])/number, 100.0*cansche6/number, 100.0*(matrix6[0][0]+matrix6[1][0])/number

#generate task set which has the most utility while still can be scheduled
#type = 1: disff
#type = 2: scan
#type = 3: ffit
def utility_schedule(number, resourceaug, type):
	progcount = 0
	meanutil = 0
	for i in range(0, number):
		rawtaskset = []
		newrawtaskset = []
		sche = 0
		prognum = 0
		taskset = []
		util = 0
		newprognum = 0
		newtaskset = []
		newutil = 0
		while sche == 0:
			rawtaskset = copy.deepcopy(newrawtaskset)
			taskset = copy.deepcopy(newtaskset)
			prognum = copy.copy(newprognum)
			util = copy.copy(newutil)
			(newprognum, newrawtaskset, newutil) = addoneprogram(prognum, rawtaskset, util, resourceaug)
			newtaskset = copy.deepcopy(newrawtaskset)
			newtaskset.sort(sortreldead)
			assign_priority(newtaskset)
			(sche, corestr) = assign_core(newtaskset, newprognum, corenum)
			#print newrawtaskset
	#	taskset.sort(sortprog)
		#print taskset
	#	dir = 'validation/taskset'+str(i+1)
	#	os.system('mkdir '+dir)
	#	print_taskset(dir, taskset, sche)
		progcount += prognum
		meanutil += util
	print 1.0*progcount/number, meanutil/number

#generate task set which has the most utility while still can be scheduled
def utility_schedule_compare(number, resourceaug):
	progcount1 = 0
	meanutil1 = 0
	progcount2 = 0
	meanutil2 = 0
	progcount3 = 0
	meanutil3 = 0
	for i in range(0, number):
		done1 = 0
		done2 = 0
		done3 = 0
		rawtaskset = []
		newrawtaskset = []
		sche1 = 0
		sche2 = 0
		sche3 = 0
		prognum = 0
		prognum1 = 0
		prognum2 = 0
		prognum3 = 0
		taskset = []
		taskset1 = []
		taskset2 = []
		taskset3 = []
		util = 0
		util1 = 0
		util2 = 0
		util3 = 0
		newprognum = 0
		#newtaskset = []
		newtaskset1 = []
		newtaskset2 = []
		newtaskset3 = []
		newutil = 0
		while done3 == 0 or done1 == 0 or done2 == 0:
			rawtaskset = copy.deepcopy(newrawtaskset)
			#taskset = copy.deepcopy(newtaskset)
			prognum = copy.copy(newprognum)
			util = copy.copy(newutil)
			if done1 == 0:
				taskset1 = copy.deepcopy(newtaskset1)
				prognum1 = copy.copy(newprognum)
				util1 = copy.copy(newutil)
			if done2 == 0:
				taskset2 = copy.deepcopy(newtaskset2)
				prognum2 = copy.copy(newprognum)
				util2 = copy.copy(newutil)
			if done3 == 0:
				taskset3 = copy.deepcopy(newtaskset3)
				prognum3 = copy.copy(newprognum)
				util3 = copy.copy(newutil)
			(newprognum, newrawtaskset, newutil) = addoneprogram(prognum, rawtaskset, util, resourceaug)
			newtaskset = copy.deepcopy(newrawtaskset)
			newtaskset.sort(sortreldead)
			assign_priority(newtaskset)
			if done1 == 0:
				newtaskset1 = copy.deepcopy(newtaskset)
				(sche1, corestr1) = assign_core(newtaskset1, newprognum, corenum)
				if sche1 == 1:
					done1 = 1
			if done2 == 0:
				newtaskset2 = copy.deepcopy(newtaskset)
				(sche2, corestr2) = scan_assign_core(newtaskset2, newprognum, corenum)
				if sche2 == 1:
					done2 = 1
			if done3 == 0:
				newtaskset3 = copy.deepcopy(newtaskset)
				(sche3, corestr3) = ffit_assign_core(newtaskset3, newprognum, corenum)
				if sche3 == 1:
					done3 = 1
		#	print '.',
		#print ''
		taskset1.sort(sortprog)
		dir1 = 'aug5boundaryn100/disff/taskset'+str(i+1)
		os.system('mkdir '+dir1)
		print_taskset(dir1, taskset1, done1)
		taskset2.sort(sortprog)
		dir2 = 'aug5boundaryn100/scan/taskset'+str(i+1)
		os.system('mkdir '+dir2)
		print_taskset(dir2, taskset2, done2)
		taskset3.sort(sortprog)
		dir3 = 'aug5boundaryn100/ffit/taskset'+str(i+1)
		os.system('mkdir '+dir3)
		print_taskset(dir3, taskset3, done3)
		progcount1 += prognum1
		meanutil1 += util1
		progcount2 += prognum2
		meanutil2 += util2
		progcount3 += prognum3
		meanutil3 += util3
	print 1.0*progcount1/number, meanutil1/number
	print 1.0*progcount2/number, meanutil2/number
	print 1.0*progcount3/number, meanutil3/number


def utility_diffaug(number, utility, resourceauglist):
	cansche = []
	better = []
	worse = []
	for i in range(0, len(resourceauglist)):
		cansche.append(0)
		better.append(0)
		worse.append(0)
	progcount = 0
	for i in range(0, number):
		#generate a tasksetlist
		(prognum, tasksetlist) = addprogram_diffaug(utility, resourceauglist)
		progcount += prognum
		sche5 = 0
		for i in range(0, len(resourceauglist)):
			each = tasksetlist[i]
			#assign priority
			each.sort(sortreldead)
			assign_priority(each)
			#assign core and schedule
			(sche, corestr) = assign_core(each, prognum, corenum)
			if sche == 0:
				cansche[i] += 1
			if i == 0:
				sche5 = sche
			else:
				if sche5 == 0 and sche == 1:
					worse[i] += 1
				elif sche5 == 1 and sche == 0:
					better[i] += 1
			#output taskset
			#each.sort(sortprog)
	for item in cansche:
		print 100.0*item/number
	for i in range(0, len(better)):
		print worse[i], better[i]
	#print 1.0*progcount/number
	

#beginutil = 0.05
beginutil = 0.2
utillist = []
#while beginutil < 0.16:
while beginutil < 0.91:
	utillist.append(beginutil)
	beginutil+=0.05
#print utillist
utillist = [0.2,0.3,0.4,0.5,0.6,0.7,0.8]
#utillist = [0.6, 0.65, 0.7, 0.75, 0.8, 0.85]
#utillist = [0.4, 0.5]
resourceauglist = [5, 4, 3, 2, 1]

#for resourceaug in resourceauglist:
#	for util in utillist:
#		utility_test(1000, util, resourceaug)
#	print '\n'

analysis = []
simulate = []
analysis1 = []
simulate1 = []
analysis2 = []
simulate2 = []
analysis3 = []
simulate3 = []
analysis4 = []
simulate4 = []
analysis5 = []
simulate5 = []
analysis6 = []
simulate6 = []
split = 1
setnumber = 100
for util in utillist:
	aa=0
	bb=0
	aa1=0
	bb1=0
	aa2=0
	bb2=0
	aa3=0
	bb3=0
	aa4=0
	bb4=0
	aa5=0
	bb5=0
	aa6=0
	bb6=0
	for i in range(0, split):
		(a, b, a1, b1, a2, b2, a3, b3, a4, b4, a5, b5, a6, b6) = utility_test(setnumber, util, 2)
		#(a, b, a1, b1, a2, b2, a3, b3, a4, b4) = utility_test(setnumber, util, 5)
		aa+=a
		bb+=b
		aa1+=a1
		bb1+=b1
		aa2+=a2
		bb2+=b2
		aa3+=a3
		bb3+=b3
		aa4+=a4
		bb4+=b4
		aa5+=a5
		bb5+=b5
		aa6+=a6
		bb6+=b6
	aa=aa/split
	bb=bb/split
	aa1=aa1/split
	bb1=bb1/split
	aa2=aa2/split
	bb2=bb2/split
	aa3=aa3/split
	bb3=bb3/split
	aa4=aa4/split
	bb4=bb4/split
	aa5=aa5/split
	bb5=bb5/split
	aa6=aa6/split
	bb6=bb6/split
	print '\t',aa,bb,aa1,bb1,aa2,bb2,aa3,bb3,aa4,bb4,aa5,bb5,aa6,bb6
	analysis.append(aa)
	simulate.append(bb)
	analysis1.append(aa1)
	simulate1.append(bb1)
	analysis2.append(aa2)
	simulate2.append(bb2)
	analysis3.append(aa3)
	simulate3.append(bb3)
	analysis4.append(aa4)
	simulate4.append(bb4)
	analysis5.append(aa5)
	simulate5.append(bb5)
	analysis6.append(aa6)
	simulate6.append(bb6)
print utillist
print analysis2
print simulate2
print analysis1
print simulate1
print analysis
print simulate
print analysis3
print simulate3
print analysis4
print simulate4
print analysis5
print simulate5
print analysis6
print simulate6

#for i in range(0,10):
#	utility_test(10000, 0.2, 5)
#utility_test(1000, 0.5, 2)
#utility_test(1000, 0.333, 3)
#utility_test(1000, 0.25, 4)
#utility_test(1000, 0.2, 5)

#utility_schedule_compare(100, 5)

#utility_schedule(100, 5)

#utility_test(100, 0.2, 5)

#for util in utillist:
#	utility_diffaug(10000, util, resourceauglist)
#	print ''











