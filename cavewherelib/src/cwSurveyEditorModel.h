#ifndef CWSURVEXEDITORMODEL_H
#define CWSURVEXEDITORMODEL_H

//Our includes
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwSurveyEditorRowIndex.h"
#include "cwSurveyEditorCellIndex.h"
#include "CaveWhereLibExport.h"

//Qt includes
#include <QAbstractListModel>
#include <QHash>
#include <QMap>
#include <QPointer>
#include <QPersistentModelIndex>
#include <QQmlEngine>

class cwFixStationModel;

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

    //A move waiting for the user to pick the station it lands on. All three
    //answer for the same pending move, so they change together
    Q_PROPERTY(bool splayMoveActive READ splayMoveActive NOTIFY splayMoveChanged)
    Q_PROPERTY(int splayMoveCount READ splayMoveCount NOTIFY splayMoveChanged)
    Q_PROPERTY(QString splayMoveStationName READ splayMoveStationName NOTIFY splayMoveChanged)

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

        //True when this station is anchored by one of its cave's fix stations.
        //Read-only and derived: the fixes live in cwFixStationModel, not in the
        //chunk, so this role is the survey table's only view of them.
        StationFixedRole,

        //Shot Data Roles
        ShotDistanceRole,
        ShotDistanceIncludedRole,
        ShotCompassRole,
        ShotBackCompassRole,
        ShotClinoRole,
        ShotBackClinoRole,
        ShotCalibrationRole,

        //Splay Data Roles

        //On a station row, how many splays it carries and whether its cluster is
        //open. On a shot row, StationSplaysExpandedRole answers for the station
        //above it, which is what tells the shot's boxes to step out of the
        //cluster's way
        StationSplayCountRole,
        StationSplaysExpandedRole,

        //The three readings of one splay, in the shape the editor's boxes read.
        //Which splay is pinned by the row rather than the role, so they're
        //written back through the cells in cwSurveyEditorCellIndex
        SplayDistanceRole,
        SplayCompassRole,
        SplayClinoRole,

        //The name of the station a splay row hangs under, so a splay deep in a
        //long cluster can still say whose it is
        SplayStationNameRole,
    };
    Q_ENUM(Role)

    cwTrip* trip() const;
    void setTrip(cwTrip* trip);
    Q_INVOKABLE void setFocusedChunk(cwSurveyChunk* chunk);

    QVariant data(const QModelIndex &index, int role) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    Q_INVOKABLE cwSurveyEditorCellIndex cellIndex(int modelRow, cwSurveyEditorCellIndex::CellRole cellRole) const
    {
        return cwSurveyEditorCellIndex(modelRow, cellRole);
    }
    Q_INVOKABLE bool setDataAt(const cwSurveyEditorCellIndex& cell, const QVariant& data);
    Q_INVOKABLE QString guessStationNameAt(const cwSurveyEditorCellIndex& cell) const;
    Q_INVOKABLE bool shotDistanceIncludedAt(const cwSurveyEditorCellIndex& cell) const;
    Q_INVOKABLE bool canRemoveStationAt(const cwSurveyEditorCellIndex& cell, cwSurveyChunk::Direction direction) const;
    Q_INVOKABLE bool canRemoveShotAt(const cwSurveyEditorCellIndex& cell, cwSurveyChunk::Direction direction) const;
    Q_INVOKABLE bool canInsertStationAt(const cwSurveyEditorCellIndex& cell, cwSurveyChunk::Direction direction) const;
    Q_INVOKABLE bool canInsertShotAt(const cwSurveyEditorCellIndex& cell, cwSurveyChunk::Direction direction) const;
    Q_INVOKABLE void removeStationAt(const cwSurveyEditorCellIndex& cell, cwSurveyChunk::Direction direction);
    Q_INVOKABLE void removeShotAt(const cwSurveyEditorCellIndex& cell, cwSurveyChunk::Direction direction);
    Q_INVOKABLE void insertStationAt(const cwSurveyEditorCellIndex& cell, cwSurveyChunk::Direction direction);
    Q_INVOKABLE void insertShotAt(const cwSurveyEditorCellIndex& cell, cwSurveyChunk::Direction direction);


    QHash<int, QByteArray> roleNames() const;

    Q_INVOKABLE void addShotCalibration(int index);

    Q_INVOKABLE cwSurveyChunk* chunkForRow(int modelRow) const;
    Q_INVOKABLE bool isCellValid(const cwSurveyEditorCellIndex& cell) const;
    Q_INVOKABLE int modelRowForCellRole(cwSurveyChunk* chunk, int indexInChunk, cwSurveyEditorCellIndex::CellRole role) const;
    Q_INVOKABLE bool isStationCell(cwSurveyEditorCellIndex::CellRole role) const;
    Q_INVOKABLE bool isShotCell(cwSurveyEditorCellIndex::CellRole role) const;
    Q_INVOKABLE bool isSplayCell(cwSurveyEditorCellIndex::CellRole role) const;
    Q_INVOKABLE bool isCellSelected(const cwSurveyEditorCellIndex& selectedCell,
                                    const cwSurveyEditorCellIndex& candidateCell) const;
    Q_INVOKABLE bool isFocusedCell(const cwSurveyEditorCellIndex& cell) const;
    int focusedRow() const;
    int focusedRole() const;
    Q_INVOKABLE void setFocusedCell(const cwSurveyEditorCellIndex& cell);
    Q_INVOKABLE void dumpModel();
    Q_INVOKABLE void focusOnLastChunk();
    Q_INVOKABLE cwSurveyEditorCellIndex nextCell(const cwSurveyEditorCellIndex& currentCell,
                                                 NavigationKey key,
                                                 bool frontSights,
                                                 bool backSights) const;

    Q_INVOKABLE int toModelRow(const cwSurveyEditorRowIndex& rowIndex) const;

    Q_INVOKABLE void toggleSplaysExpanded(const cwSurveyEditorRowIndex& rowIndex);
    Q_INVOKABLE void removeSplayAt(const cwSurveyEditorRowIndex& rowIndex);
    Q_INVOKABLE void clearSplaysAt(const cwSurveyEditorRowIndex& rowIndex);

    Q_INVOKABLE void startSplayMove(const cwSurveyEditorRowIndex& rowIndex, bool allSplays);
    Q_INVOKABLE void cancelSplayMove();
    Q_INVOKABLE void commitSplayMove(const cwSurveyEditorRowIndex& targetRowIndex);
    Q_INVOKABLE bool isSplayMoveTarget(const cwSurveyEditorRowIndex& rowIndex) const;
    Q_INVOKABLE bool isSplayMoveSource(const cwSurveyEditorRowIndex& rowIndex) const;
    bool splayMoveActive() const;
    int splayMoveCount() const;
    QString splayMoveStationName() const;

    Q_INVOKABLE cwSurveyEditorRowIndex rowIndex(cwSurveyChunk *chunk, int chunkIndex, cwSurveyEditorRowIndex::RowType type) const
    {
        return cwSurveyEditorRowIndex(chunk, chunkIndex, type);
    }

private:
    struct RemoveToken {
        cwSurveyChunk* chunk = nullptr;
        int firstIndex = -1;
    };

    //!< The splay clusters the user has opened, and how many rows each one is
    //!< currently showing. A station is expanded exactly when it has a key here.
    //!< The row count is remembered rather than re-read from the chunk so that a
    //!< splay appearing or disappearing can be turned into an insert or a remove.
    using ExpandedSplays = QMap<int, int>;

    //!< Splays armed for a move, and the station they came off. The station
    //!< they land on is picked by clicking rows away from the menu that armed
    //!< the move, and the view recycles every delegate in between, so the
    //!< pending move waits here rather than in any of them.
    struct PendingSplayMove {
        QPointer<cwSurveyChunk> chunk;
        int stationIndex = -1;
        QList<int> splayIndices;
    };

    QHash<const cwSurveyChunk*, ExpandedSplays> m_expandedSplays;
    PendingSplayMove m_pendingSplayMove;

    QPointer<cwTrip> m_trip; //!<
    QPointer<cwFixStationModel> m_fixStations; //!< The trip's cave's fixes, for StationFixedRole
    QPointer<cwSurveyChunk> m_focusedChunk;
    QPointer<cwSurveyChunk> m_virtualRowsVisibleChunk;
    QPersistentModelIndex m_focusedRowIndex;
    cwSurveyEditorCellIndex::CellRole m_focusedCellRole = static_cast<cwSurveyEditorCellIndex::CellRole>(-1);
    int m_lastNotifiedFocusedRow = -1;
    int m_lastNotifiedFocusedRole = -1;

    const int m_titleRowOffset = 1;

    //For bookkeeping on row removal
    RemoveToken m_removeToken;

    enum TrimType {
        FullTrim,
        PreserveLastEmptyOne
    };

    bool setSplayDataAt(const cwSurveyEditorCellIndex& cell,
                        cwSurveyChunk::DataRole readingRole,
                        const QVariant& data);

    cwSurveyEditorRowIndex toRowIndex(const QModelIndex& index) const;
    cwSurveyEditorRowIndex toRowIndex(int index) const;
    QModelIndex toModelIndex(const cwSurveyEditorRowIndex& rowIndex) const;

    Role toModelRole(cwSurveyChunk::DataRole chunkRole) const;
    QList<int> changedRolesFor(cwSurveyChunk::DataRole chunkRole) const;
    cwSurveyEditorRowIndex::RowType toRowType(cwSurveyEditorCellIndex::CellRole cellRole) const;
    int stationCount(const cwSurveyChunk* chunk) const;
    int shotCount(const cwSurveyChunk* chunk) const;
    int chunkRowCount(const cwSurveyChunk* chunk) const;
    const ExpandedSplays* expandedSplays(const cwSurveyChunk* chunk) const;
    cwSurveyChunk* splayClusterChunk(const cwSurveyEditorRowIndex& rowIndex) const;
    int splayRowCount(const cwSurveyChunk* chunk, int stationIndex) const;
    int splayRowsBefore(const cwSurveyChunk* chunk, int stationIndex) const;
    int splayRowsInChunk(const cwSurveyChunk* chunk) const;
    int firstVirtualRow(cwSurveyChunk* chunk) const;
    void expandSplays(cwSurveyChunk* chunk, int stationIndex);
    void collapseSplays(cwSurveyChunk* chunk, int stationIndex);
    void emitSplayExpansionChanged(cwSurveyChunk* chunk, int stationIndex);
    void reconcileSplayRows(cwSurveyChunk* chunk, int stationIndex);
    void shiftExpandedSplays(cwSurveyChunk* chunk, int firstStationIndex, int offset);
    bool hasVirtualTrailingStationShot(const cwSurveyChunk* chunk) const;
    bool hasVisibleVirtualRows(const cwSurveyChunk* chunk) const;
    // void removeVisibleVirtualRows(cwSurveyChunk* chunk);
    void syncVirtualRows(cwSurveyChunk* chunk);
    void connectChunkSignals(cwSurveyChunk* chunk);
    void disconnectChunkSignals(cwSurveyChunk* chunk);
    void syncFixStationSignals();
    void invalidateStationFixed();
    void syncFocusedCellSignals();
    static bool isStationShotEmpty(cwSurveyChunk* chunk, int stationIndex);
    static void trim(cwSurveyChunk* chunk, TrimType trimType);
    static void trim(cwSurveyChunk* chunk);
    cwSurveyEditorCellIndex nextCellIndex(const cwSurveyEditorCellIndex& currentCell,
                                          NavigationKey key,
                                          bool frontSights,
                                          bool backSights) const;


signals:
    void tripChanged();
    void focusedRowChanged();
    void focusedRoleChanged();
    void splayMoveChanged();

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
