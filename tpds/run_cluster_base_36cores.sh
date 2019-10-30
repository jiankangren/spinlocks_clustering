#!/bin/bash

PROC_NUM=36
NUM_RESOURCES=1
TOTAL_UTIL=0.75
NUM_TASKS=8

for num_req in 768 896 1024
do
	opt_path='data_tpds/core='${PROC_NUM}'n='${NUM_TASKS}'util='${TOTAL_UTIL}'resources='${NUM_RESOURCES}'requests='${num_req}'type=mod'
	path=${opt_path}'/dm_prio_assignment'
	for i in {1..100}
	do
		# Create output folder beforehand
		output_folder=${path}'/taskset'${i}'_output'
		mkdir ${output_folder}

		fifo_rtps=${path}'/taskset'${i}'_fifo'
		prio_rtps=${path}'/taskset'${i}'_prio'

		# Sleep 3 seconds between runs	
		./clustering_launcher_wlocks ${fifo_rtps}
		sleep 3s
		./clustering_launcher_wlocks ${prio_rtps}
		sleep 3s

		# Run optimal locking priority task set
		opt_output_folder=${opt_path}'/taskset'${i}'_output'
		mkdir ${opt_output_folder}

		opt_prio_rtps=${opt_path}'/taskset'${i}'_prio'
		./clustering_launcher_wlocks ${opt_prio_rtps}
		sleep 3s
	done
done

echo "RT-OMP cluster finished!"