#pragma once

#include "cwSurveyNoteModelBase.h"
#include "cwSurveyNoteLiDARModelData.h"
#include "cwNoteLiDAR.h"

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

    void setData(const cwSurveyNoteLiDARModelData& data);
    cwSurveyNoteLiDARModelData data() const;

protected:
    void onParentTripChanged() override;

private:
    template <typename T>
    void connectNotes(const QList<T*>& notes) {
        for (T* note : notes) {
            if (note == nullptr) {
                continue;
            }

            qDebug() << "Connectiong note:" << note;
            Q_ASSERT(dynamic_cast<cwNoteLiDAR*>(note));
            connect(static_cast<cwNoteLiDAR*>(note), &cwNoteLiDAR::iconImagePathChanged,
                    this, [this, note]()
                    {
                        qDebug() << "I get here!" << this;
                        const QList<QObject*> objNotes = this->notes();
                        const int row = objNotes.indexOf(static_cast<QObject*>(note));
                        const QModelIndex modelIndex = index(row);
                        qDebug() << "Icon path change!" << modelIndex;
                        if (modelIndex.isValid()) {
                            qDebug() << "Real change!" << modelIndex;
                            emit dataChanged(modelIndex, modelIndex, { IconPathRole });
                        }
                    });
        }
    }
};
