/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWCAPTURESCENECOMMAND_H
#define CWCAPTURESCENECOMMAND_H

//Qt includes
#include <QObject>
#include <QOpenGLFramebufferObject>
#include <QPointer>
#include <QOpenGLFunctions>

//Our includes
#include "cwSceneCommand.h"
class cwScene;
class cwCamera;

class cwCaptureSceneCommand : public QObject, public cwSceneCommand, private QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit cwCaptureSceneCommand(QObject *parent = 0);

    void setScene(cwScene* scene);
    cwScene* scene() const;

    void setCamera(cwCamera* camera);
    cwCamera* camera() const;

    void excute();

signals:
    void imageCreated(QImage image); //This is the image that's captured

public slots:

private:
    QPointer<cwScene> Scene;
    QPointer<cwCamera> Camera;

    GLint OldFramebufferObject;
    QOpenGLFramebufferObject* FramebufferObject;

    void initilizeFramebuffer();

};

#endif // CWCAPTURESCENECOMMAND_H
