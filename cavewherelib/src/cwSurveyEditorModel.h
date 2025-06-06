#ifndef CWSURVEXEDITORMODEL_H
#define CWSURVEXEDITORMODEL_H

//Our includes
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwSurveyEditorRowIndex.h"
#include "cwSurveyEditorBoxIndex.h"
#include "CaveWhereLibExport.h"

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
class CAVEWHERE_LIB_EXPORT cwSurveyEditorModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SurveyEditorModel)

    Q_PROPERTY(cwTrip* trip READ trip WRITE setTrip NOTIFY tripChanged)

public:
    cwSurveyEditorModel();

    enum Role {
        //The type of role: TitleRow, StationRow, ShotRow
        RowIndexRole,

        //Station Data Roles
        StationNameRole,
        StationLeftRole,
        StationRightRole,
        StationUpRole,
        StationDownRole,

        //Shot Data Roles
        ShotDistanceRole,
        ShotCompassRole,
        ShotBackCompassRole,
        ShotClinoRole,
        ShotBackClinoRole,
        ShotCalibrationRole,
    };
    Q_ENUM(Role)

    cwTrip* trip() const;
    void setTrip(cwTrip* trip);

    QVariant data(const QModelIndex &index, int role) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;


    QHash<int, QByteArray> roleNames() const;

    Q_INVOKABLE void addShotCalibration(int index);

    Q_INVOKABLE int toModelRow(const cwSurveyEditorRowIndex& rowIndex) const;

    Q_INVOKABLE cwSurveyEditorRowIndex rowIndex(cwSurveyChunk *chunk, int chunkIndex, cwSurveyEditorRowIndex::RowType type) const
    {
        return cwSurveyEditorRowIndex(chunk, chunkIndex, type);
    }
    Q_INVOKABLE cwSurveyEditorBoxIndex boxIndex(const cwSurveyEditorRowIndex& rowIndex, cwSurveyChunk::DataRole dataRole) const
    {
        return cwSurveyEditorBoxIndex(rowIndex, dataRole);
    }
    Q_INVOKABLE cwSurveyEditorBoxIndex boxIndex(cwSurveyChunk *chunk,
                                                int chunkIndex,
                                                cwSurveyEditorRowIndex::RowType type,
                                                cwSurveyChunk::DataRole dataRole) const
    {
        return boxIndex(rowIndex(chunk, chunkIndex, type), dataRole);
    }
    Q_INVOKABLE cwSurveyEditorBoxIndex boxIndex() const { return cwSurveyEditorBoxIndex(); }

    Q_INVOKABLE cwSurveyEditorBoxIndex offsetBoxIndex(const cwSurveyEditorBoxIndex& boxIndex, int offsetIndex) const;

private:
    QPointer<cwTrip> m_trip; //!<

    const int m_titleRowOffset = 1;

    cwSurveyEditorRowIndex toRowIndex(const QModelIndex& index) const;
    cwSurveyEditorRowIndex toRowIndex(int index) const;
    QModelIndex toModelIndex(const cwSurveyEditorRowIndex& rowIndex) const;

    Role toModelRole(cwSurveyChunk::DataRole chunkRole) const;
    cwSurveyEditorRowIndex::RowType toRowType(cwSurveyChunk::DataRole chunkRole) const;


signals:
    void tripChanged();

    //Called when a chunk has been added to the end of the model
    void lastChunkAdded();
};

/**
* @brief cwSurveyEditorModel::trip
* @return
*/
inline cwTrip* cwSurveyEditorModel::trip() const {
    return m_trip;
}

#endif // CWSURVEXEDITORMODEL_H
