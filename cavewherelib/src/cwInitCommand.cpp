/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#include "cwInitCommand.h"
#include "cwRenderObject.h"

cwInitCommand::cwInitCommand()
{
}

/**
 * @brief cwInitCommand::setGLObject
 * @param glObject
 */
void cwInitCommand::setGLObject(cwRenderObject *glObject)
{
    GLObject = glObject;
}

/**
 * @brief cwInitCommand::excute
 */
void cwInitCommand::excute()
{
    if(!GLObject.isNull()) {
        // GLObject->initilizeGLFunctions();
        GLObject->initialize();
    }
}
