#ifndef CWSURVEYEDITORCELLINDEX_H
#define CWSURVEYEDITORCELLINDEX_H

#include "cwSurveyChunk.h"
#include "CaveWhereLibExport.h"

#include <QQmlEngine>
#include <QMetaType>

class CAVEWHERE_LIB_EXPORT cwSurveyEditorCellIndex
{
    Q_GADGET
    QML_VALUE_TYPE(cwSurveyEditorCellIndex)
    Q_PROPERTY(int modelRow READ modelRow WRITE setModelRow)
    Q_PROPERTY(cwSurveyChunk::DataRole dataRole READ dataRole WRITE setDataRole)

public:
    cwSurveyEditorCellIndex() = default;
    cwSurveyEditorCellIndex(int modelRow, cwSurveyChunk::DataRole dataRole)
        : m_modelRow(modelRow),
          m_dataRole(dataRole)
    {
    }

    int modelRow() const { return m_modelRow; }
    void setModelRow(int row) { m_modelRow = row; }

    cwSurveyChunk::DataRole dataRole() const { return m_dataRole; }
    void setDataRole(cwSurveyChunk::DataRole role) { m_dataRole = role; }

    bool operator==(const cwSurveyEditorCellIndex& rhs) const
    {
        return m_modelRow == rhs.m_modelRow && m_dataRole == rhs.m_dataRole;
    }

    bool operator!=(const cwSurveyEditorCellIndex& rhs) const
    {
        return !(*this == rhs);
    }

private:
    int m_modelRow = -1;
    cwSurveyChunk::DataRole m_dataRole = static_cast<cwSurveyChunk::DataRole>(-1);
};

Q_DECLARE_METATYPE(cwSurveyEditorCellIndex)

#endif
