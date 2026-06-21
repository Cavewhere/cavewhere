/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwBrushEditor.h"
#include "cwBrushStructureModel.h"
#include "cwSymbologyPalette.h"
#include "cwSymbologyPaletteManager.h"
#include "cwSketchCanvas.h"
#include "cwCavingRegion.h"
#include "cwSymbologyPaletteData.h"
// cwSketchCanvas exposes inline accessors over QPointer<cwScrapManager>/<cwTrip>
// members it only forward-declares; pull the complete types in so this TU can
// instantiate them.
#include "cwScrapManager.h"
#include "cwTrip.h"

cwBrushEditor::cwBrushEditor(QObject *parent) :
    cwPaletteSnapshotSource(parent)
{
    m_structureModel = new cwBrushStructureModel(this);
}

cwBrushEditor::~cwBrushEditor()
{
    clearPreview();
}

void cwBrushEditor::setPalette(cwSymbologyPalette *palette)
{
    if (m_palette == palette) {
        return;
    }

    if (m_paletteWritableConnection) {
        QObject::disconnect(m_paletteWritableConnection);
        m_paletteWritableConnection = {};
    }

    m_palette = palette;

    if (m_palette != nullptr) {
        m_paletteWritableConnection =
            connect(m_palette, &cwSymbologyPalette::writableChanged,
                    this, &cwBrushEditor::paletteWritableChanged);
    }

    emit paletteChanged();
    emit paletteWritableChanged();
}

void cwBrushEditor::setPreviewCanvas(cwSketchCanvas *canvas)
{
    if (m_previewCanvas == canvas) {
        return;
    }

    // Drop our source on the canvas we're leaving so a stale preview doesn't
    // linger on it.
    clearPreview();

    m_previewCanvas = canvas;
    emit previewCanvasChanged();

    pushPreview();
}

void cwBrushEditor::setRegion(cwCavingRegion *region)
{
    if (m_region == region) {
        return;
    }
    m_region = region;
    emit regionChanged();
}

bool cwBrushEditor::isPaletteWritable() const
{
    return m_palette != nullptr && m_palette->isWritable();
}

void cwBrushEditor::loadBrushNamed(const QString &name)
{
    const std::optional<cwLineBrush> brush =
        m_palette != nullptr ? m_palette->brush(name) : std::nullopt;

    if (!brush) {
        m_working = cwLineBrush();
        m_baseline = cwLineBrush();
        m_loaded = false;
        setDirty(false);
        m_structureModel->reset();
        emit brushLoaded();
        emit structureChanged();
        clearPreview();
        return;
    }

    m_working = *brush;
    m_baseline = *brush;
    m_loaded = true;
    setDirty(false);
    m_structureModel->reset();
    emit brushLoaded();
    emit structureChanged();
    pushPreview();
}

void cwBrushEditor::setRuleEnabled(int layerIndex, int ruleIndex, bool enabled)
{
    if (!m_loaded || layerIndex < 0 || layerIndex >= m_working.decorations.size()) {
        return;
    }

    cwDecorationLayer layer = m_working.decorations.at(layerIndex);
    if (ruleIndex < 0 || ruleIndex >= layer.rules.size()) {
        return;
    }
    if (layer.rules.at(ruleIndex).enabled == enabled) {
        return;
    }

    cwPlacementRuleData rule = layer.rules.at(ruleIndex);
    rule.enabled = enabled;
    layer.rules.replace(ruleIndex, rule);
    m_working.decorations.replace(layerIndex, layer);

    recomputeDirty();
    m_structureModel->ruleChanged(layerIndex, ruleIndex);
    emit structureChanged();
    pushPreview();
}

void cwBrushEditor::discard()
{
    if (!m_loaded) {
        return;
    }
    m_working = m_baseline;
    setDirty(false);
    m_structureModel->reset();
    emit structureChanged();
    clearPreview();
}

void cwBrushEditor::apply()
{
    if (!m_loaded || m_palette == nullptr) {
        return;
    }

    cwSymbologyPalette *target = m_palette;

    // Fork-on-save: a read-only palette (the shipped default a fresh region
    // resolves to) is never mutated. Duplicate it into a writable fork, repoint
    // the region at the fork, and write the working copy there instead.
    if (!target->isWritable()) {
        cwSymbologyPaletteManager *manager = cwSymbologyPaletteManager::instance();
        if (manager == nullptr) {
            return;
        }
        cwSymbologyPalette *fork = manager->duplicatePalette(target, target->name());
        if (fork == nullptr) {
            return;   // reason already on the manager's error model
        }
        if (m_region != nullptr) {
            m_region->setDefaultPaletteId(fork->id());
        }
        setPalette(fork);
        target = fork;
    }

    target->setBrush(m_working);
    m_baseline = m_working;
    setDirty(false);

    // The real palette now carries the edit; drop the source so the sketch's
    // resolved snapshot (now including it) is what renders.
    clearPreview();
}

int cwBrushEditor::layerCount() const
{
    return m_loaded ? static_cast<int>(m_working.decorations.size()) : 0;
}

QString cwBrushEditor::layerLabel(int layerIndex) const
{
    if (!m_loaded || layerIndex < 0 || layerIndex >= m_working.decorations.size()) {
        return QString();
    }
    const cwDecorationLayer &layer = m_working.decorations.at(layerIndex);
    return layer.glyphName;   // empty for a line (offset-polyline) layer
}

int cwBrushEditor::ruleCount(int layerIndex) const
{
    if (!m_loaded || layerIndex < 0 || layerIndex >= m_working.decorations.size()) {
        return 0;
    }
    return static_cast<int>(m_working.decorations.at(layerIndex).rules.size());
}

QString cwBrushEditor::ruleName(int layerIndex, int ruleIndex) const
{
    if (!m_loaded || layerIndex < 0 || layerIndex >= m_working.decorations.size()) {
        return QString();
    }
    const cwDecorationLayer &layer = m_working.decorations.at(layerIndex);
    if (ruleIndex < 0 || ruleIndex >= layer.rules.size()) {
        return QString();
    }
    return layer.rules.at(ruleIndex).name;
}

bool cwBrushEditor::ruleEnabled(int layerIndex, int ruleIndex) const
{
    if (!m_loaded || layerIndex < 0 || layerIndex >= m_working.decorations.size()) {
        return false;
    }
    const cwDecorationLayer &layer = m_working.decorations.at(layerIndex);
    if (ruleIndex < 0 || ruleIndex >= layer.rules.size()) {
        return false;
    }
    return layer.rules.at(ruleIndex).enabled;
}

void cwBrushEditor::setDirty(bool dirty)
{
    if (m_dirty == dirty) {
        return;
    }
    m_dirty = dirty;
    emit dirtyChanged();
}

void cwBrushEditor::recomputeDirty()
{
    setDirty(m_working != m_baseline);
}

cwPaletteSnapshot cwBrushEditor::snapshot() const
{
    if (m_palette == nullptr) {
        return cwPaletteSnapshot();
    }
    if (!m_loaded) {
        return m_palette->snapshot();
    }

    // The resolved palette's data with the working brush swapped in — a
    // copy-with-substitution, so the live preview reflects the edit while the
    // real palette stays untouched.
    cwSymbologyPaletteData data = m_palette->data();
    int found = -1;
    for (int i = 0; i < data.lineBrushes.size(); ++i) {
        if (data.lineBrushes.at(i).name == m_working.name) {
            found = i;
            break;
        }
    }
    if (found >= 0) {
        data.lineBrushes.replace(found, m_working);
    } else {
        data.lineBrushes.append(m_working);
    }
    return data.snapshot();
}

void cwBrushEditor::pushPreview()
{
    if (m_previewCanvas == nullptr || m_palette == nullptr || !m_loaded) {
        return;
    }

    if (m_previewCanvas->snapshotSource() != this) {
        m_previewCanvas->setSnapshotSource(this);   // installs and re-pulls snapshot()
    } else {
        emit snapshotChanged();                      // already ours; re-pull
    }
}

void cwBrushEditor::clearPreview()
{
    if (m_previewCanvas != nullptr && m_previewCanvas->snapshotSource() == this) {
        m_previewCanvas->setSnapshotSource(nullptr);
    }
}
