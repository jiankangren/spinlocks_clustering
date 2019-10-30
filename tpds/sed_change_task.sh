#!/bin/bash

for num_req in 128 256 384 512 640 768 896 1024
do
	path='data_tpds/core=36util=0.75resources=1requests='${num_req}'type=mod'
	for i in {1..100}
	do
		file=${path}'/taskset'${i}'_prio.rtps'
		sed -i "s/synthetic_task_fast/synthetic_task_wlocks/g" ${file}
	done
done