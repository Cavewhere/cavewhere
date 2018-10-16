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
#include <QSharedPointer>

/**
 * @brief The cwScreenCaptureCommand class
 *
 * A command is generated and a Qt3D Capture will generate the image for it. Once
 * the image is stored the command will be used to update a QGraphicsScene.
 */
class cwScreenCaptureCommand : public QObject
{
    Q_OBJECT

    Q_PROPERTY(cwCamera* camera READ camera WRITE setCamera NOTIFY cameraChanged)
    Q_PROPERTY(QPointF origin READ origin WRITE setOrigin NOTIFY originChanged)
    Q_PROPERTY(QImage image READ image WRITE setImage NOTIFY imageChanged)

public:
    cwScreenCaptureCommand(QObject* parent = nullptr);
    virtual ~cwScreenCaptureCommand();

    cwCamera* camera() const;
    void setCamera(cwCamera* camera);

    QPointF origin() const;
    void setOrigin(QPointF origin);

    QImage image() const;
    void setImage(QImage image);

signals:
    void cameraChanged();
    void originChanged();
    void imageChanged();

private:
    QPointer<cwCamera> Camera;
    QPointF Origin; //!<
    QImage Image; //!<
};



/**
* @brief cwScreenCaptureCommand::origin
* @return The origin of the image in the command
*/
inline QPointF cwScreenCaptureCommand::origin() const {
    return Origin;
}

/**
* @brief cwScreenCaptureCommand::image
* @return The image that command will store
*/
inline QImage cwScreenCaptureCommand::image() const {
    return Image;
}


#endif // CWSCREENCAPTURECOMMAND_H
