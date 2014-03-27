/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#include "cwInitCommand.h"
#include "cwGLObject.h"

cwInitCommand::cwInitCommand()
{
}

/**
 * @brief cwInitCommand::setGLObject
 * @param glObject
 */
void cwInitCommand::setGLObject(cwGLObject *glObject)
{
    GLObject = glObject;
}

/**
 * @brief cwInitCommand::excute
 */
void cwInitCommand::excute()
{
    if(!GLObject.isNull()) {
        GLObject->initialize();
    }
}
