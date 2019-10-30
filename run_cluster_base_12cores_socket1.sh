#!/bin/bash

PROC_NUM=12
NUM_RESOURCES=1
TOTAL_UTIL=0.5

for num_req in 128 256 384 512 640 768 896 1024
do
	path='ecrts_data_base/core='${PROC_NUM}'util='${TOTAL_UTIL}'resources='${NUM_RESOURCES}'requests='${num_req}'type=mod'
	
	for i in {36..70}
	do
		# Create output folder beforehand
		output_folder=${path}'/taskset'${i}_'output'
		mkdir ${output_folder}

		fifo_rtps=${path}'/taskset'${i}'_fifo'
		prio_rtps=${path}'/taskset'${i}'_prio'
		
		./clustering_launcher_wlocks2 ${fifo_rtps}
		# Sleep 2 seconds between runs
		sleep 2s
		./clustering_launcher_wlocks2 ${prio_rtps}
		sleep 2s
	done
done

echo "RT-OMP cluster finished!"