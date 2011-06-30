#include "cwSurveyNoteModel.h"

cwSurveyNoteModel::cwSurveyNoteModel(QObject *parent) :
    QAbstractListModel(parent)
{
    //For qml
    QHash<int, QByteArray> roles;
    roles.reserve(2);
    roles[ImagePathRole] = "imagePath";
    roles[NoteObjectRole] = "noteObject";
    setRoleNames(roles);

    //Add a few pages of notes
    cwNote* note1 = new cwNote(this);
    cwNote* note2 = new cwNote(this);
    cwNote* note3 = new cwNote(this);
    cwNote* note4 = new cwNote(this);

    note1->setImagePath("/home/blitz/documents/caving/survey/cave/us/va/washington/Debusk Mill/David Debusk Cave/notes/trip001-02.png");
    note2->setImagePath("/home/blitz/documents/caving/survey/cave/us/va/washington/Debusk Mill/David Debusk Cave/notes/trip001-04.png");
    note3->setImagePath("/home/blitz/documents/caving/survey/cave/us/va/washington/Debusk Mill/David Debusk Cave/notes/trip001-06.png");
    note4->setImagePath("/home/blitz/documents/caving/survey/cave/us/va/washington/Debusk Mill/David Debusk Cave/notes/trip001-08.png");

    Notes.append(note1);
    Notes.append(note2);
    Notes.append(note3);
    Notes.append(note4);

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
    case ImagePathRole: {
        QString imagePath = Notes[row]->imagePath();
        return QVariant(imagePath); //Notes[row]->imagePath();
    }
    case NoteObjectRole:
        return QVariant::fromValue(qobject_cast<QObject*>(Notes[row]));
    default:
        return QVariant();
    }
    return QVariant();
}
