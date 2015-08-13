/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWMETACAVELOADTASK_H
#define CWMETACAVELOADTASK_H

//Our includes
#include <cwTask.h>

class cwMetaCaveLoadTask : public cwTask
{
public:
    cwMetaCaveLoadTask();

protected:
    void runTask();
};

#endif // CWMETACAVELOADTASK_H
