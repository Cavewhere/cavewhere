#ifndef CWSURVEYNOTEMODEL_H
#define CWSURVEYNOTEMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QList>

//Our includes
#include "cwNote.h"
#include "cwImage.h"
class cwProject;

class cwSurveyNoteModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        ImageOriginalPathRole = Qt::UserRole + 1,
        ImageIconPathRole,
        ImageRole,   //Just get's the QImage
        NoteObjectRole //Gets the whole object
    };

    explicit cwSurveyNoteModel(QObject *parent = 0);
    cwSurveyNoteModel(const cwSurveyNoteModel& object);
    cwSurveyNoteModel& operator=(const cwSurveyNoteModel& object);

    QList<cwNote*> notes() const;
    void addNotes(QList<cwNote*> notes);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    Q_INVOKABLE QVariant data(const QModelIndex &index, int role) const;

    Q_INVOKABLE void addFromFiles(QStringList files, cwProject* project);

signals:

public slots:

private:
    static QString ImagePathString;

    QList<cwNote*> Notes;

    void initModel();
    void copy(const cwSurveyNoteModel& object);

private slots:
    void addNotesWithNewImages(QList<cwImage> images);

};

/**
  \brief Gets all the survey notes in the model
  */
inline QList<cwNote*> cwSurveyNoteModel::notes() const {
    return Notes;
}

#endif // CWSURVEYNOTEMODEL_H
