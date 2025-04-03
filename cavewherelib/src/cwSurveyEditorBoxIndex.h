#ifndef CWSURVEYEDITORBOXINDEX_H
#define CWSURVEYEDITORBOXINDEX_H

#include "cwSurveyChunk.h"
#include "cwSurveyEditorRowIndex.h"

//Qt includes
#include <QQmlEngine>

class cwSurveyEditorBoxIndex {
    Q_GADGET
    QML_VALUE_TYPE(cwSurveyEditorBoxIndex)
    Q_PROPERTY(cwSurveyEditorRowIndex rowIndex READ rowIndex WRITE setRowIndex)
    Q_PROPERTY(cwSurveyChunk::DataRole chunkDataRole READ chunkDataRole WRITE setChunkDataRole)

    //Helper properties
    Q_PROPERTY(cwSurveyChunk* chunk READ chunk WRITE setChunk)
    Q_PROPERTY(int indexInChunk READ indexInChunk WRITE setIndexInChunk)
    Q_PROPERTY(cwSurveyEditorRowIndex::RowType rowType READ rowType WRITE setRowType)


public:
    cwSurveyEditorBoxIndex() = default;

    cwSurveyEditorBoxIndex(const cwSurveyEditorRowIndex &rowIndex,
                           cwSurveyChunk::DataRole chunkDataRole)
        : m_rowIndex(rowIndex),
        m_chunkDataRole(chunkDataRole)
    {
    }

    cwSurveyEditorRowIndex rowIndex() const {
        return m_rowIndex;
    }
    void setRowIndex(const cwSurveyEditorRowIndex &rowIndex) {
        m_rowIndex = rowIndex;
    }

    cwSurveyChunk::DataRole chunkDataRole() const {
        return m_chunkDataRole;
    }
    void setChunkDataRole(cwSurveyChunk::DataRole chunkDataRole) {
        m_chunkDataRole = chunkDataRole;
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

        // qDebug() << "Equals:" << (m_rowIndex == rhs.m_rowIndex &&
        //                           m_chunkDataRole == rhs.m_chunkDataRole)
        //          << "rowIndex:" << m_rowIndex.rowType() << rhs.m_rowIndex.rowType()
        //          << "chunk:" << m_rowIndex.chunk() << rhs.m_rowIndex.chunk()
        //          << "indexInChunk:" << m_rowIndex.indexInChunk() << rhs.m_rowIndex.indexInChunk()
        //          << "role:" << m_chunkDataRole << rhs.m_chunkDataRole;

        return m_rowIndex == rhs.m_rowIndex &&
               m_chunkDataRole == rhs.m_chunkDataRole;
    }

    bool operator!=(const cwSurveyEditorBoxIndex &rhs) const {
        return !operator==(rhs);
    }

private:
    cwSurveyEditorRowIndex m_rowIndex;
    cwSurveyChunk::DataRole m_chunkDataRole = cwSurveyChunk::StationNameRole;
};


#endif // CWSURVEYEDITORBOXINDEX_H
