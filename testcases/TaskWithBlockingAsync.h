#ifndef TASKWITHBLOCKINGASYNC_H
#define TASKWITHBLOCKINGASYNC_H

//Our includes
#include "cwTask.h"

class TaskWithBlockingAsync : public cwTask
{
    Q_OBJECT

public:
    TaskWithBlockingAsync();

    int result() { return Result; }

protected:
    void runTask() override;

private:
    int Result = 0;
};

#endif // TASKWITHBLOCKINGASYNC_H
