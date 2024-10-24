/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWINITCOMMAND_H
#define CWINITCOMMAND_H

//Our includes
#include "cwSceneCommand.h"
class cwRenderObject;

//Qt includes
#include <QPointer>

/**
 * @brief The cwInitCommand class
 *
 * This scene command will call initialize on cwGLObject
 */
class cwInitCommand : public cwSceneCommand
{
public:
    cwInitCommand();

    void setGLObject(cwRenderObject* glObject);

    void excute();

private:
    QPointer<cwRenderObject> GLObject;

};

#endif // CWINITCOMMAND_H
