#ifndef CWSURVEXEDITORMODEL_H
#define CWSURVEXEDITORMODEL_H

//Our includes
#include "cwTrip.h"
class cwSurveyChunk;

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
        ChunkIdRole,
        StationVisibleRole,
        ShotVisibleRole,
        IndexInChunkRole
    };

    cwTrip* trip() const;
    void setTrip(cwTrip* trip);

    QVariant data(const QModelIndex &index, int role) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;

    QHash<int, QByteArray> roleNames() const;

    Q_INVOKABLE void addShotCalibration(int index);

private:

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

    QPointer<cwTrip> Trip; //!<
    QMap<Range, cwSurveyChunk*> IndexToChunk;

    void updateIndexToChunk();
    QPair<cwSurveyChunk*, int> modelIndexToStationChunk(const QModelIndex& index) const;
    QPair<cwSurveyChunk*, int> indexToStationChunk(int index) const;


signals:
    void tripChanged();


};

/**
* @brief cwSurveyEditorModel::trip
* @return
*/
inline cwTrip* cwSurveyEditorModel::trip() const {
    return Trip;
}

#endif // CWSURVEXEDITORMODEL_H
