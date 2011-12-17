//Our inculdes
#include "cwSurveyNoteModel.h"
#include "cwProject.h"
#include "cwProjectImageProvider.h"
#include "cwTrip.h"

//Qt includes
#include <QDebug>

QString cwSurveyNoteModel::ImagePathString; // = QString("image://") + cwProjectImageProvider::Name + QString("/%1");

cwSurveyNoteModel::cwSurveyNoteModel(QObject *parent) :
    QAbstractListModel(parent),
    ParentTrip(NULL),
    ParentCave(NULL)
{
    ImagePathString = QString("image://") + cwProjectImageProvider::Name + QString("/%1");

    //For qml
    QHash<int, QByteArray> roles;
    roles.reserve(2);
    roles[ImageOriginalPathRole] = "imageOriginalPath";
    roles[ImageIconPathRole] = "imageIconPath";
    roles[ImageRole] = "image";
    roles[NoteObjectRole] = "noteObject";
    setRoleNames(roles);
}

cwSurveyNoteModel::cwSurveyNoteModel(const cwSurveyNoteModel& object) :
    QAbstractListModel(NULL),
    ParentTrip(NULL),
    ParentCave(NULL)
{
    copy(object);
}

cwSurveyNoteModel& cwSurveyNoteModel::operator=(const cwSurveyNoteModel& object) {
    if(&object == this) { return *this; }
    copy(object);
    return *this;
}

/**
  \brief Does a deap copy of all the notes in the notemodel
  */
void cwSurveyNoteModel::copy(const cwSurveyNoteModel& object) {

    setRoleNames(object.roleNames());

    beginResetModel();

    //Delete all the old notes
    foreach(cwNote* note, Notes) {
        delete note;
    }
    Notes.clear();

    foreach(cwNote* note, object.Notes) {
        cwNote* copyNote = new cwNote(*note);
        copyNote->setParentTrip(parentTrip());
        Notes.append(copyNote);
    }

    endResetModel();
}
/**
  \brief Get's the number of row in the model
  */
int cwSurveyNoteModel::rowCount(const QModelIndex &parent) const {
    if(parent != QModelIndex()) { return 0; } //There's data at the root
    return Notes.size();
}

/**
  \brief Get's the data in the model
  */
QVariant cwSurveyNoteModel::data(const QModelIndex &index, int role) const {
    if(!index.isValid()) { return QVariant(); }
    int row = index.row();

    switch(role) {
    case ImageOriginalPathRole: {
        //Get's the full blown note
        cwImage imagePath = Notes[row]->image();
        return ImagePathString.arg(imagePath.original());
    }
    case ImageIconPathRole: {
        //Get's the icon for the note
        cwImage imagePath = Notes[row]->image();
        return ImagePathString.arg(imagePath.icon());
    }
    case ImageRole: {
        return QVariant::fromValue(Notes[row]->image());
    }
    case NoteObjectRole:
        return QVariant::fromValue(qobject_cast<QObject*>(Notes[row]));
    default:
        qDebug() << "Return undefined variant";
        return QVariant();
    }
    return QVariant();
}

/**
  \brief This adds notes to the notes model

  \param files - The files that'll be added to the notes model
  \param project - The project where the files will be added to

  This will create new notes objects for each file. This will mipmap the files and also
  create a icon for each file.  The original file, icon, and mipmaps will be stored in the
  project.
  */
void cwSurveyNoteModel::addFromFiles(QStringList files, cwProject* project) {
    project->addImages(files, this, SLOT(addNotesWithNewImages(QList<cwImage>)));
}

/**
  This will create new notes foreach cwImage.  This assumes the the cwImage is valid.
  */
void cwSurveyNoteModel::addNotesWithNewImages(QList<cwImage> images) {
    if(images.empty()) { return; }

    QList<cwNote*> newNotes;
    foreach(cwImage image, images) {
        cwNote* note = new cwNote(this);
        note->setImage(image);
        newNotes.append(note);
    }

    addNotes(newNotes);
}
/**
  This adds valid notes to the note model

  The note model takes responibility for the notes, ie. it owns them
  */
void cwSurveyNoteModel::addNotes(QList<cwNote*> notes) {
    int lastIndex = rowCount();
    beginInsertRows(QModelIndex(), lastIndex, lastIndex + notes.size());

    Notes.append(notes);

    //Update the parent trips in the notes
    foreach(cwNote* note, notes) {
        note->setParentTrip(parentTrip());
    }

    endInsertRows();
}

/**
  \brief Sets the survey trip for the notes model

  This will update all the note's parents to the trip
*/
void cwSurveyNoteModel::setParentTrip(cwTrip* trip) {
    if(ParentTrip != trip) {
        ParentTrip = trip;
        setParent(ParentTrip);
        foreach(cwNote* note, Notes) {
            note->setParentTrip(ParentTrip);
            note->setParentCave(trip->parentCave());
        }
    }
}

/**
  \brief Set the parent cave
  */
void cwSurveyNoteModel::setParentCave(cwCave *cave) {
    if(ParentCave != cave) {
        ParentCave = cave;
        foreach(cwNote* note, Notes) {
            note->setParentCave(ParentCave);
        }
    }
}

