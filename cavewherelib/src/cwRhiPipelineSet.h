#ifndef CWRHIPIPELINESET_H
#define CWRHIPIPELINESET_H

// Our includes
#include "cwRhiPipelineTypes.h"

// Qt includes
#include <QHash>

// Std includes
#include <utility>

class cwRhiFrameRenderer;

/**
 * A per-render-object cache of pipeline records, one entry per distinct
 * cwRhiPipelineKey the object has drawn with.
 *
 * A render object draws the resident scene from more than one render target
 * within a single frame: the live swap chain and, while an offscreen render is
 * in flight, an offscreen target with a different render-pass descriptor and
 * sample count. Each target yields a different pipeline key. Holding only the
 * most-recent record and releasing it on every key change made the two targets
 * evict and rebuild each other's pipeline every frame — expensive PSO churn.
 *
 * This keeps a reference to every key in play, so the engine's shared, refcounted
 * pipeline cache keeps all of them resident. Nothing is released until the
 * object is destroyed (releaseAll) or the target's render-pass descriptor is
 * torn down (purgeFor), at which point the held reference is dropped so the
 * record can actually be freed instead of orphaned.
 *
 * cwRHIObject owns one of these (and the matching current-pass record cursor),
 * so any render object caches pipelines per target by default; cwRhiTexturedItems
 * holds one per item instead. The shader-resource bindings are not cached here:
 * they are independent of the render-pass descriptor (they bind buffers and
 * textures, never the rpDesc), so a single SRB built once is layout-compatible
 * with every keyed pipeline.
 */
class cwRhiPipelineSet
{
public:
    cwRhiPipelineSet() = default;
    ~cwRhiPipelineSet() { releaseAll(); }

    cwRhiPipelineSet(const cwRhiPipelineSet&) = delete;
    cwRhiPipelineSet& operator=(const cwRhiPipelineSet&) = delete;

    // Return the cached record for @a key, or call @a miss to obtain one on the
    // first use of that key. @a miss must return a record with a reference
    // already taken (i.e. via cwRhiFrameRenderer::acquirePipeline); the set caches
    // it and holds that single reference for the key's lifetime, so drawing the
    // same key again never re-acquires. @a miss is a template parameter so the
    // hot cache-hit path constructs no std::function (this runs per frame per
    // object); it is only invoked on a miss.
    template <class Miss>
    cwRhiPipelineRecord* acquire(cwRhiFrameRenderer* frame,
                                 const cwRhiPipelineKey& key,
                                 Miss&& miss)
    {
        if (!frame) {
            return nullptr;
        }
        m_frame = frame;

        auto it = m_records.find(key);
        if (it != m_records.end()) {
            return it.value();
        }

        cwRhiPipelineRecord* record = std::forward<Miss>(miss)();
        if (record) {
            m_records.insert(key, record);
        }
        return record;
    }

    // Drop every cached record built against @a descriptor, releasing this
    // object's reference. Called just before the target that descriptor belongs to
    // is torn down, so the records are freed rather than left dangling on a
    // destroyed render-pass descriptor.
    void purgeFor(QRhiRenderPassDescriptor* descriptor);

    // Release every held reference. Called from the owning object's destructor.
    void releaseAll();

    bool isEmpty() const { return m_records.isEmpty(); }
    int size() const { return int(m_records.size()); }

private:
    cwRhiFrameRenderer* m_frame = nullptr;
    QHash<cwRhiPipelineKey, cwRhiPipelineRecord*> m_records;
};

#endif // CWRHIPIPELINESET_H
