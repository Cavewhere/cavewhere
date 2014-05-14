/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWSCREENCAPTURECOMMAND_H
#define CWSCREENCAPTURECOMMAND_H

//Our includes
#include "cwSceneCommand.h"
class cwScene;
class cwCamera;

//Qt includes
#include <QPointer>
#include <QImage>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>

class cwScreenCaptureCommand : public QObject, public cwSceneCommand, public QOpenGLFunctions
{
    Q_OBJECT

public:
    cwScreenCaptureCommand();

    void setScene(cwScene* scene);
    void setCamera(cwCamera* camera);

    void excute();

    void setId(int id);

signals:
    void createdImage(QImage image, int id);

private:
    QPointer<cwScene> Scene;
    QPointer<cwCamera> Camera;
    int Id;
};

#endif // CWSCREENCAPTURECOMMAND_H
