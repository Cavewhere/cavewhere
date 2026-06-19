/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWAPPEARANCEOVERRIDE_H
#define CWAPPEARANCEOVERRIDE_H

// Std includes
#include <any>
#include <utility>

// Opaque, type-erased per-render-object appearance payload carried on an
// offscreen render job (cwOffscreenRenderParameters::appearanceOverrides, keyed
// by cwRenderObjectId). The shared job API knows no object's appearance schema:
// each render object defines its own payload struct (e.g. cwPointCloudAppearance)
// and only that object's back-end unpacks it in cwAppearanceSlotted::
// uploadAppearance(). Holding the payload here — rather than a bare slot index
// plus persistent renderer state — keeps the job self-describing and lets each
// object's schema diverge per build without touching this type. See
// plans/APPEARANCE_OVERRIDE_ONJOB_PLAN.html §7.
class cwAppearanceOverride
{
public:
    cwAppearanceOverride() = default;

    template <class Payload>
    explicit cwAppearanceOverride(Payload payload)
        : m_payload(std::move(payload)) {}

    // The held payload as @c Payload, or nullptr when empty or holding a
    // different type — so a back-end that does not recognise the payload simply
    // renders the object at its live appearance.
    template <class Payload>
    const Payload* payload() const { return std::any_cast<Payload>(&m_payload); }

    bool isEmpty() const { return !m_payload.has_value(); }

private:
    std::any m_payload;
};

#endif // CWAPPEARANCEOVERRIDE_H
