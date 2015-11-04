/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef SURVEYCHUNKSIGNALERSLOTHELPER_H
#define SURVEYCHUNKSIGNALERSLOTHELPER_H

#include <QObject>
#include <QPair>

typedef QPair<int, int> BeginEndPair;

class SurveyChunkSignalerSlotHelper : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject* caveNameChanged READ caveNameChanged CONSTANT)
    Q_PROPERTY(QObject* tripNameChanged READ tripNameChanged CONSTANT)
    Q_PROPERTY(QObject* chunkSender READ chunkSender CONSTANT)
    Q_PROPERTY(BeginEndPair chunkStationAddedIndexes READ chunkStationAddedIndexes CONSTANT)
    Q_PROPERTY(QObject* calibrationSender READ calibrationSender CONSTANT)

public:
    explicit SurveyChunkSignalerSlotHelper(QObject *parent = 0);

    QObject* caveNameChanged() const;
    QObject* tripNameChanged() const;
    QObject* chunkSender() const;
    QObject* calibrationSender() const;
    BeginEndPair chunkStationAddedIndexes() const;

signals:

public slots:

    //Cave
    void caveNameChangedCalled();
    void tripNameChangedCalled();
    void calibrationChangedCalled();
    void chunkStationAdded(int begin, int end);

private:
    QObject* CaveNameChanged; //!<
    QObject* TripNameChanged; //!<
    QObject* ChunkSender; //!<
    QObject* CalibrationSender;
    BeginEndPair StationAddedIndexes; //!<
};

/**
* @brief SurveyChunkSignalerSlotHelper::nameChanged
* @return
*/
inline QObject *SurveyChunkSignalerSlotHelper::caveNameChanged() const {
    return CaveNameChanged;
}

/**
* @brief class::tripNameChanged
* @return
*/
inline QObject* SurveyChunkSignalerSlotHelper::tripNameChanged() const {
    return TripNameChanged;
}

/**
* @brief SurveyChunkSignalerSlotHelper::chunkSender
* @return
*/
inline QObject* SurveyChunkSignalerSlotHelper::chunkSender() const {
    return ChunkSender;
}

/**
* @brief SurveyChunkSignalerSlotHelper::chunkStationAddedIndexes
* @return
*/
inline BeginEndPair SurveyChunkSignalerSlotHelper::chunkStationAddedIndexes() const {
    return StationAddedIndexes;
}

/**
* @brief SurveyChunkSignalerSlotHelper::calibrationSender
* @return
*/
inline QObject* SurveyChunkSignalerSlotHelper::calibrationSender() const {
    return CalibrationSender;
}

#endif // SURVEYCHUNKSIGNALERSLOTHELPER_H
