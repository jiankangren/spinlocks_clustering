#!/bin/bash

PROC_NUM=36
NUM_RESOURCES=1
TOTAL_UTIL=0.75

for num_req in 128
do
	path='data_tpds/core='${PROC_NUM}'util='${TOTAL_UTIL}'resources='${NUM_RESOURCES}'requests='${num_req}'type=mod/dm_prio_assignment'
	
	for i in {1..1}
	do
		# Create output folder beforehand
		output_folder=${path}'/taskset'${i}_'output'
		mkdir ${output_folder}

		fifo_rtps=${path}'/taskset'${i}'_fifo'
		prio_rtps=${path}'/taskset'${i}'_prio'
		
		./clustering_launcher_wlocks ${fifo_rtps}
		# Sleep 2 seconds between runs
		sleep 2s
		./clustering_launcher_wlocks ${prio_rtps}
		#sleep 2s
	done
done

echo "RT-OMP cluster finished!"