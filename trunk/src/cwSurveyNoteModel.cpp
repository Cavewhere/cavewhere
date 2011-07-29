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

//    //Add a few pages of notes
//    cwNote* note1 = new cwNote(this);
//    cwNote* note2 = new cwNote(this);
//    cwNote* note3 = new cwNote(this);
//    cwNote* note4 = new cwNote(this);

//    note1->setImagePath("/home/blitz/documents/caving/survey/cave/us/va/washington/Debusk Mill/David Debusk Cave/notes/trip001-02.png");
//    note2->setImagePath("/home/blitz/documents/caving/survey/cave/us/va/washington/Debusk Mill/David Debusk Cave/notes/trip001-04.png");
//    note3->setImagePath("/home/blitz/documents/caving/survey/cave/us/va/washington/Debusk Mill/David Debusk Cave/notes/trip001-06.png");
//    note4->setImagePath("/home/blitz/documents/caving/survey/cave/us/va/washington/Debusk Mill/David Debusk Cave/notes/trip001-08.png");

//    Notes.append(note1);
//    Notes.append(note2);
//    Notes.append(note3);
//    Notes.append(note4);

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
void cwSurveyNoteModel::addNotes(QStringList files, cwProject* project) {
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
