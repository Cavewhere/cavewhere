#pragma once

#include "cwSurveyNoteModelBase.h"

class cwNoteLiDAR;

/**
 * @brief Model for LiDAR notes (cwNoteLiDAR) driven by .glb files.
 */
class CAVEWHERE_LIB_EXPORT cwSurveyNoteLiDARModel : public cwSurveyNoteModelBase
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SurveyNoteLiDARModel)

public:
    explicit cwSurveyNoteLiDARModel(QObject* parent = nullptr);

    Q_INVOKABLE void addFromFiles(QList<QUrl> files) override;
    QVariant data(const QModelIndex& index, int role) const override;

    void addNotes(const QList<cwNoteLiDAR*> lidarNotes);

protected:
    void onParentTripChanged() override;
};
