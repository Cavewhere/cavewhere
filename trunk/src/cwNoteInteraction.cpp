#include "cwNoteInteraction.h"

cwNoteInteraction::cwNoteInteraction(QDeclarativeItem *parent) :
    cwBasePanZoomInteraction(parent),
    Note(NULL)
{
}

void cwNoteInteraction::setNote(cwNote* note) {
    if(Note != note) {
        Note = note;
        emit noteChanged();
    }
}
