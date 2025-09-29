#pragma once

/**************************************************************************
**
**    Copyright (C) 2025 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Qt
#include <QAbstractListModel>
#include <QList>
#include <QQmlEngine>


// Fwd decls
class cwTrip;
class cwCave;
class cwProject;

//Our includes
#include "cwGlobals.h"

/**
 * @brief Base list model for survey note-like objects (QObject-derived).
 *
 * Subclasses define how to ingest files and how to translate their note
 * objects into the unified roles below.
 */
class CAVEWHERE_LIB_EXPORT cwSurveyNoteModelBase : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SurveyNoteModelBase)

public:
    enum Roles {
        PathRole        = Qt::UserRole + 1,  // display path (absolute or project-relative)
        IconPathRole,                        // icon/thumbnail path (may be empty for some notes)
        ImageRole,                           // QVariant-wrapped cwImage (empty for non-image notes)
        NoteObjectRole                       // QObject* for the note itself
    };

    explicit cwSurveyNoteModelBase(QObject* parent = nullptr);

    // QAbstractListModel
    Q_INVOKABLE int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    Q_INVOKABLE QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Access
    QList<QObject*> notes() const;
    Q_INVOKABLE void removeNote(int index);
    Q_INVOKABLE bool hasNotes() const;

    // Parent wiring
    void setParentTrip(cwTrip* trip);
    cwTrip* parentTrip() const;

    // void setParentCave(cwCave* cave);
    cwCave* parentCave() const;

    // File ingestion — subclass implements
    Q_INVOKABLE virtual void addFromFiles(QList<QUrl> files) = 0;

protected:
    // Helpers for subclasses
    void addNotes(QList<QObject*> newNotes);
    void clearNotes();
    cwProject* project() const;

    // Hooks for subclasses to push parents into notes
    virtual void onParentTripChanged();

private:
    QList<QObject*> m_notes;
    cwTrip* m_parentTrip;
    cwCave* m_parentCave;
};
