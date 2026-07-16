#ifndef CWITEM2DRENDERER_H
#define CWITEM2DRENDERER_H

// Renders a QML Item's scene-graph subtree inline inside an already-open 3D RHI
// pass, sharing that pass's depth buffer so the 2D content is occluded by 3D
// geometry. This is the same mechanism QtQuick3D uses for its Item2D content.
//
// This is one of only two files in CaveWhere that touch Qt private API (the
// other is cwQuickItemSubscene). Everything below the pimpl boundary
// (QSGRenderer / QSGRenderTarget) is hidden; the public surface is plain Qt
// public types. If a Qt upgrade breaks the private contract, this class's smoke
// test fails rather than the whole renderer.

//Qt includes
#include <QMatrix4x4>
#include <QSize>

//Std includes
#include <memory>

class QRhiRenderTarget;
class QRhiRenderPassDescriptor;
class QRhiCommandBuffer;
class QQuickWindow;

class cwItem2DRenderer
{
public:
    // Everything the inline renderer needs for one frame. rootNodeHandle is the
    // opaque QSGRootNode pointer produced by cwQuickItemSubscene::rootNodeHandle().
    struct Frame
    {
        QMatrix4x4 mvp;                       // clip-corrected model-view-projection
        QRhiRenderTarget* rt = nullptr;
        QRhiRenderPassDescriptor* rpd = nullptr;
        QRhiCommandBuffer* cb = nullptr;
        QSize deviceRect;
        float devicePixelRatio = 1.0f;
        quintptr rootNodeHandle = 0;
    };

    explicit cwItem2DRenderer(QQuickWindow* window);
    ~cwItem2DRenderer();

    cwItem2DRenderer(const cwItem2DRenderer&) = delete;
    cwItem2DRenderer& operator=(const cwItem2DRenderer&) = delete;

    // Lazily builds the QSGRenderer and uploads the subtree's resources. Must run
    // before the target's render pass is opened (RHI forbids resource updates
    // mid-pass). Returns false if the window/context/root node isn't ready yet.
    bool prepare(const Frame& frame);

    // Records the prepared draw into the currently-open render pass. Call only
    // after a prepare() that returned true.
    void render();

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

#endif // CWITEM2DRENDERER_H
