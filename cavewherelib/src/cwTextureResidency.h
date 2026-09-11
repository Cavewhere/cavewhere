#ifndef CWTEXTURERESIDENCY_H
#define CWTEXTURERESIDENCY_H

// Qt includes
#include <QMatrix4x4>
#include <QSize>
#include <QVector>

// Qt RHI
#include <rhi/qrhi.h>

// QMath3d includes
#include <QBox3D>

// Our includes
#include "CaveWhereLibExport.h"
#include "cwGeometry.h"

/**
 * Mip residency selection and eviction for streamed textures. Chain shape and
 * byte math lives in cwMipMath.h. Every function here is pure and safe to call
 * from any thread — no Qt GUI state, no RHI objects, no globals.
 */
namespace cw::residency {

    /**
     * The largest dimension a pinned base level may have. The base level and
     * everything coarser stays resident for the life of the item, so a textured
     * item always has something to draw.
     */
    constexpr int kPinnedBaseMaxDimension = 512;

    /**
     * How many triangles uvPerMeter() samples, spread evenly across the index
     * buffer.
     */
    constexpr int kDensitySampleTriangles = 64;

    /**
     * The coarsest-but-one level that stays pinned: the smallest level whose
     * largest dimension is at most kPinnedBaseMaxDimension. Returns 0 when
     * level0 is already that small.
     */
    CAVEWHERE_LIB_EXPORT int pinnedBaseLevel(QSize level0);

    /**
     * The texel density of geometry in model space: sqrt(uv area / world area),
     * averaged over up to maxSampleTriangles triangles sampled evenly across
     * the index buffer. World area uses positions mapped through modelMatrix.
     *
     * Returns 0 when the density is unknown — no indices, no TexCoord0, or zero
     * area — which selection treats as a request for full detail.
     */
    CAVEWHERE_LIB_EXPORT double uvPerMeter(const cwGeometry& geometry,
                                           const QMatrix4x4& modelMatrix,
                                           int maxSampleTriangles = kDensitySampleTriangles);

    /**
     * Everything desiredTopLevel() needs to pick a mip level for one item.
     */
    struct CAVEWHERE_LIB_EXPORT SelectionInput
    {
        QSize textureSize;            //Level-0 dimensions
        double uvPerMeter = 0.0;      //From uvPerMeter(), model space
        double modelScale = 1.0;      //Mean column norm of the model matrix
        QBox3D worldBounds;
        QMatrix4x4 viewProjection;    //Clip-space corrected, row 3 is the w row
        double absP11 = 0.0;          //|projectionMatrix()(1, 1)|
        int viewportHeightPx = 0;     //Physical pixels
        double screenSpaceErrorPx = 1.5;
    };

    /**
     * The most detailed mip level worth streaming for this item, clamped to
     * [0, pinnedBaseLevel(textureSize)]. Unknown density or a degenerate box
     * asks for level 0 and lets the budget arbitrate.
     */
    CAVEWHERE_LIB_EXPORT int desiredTopLevel(const SelectionInput& input);

    /**
     * What the planner knows about one item's resident texture.
     */
    struct CAVEWHERE_LIB_EXPORT ResidencyStats
    {
        QSize textureSize;
        QRhiTexture::Format format = QRhiTexture::UnknownFormat;
        int residentTopLevel = -1;    //-1 when nothing is resident
        //The level selection last asked the camera for; -1 when unknown
        int desiredTopLevel = -1;
        quint64 lastVisibleFrame = 0;
        bool visibleThisFrame = false;
        bool demotionInFlight = false;
    };

    /**
     * One planned demotion back to the pinned base level.
     */
    struct CAVEWHERE_LIB_EXPORT Demotion
    {
        int itemIndex = -1;
        int newTopLevel = 0;
        qint64 reclaimedBytes = 0;
    };

    /**
     * Picks items to demote to their pinned base until overshootBytes is
     * covered. Invisible items go first, oldest lastVisibleFrame first, then
     * visible items in the same order. Items already at or below their base and
     * items with a demotion in flight are left alone, as is a visible item
     * whose desiredTopLevel is finer than its base — selection would ask for
     * the detail straight back, so demoting it only churns. Returns the plan it
     * has even when it cannot cover the whole overshoot.
     */
    CAVEWHERE_LIB_EXPORT QVector<Demotion> planEvictions(const QVector<ResidencyStats>& items,
                                                         qint64 overshootBytes);

    /**
     * Charges cost against remainingBytes for this frame's uploads. Grants when
     * it fits, and always grants the first upload of a frame so a level larger
     * than the whole budget still makes progress.
     */
    CAVEWHERE_LIB_EXPORT bool takeFromBudget(qint64& remainingBytes,
                                             qint64 cost,
                                             bool anythingUploadedThisFrame);
}

#endif // CWTEXTURERESIDENCY_H
