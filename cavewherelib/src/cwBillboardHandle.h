#ifndef CWBILLBOARDHANDLE_H
#define CWBILLBOARDHANDLE_H

// Move-only RAII owner of exactly one billboard in a shared billboard layer
// (cwRenderBillboards for QQuickItem content, cwRenderSpriteBillboards for
// instances of one sprite prototype, see cwSpriteBillboardHandle.h, and
// cwRenderTextBillboards for station label text, see cwTextBillboardHandle.h).
// The layer type is a template parameter so all three layers share one
// implementation. A producer (cwLeadView with sprite handles, cwLabel3dView
// with text handles, any future overlay) holds a handle next to whatever it already
// tracks and calls set() each update pass: the first call adds the billboard and
// remembers its id, later calls update it (cheap — updateBillboard early-outs
// when nothing changed). Dropping the handle
// — reset(), move-out, or its container being destroyed/resized — removes the
// billboard from the layer, so a producer cannot leak into the shared layer and
// there are no manual removeBillboard calls or teardown loops to keep in sync.
//
// Holds a QPointer to the layer so a handle outliving the layer (teardown order)
// reduces to a harmless no-op rather than a dangling-pointer remove.

//Our includes
#include "cwRenderBillboards.h"

//Qt includes
#include <QPointer>

// Layer needs addBillboard(const Billboard&) -> cwBillboardId,
// updateBillboard(cwBillboardId, const Billboard&) and removeBillboard(cwBillboardId).
template <typename Layer, typename Billboard>
class cwLayerBillboardHandle
{
public:
    cwLayerBillboardHandle() = default;

    ~cwLayerBillboardHandle()
    {
        reset();
    }

    cwLayerBillboardHandle(cwLayerBillboardHandle&& other) noexcept
        : m_layer(other.m_layer), m_id(other.m_id)
    {
        other.m_layer = nullptr;
        other.m_id = cwBillboardId{};
    }

    cwLayerBillboardHandle& operator=(cwLayerBillboardHandle&& other) noexcept
    {
        if (this != &other) {
            reset();
            m_layer = other.m_layer;
            m_id = other.m_id;
            other.m_layer = nullptr;
            other.m_id = cwBillboardId{};
        }
        return *this;
    }

    cwLayerBillboardHandle(const cwLayerBillboardHandle&) = delete;
    cwLayerBillboardHandle& operator=(const cwLayerBillboardHandle&) = delete;

    // Adds the billboard on the first call, updates it thereafter. If @a layer
    // differs from the one this handle already owns a billboard in (a layer
    // swap), the old billboard is removed first so the handle owns at most one.
    // A null @a layer just releases any held billboard.
    void set(Layer* layer, const Billboard& billboard)
    {
        // Reset whenever our layer isn't the requested one. This also covers the
        // case where m_layer (a QPointer) auto-nulled because the old layer was
        // destroyed while m_id still held its old value: without the reset, set()
        // would take the updateBillboard() branch against a layer that never
        // minted m_id and silently drop the billboard.
        if (m_layer != layer) {
            reset();
        }
        if (!layer) {
            return;
        }
        m_layer = layer;
        if (m_id == cwBillboardId{}) {
            m_id = layer->addBillboard(billboard);
        } else {
            layer->updateBillboard(m_id, billboard);
        }
    }

    // Removes the held billboard (if any) and returns the handle to empty.
    void reset()
    {
        if (m_layer && m_id != cwBillboardId{}) {
            m_layer->removeBillboard(m_id);
        }
        m_layer = nullptr;
        m_id = cwBillboardId{};
    }

    bool isValid() const
    {
        return m_layer && m_id != cwBillboardId{};
    }

    cwBillboardId id() const
    {
        return m_id;
    }

private:
    QPointer<Layer> m_layer;
    cwBillboardId m_id = cwBillboardId{};
};

using cwBillboardHandle = cwLayerBillboardHandle<cwRenderBillboards, cwRenderBillboards::Billboard>;

#endif // CWBILLBOARDHANDLE_H
