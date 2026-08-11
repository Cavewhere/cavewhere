#ifndef CWSURVEYEDITORCELLINDEX_H
#define CWSURVEYEDITORCELLINDEX_H

#include "cwSurveyChunk.h"
#include "CaveWhereLibExport.h"

#include <QQmlEngine>
#include <QMetaType>

#include <optional>

class CAVEWHERE_LIB_EXPORT cwSurveyEditorCellIndex
{
    Q_GADGET
    QML_VALUE_TYPE(cwSurveyEditorCellIndex)
    Q_PROPERTY(int modelRow READ modelRow WRITE setModelRow)
    Q_PROPERTY(CellRole cellRole READ cellRole WRITE setCellRole)

public:
    //! Where the editor's own cells start, leaving a gap above every chunk role
    static constexpr int kFirstEditorCell = 100;

    /**
     * A column of the survey table. Most cells show a reading the chunk stores,
     * and those mirror cwSurveyChunk::DataRole at the same numeric value, so a
     * chunk role read out of the model can be handed straight back as the cell
     * that shows it. The rest are the editor's own: cells the table can focus
     * and act on that name nothing the chunk can store.
     */
    enum CellRole {
        StationNameCell = cwSurveyChunk::StationNameRole,
        StationLeftCell = cwSurveyChunk::StationLeftRole,
        StationRightCell = cwSurveyChunk::StationRightRole,
        StationUpCell = cwSurveyChunk::StationUpRole,
        StationDownCell = cwSurveyChunk::StationDownRole,
        ShotDistanceCell = cwSurveyChunk::ShotDistanceRole,
        ShotDistanceIncludedCell = cwSurveyChunk::ShotDistanceIncludedRole,
        ShotCompassCell = cwSurveyChunk::ShotCompassRole,
        ShotBackCompassCell = cwSurveyChunk::ShotBackCompassRole,
        ShotClinoCell = cwSurveyChunk::ShotClinoRole,
        ShotBackClinoCell = cwSurveyChunk::ShotBackClinoRole,

        //A station's splay cluster, as a cell the editor can focus and toggle.
        //It stands for the whole cluster rather than any one reading in it
        StationSplaysCell = kFirstEditorCell,

        //The three readings of one splay. A splay lives under a station at an
        //index of its own, which no chunk role can name, so these are the
        //editor's own cells and the reading each one writes comes back from
        //toSplayReadingRole
        SplayDistanceCell,
        SplayCompassCell,
        SplayClinoCell
    };
    Q_ENUM(CellRole)

    static_assert(cwSurveyChunk::ShotBackClinoRole < kFirstEditorCell,
                  "The chunk's roles have grown into the editor's own cells. Raise "
                  "kFirstEditorCell so a new reading can't alias one of them.");

    cwSurveyEditorCellIndex() = default;
    cwSurveyEditorCellIndex(int modelRow, CellRole cellRole)
        : m_modelRow(modelRow),
          m_cellRole(cellRole)
    {
    }

    int modelRow() const { return m_modelRow; }
    void setModelRow(int row) { m_modelRow = row; }

    CellRole cellRole() const { return m_cellRole; }
    void setCellRole(CellRole role) { m_cellRole = role; }

    //! The cell that shows chunk reading \a chunkRole
    static CellRole toCellRole(cwSurveyChunk::DataRole chunkRole)
    {
        return static_cast<CellRole>(chunkRole);
    }

    /**
     * The chunk reading \a cellRole shows, or nothing when the cell is one of
     * the editor's own and so has no reading behind it to write.
     */
    static std::optional<cwSurveyChunk::DataRole> toChunkRole(CellRole cellRole)
    {
        switch(cellRole) {
        case StationNameCell:
        case StationLeftCell:
        case StationRightCell:
        case StationUpCell:
        case StationDownCell:
        case ShotDistanceCell:
        case ShotDistanceIncludedCell:
        case ShotCompassCell:
        case ShotBackCompassCell:
        case ShotClinoCell:
        case ShotBackClinoCell:
            return static_cast<cwSurveyChunk::DataRole>(cellRole);
        case StationSplaysCell:
        case SplayDistanceCell:
        case SplayCompassCell:
        case SplayClinoCell:
            break;
        }
        return {};
    }

    /**
     * The reading of a splay \a cellRole shows, or nothing when the cell shows
     * something other than a splay. A splay is a front sight with no
     * destination, so it answers to the shot's three front-sight roles, and the
     * station and splay it belongs to come from the row rather than the cell.
     */
    static std::optional<cwSurveyChunk::DataRole> toSplayReadingRole(CellRole cellRole)
    {
        switch(cellRole) {
        case SplayDistanceCell:
            return cwSurveyChunk::ShotDistanceRole;
        case SplayCompassCell:
            return cwSurveyChunk::ShotCompassRole;
        case SplayClinoCell:
            return cwSurveyChunk::ShotClinoRole;
        case StationNameCell:
        case StationLeftCell:
        case StationRightCell:
        case StationUpCell:
        case StationDownCell:
        case ShotDistanceCell:
        case ShotDistanceIncludedCell:
        case ShotCompassCell:
        case ShotBackCompassCell:
        case ShotClinoCell:
        case ShotBackClinoCell:
        case StationSplaysCell:
            break;
        }
        return {};
    }

    bool operator==(const cwSurveyEditorCellIndex& rhs) const
    {
        return m_modelRow == rhs.m_modelRow && m_cellRole == rhs.m_cellRole;
    }

    bool operator!=(const cwSurveyEditorCellIndex& rhs) const
    {
        return !(*this == rhs);
    }

private:
    int m_modelRow = -1;
    CellRole m_cellRole = static_cast<CellRole>(-1);
};

Q_DECLARE_METATYPE(cwSurveyEditorCellIndex)

//Need to get Q_ENUM registered in qml
class cwSurveyEditorCellIndexDerived: public cwSurveyEditorCellIndex
{
    Q_GADGET
};

namespace SurveyEditorCellIndexForeign
{
Q_NAMESPACE
QML_NAMED_ELEMENT(SurveyEditorCellIndex)
QML_FOREIGN_NAMESPACE(cwSurveyEditorCellIndexDerived)
}

#endif
