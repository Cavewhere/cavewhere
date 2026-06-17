#ifndef CWOFFSCREENRENDERPARAMETERS_H
#define CWOFFSCREENRENDERPARAMETERS_H

//Qt includes
#include <QColor>
#include <QMatrix4x4>
#include <QSize>

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
};

#endif // CWOFFSCREENRENDERPARAMETERS_H
