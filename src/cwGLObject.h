/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWGLOBJECT_H
#define CWGLOBJECT_H

//Qt includes
#include <QObject>

//Our includes
class cwCamera;
class cwShaderDebugger;
class cwGLViewer;
class cwScene;
class cwUpdateDataCommand;
#include "cwGeometryItersecter.h"

class cwGLObject : public QObject
{
public:
    cwGLObject(QObject* parent = NULL);
    ~cwGLObject();

    //These methods should only be called in the rendering thread
    virtual void initialize() = 0;
    virtual void draw() = 0;
    virtual void updateData();

    void setScene(cwScene *scene);
    cwScene *scene() const;

    cwGeometryItersecter* geometryItersecter() const;
    cwCamera* camera() const;
    cwShaderDebugger* shaderDebugger() const;

    void markDataAsDirty();

//    bool isDirty() const;

//protected:
//    void setDirty(bool isDirty);

private:
    cwScene* Scene;

    //This is the last QueuedDataCommand, if this isn't NULL
    //Then this command
    cwUpdateDataCommand* QueuedDataCommand;

//    bool Dirty;
};

//inline bool cwGLObject::isDirty() const
//{
//    return Dirty;
//}



/**
 * @brief cwGLObject::scene
 * @returns The scene that is resposible for this object
 */
inline cwScene *cwGLObject::scene() const
{
    return Scene;
}

#endif // CWGLOBJECT_H
