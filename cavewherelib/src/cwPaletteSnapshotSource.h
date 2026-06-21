/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPALETTESNAPSHOTSOURCE_H
#define CWPALETTESNAPSHOTSOURCE_H

//Qt includes
#include <QObject>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwPaletteSnapshot.h"

// Read-only provider of the palette snapshot a cwSketchCanvas skins its finished
// strokes with. The canvas observes one source: it pulls snapshot() to feed the
// path model and re-pulls whenever snapshotChanged() fires. This keeps authoring
// concerns out of the canvas — a glyph editor's directly-injected palette and a
// brush editor's live working-copy preview are each just a different source the
// canvas is pointed at; clearing the source (nullptr) falls back to the sketch's
// region-resolved palette.
class CAVEWHERE_LIB_EXPORT cwPaletteSnapshotSource : public QObject
{
    Q_OBJECT

public:
    explicit cwPaletteSnapshotSource(QObject *parent = nullptr);
    ~cwPaletteSnapshotSource() override;

    virtual cwPaletteSnapshot snapshot() const = 0;

signals:
    void snapshotChanged();
};

#endif // CWPALETTESNAPSHOTSOURCE_H
