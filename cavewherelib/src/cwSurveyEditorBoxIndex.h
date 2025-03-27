#ifndef CWSURVEYEDITORBOXINDEX_H
#define CWSURVEYEDITORBOXINDEX_H

#include <QObject>
#include <QQmlEngine>

//Our inculdes
class cwSurveyChunk;


#include "cwSurveyChunk.h"
// #include "cwSurveyEditorModel.h"

class cwSurveyEditorBoxIndex {
    Q_GADGET
    QML_VALUE_TYPE(cwSurveyEditorBoxIndex)
    Q_PROPERTY(int rowType MEMBER m_rowType)
    Q_PROPERTY(cwSurveyChunk* chunk MEMBER m_chunk)
    Q_PROPERTY(int indexInChunk MEMBER m_indexInChunk)
    Q_PROPERTY(int chunkRole MEMBER m_chunkRole)

public:
    cwSurveyEditorBoxIndex()
    {
    }

    cwSurveyEditorBoxIndex(int rowType,
                           cwSurveyChunk* chunk,
                           int indexInChunk,
                           int chunkRole)
        : m_chunk(chunk)
        , m_indexInChunk(indexInChunk)
        , m_chunkRole(chunkRole)
        , m_rowType(rowType)
    {
    }

    // Equality operator for QML
    friend bool operator==(const cwSurveyEditorBoxIndex &lhs, const cwSurveyEditorBoxIndex &rhs) {
        return lhs.m_chunk == rhs.m_chunk &&
               lhs.m_indexInChunk == rhs.m_indexInChunk &&
               lhs.m_chunkRole == rhs.m_chunkRole &&
               lhs.m_rowType == rhs.m_rowType;
    }

    friend bool operator!=(const cwSurveyEditorBoxIndex &lhs, const cwSurveyEditorBoxIndex &rhs) {
        return !(lhs == rhs);
    }

private:
    cwSurveyChunk* m_chunk = nullptr;
    int m_indexInChunk = -1;
    int m_chunkRole = -1;
    int m_rowType = -1;
};

Q_DECLARE_METATYPE(cwSurveyEditorBoxIndex)

#endif // CWSURVEYEDITORBOXINDEX_H
