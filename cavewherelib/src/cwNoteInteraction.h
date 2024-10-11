/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWNOTEINTERACTION_H
#define CWNOTEINTERACTION_H

//Our includes
#include "cwNote.h"
#include "cwBasePanZoomInteraction.h"


class cwNoteInteraction : public cwInteraction
{
    Q_OBJECT

    Q_PROPERTY(cwNote* note READ note WRITE setNote NOTIFY noteChanged)

public:
    explicit cwNoteInteraction(QQuickItem *parent = 0);

    void setNote(cwNote* note);
    cwNote* note() const;

signals:
    void noteChanged();

public slots:

private:
    cwNote* Note;
};

/**
  Sets the note that this interaction will modify
  */
inline cwNote* cwNoteInteraction::note() const {
    return Note;
}

#endif // CWNOTEINTERACTION_H
