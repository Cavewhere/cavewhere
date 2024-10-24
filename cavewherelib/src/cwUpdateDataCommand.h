/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWUPDATEDATACOMMAND_H
#define CWUPDATEDATACOMMAND_H

//Our includes
#include "cwSceneCommand.h"
class cwRenderObject;

//Qt includes
#include <QPointer>

class cwUpdateDataCommand : public cwSceneCommand
{
public:
    cwUpdateDataCommand();

    void setGLObject(cwRenderObject* object);

    void excute();

private:
    QPointer<cwRenderObject> Object;
};

#endif // CWUPDATEDATACOMMAND_H
