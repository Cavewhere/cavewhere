//Qt includes
#include <QString>
#include <QMetaType>
#include <QQmlEngine>

//Our includes
#include "cwReading.h"
#include "cwSurveyEditorRowIndex.h"
#include "cwSurveyChunk.h"

class cwErrorModel; // Forward declaration for cwErrorModel

class cwSurveyEditorBoxData {
    Q_GADGET
    QML_VALUE_TYPE(cwSurveyEditorBoxData)
    Q_PROPERTY(cwErrorModel* errorModel READ errorModel WRITE setErrorModel)
    Q_PROPERTY(cwReading reading READ reading WRITE setReading)
    Q_PROPERTY(cwSurveyEditorRowIndex rowIndex READ rowIndex WRITE setRowIndex)
    Q_PROPERTY(cwSurveyChunk::DataRole chunkDataRole READ chunkDataRole WRITE setChunkDataRole)

public:
    cwSurveyEditorBoxData() = default;

    cwSurveyEditorBoxData(const cwReading &reading,
                          const cwSurveyEditorRowIndex &rowIndex,
                          cwSurveyChunk::DataRole chunkDataRole,
                          cwErrorModel* errorModel = nullptr)
        : m_reading(reading)
        , m_errorModel(errorModel)
        , m_rowIndex(rowIndex)
        , m_chunkDataRole(chunkDataRole)
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

private:
    cwErrorModel* m_errorModel = nullptr;
    cwReading m_reading;
    cwSurveyEditorRowIndex m_rowIndex;
    cwSurveyChunk::DataRole m_chunkDataRole = cwSurveyChunk::StationNameRole;
};

Q_DECLARE_METATYPE(cwSurveyEditorBoxData)
