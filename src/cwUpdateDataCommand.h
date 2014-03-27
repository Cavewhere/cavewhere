/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWUPDATEDATACOMMAND_H
#define CWUPDATEDATACOMMAND_H

//Our includes
#include <cwSceneCommand.h>
class cwGLObject;

//Qt includes
#include <QPointer>

class cwUpdateDataCommand : public cwSceneCommand
{
public:
    cwUpdateDataCommand();

    void setGLObject(cwGLObject* object);

    void excute();

private:
    QPointer<cwGLObject> Object;
};

#endif // CWUPDATEDATACOMMAND_H
