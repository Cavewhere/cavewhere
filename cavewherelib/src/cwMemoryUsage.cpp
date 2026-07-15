/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwMemoryUsage.h"

#ifdef Q_OS_MACOS
#include <mach/mach.h>

qint64 cwMemoryUsage::physFootprint()
{
    task_vm_info_data_t info;
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if(task_info(mach_task_self(), TASK_VM_INFO,
                 reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS) {
        return -1;
    }
    return static_cast<qint64>(info.phys_footprint);
}

#else

qint64 cwMemoryUsage::physFootprint()
{
    return -1;
}

#endif

qint64 cwMemoryUsage::physFootprintMB()
{
    const qint64 bytes = physFootprint();
    return bytes < 0 ? -1 : bytes / (1024 * 1024);
}
