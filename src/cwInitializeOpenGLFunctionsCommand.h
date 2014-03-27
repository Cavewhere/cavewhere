/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWINITIALIZEOPENGLFUNCTIONSCOMMAND_H
#define CWINITIALIZEOPENGLFUNCTIONSCOMMAND_H

//Our includes
#include "cwSceneCommand.h"

//Qt includes
class QOpenGLFunctions;

class cwInitializeOpenGLFunctionsCommand : public cwSceneCommand
{
public:
    cwInitializeOpenGLFunctionsCommand();

    void setOpenGLFunctionsObject(QOpenGLFunctions* functionsObject);

    void excute();

private:
    QOpenGLFunctions* FunctionsObject;
};

#endif // CWINITIALIZEOPENGLFUNCTIONSCOMMAND_H
