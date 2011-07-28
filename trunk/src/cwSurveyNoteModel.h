#ifndef CWSURVEYNOTEMODEL_H
#define CWSURVEYNOTEMODEL_H

//Qt includes
#include <QAbstractListModel>

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

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;

    Q_INVOKABLE void addNotes(QStringList files, cwProject* project);

signals:

public slots:

private:
    static const QString ImagePathString;

    QList<cwNote*> Notes;

private slots:
    void addNotesWithNewImages(QList<cwImage> images);

};

#endif // CWSURVEYNOTEMODEL_H
