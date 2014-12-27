/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwEventRecorderModel.h"
#include "cwEventRecorderObject.h"
#include "cwDebug.h"
#include "cwMetaEvent.h"

//Qt includes
#include <QCoreApplication>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTabletEvent>
#include <QTouchEvent>
#include <QWheelEvent>
#include <QDebug>
#include <QUuid>
#include <QStandardPaths>
#include <QDir>
//#include <private/qobject_p.h>

cwEventRecorderModel::cwEventRecorderModel(QObject *parent) :
    QAbstractListModel(parent),
    Recording(false),
    ReplayTimer(new QTimer(this)),
    Acceleration(10.0)
{
    QCoreApplication::instance()->installEventFilter(this);

    ReplayTimer->setSingleShot(true);
    connect(ReplayTimer, &QTimer::timeout, this, &cwEventRecorderModel::playBackCurrentRecord);
}

/**
* @brief cwEventRecorderModel::setRootEventObject
* @param rootEventObject
*/
void cwEventRecorderModel::setRootEventObject(QObject* rootEventObject) {
    if(RootEventObject != rootEventObject) {
        RootEventObject = rootEventObject;
        emit rootEventObjectChanged();
    }
}

/**
 * @brief cwEventRecorderModel::eventFilter
 * @param object
 * @param event
 * @return Always return false
 *
 * This will filter all events that are children of rootEventObject from the QCoreApplication
 * This will do nothing if the recorder isn't recording. To start recording call startRecording
 */
bool cwEventRecorderModel::eventFilter(QObject *object, QEvent *event)
{
    if(Recording) {

        if(object != rootEventObject()) {
            return false;
        }

        QEvent* newEvent = cloneEvent(event);
        if(newEvent != nullptr) {
            EventRecord record;
            record.Event = newEvent;
            record.Object = object;
            record.ObjectMetaData = cwEventRecorderObject(object);
            record.TimeOffset = ElapsedTimer.elapsed();

            Q_ASSERT(!record.ObjectMetaData.fullName().isEmpty());

            ElapsedTimer.start();

            beginInsertRows(QModelIndex(), rowCount(QModelIndex()), rowCount(QModelIndex()));
            Records.append(record);
            endInsertRows();

            saveRecord(record);
        }
    }

    return false;
}

/**
 * @brief cwEventRecorderModel::cloneEvent
 * @param event
 * @return A cloned event
 *
 * Since events are owned by the event loop, this function allows use to clone the event so
 * we can save it.
 */
QEvent* cwEventRecorderModel::cloneEvent(QEvent *event) const
{
    if(event == nullptr) {
        return nullptr;
    }

    if (dynamic_cast<QContextMenuEvent*>(event))
        return new QContextMenuEvent(*static_cast<QContextMenuEvent*>(event));
    else if (dynamic_cast<QKeyEvent*>(event))
        return new QKeyEvent(*static_cast<QKeyEvent*>(event));
    else if (dynamic_cast<QMouseEvent*>(event))
        return new QMouseEvent(*static_cast<QMouseEvent*>(event));
    else if (dynamic_cast<QTabletEvent*>(event))
        return new QTabletEvent(*static_cast<QTabletEvent*>(event));
    else if (dynamic_cast<QTouchEvent*>(event))
        return new QTouchEvent(*static_cast<QTouchEvent*>(event));
    else if (dynamic_cast<QWheelEvent*>(event))
        return new QWheelEvent(*static_cast<QWheelEvent*>(event));
    if(event->type() == QEvent::MetaCall) {
        return nullptr;
//        return new cwMetaEvent();
    }

    return nullptr;
}

/**
 * @brief cwEventRecorderModel::saveRecord
 * @param record
 *
 * This function will save a signle record to the active TempFile. The TempFile will be flush
 * for every record incase the application crashes.
 */
void cwEventRecorderModel::saveRecord(const cwEventRecorderModel::EventRecord &record)
{
    DataStream << record.ObjectMetaData.fullName()
               << static_cast<QInputEvent*>(record.Event)
               << (qint32)record.TimeOffset;
    TempFile.flush();
}

/**
 * @brief cwEventRecorderModel::loadRecords
 * @param filename
 *
 * This will load records from a file. This will clear all the current records from the model
 */
void cwEventRecorderModel::loadRecords(QString filename)
{
    QFile file(filename);
    bool okay = file.open(QFile::ReadOnly);
    if(okay) {
        QList<EventRecord> newRecords;
        QDataStream stream(&file);
        while(!stream.atEnd()) {
            QByteArray fullName;
            QInputEvent* event = nullptr;
            qint32 timeOffset;
            stream >> fullName;
            stream >> event;
            stream >> timeOffset;

//            qDebug() << "Loaded:" << fullName; //.ObjectMetaData.fullName();
//            qDebug() << "Event:" << event;

            EventRecord record;
            record.Event = event;
            record.ObjectMetaData = cwEventRecorderObject(fullName);
            record.TimeOffset = timeOffset;

            newRecords.append(record);
        }

        beginResetModel();
        Records = newRecords;
        endResetModel();
    } else {
        qDebug() << "Couldn't load file:" << filename << file.errorString();
    }
}

/**
 * @brief cwEventRecorderModel::rowCount
 * @param parent
 * @return The number of event records in this model
 */
int cwEventRecorderModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return Records.size();
}

/**
 * @brief cwEventRecorderModel::columnCount
 * @param parent
 * @return Always returns 0
 */
int cwEventRecorderModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 0;
}

/**
 * @brief cwEventRecorderModel::data
 * @param index
 * @param role
 * @return
 *
 * Currently unimplemented, always returns QVariant();
 */
QVariant cwEventRecorderModel::data(const QModelIndex &index, int role) const
{
    Q_UNUSED(index);
    Q_UNUSED(role);
    return QVariant();
}

/**
 * @brief cwEventRecorderModel::startRecording
 *
 * Calling this function will case the model to start recording events. Events will automatically
 * be written to disk in the QStandardPaths::CacheLocation.
 *
 * If the model is already recording events, this will do nothing. If the model is replaying events
 * this function will also do nothing.
 *
 * This function will also clear all records in the model.
 */
void cwEventRecorderModel::startRecording()
{
    if(recording()) { return; }
    if(ReplayingRecords) { return; }

    beginResetModel();
    Records.clear();
    endResetModel();

    QUuid uuid = QUuid::createUuid();
    QString cacheLocation = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QString tempFilename = QString("%1/%2.cwEvents")
            .arg(cacheLocation)
            .arg(uuid.toString().replace(QRegExp("\\}|\\{"),""));

    Q_ASSERT(!TempFile.isOpen());

    QDir dir;
    dir.mkpath(cacheLocation);

    qDebug() << "Temp:" << tempFilename;

    TempFile.setFileName(tempFilename);
    bool okay = TempFile.open(QFile::WriteOnly);
    if(!okay) {
        qDebug() << "Can't open temp file" << tempFilename << TempFile.errorString() << LOCATION;
        return;
    }
    Q_ASSERT(okay);

    DataStream.setDevice(&TempFile);

    ElapsedTimer.start();

    Recording = true;
    emit recordingChanged();
}

/**
 * @brief cwEventRecorderModel::stopRecording
 *
 * This will cause the model to stop reording events. This function will do nothing if startRecording()
 * isn't called first.
 */
void cwEventRecorderModel::stopRecording()
{
    if(!recording()) { return; }
    if(ReplayingRecords) { return; }
    TempFile.close();
    Recording = false;
    emit recordingChanged();
}

/**
 * @brief cwEventRecorderModel::replayLastRecording
 *
 * This will replay last recorded records. If no records are currently found in this model.
 * This function will look up the last record from the QStandardPaths::CacheLocation and load
 * all the records from that location.
 */
void cwEventRecorderModel::replayLastRecording()
{
    if(recording()) { return; }
    if(ReplayingRecords) { return; }

    //Load the last recording from disk
    if(rowCount() == 0) {
        QString cacheLocation = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        QDir dir(cacheLocation);
        QStringList filenames = dir.entryList(QStringList(), QDir::Files, QDir::Time);
        if(!filenames.isEmpty()) {
            loadRecords(dir.absoluteFilePath(filenames.first()));
        }
    }

    ReplayingRecords = true;
    CurrentRecordIndex = 0;
    playBackCurrentRecord();

}

/**
 * @brief cwEventRecorderModel::playBackCurrentRecord
 *
 * This will push the current record's event to the event loop. If this function can't find the
 * record's object, the play back will stop and print out an error message.
 *
 * You can change the play back speed by adjusting the acceleration().
 */
void cwEventRecorderModel::playBackCurrentRecord()
{
    Q_ASSERT(ReplayingRecords);

    if(CurrentRecordIndex < Records.size()) {
        const EventRecord& record = Records.at(CurrentRecordIndex);

        QObject* object = cwEventRecorderObject::findObject(rootEventObject(), record.ObjectMetaData);
        if(object != nullptr) {
            QEvent* eventCopy = cloneEvent(record.Event);
            if(eventCopy != nullptr) {
                QCoreApplication::postEvent(object, eventCopy);
            }
        } else {
            qDebug() << "Couldn't find object" << record.ObjectMetaData.fullName() << LOCATION;
            qDebug() << "Play back stopped";
            CurrentRecordIndex = -1;
            ReplayingRecords = false;
            return;
        }

        CurrentRecordIndex++;

        if(CurrentRecordIndex < Records.size()) {
            const EventRecord& nextRecord = Records.at(CurrentRecordIndex);
            ReplayTimer->setInterval(nextRecord.TimeOffset / Acceleration);
            ReplayTimer->start();
        }
    } else {
        CurrentRecordIndex = -1;
        ReplayingRecords =  false;
    }
}

/**
* @brief cwEventRecorderModel::setAcceleration
* @param acceleration < 1.0 will make the playback slower that what was recorded.
* accelation == 1.0 will make the playback the same speed as what was recorded.
* accelation > 1.0 will make the playback faster than what was recorded.
*
* For example an accelation of 2.0 will make the play back go twice as fast as what was
* recorded.
*/
void cwEventRecorderModel::setAcceleration(double acceleration) {
    if(Acceleration != acceleration) {
        Acceleration = acceleration;
        emit accelerationChanged();
    }
}

/**
* @brief cwEventRecorderModel::rootEventObject
* @return The root event object that all other events will come from.
*/
QObject* cwEventRecorderModel::rootEventObject() const {
    return RootEventObject;
}

cwEventRecorderModel::EventRecord::EventRecord() :
    TimeOffset(0),
    Event(nullptr)
{

}
