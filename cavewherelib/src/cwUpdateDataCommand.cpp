/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


//Our includes
#include "cwUpdateDataCommand.h"
#include "cwGLObject.h"

cwUpdateDataCommand::cwUpdateDataCommand()
{
}

void cwUpdateDataCommand::setGLObject(cwGLObject *object)
{
    Object = object;
}

void cwUpdateDataCommand::excute()
{
    if(!Object.isNull()) {
        Object->updateData();
    }
}
