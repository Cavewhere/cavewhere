#pragma once

#include "cwSurveyNoteModelBase.h"
#include "cwSurveyNoteSketchModelData.h"
#include "cwSketch.h"

class CAVEWHERE_LIB_EXPORT cwSurveyNoteSketchModel : public cwSurveyNoteModelBase
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SurveyNoteSketchModel)

public:
    explicit cwSurveyNoteSketchModel(QObject* parent = nullptr);

    // Sketches are created programmatically, not ingested from files.
    Q_INVOKABLE void addFromFiles(QList<QUrl> files) override;
    QVariant data(const QModelIndex& index, int role) const override;

    Q_INVOKABLE cwSketch* addSketch(cwSketch::ViewType viewType);
    void addSketches(const QList<cwSketch*>& sketches);

    void setData(const cwSurveyNoteSketchModelData& data);
    cwSurveyNoteSketchModelData data() const;

protected:
    void onParentTripChanged() override;
};
