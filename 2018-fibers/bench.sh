#!/bin/bash

for (( i=0; i<$2; i++ ))
do
	gnome-terminal -- ./exec_one_test.sh $1 $i
done
