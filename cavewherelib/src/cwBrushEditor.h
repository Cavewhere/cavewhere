/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWBRUSHEDITOR_H
#define CWBRUSHEDITOR_H

//Qt includes
#include <QPointer>
#include <QQmlEngine>
#include <QString>
#include <QVariantList>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwLineBrush.h"
#include "cwPaletteSnapshotSource.h"

// moc needs the complete types to register the QObject* property metatypes;
// Q_MOC_INCLUDE keeps the heavy includes out of this header.
Q_MOC_INCLUDE("cwSymbologyPalette.h")
Q_MOC_INCLUDE("cwSketchCanvas.h")
Q_MOC_INCLUDE("cwCavingRegion.h")
Q_MOC_INCLUDE("cwBrushStructureModel.h")

class cwSymbologyPalette;
class cwSketchCanvas;
class cwCavingRegion;
class cwBrushStructureModel;

// Working-copy controller for the in-app brush editor (commit 9, phase 1). Holds
// one editable cwLineBrush working copy plus the unedited baseline it was loaded
// from; every mutation touches only the working copy, so the real palette is
// never written until apply().
//
// It is itself the live preview's cwPaletteSnapshotSource: snapshot() returns the
// resolved palette with the working brush swapped in, and each edit installs the
// editor as previewCanvas's source and re-pushes, so the live sketch re-skins
// immediately without mutating the palette. discard()/apply() clear the source so
// the canvas falls back to its sketch-resolved snapshot.
//
// apply() commits: on a writable palette it is palette->setBrush(working); on a
// read-only palette (e.g. the shipped default a fresh region resolves to) it
// forks instead — duplicatePalette, repoint region->defaultPaletteId() at the
// fork, and write the working copy into it ("Duplicate & Save"). discard()
// restores the baseline. The brush structure is exposed through plain invokable
// accessors (cwLineBrush is not a QML gadget) for the read-only structure tree.
class CAVEWHERE_LIB_EXPORT cwBrushEditor : public cwPaletteSnapshotSource
{
    Q_OBJECT
    QML_NAMED_ELEMENT(BrushEditor)

    Q_PROPERTY(cwSymbologyPalette* palette READ palette WRITE setPalette NOTIFY paletteChanged)
    Q_PROPERTY(cwSketchCanvas* previewCanvas READ previewCanvas WRITE setPreviewCanvas NOTIFY previewCanvasChanged)
    Q_PROPERTY(cwCavingRegion* region READ region WRITE setRegion NOTIFY regionChanged)
    Q_PROPERTY(QString brushName READ brushName NOTIFY brushLoaded)
    Q_PROPERTY(QString brushDisplayName READ brushDisplayName NOTIFY brushLoaded)
    Q_PROPERTY(bool loaded READ isLoaded NOTIFY brushLoaded)
    Q_PROPERTY(bool dirty READ isDirty NOTIFY dirtyChanged)
    Q_PROPERTY(bool paletteWritable READ isPaletteWritable NOTIFY paletteWritableChanged)
    Q_PROPERTY(cwBrushStructureModel* structureModel READ structureModel CONSTANT)

public:
    explicit cwBrushEditor(QObject *parent = nullptr);
    ~cwBrushEditor() override;

    cwSymbologyPalette *palette() const { return m_palette; }
    void setPalette(cwSymbologyPalette *palette);

    cwSketchCanvas *previewCanvas() const { return m_previewCanvas; }
    void setPreviewCanvas(cwSketchCanvas *canvas);

    cwCavingRegion *region() const { return m_region; }
    void setRegion(cwCavingRegion *region);

    QString brushName() const;
    QString brushDisplayName() const;
    bool isLoaded() const { return m_loaded; }
    bool isDirty() const { return m_dirty; }
    bool isPaletteWritable() const;

    // The tree model (layers -> rules) driving the editor's TreeView. Owned by
    // the editor; the editor drives its change notifications.
    cwBrushStructureModel *structureModel() const { return m_structureModel; }

    // Load the named brush from the current palette into a fresh working copy and
    // baseline (clears dirty). An empty or unknown name unloads the editor. Pushes
    // the initial preview (working == baseline, so the sketch looks unchanged).
    Q_INVOKABLE void loadBrushNamed(const QString &name);

    // Toggle one rule's enabled flag on the working copy and re-push the preview.
    // Out-of-range indices are ignored.
    Q_INVOKABLE void setRuleEnabled(int layerIndex, int ruleIndex, bool enabled);

    // Append a rule (by registry display name) to a layer, or remove one, then
    // re-push the preview. Out-of-range / unknown requests are ignored. The
    // structure model brackets the row insert/remove; this layer owns the
    // dirty/preview side effects.
    Q_INVOKABLE void addRule(int layerIndex, const QString &ruleName);
    Q_INVOKABLE void removeRule(int layerIndex, int ruleIndex);

    // The add-rule picker's data: rules grouped by category (derived from each
    // rule's stage), in pipeline order. Each group is { "category": QString,
    // "rules": [ { "name": QString, "description": QString }, ... ] }.
    Q_INVOKABLE QVariantList availableRuleGroups() const;

    // Restore the working copy to the baseline and drop the preview override.
    Q_INVOKABLE void discard();

    // Commit the working copy: setBrush on a writable palette, or fork-on-save on
    // a read-only one (Duplicate & Save). The baseline becomes the working copy
    // and the preview override is dropped so the real re-skin shows.
    Q_INVOKABLE void apply();

    // Read-only structure accessors for the QML tree (cwLineBrush isn't a gadget),
    // delegating to the structure model. The model's own row/data signals notify
    // the view of changes.
    Q_INVOKABLE int layerCount() const;
    Q_INVOKABLE QString layerLabel(int layerIndex) const;
    Q_INVOKABLE int ruleCount(int layerIndex) const;
    Q_INVOKABLE QString ruleName(int layerIndex, int ruleIndex) const;
    Q_INVOKABLE bool ruleEnabled(int layerIndex, int ruleIndex) const;

    // cwPaletteSnapshotSource: the resolved palette with the working brush swapped
    // in. Returns the unmodified palette snapshot when no brush is loaded.
    cwPaletteSnapshot snapshot() const override;

signals:
    void paletteChanged();
    void previewCanvasChanged();
    void regionChanged();
    void brushLoaded();
    void dirtyChanged();
    void paletteWritableChanged();

private:
    void setDirty(bool dirty);
    void recomputeDirty();

    // Install the editor as previewCanvas's snapshot source and re-push, so the
    // working copy re-skins the live sketch. No-op until a brush is loaded.
    void pushPreview();
    // Drop the editor as previewCanvas's source (falling the canvas back to its
    // sketch-resolved snapshot), but only if we are still the active source.
    void clearPreview();

    cwBrushStructureModel *m_structureModel = nullptr;
    QPointer<cwSymbologyPalette> m_palette;
    QPointer<cwSketchCanvas> m_previewCanvas;
    QPointer<cwCavingRegion> m_region;
    QMetaObject::Connection m_paletteWritableConnection;

    // The working brush lives in m_structureModel (it is the mutation surface);
    // m_baseline is the unedited copy load() captured, for dirty/discard.
    cwLineBrush m_baseline;
    bool m_loaded = false;
    bool m_dirty = false;
};

#endif // CWBRUSHEDITOR_H
