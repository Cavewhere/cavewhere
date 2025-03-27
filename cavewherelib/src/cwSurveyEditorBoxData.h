#ifndef CWSURVEYEDITORBOXDATA
#define CWSURVEYEDITORBOXDATA

//Qt includes
#include <QString>
#include <QMetaType>
#include <QQmlEngine>

//Our includes
#include "cwReading.h"
#include "cwSurveyEditorRowIndex.h"
#include "cwSurveyChunk.h"
#include "cwSurveyEditorBoxIndex.h" // Include the header for cwSurveyEditorBoxIndex

class cwErrorModel; // Forward declaration for cwErrorModel

class cwSurveyEditorBoxData {
    Q_GADGET
    QML_VALUE_TYPE(cwSurveyEditorBoxData)
    Q_PROPERTY(cwErrorModel* errorModel READ errorModel WRITE setErrorModel)
    Q_PROPERTY(cwReading reading READ reading WRITE setReading)
    Q_PROPERTY(cwSurveyEditorBoxIndex boxIndex READ boxIndex WRITE setBoxIndex)
    Q_PROPERTY(cwSurveyEditorRowIndex rowIndex READ rowIndex WRITE setRowIndex)

    //Helper accessor properties
    Q_PROPERTY(cwSurveyChunk::DataRole chunkDataRole READ chunkDataRole WRITE setChunkDataRole)
    Q_PROPERTY(cwSurveyChunk* chunk READ chunk WRITE setChunk)
    Q_PROPERTY(int indexInChunk READ indexInChunk WRITE setIndexInChunk)
    Q_PROPERTY(cwSurveyEditorRowIndex::RowType rowType READ rowType WRITE setRowType)

public:
    cwSurveyEditorBoxData() = default;

    cwSurveyEditorBoxData(const cwReading &reading,
                          const cwSurveyEditorRowIndex &rowIndex,
                          cwSurveyChunk::DataRole chunkDataRole,
                          cwErrorModel* errorModel = nullptr)
        : m_reading(reading)
        , m_errorModel(errorModel)
        , m_boxIndex(rowIndex, chunkDataRole)
    {
    }
    cwSurveyEditorBoxData(const cwReading &reading,
                          const cwSurveyEditorBoxIndex &boxIndex,
                          cwErrorModel* errorModel = nullptr)
        : m_reading(reading)
        , m_errorModel(errorModel)
        , m_boxIndex(boxIndex)
    {
    }

    cwErrorModel* errorModel() const {
        return m_errorModel;
    }
    void setErrorModel(cwErrorModel* errorModel) {
        m_errorModel = errorModel;
    }

    cwReading reading() const {
        return m_reading;
    }
    void setReading(const cwReading &reading) {
        m_reading = reading;
    }

    cwSurveyEditorBoxIndex boxIndex() const {
        return m_boxIndex;
    }
    void setBoxIndex(const cwSurveyEditorBoxIndex &boxIndex) {
        m_boxIndex = boxIndex;
    }

    cwSurveyEditorRowIndex rowIndex() const {
        return m_boxIndex.rowIndex();
    }
    void setRowIndex(const cwSurveyEditorRowIndex &rowIndex) {
        m_boxIndex.setRowIndex(rowIndex);
    }

    cwSurveyChunk::DataRole chunkDataRole() const {
        return m_boxIndex.chunkDataRole();
    }
    void setChunkDataRole(cwSurveyChunk::DataRole chunkDataRole) {
        m_boxIndex.setChunkDataRole(chunkDataRole);
    }

    cwSurveyChunk* chunk() const {
        return m_boxIndex.chunk();
    }
    void setChunk(cwSurveyChunk* chunk) {
        m_boxIndex.setChunk(chunk);
    }

    int indexInChunk() const {
        return m_boxIndex.indexInChunk();
    }
    void setIndexInChunk(int indexInChunk) {
        m_boxIndex.setIndexInChunk(indexInChunk);
    }

    cwSurveyEditorRowIndex::RowType rowType() const {
        return m_boxIndex.rowType();
    }
    void setRowType(cwSurveyEditorRowIndex::RowType rowType) {
        m_boxIndex.setRowType(rowType);
    }

private:
    cwErrorModel* m_errorModel = nullptr;
    cwReading m_reading;
    cwSurveyEditorBoxIndex m_boxIndex; // Contains rowIndex and chunkDataRole
};

Q_DECLARE_METATYPE(cwSurveyEditorBoxData)

#endif
