//Our inculdes
#include "cwSurveyNoteModel.h"
#include "cwProject.h"
#include "cwImageProvider.h"
#include "cwTrip.h"
#include "cwNote.h"

//Qt includes
#include <QDebug>

QString cwSurveyNoteModel::ImagePathString; // = QString("image://") + cwProjectImageProvider::Name + QString("/%1");

cwSurveyNoteModel::cwSurveyNoteModel(QObject *parent) :
    QAbstractListModel(parent),
    ParentTrip(NULL),
    ParentCave(NULL)
{
    ImagePathString = QString("image://") + cwImageProvider::Name + QString("/%1");
}

QHash<int, QByteArray> cwSurveyNoteModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[ImageOriginalPathRole] = "imageOriginalPath";
    roles[ImageIconPathRole] = "imageIconPath";
    roles[ImageRole] = "image";
    roles[NoteObjectRole] = "noteObject";
    return roles;
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
 * @brief cwSurveyNoteModel::validateNoteImages
 * @param notes
 * @return A list of notes that are valid.  Valid notes are ones with valid original images.
 *
 * If it's note a valid note. The note will be deleted. NULL notes aren't added to the the valid note
 * list
 */
QList<cwNote *> cwSurveyNoteModel::validateNoteImages(QList<cwNote *> notes) const
{
    QList<cwNote*> validNotes;
    foreach(cwNote* note, notes) {
        if(note != NULL) {
            if(note->image().isValid() && note->image().iconIsValid()) {
                validNotes.append(note);
            } else {
                note->deleteLater();
            }
        }
    }
    return validNotes;
}

/**
  \brief Get's the number of row in the model
  */
int cwSurveyNoteModel::rowCount(const QModelIndex &parent) const {
    if(parent != QModelIndex()) { return 0; } //There's data at the root
    qDebug() << "Notes size:" << Notes.size();
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
 * @brief cwTrip::stationPositionModelUpdated
 *
 * Called from the cwCave that the stations position model has updated
 */
void cwSurveyNoteModel::stationPositionModelUpdated() {
    foreach(cwNote* note, Notes) {
        note->updateScrapNoteTransform(); //The note transform depends on the model's positions
    }
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

    //Remove all invalid notes
    QList<cwNote*> validNotes = validateNoteImages(notes);

    beginInsertRows(QModelIndex(), lastIndex, lastIndex + validNotes.size() - 1);

    Notes.append(validNotes);

    //Update the parent trips in the notes
    foreach(cwNote* note, validNotes) {
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

