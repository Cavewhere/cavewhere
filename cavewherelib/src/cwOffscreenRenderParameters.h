#ifndef CWOFFSCREENRENDERPARAMETERS_H
#define CWOFFSCREENRENDERPARAMETERS_H

//Qt includes
#include <QColor>
#include <QMatrix4x4>
#include <QSet>
#include <QSize>

//Std includes
#include <optional>

//Our includes
#include "cwRenderObjectId.h"
#include "cwEdlParametersData.h"

/**
 * @brief Pure-input description of an offscreen render of the resident scene.
 *
 * A GUI-thread consumer (the hi-res map exporter, the sink classifier, the debug
 * render hook) fills in a camera (view + projection matrices), a clear colour, an
 * output size, and a device pixel ratio, then passes it by const ref to
 * cwScene::renderOffscreen(), which returns a QFuture<QImage> for the result.
 *
 * This struct is input only: it carries no promise/future and no ownership
 * machinery. cwScene owns the QPromise internally (see cwOffscreenRenderJob), so
 * a caller can never resolve, cancel, or otherwise manipulate the result channel
 * except through the QFuture it is handed back.
 */
struct cwOffscreenRenderParameters {
    QMatrix4x4 viewMatrix;
    QMatrix4x4 projectionMatrix;
    QColor backgroundColor = QColor::fromRgbF(0.0, 0.0, 0.0, 0.0);
    QSize outputSize;
    // Device pixel ratio fed to the camera UBO. Screen-size-dependent geometry
    // (notably point-cloud point size, which scales by devicePixelRatio in the
    // shader) needs this to match the live view's DPR, otherwise an offscreen
    // render on a Retina/HiDPI display draws points at the wrong size. Defaults
    // to 1.0 for display-independent renders (e.g. a fixed-resolution export);
    // the "render what's on screen" path sets it from the live camera.
    float devicePixelRatio = 1.0f;
    // MSAA sample count the scene is rasterized at, then resolved down to the 1x
    // read-back image, so anti-aliased edges match the live view. Capability-gated at
    // render time (falls back to 1 when the backend lacks MSAA support for the count).
    // Defaults to 1 (no AA) for display-independent renders (a fixed-resolution export
    // / the sink classifier); the "render what's on screen" path sets it from the live
    // framebuffer's sample count (cwRenderingSettings::sampleCount) so the render is a
    // faithful copy.
    int sampleCount = 1;

    // Per-job appearance overrides. Both default to "render the scene exactly as the
    // live frame does", so a plain capture is unchanged.
    //
    // Render objects to suppress for this job only, by stable render-object id. The
    // live frame's visibility is untouched (no setVisible mutation, no race): the ids
    // are resolved to render objects inside cwRhiScene::gatherScene and skipped there.
    // Lets a capture hide chrome (gradient/grid/line-plot) while the on-screen view
    // stays interactive — replacing the global cwRegionSceneManager::setCapturing hack.
    QSet<cwRenderObjectId> hiddenObjectIds;
    // EDL tuning for this job. Absent = use the scene's live EDL parameters (the
    // back-compatible default); set it to render with a different eye-dome look than
    // the on-screen view.
    std::optional<EdlParametersData> edlOverride = std::nullopt;

    // Per-object appearance slot this job renders with. 0 (default) = the live
    // appearance every render object wrote to slot 0, so a plain capture matches
    // the on-screen look. A consumer that wants an overridden look (e.g. a
    // monochrome point cloud at a custom radius) first writes those values into a
    // higher appearance slot of the relevant render objects, then issues the job
    // with that slot here; cwRhiScene stamps it into every GatherContext and each
    // object resolves it to its own dynamic UBO offset. Valid range is
    // [0, cwRHIObject::kAppearanceSlotCount).
    int appearanceSlot = 0;
};

#endif // CWOFFSCREENRENDERPARAMETERS_H
