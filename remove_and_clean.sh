#!/bin/bash
sudo rmmod FibersModule
cd ./Kernelspace
make clean
cd ../Userspace
make clean
cd ../2018-fibers
make clean
