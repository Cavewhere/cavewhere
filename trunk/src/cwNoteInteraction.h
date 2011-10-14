#ifndef CWNOTEINTERACTION_H
#define CWNOTEINTERACTION_H

//Our includes
class cwNote;
#include "cwBasePanZoomInteraction.h"


class cwNoteInteraction : public cwBasePanZoomInteraction
{
    Q_OBJECT

    Q_PROPERTY(cwNote* note READ note WRITE setNote NOTIFY noteChanged)

public:
    explicit cwNoteInteraction(QDeclarativeItem *parent = 0);

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
