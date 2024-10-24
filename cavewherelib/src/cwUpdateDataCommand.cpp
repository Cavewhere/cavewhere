/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


//Our includes
#include "cwUpdateDataCommand.h"
#include "cwRenderObject.h"

cwUpdateDataCommand::cwUpdateDataCommand()
{
}

void cwUpdateDataCommand::setGLObject(cwRenderObject *object)
{
    Object = object;
}

void cwUpdateDataCommand::excute()
{
    // if(!Object.isNull()) {
    //     Object->updateData();
    // }
}
