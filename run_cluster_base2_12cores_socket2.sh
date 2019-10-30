#!/bin/bash

PROC_NUM=12
NUM_REQ=128
TOTAL_UTIL=0.75

for num_res in 1 3 5 7 9 11 13 15
do
	path='ecrts_data_base/varying_num_requests/core='${PROC_NUM}'util='${TOTAL_UTIL}'resources='${num_res}'requests='${NUM_REQ}'type=mod'
	
	for i in {71..100}
	do
		# Create output folder beforehand
		output_folder=${path}'/taskset'${i}_'output'
		mkdir ${output_folder}

		fifo_rtps=${path}'/taskset'${i}'_fifo'
		prio_rtps=${path}'/taskset'${i}'_prio'
		
		./clustering_launcher_wlocks3 ${fifo_rtps}
		# Sleep 2 seconds between runs
		sleep 2s
		./clustering_launcher_wlocks3 ${prio_rtps}
		sleep 2s
	done
done

echo "RT-OMP cluster finished!"