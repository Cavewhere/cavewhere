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

    Q_PROPERTY(cwTrip* trip READ trip WRITE setTrip NOTIFY tripChanged)

public:
    cwSurveyEditorModel();

    enum RowType {
        TitleRow,
        StationRow,
        ShotRow
    };
    Q_ENUM(RowType)

    enum Role {
        RowTypeRole,
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
        // StationVisibleRole,
        // ShotVisibleRole,
        // TitleVisibleRole,
        IndexInChunkRole
    };
    Q_ENUM(Role)

    cwTrip* trip() const;
    void setTrip(cwTrip* trip);

    QVariant data(const QModelIndex &index, int role) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;

    QHash<int, QByteArray> roleNames() const;

    Q_INVOKABLE void addShotCalibration(int index);

private:
    struct ChunkIndex {
        cwSurveyChunk* chunk;
        RowType type;
        int index;
    };

    QPointer<cwTrip> m_trip; //!<
    // QMap<Range, cwSurveyChunk*> m_indexToChunk;
    const int m_titleRowOffset = 1;

    ChunkIndex toChunkIndex(const QModelIndex& index) const;
    ChunkIndex toChunkIndex(int index) const;
    int toRow(RowType type, const cwSurveyChunk* chunk, int chunkIndex) const;
    QModelIndex toModelIndex(RowType type, const cwSurveyChunk* chunk, int chunkIndex) const;

    Role toModelRole(cwSurveyChunk::DataRole chunkRole);
    RowType toRowType(cwSurveyChunk::DataRole chunkRole);


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
