#!/bin/bash

for req in 128 256 384 512 640 768 896 1024
do
	folder='data_tpds/core=36n=8util=0.75resources=1requests='${req}'type=mod'
	dm_folder=$folder'/dm_prio_assignment'

	mkdir $dm_folder

	for i in {1..100}
	do
		mv $folder'/taskset'$i'_fifo.rtps' $dm_folder
		mv $folder'/taskset'$i'_prio_dm.rtps' $dm_folder

		mv $dm_folder'/taskset'$i'_prio_dm.rtps' $dm_folder'/taskset'$i'_prio.rtps'
		mv $folder'/taskset'$i'_prio_opt.rtps' $folder'/taskset'$i'_prio.rtps'
	done
done