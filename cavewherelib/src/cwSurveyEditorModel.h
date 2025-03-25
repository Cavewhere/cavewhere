#ifndef CWSURVEXEDITORMODEL_H
#define CWSURVEXEDITORMODEL_H

//Our includes
#include "cwTrip.h"
#include "cwSurveyChunk.h"

//Qt includes
#include <QAbstractListModel>
#include <QPointer>
#include <QQmlEngine>


/**
 * @brief The cwSurveyEditorModel class
 *
 * This class transforms a cwTrip's cwSurveyChunk's into a data format that
 * can be read into by default Qt views like a Listview. This class doesn't
 * store any data. It only translate cwTrip's data into a list model
 */
class cwSurveyEditorModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SurveyEditorModel)

    Q_ENUMS(Roles)

    Q_PROPERTY(cwTrip* trip READ trip WRITE setTrip NOTIFY tripChanged)



public:
    cwSurveyEditorModel();

    enum Roles {
        StationNameRole,
        StationLeftRole,
        StationRightRole,
        StationUpRole,
        StationDownRole,
        ShotDistanceRole,
        ShotCompassRole,
        ShotBackCompassRole,
        ShotClinoRole,
        ShotBackClinoRole,
        ShotCalibrationRole,
        ChunkRole,
        StationVisibleRole,
        ShotVisibleRole,
        TitleVisibleRole,
        IndexInChunkRole
    };
    Q_ENUM(Roles)

    cwTrip* trip() const;
    void setTrip(cwTrip* trip);

    QVariant data(const QModelIndex &index, int role) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;

    QHash<int, QByteArray> roleNames() const;

    Q_INVOKABLE void addShotCalibration(int index);

private:

    struct ChunkIndex {
        cwSurveyChunk* chunk;
        int index;
    };



    class Range
    {
    public:
        explicit Range(int item) : mLow(item), mHigh(item) { }  // [item,item]
        Range(int low, int high) : mLow(low), mHigh(high) { }  // [low,high]
        Range() : mLow(-1), mHigh(-1) {}

        bool operator<(const Range& rhs) const
        {
            if (mLow < rhs.mLow && mHigh < rhs.mLow)
            {
                Q_ASSERT(mHigh < rhs.mLow); // sanity check
                return true;
            }
            return false;
        } // operator<

        int low() const { return mLow; }
        int high() const { return mHigh; }

    private:
        int mLow;
        int mHigh;
    };

    QPointer<cwTrip> m_trip; //!<
    // QMap<Range, cwSurveyChunk*> m_indexToChunk;
    int m_skipRowOffset = 1;

    void updateIndexToChunk();
    ChunkIndex modelIndexToStationChunk(const QModelIndex& index) const;
    ChunkIndex indexToStationChunk(int index) const;
    int chunkIndexToRow(const cwSurveyChunk* chunk, int chunkIndex) const;
    QModelIndex chunkIndexToModelIndex(const cwSurveyChunk* chunk, int chunkIndex) const;

    Roles chunkRoleToModelRole(cwSurveyChunk::DataRole chunkRole);


signals:
    void tripChanged();


};

/**
* @brief cwSurveyEditorModel::trip
* @return
*/
inline cwTrip* cwSurveyEditorModel::trip() const {
    return m_trip;
}

#endif // CWSURVEXEDITORMODEL_H
