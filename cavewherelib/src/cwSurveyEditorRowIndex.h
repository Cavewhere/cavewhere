#ifndef CWSURVEYEDITORBOXINDEX_H
#define CWSURVEYEDITORBOXINDEX_H

#include <QObject>
#include <QQmlEngine>
#include "cwSurveyChunk.h"

class cwSurveyEditorRowIndex {
    Q_GADGET
    QML_VALUE_TYPE(cwSurveyEditorRowIndex)
    Q_PROPERTY(cwSurveyChunk* chunk READ chunk WRITE setChunk)
    Q_PROPERTY(int indexInChunk READ indexInChunk WRITE setIndexInChunk)
    Q_PROPERTY(RowType rowType READ rowType WRITE setRowType)

public:
    enum RowType {
        TitleRow,
        StationRow,
        ShotRow
    };
    Q_ENUM(RowType)

    cwSurveyEditorRowIndex() = default;

    cwSurveyEditorRowIndex(cwSurveyChunk* chunk,
                           int indexInChunk,
                           RowType rowType)
        : m_chunk(chunk),
        m_indexInChunk(indexInChunk),
        m_rowType(rowType)
    {
    }

    // Accessor and mutator for rowType
    RowType rowType() const {
        return m_rowType;
    }
    void setRowType(RowType rowType) {
        m_rowType = rowType;
    }

    // Accessor and mutator for chunk
    cwSurveyChunk* chunk() const {
        return m_chunk;
    }
    void setChunk(cwSurveyChunk* chunk) {
        m_chunk = chunk;
    }

    // Accessor and mutator for indexInChunk
    int indexInChunk() const {
        return m_indexInChunk;
    }
    void setIndexInChunk(int indexInChunk) {
        m_indexInChunk = indexInChunk;
    }

    // Equality operator for QML
    friend bool operator==(const cwSurveyEditorRowIndex &lhs, const cwSurveyEditorRowIndex &rhs) {
        return lhs.m_chunk == rhs.m_chunk &&
               lhs.m_indexInChunk == rhs.m_indexInChunk &&
               lhs.m_rowType == rhs.m_rowType;
    }

    friend bool operator!=(const cwSurveyEditorRowIndex &lhs, const cwSurveyEditorRowIndex &rhs) {
        return !(lhs == rhs);
    }

private:
    cwSurveyChunk* m_chunk = nullptr;
    int m_indexInChunk = -1;
    RowType m_rowType = TitleRow;
};

Q_DECLARE_METATYPE(cwSurveyEditorRowIndex)

//Need to get Q_ENUM registered in qml
class cwSurveyEditorRowIndexDerived: public cwSurveyEditorRowIndex
{
    Q_GADGET
};

namespace SurveyEditorRowIndexForeign
{
Q_NAMESPACE
QML_NAMED_ELEMENT(SurveyEditorRowIndex)
QML_FOREIGN_NAMESPACE(cwSurveyEditorRowIndexDerived)
}

#endif // CWSURVEYEDITORBOXINDEX_H
