/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWEVENTRECORDERMODEL_H
#define CWEVENTRECORDERMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QPointer>
#include <QAbstractNativeEventFilter>
#include <QFile>
#include <QDataStream>
#include <QElapsedTimer>
#include <QTimer>

//3rd party library
#include "eventserialization.h"

//Our includes
#include "cwEventRecorderObject.h"

/**
 * @brief The cwEventRecorderModel class
 *
 * This class is used to record events from the main window. This allows for
 * events to be played back. This is useful for debugging complex ui interaction.
 */
class cwEventRecorderModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(QObject* rootEventObject READ rootEventObject WRITE setRootEventObject NOTIFY rootEventObjectChanged)
    Q_PROPERTY(bool recording READ recording NOTIFY recordingChanged)
    Q_PROPERTY(double acceleration READ acceleration WRITE setAcceleration NOTIFY accelerationChanged)

public:
    explicit cwEventRecorderModel(QObject *parent = 0);

    QObject* rootEventObject() const;
    void setRootEventObject(QObject* rootEventObject);

    bool eventFilter(QObject* object, QEvent* event);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;

    bool recording() const;

    double acceleration() const;
    void setAcceleration(double acceleration);

signals:
    void rootEventObjectChanged();
    void recordingChanged();
    void accelerationChanged();

public slots:
    void startRecording();
    void stopRecording();
    void replayLastRecording();

    void playBackCurrentRecord();

private:
    class EventRecord {
    public:
        EventRecord();

        QPointer<QObject> Object;
        cwEventRecorderObject ObjectMetaData;
        int TimeOffset; //Time offset of the event ms

        QEvent* Event;
    };

    QList<EventRecord> Records;

    QPointer<QObject> RootEventObject;

    bool Recording;
    QFile TempFile;
    QDataStream DataStream;
    QElapsedTimer ElapsedTimer;

    //For replaying events
    QTimer* ReplayTimer;
    bool ReplayingRecords;
    int CurrentRecordIndex;
    double Acceleration;


    QEvent* cloneEvent(QEvent* event) const;

    void saveRecord(const EventRecord& record);
    void loadRecords(QString filename);

};

/**
* @brief cwEventRecorderModel::Recording
* @return True if the model is currently recording events and false if it isn't.
*/
inline bool cwEventRecorderModel::recording() const {
    return Recording;
}

/**
* @brief cwEventRecorderModel::acceleration
* @return The acceleration of the playback of the model
*
* See setAcceleration()
*/
inline double cwEventRecorderModel::acceleration() const {
    return Acceleration;
}



#endif // CWEVENTRECORDERMODEL_H
