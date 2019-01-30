#!/bin/bash
make
sudo insmod FibersModule.ko
cd ./2018-fibers
make
./test 5
make clean
cd ..
make clean
rm Fibers_kioctls.o.*
sudo rmmod FibersModule
