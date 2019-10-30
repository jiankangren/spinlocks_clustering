#!/bin/bash

PROC_NUM=12
NUM_RESOURCES=1

for num_req in 256
do
	path='core='${PROC_NUM}'resources='${NUM_RESOURCES}'requests='${num_req}'type=mod'
	
	for i in {1..100}
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