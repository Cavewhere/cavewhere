/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSURVEYNOTEMODEL_H
#define CWSURVEYNOTEMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QList>
#include <QUrl>
#include <QQmlEngine>

//Our includes
#include "cwNote.h"
#include "cwImage.h"
#include "cwGlobals.h"
class cwProject;

class CAVEWHERE_LIB_EXPORT cwSurveyNoteModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SurveyNoteModel)

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

    void setParentTrip(cwTrip* trip);
    cwTrip* parentTrip() const;

    void setParentCave(cwCave* cave);
    cwCave* parentCave() const;

    Q_INVOKABLE int rowCount(const QModelIndex &parent = QModelIndex()) const;
    Q_INVOKABLE QVariant data(const QModelIndex &index, int role) const;

    Q_INVOKABLE void addFromFiles(QList<QUrl> files);
    Q_INVOKABLE void removeNote(int index);

    void stationPositionModelUpdated();
    bool hasNotes() const;

    virtual QHash<int, QByteArray> roleNames() const;

signals:

private:
    QList<cwNote*> Notes;

    cwTrip* ParentTrip;
    cwCave* ParentCave;

    void initModel();
    void copy(const cwSurveyNoteModel& object);

    QList<cwNote*> validateNoteImages(QList<cwNote*> notes) const;
    void addNotesWithNewImages(QList<cwImage> images);

    cwProject* project() const;
    void updateMipmaps();

    static QString imagePathString();

};

/**
  \brief Gets all the survey notes in the model
  */
inline QList<cwNote*> cwSurveyNoteModel::notes() const {
    return Notes;
}

/**
  \brief Gets the parent trip for this chunk
  */
inline cwTrip* cwSurveyNoteModel::parentTrip() const {
    return ParentTrip;
}


/**
  \brief Gets the parent cave for the survey note model
  */
inline cwCave* cwSurveyNoteModel::parentCave() const {
    return ParentCave;
}

/**
 * @brief cwSurveyNoteModel::hasNotes
 * @return True if the model has notes and false if it doesn't
 */
inline bool cwSurveyNoteModel::hasNotes() const
{
    return !Notes.isEmpty();
}




#endif // CWSURVEYNOTEMODEL_H
