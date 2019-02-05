#!/bin/bash
sudo rmmod FibersModule
cd ./Kernelspace
make
sudo insmod FibersModule.ko
cd ../Userspace
make
cd ../2018-fibers
make
