#!/bin/bash

PROC_NUM=12

for num_res in 1
do
	for num_req in 128
	do
		for i in {1..100}
		do
			python taskgen_wlocks.py ${PROC_NUM} ${num_res} ${num_req} mod
		done
	done
done

echo "Finished!"