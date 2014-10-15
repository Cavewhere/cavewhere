/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWEVENTRECORDERMODEL_H
#define CWEVENTRECORDERMODEL_H

//Qt includes
#include <QAbstractItemModel>

//3rd party includes
class QInputEventRecorder;

/**
 * @brief The cwEventRecorderModel class
 *
 * This class is used to record events from the main window. This allows for
 * events to be played back. This is useful for debugging complex ui interaction.
 */
class cwEventRecorderModel : public QAbstractItemModel
{
    Q_OBJECT

    Q_PROPERTY(QObject* rootEventObject READ rootEventObject WRITE setRootEventObject NOTIFY rootEventObjectChanged)

public:
    explicit cwEventRecorderModel(QObject *parent = 0);

    QObject* rootEventObject() const;
    void setRootEventObject(QObject* rootEventObject);

signals:
    void rootEventObjectChanged();

public slots:
    void startRecording();
    void stopRecording();
    void replayRecording(int index);

private:
    QInputEventRecorder* Recorder;



};


#endif // CWEVENTRECORDERMODEL_H
