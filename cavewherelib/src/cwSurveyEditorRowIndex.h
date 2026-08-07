#ifndef CWSURVEYEDITORROWINDEX_H
#define CWSURVEYEDITORROWINDEX_H

//Qt includes
#include <QObject>
#include <QPointer>
#include <QQmlEngine>

//Our includes
#include "cwSurveyChunk.h"
#include "CaveWhereLibExport.h"

class CAVEWHERE_LIB_EXPORT cwSurveyEditorRowIndex {
    Q_GADGET
    QML_VALUE_TYPE(cwSurveyEditorRowIndex)
    Q_PROPERTY(cwSurveyChunk* chunk READ chunk WRITE setChunk)
    Q_PROPERTY(int indexInChunk READ indexInChunk WRITE setIndexInChunk)
    Q_PROPERTY(int splayIndex READ splayIndex WRITE setSplayIndex)
    Q_PROPERTY(RowType rowType READ rowType WRITE setRowType)

public:
    enum RowType {
        TitleRow,
        StationRow,
        ShotRow,

        //A read-only splay hanging under a station row. indexInChunk is the
        //station it was shot from and splayIndex picks it out of that station's
        //splays, so the row keeps its identity while other stations expand and
        //collapse around it
        SplayRow
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

    cwSurveyEditorRowIndex(cwSurveyChunk* chunk,
                           int stationIndexInChunk,
                           int splayIndex,
                           RowType rowType)
        : m_chunk(chunk),
        m_indexInChunk(stationIndexInChunk),
        m_splayIndex(splayIndex),
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

    // Accessor and mutator for splayIndex
    int splayIndex() const {
        return m_splayIndex;
    }
    void setSplayIndex(int splayIndex) {
        m_splayIndex = splayIndex;
    }

    // Equality operator for QML
    bool operator==(const cwSurveyEditorRowIndex &rhs) const {
        return m_chunk == rhs.m_chunk &&
               m_indexInChunk == rhs.m_indexInChunk &&
               m_splayIndex == rhs.m_splayIndex &&
               m_rowType == rhs.m_rowType;
    }

    bool operator!=(const cwSurveyEditorRowIndex &rhs) const {
        return !operator==(rhs);
    }

private:
    QPointer<cwSurveyChunk> m_chunk;
    int m_indexInChunk = -1;
    int m_splayIndex = -1;
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

#endif // CWSURVEYEDITORROWINDEX_H
