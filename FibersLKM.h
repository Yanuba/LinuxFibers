#pragma once

#include <linux/ioctl.h>
 
#define IOCTL_MAGIC_NUM ']' //it is not used by any device driver (it should be safe)

#define IOCTL_EX _IO(IOCTL_MAGIC_NUM, 1)

struct fiber_struct {
    //
    //
    //
    //
    // Missing Parameters
    //
    //
    //
    //
    //
}