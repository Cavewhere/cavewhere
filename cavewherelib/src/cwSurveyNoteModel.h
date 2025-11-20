#pragma once

#include "cwSurveyNoteModelBase.h"
#include "cwSurveyNoteModelData.h"

// Fwd
class cwImage;
class cwNote;

/**
 * @brief Model for image/PDF survey notes (cwNote).
 */
class CAVEWHERE_LIB_EXPORT cwSurveyNoteModel : public cwSurveyNoteModelBase
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SurveyNoteModel)

public:
    explicit cwSurveyNoteModel(QObject* parent = nullptr);

    QVariant data(const QModelIndex& index, int role) const override;
    Q_INVOKABLE void addFromFiles(QList<QUrl> files) override;

    QList<cwNote*> notes() const;
    void addNotes(const QList<cwNote *> &notes);

    void setData(const cwSurveyNoteModelData& data);
    cwSurveyNoteModelData data() const;

protected:
    void onParentTripChanged() override;

private:
    QList<cwNote*> validateNoteImages(QList<cwNote*> notes) const;
    void addNotesWithNewImages(QList<cwImage> images);
    static QString imagePathString();
};
