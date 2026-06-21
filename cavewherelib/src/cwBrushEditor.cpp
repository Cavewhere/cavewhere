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
#include "cwPlacementRuleRegistry.h"
#include "cwPlacementRule.h"

//Std includes
#include <algorithm>
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

QString cwBrushEditor::brushName() const
{
    return m_loaded ? m_structureModel->brush().name : QString();
}

QString cwBrushEditor::brushDisplayName() const
{
    return m_loaded ? m_structureModel->brush().displayName : QString();
}

void cwBrushEditor::loadBrushNamed(const QString &name)
{
    const std::optional<cwLineBrush> brush =
        m_palette != nullptr ? m_palette->brush(name) : std::nullopt;

    if (!brush) {
        m_baseline = cwLineBrush();
        m_structureModel->setBrush(cwLineBrush());
        m_loaded = false;
        setDirty(false);
        emit brushLoaded();
        clearPreview();
        return;
    }

    m_baseline = *brush;
    m_structureModel->setBrush(*brush);
    m_loaded = true;
    setDirty(false);
    emit brushLoaded();
    pushPreview();
}

void cwBrushEditor::setRuleEnabled(int layerIndex, int ruleIndex, bool enabled)
{
    if (!m_loaded) {
        return;
    }
    if (!m_structureModel->setRuleEnabled(layerIndex, ruleIndex, enabled)) {
        return;
    }
    recomputeDirty();
    pushPreview();
}

void cwBrushEditor::addRule(int layerIndex, const QString &ruleName)
{
    if (!m_loaded || ruleName.isEmpty()) {
        return;
    }
    cwPlacementRuleData rule;
    rule.name = ruleName;
    const int at = m_structureModel->ruleCount(layerIndex);
    if (!m_structureModel->insertRule(layerIndex, at, rule)) {
        return;
    }
    recomputeDirty();
    pushPreview();
}

void cwBrushEditor::removeRule(int layerIndex, int ruleIndex)
{
    if (!m_loaded) {
        return;
    }
    if (!m_structureModel->removeRule(layerIndex, ruleIndex)) {
        return;
    }
    recomputeDirty();
    pushPreview();
}

QVariantList cwBrushEditor::availableRuleGroups() const
{
    // Sort by stage so each category is contiguous and the groups come out in
    // pipeline order (Stroke -> Placement -> Orientation -> Output); stable so the
    // registry order is kept within a stage.
    QVector<const cwPlacementRule *> rules = cwPlacementRuleRegistry::instance().allRules();
    std::stable_sort(rules.begin(), rules.end(),
                     [](const cwPlacementRule *a, const cwPlacementRule *b) {
                         return a->stage() < b->stage();
                     });

    QVariantList groups;
    QString currentCategory;
    QVariantList currentRules;
    const auto flush = [&]() {
        if (!currentRules.isEmpty()) {
            groups.append(QVariantMap{
                {QStringLiteral("category"), currentCategory},
                {QStringLiteral("rules"), currentRules},
            });
            currentRules.clear();
        }
    };

    for (const cwPlacementRule *rule : rules) {
        const QString category = cwPlacementRule::categoryLabel(rule->stage());
        if (category != currentCategory) {
            flush();
            currentCategory = category;
        }
        currentRules.append(QVariantMap{
            {QStringLiteral("name"), rule->displayName()},
            {QStringLiteral("description"), rule->description()},
        });
    }
    flush();
    return groups;
}

void cwBrushEditor::discard()
{
    if (!m_loaded) {
        return;
    }
    m_structureModel->setBrush(m_baseline);
    setDirty(false);
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

    target->setBrush(m_structureModel->brush());
    m_baseline = m_structureModel->brush();
    setDirty(false);

    // The real palette now carries the edit; drop the source so the sketch's
    // resolved snapshot (now including it) is what renders.
    clearPreview();
}

int cwBrushEditor::layerCount() const
{
    return m_structureModel->layerCount();
}

QString cwBrushEditor::layerLabel(int layerIndex) const
{
    return m_structureModel->layerLabel(layerIndex);
}

int cwBrushEditor::ruleCount(int layerIndex) const
{
    return m_structureModel->ruleCount(layerIndex);
}

QString cwBrushEditor::ruleName(int layerIndex, int ruleIndex) const
{
    return m_structureModel->ruleName(layerIndex, ruleIndex);
}

bool cwBrushEditor::ruleEnabled(int layerIndex, int ruleIndex) const
{
    return m_structureModel->ruleEnabled(layerIndex, ruleIndex);
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
    setDirty(m_structureModel->brush() != m_baseline);
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
    const cwLineBrush &working = m_structureModel->brush();
    cwSymbologyPaletteData data = m_palette->data();
    int found = -1;
    for (int i = 0; i < data.lineBrushes.size(); ++i) {
        if (data.lineBrushes.at(i).name == working.name) {
            found = i;
            break;
        }
    }
    if (found >= 0) {
        data.lineBrushes.replace(found, working);
    } else {
        data.lineBrushes.append(working);
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
