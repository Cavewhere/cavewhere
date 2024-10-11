/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


//Our includes
#include "cwInitializeOpenGLFunctionsCommand.h"

//Qt includes
#include <QOpenGLFunctions>

cwInitializeOpenGLFunctionsCommand::cwInitializeOpenGLFunctionsCommand()
{
}

void cwInitializeOpenGLFunctionsCommand::setOpenGLFunctionsObject(QOpenGLFunctions *functionsObject)
{
    FunctionsObject = functionsObject;
}

void cwInitializeOpenGLFunctionsCommand::excute()
{
    FunctionsObject->initializeOpenGLFunctions();
}
