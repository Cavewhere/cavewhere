/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPOINTCLOUDFOREST_H
#define CWPOINTCLOUDFOREST_H

//Our includes
#include "CaveWhereLibExport.h"
#include "cwFrustum.h"
#include "cwPointOctreeSelection.h"
#include "cwRHIObject.h"

//Qt includes
#include <QVector>

//Std includes
#include <functional>

class cwPointOctreeManifest;
struct cwRenderBudgets;

/**
 * The one cut every point cloud of the live view draws from, and the one
 * governor that sizes it.
 *
 * Each cloud used to select its own cut against a share of the point budget
 * and step its own screen-space-error inflation, so two layers of the same
 * survey settled at different depths and their sprites stepped in size across
 * the file boundary. The forest holds the trees of every visible cloud, cuts
 * them together under one cap, and hands each cloud back its slice, so a node
 * of one cloud and a node of another that project the same land at the same
 * level.
 *
 * Render-thread only, and plain C++: cwRhiFrameRenderer owns one.
 */
class CAVEWHERE_LIB_EXPORT cwPointCloudForest
{
public:
    //! @a cloud's tree, as of its current source. A null @a manifest removes it.
    void setTree(const cwRHIObject* cloud, const cwPointOctreeManifest* manifest);

    //! Drops @a cloud, so the next cut spans the trees that are left
    void removeTree(const cwRHIObject* cloud);

    /**
     * The governor's step for this frame, taken at the top of a live frame from
     * the cut selected last frame against what @a budgets leave the clouds.
     *
     * Runs before any cloud streams, so the threshold the shader sizes against
     * and the cut gather() picks are one frame's inflation rather than two.
     */
    void advance(const cwRenderBudgets& budgets);

    /**
     * This frame's cut over every tree @a draws admits, from the camera of
     * @a renderData and the frame's @a frustum. Live frames only: an export job
     * renders its own camera and leaves the forest where the live frame put it.
     */
    void select(const cwRHIObject::RenderData& renderData,
                const cwFrustum& frustum,
                const std::function<bool(const cwRHIObject*)>& draws);

    //! The slice of the last select() that belongs to @a cloud, empty for a
    //! cloud that select() left out
    const cw::octree::Selection& selectionFor(const cwRHIObject* cloud) const;

    double sseInflation() const { return m_sseInflation; }
    bool pointCapped() const { return m_selected.pointCapped; }
    qint64 points() const { return m_selected.points; }
    qint64 bytes() const { return m_selected.bytes; }

    //! Frames since the last relax probe, which only runs while inflated
    int relaxProbeFrame() const { return m_relaxProbeFrame; }

    //! Bytes of the last probed cut, -1 when the probe has not run
    qint64 desiredBytesRelaxed() const { return m_desiredBytesRelaxed; }

private:
    //! One cloud's tree and the slice of the last cut that belongs to it
    struct Tree {
        const cwRHIObject* cloud = nullptr;
        const cwPointOctreeManifest* manifest = nullptr;
        cw::octree::Selection selection;
    };

    //! The cut one step finer, so nextSseInflation() has evidence to step down on
    void probeRelaxedCut();

    bool anyNodeSelected() const;

    //! Drops the last cut, whose trees the probe would otherwise re-select from
    void forgetLastCut();

    int indexOfCloud(const cwRHIObject* cloud) const;

    //In registration order, which is the cut's tie-break order
    QVector<Tree> m_trees;

    cw::octree::ForestSelection m_selected;

    double m_sseInflation = 1.0;
    qint64 m_desiredBytesRelaxed = -1;
    bool m_pointCappedRelaxed = false;
    int m_relaxedSteps = 1;
    int m_relaxProbeFrame = 0;
    qint64 m_availableBytes = 0;

    //What the probe re-selects from, with m_lastFrustum standing in for the
    //frame's own, which is gone by the time the probe runs
    cw::octree::ForestInput m_lastInput;
    cwFrustum m_lastFrustum;
};

#endif // CWPOINTCLOUDFOREST_H
