//Our inculdes
#include "cwSurveyNoteModel.h"
#include "cwProject.h"
#include "cwProjectImageProvider.h"

//Qt includes
#include <QDebug>

const QString cwSurveyNoteModel::ImagePathString = "image://" + cwProjectImageProvider::Name + "/%1";

cwSurveyNoteModel::cwSurveyNoteModel(QObject *parent) :
    QAbstractListModel(parent)
{
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
    QAbstractListModel(NULL)
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

    qDebug() << "Role:" << role << index;

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

    //Add all the images to the model
    beginInsertRows(QModelIndex(), rowCount(), rowCount() + images.size());
    foreach(cwImage image, images) {
        cwNote* note = new cwNote(this);
        note->setImage(image);

        Notes.append(note);
    }
    endInsertRows();
}

/**
  This adds valid notes to the note model

  The note model takes responibility for the notes, ie. it owns them
  */
void cwSurveyNoteModel::addNotes(QList<cwNote*> notes) {
    int lastIndex = rowCount();
    beginInsertRows(QModelIndex(), lastIndex, lastIndex + notes.size());

    Notes.append(notes);

    foreach(cwNote* note, notes) {
        note->setParent(this);
    }

    endInsertRows();
}
