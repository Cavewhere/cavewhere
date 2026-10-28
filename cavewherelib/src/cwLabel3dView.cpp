/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwLabel3dView.h"
#include "cwLabel3dItem.h"
#include "cwCamera.h"
#include "cwLabel3dGroup.h"
#include "cwRenderTextBillboards.h"
#include "cwLabelDeclutter.h"

//Qt includes
#include <algorithm>
#include <cmath>
#include <QRectF>
#include <QSizeF>

namespace {
// Pull a label toward the camera (along the line of sight, so its screen
// position and size are unaffected) so local scrap/centerline geometry near the
// station doesn't draw over the text and make it unreadable. Biased enough to
// win against local clutter while a more distant wall still occludes it. World
// units (cave coordinates are metres); tunable.
constexpr float kLabelDepthBias = 1.0f;

// Collision rects are padded around the label text: a little for labels already
// shown (so they stay put), more for hidden ones (so they only appear with room).
constexpr double kVisibleMarginFactor = 1.05;
constexpr double kInvisibleMarginFactor = 2.0;

// Declutter grid cell sized to one padded average visible label, so an average
// label covers at most 2x2 cells.
QSize paddedLabelCellSize(const QSizeF& averageLabelSize)
{
    return QSize(int(std::ceil(averageLabelSize.width() * kVisibleMarginFactor)),
                 int(std::ceil(averageLabelSize.height() * kVisibleMarginFactor)));
}

bool hasArea(const QSizeF& size)
{
    return size.width() > 0.0 && size.height() > 0.0;
}

void releaseAll(std::vector<cwTextBillboardHandle>& handles)
{
    for(cwTextBillboardHandle& handle : handles) {
        handle.reset();
    }
}
}
cwLabel3dView::cwLabel3dView(QQuickItem *parent) :
    cwBillboardOverlayItem(parent)
{
    // Text billboards draw in the 3D pass, outside this item's visibility, so a
    // hidden view drops them. Showing it again re-places them through
    // repositionBillboards().
    connect(this, &QQuickItem::visibleChanged, this, [this]() {
        if(!isVisible()) {
            releaseBillboards();
        }
    });
}

cwLabel3dView::~cwLabel3dView()
{
}

QFont cwLabel3dView::font() const
{
    return m_font;
}

void cwLabel3dView::setFont(const QFont& font)
{
    if(m_font == font) {
        return;
    }
    m_font = font;
    emit fontChanged();
    restyle();
}

QColor cwLabel3dView::color() const
{
    return m_color;
}

void cwLabel3dView::setColor(const QColor& color)
{
    if(m_color == color) {
        return;
    }
    m_color = color;
    emit colorChanged();
    restyle();
}

QColor cwLabel3dView::outlineColor() const
{
    return m_outlineColor;
}

void cwLabel3dView::setOutlineColor(const QColor& outlineColor)
{
    if(m_outlineColor == outlineColor) {
        return;
    }
    m_outlineColor = outlineColor;
    emit outlineColorChanged();
    restyle();
}

/**
 * @brief cwLabel3dView::textLayer
 *
 * The shared text layer with this view's style applied, or null while the view
 * has no window or scene.
 */
cwRenderTextBillboards* cwLabel3dView::textLayer()
{
    if(renderTextBillboards() == nullptr && ensureRenderTextBillboards() != nullptr) {
        // A new layer has no billboards yet, so only the style needs pushing.
        // Relayout here would re-enter updatePositions() mid-pass.
        applyStyle();
    }
    return renderTextBillboards();
}

/**
 * @brief cwLabel3dView::applyStyle
 *
 * Pushes this view's style to the text layer. Returns true when the layer's
 * style changed.
 */
bool cwLabel3dView::applyStyle()
{
    cwRenderTextBillboards* layer = renderTextBillboards();
    if(layer == nullptr) {
        return false;
    }

    cwRenderTextBillboards::Style style;
    style.font = m_font;
    style.color = m_color;
    style.outlineColor = m_outlineColor;
    if(layer->style() == style) {
        return false;
    }
    layer->setStyle(style);
    return true;
}

void cwLabel3dView::restyle()
{
    if(applyStyle()) {
        // Placed labels changed size; declutter again with the new rects.
        if(cwLabelDeclutter* declutter = labelDeclutter()) {
            declutter->schedulePass();
        } else {
            updatePositions();
        }
    }
}

void cwLabel3dView::releaseBillboards()
{
    for(cwLabel3dGroup* group : std::as_const(m_labelGroups)) {
        releaseAll(group->m_billboardHandles);
    }
}

void cwLabel3dView::addGroup(cwLabel3dGroup *group) {
    if(!m_labelGroups.contains(group)) {
        m_labelGroups.insert(group);
        group->setParentView(this);
        updateGroup(group);
    }
}

void cwLabel3dView::removeGroup(cwLabel3dGroup *group) {
    if(m_labelGroups.contains(group)) {
        m_labelGroups.remove(group);
        group->setParentView(nullptr);
    }
}

/**
  * @brief updateGroup
  * @param group
  *
  * Resizes the group's billboard handles to its labels (dropping the billboards
  * of removed labels) and declutters the group again. In the scene's declutter
  * pass, a hidden group releases its billboards at once and the rest waits for
  * one queued pass, so many groups changing in one turn cost one pass.
  */
void cwLabel3dView::updateGroup(cwLabel3dGroup* group)
{
    Q_ASSERT(m_labelGroups.contains(group));

    // New entries are empty handles; dropped ones remove their billboards.
    group->m_billboardHandles.resize(group->m_labels.size());

    if(cwLabelDeclutter* declutter = labelDeclutter()) {
        if(!group->isVisible()) {
            releaseAll(group->m_billboardHandles);
        }
        declutter->schedulePass();
    } else {
        updateGroupPositions(group, m_labelGrid);
    }
}

void cwLabel3dView::repositionBillboards() {
    if(cwLabelDeclutter* declutter = labelDeclutter()) {
        declutter->ensurePass(camera());
    } else {
        updatePositions();
    }
}

void cwLabel3dView::claimDeclutterSpace(cwCollisionRectGrid& grid)
{
    if(!isVisible()) {
        return;
    }

    for(cwLabel3dGroup* group : std::as_const(m_labelGroups)) {
        if(group->isVisible()) {
            updateGroupPositions(group, grid);
        }
    }
}

QSize cwLabel3dView::declutterCellSize() const
{
    return paddedLabelCellSize(m_averageLabelSize);
}

/**
 * @brief cwLabel3dView::updateGroupPositions
 *
 * Declutters one group's labels into @a grid and adds, updates or
 * removes their text billboards to match.
 *
 * If camera is null, this does nothing
 */
void cwLabel3dView::updateGroupPositions(cwLabel3dGroup* group, cwCollisionRectGrid& grid)
{
    if(camera() == nullptr) {
        return;
    }

    std::vector<cwTextBillboardHandle>& handles = group->m_billboardHandles;

    // A keyword-hidden group renders nothing and claims no declutter space:
    // release its billboards and skip projection entirely. Reached when the
    // group is toggled hidden (updateGroup); the per-frame updatePositions skips
    // hidden groups outright so this loop runs once per toggle, not every frame.
    if(!group->isVisible()) {
        releaseAll(handles);
        return;
    }

    cwRenderTextBillboards* layer = textLayer();
    if(layer == nullptr) {
        return;
    }

    Q_ASSERT(group->m_labels.size() == int(handles.size()));

    QList<cwLabel3dItem>& labels = group->m_labels;

    const QMatrix4x4 matrix = camera()->qtViewportMatrix() * camera()->viewProjectionMatrix();
    const QSizeF averageSize = m_averageLabelSize;

    const int labelCount = labels.size();

    auto processIndex = [&](int i) {
        cwTextBillboardHandle& handle = handles.at(i);
        const cwLabel3dItem& label = labels.at(i);
        const QVector3D qtViewportCoordinate = matrix.map(label.position());

        if(camera()->isQtViewportCoordinateClipped(qtViewportCoordinate)) {
            handle.reset();
            return;
        }

        // A placed label declutters with its exact laid-out size; a candidate
        // with the running average.
        const bool placed = handle.isValid();
        QSizeF stationNameTextSize = averageSize;
        if(placed) {
            const QSizeF textSize = layer->textSize(label.text());
            if(hasArea(textSize)) {
                stationNameTextSize = textSize;
            }
        }

        const double marginFactor = placed ? kVisibleMarginFactor : kInvisibleMarginFactor;
        stationNameTextSize = stationNameTextSize * marginFactor;
        const QRectF stationRect(QPointF(qtViewportCoordinate.x() - stationNameTextSize.width() * 0.5,
                                         qtViewportCoordinate.y() - stationNameTextSize.height() * 0.5),
                                 stationNameTextSize);

        if(grid.addRect(stationRect.toAlignedRect())) {
            // The billboard's model matrix places the text in 3D, depth-tested
            // against cave geometry. A station's world position is
            // camera-invariant, so this only does real work on first placement.
            setTextBillboard(handle, label.text(), label.position(), kLabelDepthBias);
        } else {
            handle.reset();
        }
    };

    auto sortVisibleEntries = [&]() {
        group->m_visibleEntries.reserve(labelCount);
        group->m_visibleEntries.clear();
        for(int i = 0; i < labelCount; i++) {
            if(labels.at(i).wasVisible()) {
                QVector3D qtViewportCoordinate = matrix.map(labels.at(i).position());
                group->m_visibleEntries.emplaceBack(i, qtViewportCoordinate);
            }
        }

        std::sort(group->m_visibleEntries.begin(), group->m_visibleEntries.end(),
                  [&](const auto& lhs, const auto& rhs) {
                      if(lhs.position.z() != rhs.position.z()) {
                          return lhs.position.z() < rhs.position.z();
                      }
                      if(lhs.position.y() != rhs.position.y()) {
                          return lhs.position.y() < rhs.position.y();
                      }
                      if(lhs.position.x() != rhs.position.x()) {
                          return lhs.position.x() < rhs.position.x();
                      }
                      return lhs.index < rhs.index;
                  });
    };

    sortVisibleEntries();

    //Go through all the station points and place the text (visible labels first)
    for(int i = 0; i < group->m_visibleEntries.size(); i++) {
        processIndex(group->m_visibleEntries.at(i).index);
    }

    for(int i = 0; i < labelCount; i++) {
        if(!labels.at(i).wasVisible()) {
            processIndex(i);
        }
    }

    double widthSum = 0.0;
    double heightSum = 0.0;
    int sizeCount = 0;
    for(int i = 0; i < labelCount; i++) {
        const bool isPlaced = handles.at(i).isValid();
        labels[i].setWasVisible(isPlaced ? 1 : 0);
        if(isPlaced) {
            const QSizeF textSize = layer->textSize(labels.at(i).text());
            if(hasArea(textSize)) {
                widthSum += textSize.width();
                heightSum += textSize.height();
                sizeCount++;
            }
        }
    }

    if(sizeCount > 0) {
        m_averageLabelSize.setWidth(widthSum / sizeCount);
        m_averageLabelSize.setHeight(heightSum / sizeCount);
    }
}

/**
 * @brief cwLabel3dView::updatePositions
 *
 * Go through each group and update the positions
 */
void cwLabel3dView::updatePositions()
{
    if(camera() == nullptr) { return; }

    if(isVisible()) {
        m_labelGrid.reset(camera()->viewport(), paddedLabelCellSize(m_averageLabelSize));

        for(cwLabel3dGroup* group : std::as_const(m_labelGroups)) {
            // Hidden groups already released their billboards when toggled; skip
            // the per-frame work entirely rather than re-walking an empty list.
            if(!group->isVisible()) {
                continue;
            }
            updateGroupPositions(group, m_labelGrid);
        }

        // Leave the grid empty for updateGroup, which declutters a single group
        // between frames.
        m_labelGrid.clear();
    }
}
