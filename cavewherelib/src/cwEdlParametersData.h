/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWEDLPARAMETERSDATA_H
#define CWEDLPARAMETERSDATA_H

// Plain-data bundle of the Eye-Dome Lighting tuning knobs. The member
// initializers are the single source of truth for the defaults, shared by the
// QML-facing cwEDLSettings, the render-thread cwTracked relay in cwRhiScene, and
// the GPU-side cwEDLEffect. See cwEDLEffect / EDL.frag for what each one does.
struct EdlParametersData {
    float strength = 1500.0f;   // baseline slope of the darkening transfer function
    float maxDarken = 3.0f;     // per-neighbor darkening ceiling
    float radiusPx = 1.4f;      // neighbor sample spread, in device pixels

    bool operator==(const EdlParametersData& other) const = default;
};

#endif // CWEDLPARAMETERSDATA_H
