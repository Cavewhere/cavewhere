#ifndef CWSURVEYEDITORBOXINDEX_H
#define CWSURVEYEDITORBOXINDEX_H

//Our inludes
#include "cwSurveyChunk.h"
#include "cwSurveyEditorCellIndex.h"
#include "cwSurveyEditorRowIndex.h"
#include "CaveWhereLibExport.h"

//Qt includes
#include <QQmlEngine>

/**
 * Where one box of the survey table lives: the row it sits in and the cell of
 * that row it shows. The cell is the editor's own vocabulary, so a splay's box
 * names a splay cell instead of borrowing a shot reading's chunk role.
 */
class CAVEWHERE_LIB_EXPORT cwSurveyEditorBoxIndex {
    Q_GADGET
    QML_VALUE_TYPE(cwSurveyEditorBoxIndex)
    Q_PROPERTY(cwSurveyEditorRowIndex rowIndex READ rowIndex WRITE setRowIndex)
    Q_PROPERTY(cwSurveyEditorCellIndex::CellRole cellRole READ cellRole WRITE setCellRole)

    //Helper properties
    Q_PROPERTY(cwSurveyChunk* chunk READ chunk WRITE setChunk)
    Q_PROPERTY(int indexInChunk READ indexInChunk WRITE setIndexInChunk)
    Q_PROPERTY(cwSurveyEditorRowIndex::RowType rowType READ rowType WRITE setRowType)


public:
    cwSurveyEditorBoxIndex() = default;

    cwSurveyEditorBoxIndex(const cwSurveyEditorRowIndex &rowIndex,
                           cwSurveyEditorCellIndex::CellRole cellRole)
        : m_rowIndex(rowIndex),
        m_cellRole(cellRole)
    {
    }

    cwSurveyEditorRowIndex rowIndex() const {
        return m_rowIndex;
    }
    void setRowIndex(const cwSurveyEditorRowIndex &rowIndex) {
        m_rowIndex = rowIndex;
    }

    cwSurveyEditorCellIndex::CellRole cellRole() const {
        return m_cellRole;
    }
    void setCellRole(cwSurveyEditorCellIndex::CellRole cellRole) {
        m_cellRole = cellRole;
    }

    cwSurveyChunk* chunk() const {
        return m_rowIndex.chunk();
    }
    void setChunk(cwSurveyChunk* chunk) {
        m_rowIndex.setChunk(chunk);
    }

    int indexInChunk() const {
        return m_rowIndex.indexInChunk();
    }
    void setIndexInChunk(int indexInChunk) {
        m_rowIndex.setIndexInChunk(indexInChunk);
    }

    cwSurveyEditorRowIndex::RowType rowType() const {
        return m_rowIndex.rowType();
    }
    void setRowType(cwSurveyEditorRowIndex::RowType rowType) {
        m_rowIndex.setRowType(rowType);
    }

    // Equality operator for QML
    bool operator==(const cwSurveyEditorBoxIndex &rhs) const {
        return m_rowIndex == rhs.m_rowIndex &&
               m_cellRole == rhs.m_cellRole;
    }

    bool operator!=(const cwSurveyEditorBoxIndex &rhs) const {
        return !operator==(rhs);
    }

private:
    cwSurveyEditorRowIndex m_rowIndex;
    cwSurveyEditorCellIndex::CellRole m_cellRole = cwSurveyEditorCellIndex::StationNameCell;
};


#endif // CWSURVEYEDITORBOXINDEX_H
