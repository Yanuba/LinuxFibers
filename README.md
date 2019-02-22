# UnixFibers
Implementation of Windows fibers in Unix using a LKM

## How it works
It is an attempt to implement Windows Fibers in Linux Kernel, using a Loadable Kernel module.
It relies on a char device in order to implemnet the calls. 
There is also an Intefrace that allow user process to use the facilities.
