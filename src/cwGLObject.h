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
#include <QOpenGLFunctions>
class QOpenGLShaderProgram;

//Our includes
class cwCamera;
class cwShaderDebugger;
class cwGLViewer;
class cwScene;
class cwUpdateDataCommand;
#include "cwGeometryItersecter.h"

class cwGLObject : public QObject, protected QOpenGLFunctions
{
public:
    cwGLObject(QObject* parent = nullptr);
    ~cwGLObject();

    //These methods should only be called in the rendering thread
    void initilizeGLFunctions();
    virtual void initialize() = 0;
    virtual void releaseResources() = 0;
    virtual void draw() = 0;
    virtual void updateData();

    void setScene(cwScene *scene);
    cwScene *scene() const;

    cwGeometryItersecter* geometryItersecter() const;
    cwCamera* camera() const;
    cwShaderDebugger* shaderDebugger() const;

    void markDataAsDirty();

    static void deleteShaders(QOpenGLShaderProgram* program);


private:
    cwScene* Scene;

    //This is the last QueuedDataCommand, if this isn't nullptr
    //Then this command
    cwUpdateDataCommand* QueuedDataCommand;

protected:

};

/**
 * @brief cwGLObject::scene
 * @returns The scene that is resposible for this object
 */
inline cwScene *cwGLObject::scene() const
{
    return Scene;
}

#endif // CWGLOBJECT_H
