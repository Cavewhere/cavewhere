#ifndef CWRHISCENETESTACCESS_H
#define CWRHISCENETESTACCESS_H

//Our includes
#include "cwRhiScene.h"
#include "cwRenderObjectId.h"

// Friend accessor (declared `friend struct CwRhiSceneTestAccess` in cwRhiScene.h)
// so a test can drive the backend's private apply step — normally called only by
// cwRhiItemRenderer — and inspect the resulting render-object registry, without
// putting test scaffolding on the production API.
//
// This is the render-side seam the scene-lifetime tests observe through. After
// synchroize() drains cwScene's pending queue, the registry reflects exactly what
// that queue held: an id that resolves to a live cwRHIObject was Added and not yet
// Deleted; one that resolves to nullptr was never registered or has been dropped.
// So a test never needs to peek at cwScene's front-end queue directly — which is
// why cwScene exposes no test-only accessor into it.
struct CwRhiSceneTestAccess {
    static void synchroize(cwRhiScene& rhiScene, cwScene* scene) {
        rhiScene.synchroize(scene, nullptr);
    }

    static cwRHIObject* renderObjectForId(cwRhiScene& rhiScene, cwRenderObjectId id) {
        return rhiScene.m_frame.renderObjectForId(id);
    }

    // The frame's captured visibility snapshot — the render thread's view of
    // visibility after a synchroize(). This is what gatherScene's object gate
    // and the per-object readers consult; tests assert against it in place of
    // the retired cwRHIObject::isVisible mirror.
    static cwVisibilitySnapshot visibilitySnapshot(cwRhiScene& rhiScene) {
        return rhiScene.m_frame.visibilitySnapshot();
    }
};

#endif // CWRHISCENETESTACCESS_H
