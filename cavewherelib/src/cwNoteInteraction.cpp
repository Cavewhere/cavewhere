/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwNoteInteraction.h"

cwNoteInteraction::cwNoteInteraction(QQuickItem *parent) :
    cwInteraction(parent), Note(nullptr)
{
}

void cwNoteInteraction::setNote(cwNote* note) {
    if(Note != note) {
        Note = note;
        emit noteChanged();
    }
}
