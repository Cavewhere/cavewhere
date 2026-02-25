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
#include <QPersistentModelIndex>
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
    Q_PROPERTY(int focusedRow READ focusedRow NOTIFY focusedRowChanged)
    Q_PROPERTY(int focusedRole READ focusedRole NOTIFY focusedRoleChanged)

public:
    cwSurveyEditorModel();

    enum NavigationKey {
        Tab,
        BackTab,
        Left,
        Right,
        Up,
        Down
    };
    Q_ENUM(NavigationKey)

    enum Role {
        //The type of role: TitleRow, StationRow, ShotRow
        RowIndexRole,
        RowTypeRole,
        IndexInChunkRole,
        ChunkRole,
        IsVirtualRole,

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
    Q_INVOKABLE void setFocusedChunk(cwSurveyChunk* chunk);

    QVariant data(const QModelIndex &index, int role) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    Q_INVOKABLE bool setData(const cwSurveyEditorBoxIndex& boxIndex, const QVariant& data);
    Q_INVOKABLE bool setDataAt(int modelRow, cwSurveyChunk::DataRole dataRole, const QVariant& data);
    Q_INVOKABLE bool shotDistanceIncluded(const cwSurveyEditorBoxIndex& boxIndex) const;
    Q_INVOKABLE bool shotDistanceIncludedAt(int modelRow, cwSurveyChunk::DataRole dataRole) const;
    Q_INVOKABLE bool canRemoveStation(const cwSurveyEditorBoxIndex& boxIndex, cwSurveyChunk::Direction direction) const;
    Q_INVOKABLE bool canRemoveStationAt(int modelRow, cwSurveyChunk::DataRole dataRole, cwSurveyChunk::Direction direction) const;
    Q_INVOKABLE bool canRemoveShot(const cwSurveyEditorBoxIndex& boxIndex, cwSurveyChunk::Direction direction) const;
    Q_INVOKABLE bool canRemoveShotAt(int modelRow, cwSurveyChunk::DataRole dataRole, cwSurveyChunk::Direction direction) const;
    Q_INVOKABLE bool canInsertStation(const cwSurveyEditorBoxIndex& boxIndex, cwSurveyChunk::Direction direction) const;
    Q_INVOKABLE bool canInsertStationAt(int modelRow, cwSurveyChunk::DataRole dataRole, cwSurveyChunk::Direction direction) const;
    Q_INVOKABLE bool canInsertShot(const cwSurveyEditorBoxIndex& boxIndex, cwSurveyChunk::Direction direction) const;
    Q_INVOKABLE bool canInsertShotAt(int modelRow, cwSurveyChunk::DataRole dataRole, cwSurveyChunk::Direction direction) const;
    Q_INVOKABLE void removeStation(const cwSurveyEditorBoxIndex& boxIndex, cwSurveyChunk::Direction direction);
    Q_INVOKABLE void removeStationAt(int modelRow, cwSurveyChunk::DataRole dataRole, cwSurveyChunk::Direction direction);
    Q_INVOKABLE void removeShot(const cwSurveyEditorBoxIndex& boxIndex, cwSurveyChunk::Direction direction);
    Q_INVOKABLE void removeShotAt(int modelRow, cwSurveyChunk::DataRole dataRole, cwSurveyChunk::Direction direction);
    Q_INVOKABLE void insertStation(const cwSurveyEditorBoxIndex& boxIndex, cwSurveyChunk::Direction direction);
    Q_INVOKABLE void insertStationAt(int modelRow, cwSurveyChunk::DataRole dataRole, cwSurveyChunk::Direction direction);
    Q_INVOKABLE void insertShot(const cwSurveyEditorBoxIndex& boxIndex, cwSurveyChunk::Direction direction);
    Q_INVOKABLE void insertShotAt(int modelRow, cwSurveyChunk::DataRole dataRole, cwSurveyChunk::Direction direction);


    QHash<int, QByteArray> roleNames() const;

    Q_INVOKABLE void addShotCalibration(int index);

    Q_INVOKABLE cwSurveyChunk* chunkForRow(int modelRow) const;
    Q_INVOKABLE bool isCellValid(int modelRow, cwSurveyChunk::DataRole role) const;
    Q_INVOKABLE int modelRowForChunkRole(cwSurveyChunk* chunk, int indexInChunk, cwSurveyChunk::DataRole role) const;
    Q_INVOKABLE bool isStationRole(cwSurveyChunk::DataRole role) const;
    Q_INVOKABLE bool isShotRole(cwSurveyChunk::DataRole role) const;
    Q_INVOKABLE bool isCellSelected(int selectedRow,
                                    cwSurveyChunk::DataRole selectedRole,
                                    int candidateRow,
                                    cwSurveyChunk::DataRole candidateRole) const;
    Q_INVOKABLE bool isFocusedCell(int row, cwSurveyChunk::DataRole role) const;
    int focusedRow() const;
    int focusedRole() const;
    Q_INVOKABLE void setFocusedCell(int row, cwSurveyChunk::DataRole role);
    Q_INVOKABLE void focusOnLastChunk();
    Q_INVOKABLE int nextCellRow(int modelRow,
                                cwSurveyChunk::DataRole currentRole,
                                NavigationKey key,
                                bool frontSights,
                                bool backSights) const;
    Q_INVOKABLE int nextCellRole(int modelRow,
                                 cwSurveyChunk::DataRole currentRole,
                                 NavigationKey key,
                                 bool frontSights,
                                 bool backSights) const;

    Q_INVOKABLE int toModelRow(const cwSurveyEditorRowIndex& rowIndex) const;
    Q_INVOKABLE bool isSelectedBox(const cwSurveyEditorBoxIndex& selectedBoxIndex,
                                   const cwSurveyEditorBoxIndex& candidateBoxIndex) const;
    Q_INVOKABLE bool isSelectedBoxAtRow(const cwSurveyEditorBoxIndex& selectedBoxIndex,
                                        const cwSurveyEditorBoxIndex& candidateBoxIndex,
                                        int candidateModelRow) const;

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
    Q_INVOKABLE cwSurveyEditorBoxIndex boxIndexForRowRole(int modelRow, cwSurveyChunk::DataRole dataRole) const;

    Q_INVOKABLE cwSurveyEditorBoxIndex offsetBoxIndex(const cwSurveyEditorBoxIndex& boxIndex, int offsetIndex) const;

private:
    struct RemoveToken {
        cwSurveyChunk* chunk = nullptr;
        int firstIndex = -1;
    };

    QPointer<cwTrip> m_trip; //!<
    QPointer<cwSurveyChunk> m_focusedChunk;
    QPointer<cwSurveyChunk> m_virtualRowsVisibleChunk;
    QPersistentModelIndex m_focusedRowIndex;
    cwSurveyChunk::DataRole m_focusedDataRole = static_cast<cwSurveyChunk::DataRole>(-1);
    int m_lastNotifiedFocusedRow = -1;
    int m_lastNotifiedFocusedRole = -1;

    const int m_titleRowOffset = 1;

    //For bookkeeping on row removal
    RemoveToken m_removeToken;

    enum TrimType {
        FullTrim,
        PreserveLastEmptyOne
    };

    cwSurveyEditorRowIndex toRowIndex(const QModelIndex& index) const;
    cwSurveyEditorRowIndex toRowIndex(int index) const;
    QModelIndex toModelIndex(const cwSurveyEditorRowIndex& rowIndex) const;

    Role toModelRole(cwSurveyChunk::DataRole chunkRole) const;
    cwSurveyEditorRowIndex::RowType toRowType(cwSurveyChunk::DataRole chunkRole) const;
    int stationCount(const cwSurveyChunk* chunk) const;
    int shotCount(const cwSurveyChunk* chunk) const;
    int chunkRowCount(const cwSurveyChunk* chunk) const;
    bool hasVirtualTrailingStationShot(const cwSurveyChunk* chunk) const;
    bool hasVisibleVirtualRows(const cwSurveyChunk* chunk) const;
    // void removeVisibleVirtualRows(cwSurveyChunk* chunk);
    void syncVirtualRows(cwSurveyChunk* chunk);
    void connectChunkSignals(cwSurveyChunk* chunk);
    void disconnectChunkSignals(cwSurveyChunk* chunk);
    void syncFocusedCellSignals();
    static bool isStationShotEmpty(cwSurveyChunk* chunk, int stationIndex);
    static void trim(cwSurveyChunk* chunk, TrimType trimType);
    static void trim(cwSurveyChunk* chunk);
    cwSurveyEditorBoxIndex nextCellBoxIndex(int modelRow,
                                            cwSurveyChunk::DataRole currentRole,
                                            NavigationKey key,
                                            bool frontSights,
                                            bool backSights) const;


signals:
    void tripChanged();
    void focusedRowChanged();
    void focusedRoleChanged();

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
