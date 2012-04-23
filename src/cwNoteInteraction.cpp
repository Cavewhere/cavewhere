#include "cwNoteInteraction.h"

cwNoteInteraction::cwNoteInteraction(QQuickItem *parent) :
    cwInteraction(parent), Note(NULL)
{
}

void cwNoteInteraction::setNote(cwNote* note) {
    if(Note != note) {
        Note = note;
        emit noteChanged();
    }
}
