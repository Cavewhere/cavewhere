#ifndef CWSURVEYNOTEMODEL_H
#define CWSURVEYNOTEMODEL_H

//Qt includes
#include <QAbstractListModel>

//Our includes
#include <cwNote.h>

class cwSurveyNoteModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        ImagePathRole = Qt::UserRole + 1,
        ImageRole,   //Just get's the QImage
        NoteObjectRole //Gets the whole object
    };

    explicit cwSurveyNoteModel(QObject *parent = 0);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;

    void addNoteFromFile(QString filename);

signals:

public slots:

protected:
    QList<cwNote*> Notes;

};

#endif // CWSURVEYNOTEMODEL_H
