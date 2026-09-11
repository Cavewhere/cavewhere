#include "cwRhiTexturedItems.h"

#include "cwFrustum.h"
#include "cwKtx2Codec.h"
#include "cwMipMath.h"
#include "cwRenderTexturedItems.h"
#include "cwRhiAttributeFormat.h"
#include "cwRhiItemRenderer.h"
#include "cwRhiLimits.h"
#include "cwTextureResidency.h"
#include "cwTextureStreamingStats.h"

#include <QByteArray>
#include <QDebug>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QVarLengthArray>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

namespace {
constexpr int kFallbackUniformSize = 16; // minimum to satisfy uniform alignment
// Meshes up to this many vertices index with uint16, halving the index buffer.
constexpr qsizetype kMaxUInt16VertexCount = 65535;
// Textured items upload RGBA8 with a full mip chain starting at level 0.
constexpr int kWholeImageTopLevel = 0;
// Items are tallied on this one pass so a frame that gathers every pass counts
// each item once. It is the first pass of cwRhiFrameRenderer's draw order.
constexpr cwRHIObject::RenderPass kCullingStatsPass = cwRHIObject::RenderPass::Background;
// The pinned base level is loaded ahead of every detail level: an item without a
// texture has nothing to draw, and everything else is a refinement.
constexpr quint64 kPinnedBasePriority = std::numeric_limits<quint64>::max();
// A demotion outranks every refinement — it is what brings the scene back under
// budget — but still yields to an item that has nothing to draw at all.
constexpr quint64 kDemotionPriority = kPinnedBasePriority - 1;
// An offscreen job is blocked until its levels land, so its refinements outrank
// every live one — but an item with nothing to draw, and the demotion that brings
// the scene back under budget, still go first.
constexpr quint64 kExportPriority = kDemotionPriority - 1;
}

cwRhiTexturedItems::cwRhiTexturedItems() = default;

cwRhiTexturedItems::~cwRhiTexturedItems()
{
    // Loads in flight hand their results to items, so they must be finished
    // before the items they name are freed.
    m_streamer.cancelAll();

    for (auto item : std::as_const(m_items)) {
        delete item;
    }

    delete m_sharedData.loadingTexture;
}

void cwRhiTexturedItems::initialize(const ResourceUpdateData& data)
{
    if (m_resourcesInitialized) {
        return;
    }

    QRhi* rhi = data.renderData.cb->rhi();

    // Tell the GUI side what this backend accepts, so it only ever hands us a
    // compressed texture we can upload.
    cw::ktx2::setSupportedCompressedFormat(cw::ktx2::preferredCompressedFormat(rhi));

    {
        QImage image(256, 256, QImage::Format_RGBA8888);
        image.fill(Qt::transparent);

        QPainter painter(&image);
        painter.setPen(Qt::white);
        painter.setFont(QFont(QStringLiteral("Arial"), 48));
        painter.drawText(image.rect(), Qt::AlignCenter, QStringLiteral("Loading"));
        painter.end();

        m_sharedData.loadingTexture = rhi->newTexture(QRhiTexture::RGBA8, image.size(), 1,
                                                      QRhiTexture::MipMapped | QRhiTexture::UsedWithGenerateMips);
        m_sharedData.loadingTexture->create();
        data.resourceUpdateBatch->uploadTexture(m_sharedData.loadingTexture, image);
        data.resourceUpdateBatch->generateMips(m_sharedData.loadingTexture);
    }

    // Build the vertex input layout from the canonical textured-item layout —
    // same bit-for-bit result as the previous hardcoded version, but routed
    // through buildRhiInputLayout so every cwGeometry consumer uses one path.
    const cwGeometry layoutProbe(cwRenderTexturedItems::geometryLayout());
    m_inputLayout = buildRhiInputLayout(layoutProbe);

    m_resourcesInitialized = true;
}

void cwRhiTexturedItems::synchronize(const SynchronizeData& data)
{
    Q_ASSERT(dynamic_cast<cwRenderTexturedItems*>(data.object) != nullptr);
    auto* renderItems = static_cast<cwRenderTexturedItems*>(data.object);

    // The queue is keyed by id and already coalesced, so each entry is the one
    // net change for that item this frame; iteration order doesn't matter since
    // every entry touches only its own id's render Item.
    const auto pendingChanges = renderItems->m_pendingChanges;
    using PendingItemState = cwRenderTexturedItems::PendingItemState;
    for (auto it = pendingChanges.constBegin(); it != pendingChanges.constEnd(); ++it) {
        const uint32_t id = it.key();
        const PendingItemState& state = it.value();
        const auto& payload = state.payload;

        switch (state.lifecycle) {
        case PendingItemState::Lifecycle::Add: {
            auto* item = new Item;
            item->owner = this;
            item->material = payload.material;
            item->geometry = payload.geometry;
            item->geometryNeedsUpdate = !item->geometry.indices().isEmpty();
            item->image = payload.texture;
            item->streamSource = payload.streamedTexture;
            item->textureNeedsUpdate = !item->image.isNull();
            item->uniformBlock = payload.uniformBlock;
            item->uniformNeedsUpdate = true;
            item->pipelineNeedsUpdate = true;
            item->modelMatrix = payload.modelMatrix;
            item->modelMatrixNeedsUpdate = true;
            item->updateBoundsFromGeometry();

            // Ids are monotonic, so an Add never targets a live id.
            Q_ASSERT(!m_items.contains(id));
            m_items.insert(id, item);
            break;
        }
        case PendingItemState::Lifecycle::Remove: {
            auto found = m_items.find(id);
            if (found != m_items.end()) {
                m_streamer.cancel(id);
                delete found.value();
                m_items.erase(found);
            }
            break;
        }
        case PendingItemState::Lifecycle::Update: {
            auto* item = m_items.value(id, nullptr);
            if (!item) {
                break;
            }
            if (state.geometryDirty) {
                item->geometry = payload.geometry;
                item->geometryNeedsUpdate = true;
                item->updateBoundsFromGeometry();
            }
            if (state.textureDirty) {
                item->image = payload.texture;
                // A streamed source uploads through streamResources instead, and
                // the item keeps drawing what it has until the first level lands.
                item->textureNeedsUpdate = payload.streamedTexture.isNull();
                if (!(item->streamSource == payload.streamedTexture)) {
                    item->streamSource = payload.streamedTexture;
                    m_streamer.cancel(id);
                    item->resetResidency();
                }
            }
            if (state.materialDirty && !(item->material == payload.material)) {
                item->material = payload.material;
                item->pipelineNeedsUpdate = true;
                item->uniformNeedsUpdate = true;
            }
            if (state.uniformBlockDirty) {
                item->uniformBlock = payload.uniformBlock;
                item->uniformNeedsUpdate = true;
            }
            if (state.modelMatrixDirty) {
                item->modelMatrix = payload.modelMatrix;
                item->modelMatrixNeedsUpdate = true;
                item->uniformNeedsUpdate = true;
                item->updateWorldBounds();
            }
            break;
        }
        }
    }

    renderItems->m_pendingChanges.clear();
}

void cwRhiTexturedItems::updateResources(const ResourceUpdateData& data)
{
    for (auto it = m_items.begin(); it != m_items.end(); ++it) {
        Item* item = it.value();
        if (!item) {
            continue;
        }

        if (!item->owner) {
            item->owner = this;
        }

        if (!item->resourcesInitialized) {
            item->initializeResources(data, m_sharedData);
        }

        if (item->geometryNeedsUpdate) {
            item->updateGeometryBuffers(data);
        }

        if (item->textureNeedsUpdate) {
            item->updateTextureResource(data, m_sharedData);
            item->createShaderResourceBindings(data, m_sharedData);
        }

        if (item->uniformNeedsUpdate || item->modelMatrixNeedsUpdate) {
            item->updateUniformBuffer(data);
            item->createShaderResourceBindings(data, m_sharedData);
        }

        if (item->pipelineNeedsUpdate) {
            // Live-only pre-build so createShaderResourceBindings has a
            // pipelineRecord; gather() is the authoritative build (the offscreen
            // path never comes through here). Key on this item's pass entry —
            // see ResourceUpdateData::perPassRenderData.
            Q_ASSERT(data.perPassRenderData);
            const auto pass = toRenderPass(item->material.renderPass);
            const RenderData& routed = (*data.perPassRenderData)[static_cast<size_t>(pass)];
            item->ensurePipeline(routed, m_sharedData, m_inputLayout);
            item->createShaderResourceBindings(data, m_sharedData);
        }
    }
}

bool cwRhiTexturedItems::gather(const GatherContext& context, QVector<PipelineBatch>& batches)
{
    const auto desiredPass = context.renderPass;
    bool appended = false;

    tallyCullingStats(context);

    // const iteration: the mapped values are Item* (the pointee is non-const, so
    // item->ensurePipeline() below is still callable) — avoids a QHash detach.
    for (auto it = m_items.constBegin(); it != m_items.constEnd(); ++it) {
        Item* item = it.value();
        if (!item) {
            continue;
        }

        if (toRenderPass(item->material.renderPass) != desiredPass) {
            continue;
        }

        // Per-item visibility comes from the frame's snapshot (the owner
        // publishes setSubVisible to the store; no RHI-side copy exists).
        if (context.visibility
            && !context.visibility->subVisible(renderObjectId(), it.key())) {
            continue;
        }

        // Scraps and LiDAR meshes share this one render object, so whole-object
        // culling can't help them — each item tests its own box here, ahead of
        // ensurePipeline so a culled item skips the pipeline work too.
        if (context.frustum
            && item->boundsValid
            && !context.frustum->intersects(item->worldBounds)) {
            continue;
        }

        // The item is visible and in view, so this is the one moment per frame
        // its mip level can be chosen from a camera that actually sees it.
        selectStreamLevel(it.key(), item, context);

        // Rebuild the pipeline if this pass's target changed since last frame
        // (cloud appeared/disappeared → Opaque routes through the 1x offscreen
        // or back to the swap chain). Self-guards on the key, so a no-op when
        // the routing is unchanged. The SRB layout is rpDesc-independent, so it
        // stays valid across the rebuild — no need to recreate it here.
        item->ensurePipeline(*context.renderData, m_sharedData, m_inputLayout);

        if (item->numberOfIndices <= 0 ||
            !item->pipelineRecord ||
            !item->pipelineRecord->pipeline ||
            !item->srb ||
            !item->vertexBuffer ||
            !item->indexBuffer) {
            continue;
        }

        auto* pipeline = item->pipelineRecord->pipeline;

        cwRHIObject::PipelineState state;
        state.pipeline = pipeline;
        state.sortKey = cwRHIObject::makeSortKey(context.objectOrder, pipeline);

        auto& batch = acquirePipelineBatch(batches, state);

        cwRHIObject::Drawable drawable;
        drawable.type = cwRHIObject::Drawable::Type::Indexed;
        drawable.bindings = item->srb;
        drawable.indexBuffer = item->indexBuffer;
        drawable.indexFormat = item->indexFormat;
        drawable.indexCount = static_cast<quint32>(item->numberOfIndices);
        drawable.vertexBindings.append(QRhiCommandBuffer::VertexInput(item->vertexBuffer, 0));
        // The material's global UBO binding is dynamic-offset; cwRhiScene supplies
        // the per-pass camera slot offset.
        drawable.globalCameraBinding = item->material.globalUniformBinding;

        batch.drawables.append(drawable);
        appended = true;
    }

    return appended;
}

QRhiTexture::Format cwRhiTexturedItems::streamTargetFormat()
{
    const QRhiTexture::Format compressed = cw::ktx2::supportedCompressedFormat();
    return compressed == QRhiTexture::UnknownFormat ? QRhiTexture::RGBA8 : compressed;
}

void cwRhiTexturedItems::selectStreamLevel(uint32_t id, Item* item, const GatherContext& context,
                                           StreamPriority priority)
{
    if (item->streamSource.isNull()) {
        return;
    }

    item->lastVisibleFrame = m_frame->frameCounter();

    if (item->residentTopLevel == kNoResidentLevel
        && item->requestedTopLevel == kNoResidentLevel) {
        // Nothing to draw yet: the pinned base outranks every refinement.
        const int base = cw::residency::pinnedBaseLevel(item->streamSource.size);
        item->requestedTopLevel = base;
        m_streamer.request(id, item->streamSource, streamTargetFormat(), base,
                           kPinnedBasePriority);
        return;
    }

    const RenderData& renderData = *context.renderData;

    cw::residency::SelectionInput input;
    input.textureSize = item->streamSource.size;
    input.uvPerMeter = item->uvPerMeter;
    input.worldBounds = item->worldBounds;
    input.viewProjection = renderData.viewProjectionMatrix;
    input.absP11 = std::abs(double(renderData.projectionMatrix(1, 1)));
    input.viewportHeightPx = renderData.viewportSize.height();
    input.screenSpaceErrorPx = renderData.budgets.screenSpaceErrorPx;

    const int desired = cw::residency::desiredTopLevel(input);
    item->desiredTopLevel = desired;

    // The level the item is heading for: what a load is running for, or what it
    // holds when nothing is open. Only refinements are asked for here; giving
    // detail back is the budget's job, in enforceGpuBudget.
    const int target = item->requestedTopLevel == kNoResidentLevel
                           ? item->residentTopLevel
                           : item->requestedTopLevel;

    if (desired < target) {
        if (desired == item->residentTopLevel) {
            // The open request is a demotion the camera has changed its mind
            // about, and what is resident is already the wanted level: drop the
            // request rather than reloading bytes the item holds.
            item->requestedTopLevel = kNoResidentLevel;
            item->demotionInFlight = false;
            m_streamer.cancel(id);
            return;
        }

        // A demotion in flight is superseded by this: the streamer bumps the
        // item's generation, so the coarse chain is dropped when it lands.
        item->requestedTopLevel = desired;
        item->demotionInFlight = false;
        m_streamer.request(id, item->streamSource, streamTargetFormat(), desired,
                           priority == StreamPriority::Export
                               ? kExportPriority
                               : quint64(target - desired));
    }
}

bool cwRhiTexturedItems::residencyReady(const RenderData& jobRenderData)
{
    const cwFrustum frustum =
        cwFrustum::fromViewProjection(jobRenderData.viewProjectionMatrix);

    // The job draws the scene the live frame's snapshot describes, so the same
    // per-item gate applies here (an offscreen job's own suppressions are
    // whole-object, and the offscreen renderer skips those objects before asking).
    const cwVisibilitySnapshot& visibility = m_frame->visibilitySnapshot();

    GatherContext context;
    context.renderData = &jobRenderData;

    bool ready = true;
    for (auto it = m_items.constBegin(); it != m_items.constEnd(); ++it) {
        Item* item = it.value();
        if (!item || item->streamSource.isNull()) {
            continue;
        }

        if (!visibility.subVisible(renderObjectId(), it.key())) {
            continue;
        }

        if (frustum.isValid()
            && item->boundsValid
            && !frustum.intersects(item->worldBounds)) {
            continue;
        }

        // Every visible item is asked for, even once one has reported the job
        // unready, so the whole scene's loads are in flight by the next frame.
        selectStreamLevel(it.key(), item, context, StreamPriority::Export);

        if (item->residentTopLevel == kNoResidentLevel
            || item->residentTopLevel > item->desiredTopLevel) {
            ready = false;
        }
    }

    return ready;
}

void cwRhiTexturedItems::enforceGpuBudget(const cwRenderBudgets& budgets)
{
    const qint64 overshoot =
        cwRenderMemoryLedger::instance()->totalBytes(cwRenderMemoryLedger::Residency::Gpu)
        - budgets.gpuBudgetBytes;
    if (overshoot <= 0) {
        m_atResidencyFloor = false;
        return;
    }

    const QRhiTexture::Format format = streamTargetFormat();
    const quint64 frame = m_frame->frameCounter();

    // Parallel to stats, so a plan's itemIndex names the item it was built from.
    QVector<QPair<uint32_t, Item*>> streamedItems;
    QVector<cw::residency::ResidencyStats> stats;
    streamedItems.reserve(m_items.size());
    stats.reserve(m_items.size());

    // What the demotions already running will give back. Crediting it keeps the
    // frames they take to land from unraveling the whole fleet.
    qint64 promisedBytes = 0;
    for (auto it = m_items.constBegin(); it != m_items.constEnd(); ++it) {
        Item* item = it.value();
        if (!item || item->streamSource.isNull()) {
            continue;
        }

        cw::residency::ResidencyStats itemStats;
        itemStats.textureSize = item->streamSource.size;
        itemStats.format = format;
        itemStats.residentTopLevel = item->residentTopLevel;
        itemStats.desiredTopLevel = item->desiredTopLevel;
        itemStats.lastVisibleFrame = item->lastVisibleFrame;
        // gather() runs after streamResources, so the newest frame an item was
        // seen in is the one the counter still sits on. An item that has never
        // been gathered holds nothing for the planner to take anyway.
        itemStats.visibleThisFrame = item->lastVisibleFrame == frame;
        itemStats.demotionInFlight = item->demotionInFlight;

        if (itemStats.demotionInFlight && itemStats.residentTopLevel >= 0) {
            const int itemBase = cw::residency::pinnedBaseLevel(itemStats.textureSize);
            promisedBytes +=
                cw::mip::chainBytes(format, itemStats.textureSize, itemStats.residentTopLevel)
                - cw::mip::chainBytes(format, itemStats.textureSize, itemBase);
        }

        streamedItems.append({it.key(), item});
        stats.append(itemStats);
    }

    const QVector<cw::residency::Demotion> plan =
        cw::residency::planEvictions(stats, overshoot - promisedBytes);
    if (plan.isEmpty()) {
        // Nothing planned and nothing in flight: what is resident is what the
        // views need, and the budget is simply set below that floor.
        if (promisedBytes == 0 && !m_atResidencyFloor) {
            m_atResidencyFloor = true;
            qWarning() << "Render memory is" << overshoot
                       << "bytes over the GPU budget of" << budgets.gpuBudgetBytes
                       << "and no streamed texture can give any back";
        }
        return;
    }
    m_atResidencyFloor = false;

    for (const cw::residency::Demotion& demotion : plan) {
        const uint32_t id = streamedItems.at(demotion.itemIndex).first;
        Item* item = streamedItems.at(demotion.itemIndex).second;

        // The coarse chain lands through the same drain and swap a promotion
        // uses, so the fine texture keeps drawing until it is complete and the
        // ledger drops at the swap.
        item->requestedTopLevel = demotion.newTopLevel;
        item->demotionInFlight = true;
        m_streamer.request(id, item->streamSource, format, demotion.newTopLevel,
                           kDemotionPriority);
    }
}

void cwRhiTexturedItems::releaseStreamedTextures()
{
    for (auto it = m_items.constBegin(); it != m_items.constEnd(); ++it) {
        Item* item = it.value();
        if (!item || item->streamSource.isNull()) {
            continue;
        }

        // Cancel first: the streamer bumps the item's generation, so a load
        // already in flight finishes into the void instead of landing on an item
        // that has given everything back.
        m_streamer.cancel(it.key());

        item->resetResidency();
        // Forgotten too, so the item asks again from whatever camera brings the
        // view back rather than from the one that left.
        item->desiredTopLevel = kNoResidentLevel;

        delete item->texture;
        item->texture = nullptr;
        item->textureBytes.setBytes(0);

        // The bindings sampled the texture just freed. streamResources rebuilds
        // them against the loading texture before anything gathers the item again.
        delete item->srb;
        item->srb = nullptr;
    }

    // Nothing is resident, so the next over-budget frame gets a fresh warning
    // rather than being silenced by the state this one left.
    m_atResidencyFloor = false;
}

bool cwRhiTexturedItems::streamResources(ResourceUpdateData& data, qint64& remainingUploadBytes)
{
    m_streamer.setMaxPendingCpuBytes(data.renderData.budgets.cpuBudgetBytes);

    // An item whose streamed texture was released — the view drawing it was
    // hidden — holds no bindings at all. Rebuild them here, against the loading
    // texture the first load also draws behind, so the item is drawable from
    // this frame on rather than waiting on a synchronize that may never come.
    for (auto item : std::as_const(m_items)) {
        if (item->resourcesInitialized && !item->srb) {
            item->createShaderResourceBindings(data, m_sharedData);
        }
    }

    // Results the streamer hands over are transient: what an item still wants is
    // stashed on it, and everything else dies with the local vector.
    const QVector<cwTextureStreamer::Result> ready = m_streamer.takeReady();
    for (const cwTextureStreamer::Result& result : ready) {
        Item* item = m_items.value(result.itemId, nullptr);
        if (!item
            || item->streamSource.isNull()
            || result.topLevel != item->requestedTopLevel
            || result.generation < item->acceptedGeneration) {
            continue;
        }

        if (result.texture.isNull()) {
            // A load that failed reopens the request slot: selection asks again
            // next frame rather than leaving the item stuck at this level.
            qWarning() << "Streaming level" << result.topLevel << "for item" << result.itemId
                       << "failed:" << result.error;
            item->requestedTopLevel = kNoResidentLevel;
            item->demotionInFlight = false;
            continue;
        }

        item->clearPendingUpload();
        item->acceptedGeneration = result.generation;
        item->pendingUpload.readyLevels = result.texture;
        item->pendingUpload.readyTopLevel = result.topLevel;
        item->pendingUpload.nextLevelToUpload = 0;
    }

    bool anythingUploadedThisFrame = false;
    bool levelsRemain = false;
    for (auto item : std::as_const(m_items)) {
        if (item->pendingUpload.readyTopLevel == kNoResidentLevel) {
            continue;
        }

        levelsRemain = item->uploadPendingLevels(data, m_sharedData, remainingUploadBytes,
                                                 anythingUploadedThisFrame)
                       || levelsRemain;
    }

    // Last, so this frame's swaps are already off the ledger when the total is
    // read and the plan is built from what is actually resident.
    enforceGpuBudget(data.renderData.budgets);

    publishStreamingStats();

    // hasWork() covers the loads still queued or in flight — the frame renderer
    // has no other window onto this object's streamer.
    return levelsRemain || m_streamer.hasWork();
}

void cwRhiTexturedItems::publishStreamingStats() const
{
    const cwTextureStreamer::Pending pending = m_streamer.pending();

    cwTextureStreamingStats::Counts counts;
    counts.loadsInFlight = pending.loads;
    counts.readyCpuBytes = pending.cpuBytes;

    for (const Item* item : std::as_const(m_items)) {
        if (!item || item->streamSource.isNull()) {
            continue;
        }

        ++counts.streamedItems;

        if (item->demotionInFlight) {
            ++counts.demotionsInFlight;
        }

        // Coarser than the camera asked for, counting an item that holds nothing
        // yet — its pinned base is still on the way.
        if (item->residentTopLevel == kNoResidentLevel
            || item->residentTopLevel > item->desiredTopLevel) {
            ++counts.itemsBelowDesired;
        }
    }

    cwTextureStreamingStats::instance()->publish(counts);
}

void cwRhiTexturedItems::tallyCullingStats(const GatherContext& context) const
{
    if (!context.cullingStats || context.renderPass != kCullingStatsPass) {
        return;
    }

    for (auto it = m_items.constBegin(); it != m_items.constEnd(); ++it) {
        const Item* item = it.value();
        if (!item) {
            continue;
        }

        if (context.visibility
            && !context.visibility->subVisible(renderObjectId(), it.key())) {
            continue;
        }

        ++context.cullingStats->itemsTotal;

        if (context.frustum
            && item->boundsValid
            && !context.frustum->intersects(item->worldBounds)) {
            ++context.cullingStats->itemsCulled;
        }
    }
}

std::optional<QBox3D> cwRhiTexturedItems::worldBounds() const
{
    QBox3D united;

    for (const Item* item : m_items) {
        if (item && item->boundsValid) {
            united.unite(item->worldBounds);
        }
    }

    if (united.isNull()) {
        return std::nullopt;
    }

    return united;
}

cwRhiTexturedItems::Item::Item() = default;

cwRhiTexturedItems::Item::~Item()
{
    // `pipelines` releases its held pipeline references on destruction.
    delete vertexBuffer;
    delete indexBuffer;
    delete uniformBuffer;
    delete texture;
    delete pendingUpload.stagingTexture;
    delete srb;
}

void cwRhiTexturedItems::Item::clearPendingUpload()
{
    delete pendingUpload.stagingTexture;
    pendingUpload = {};
}

void cwRhiTexturedItems::Item::failPendingUpload()
{
    clearPendingUpload();
    requestedTopLevel = kNoResidentLevel;
    demotionInFlight = false;
}

void cwRhiTexturedItems::Item::resetResidency()
{
    clearPendingUpload();
    residentTopLevel = kNoResidentLevel;
    requestedTopLevel = kNoResidentLevel;
    demotionInFlight = false;
}

bool cwRhiTexturedItems::Item::uploadPendingLevels(const ResourceUpdateData& data,
                                                   const SharedItemData& sharedData,
                                                   qint64& remainingUploadBytes,
                                                   bool& anythingUploadedThisFrame)
{
    const QVector<QByteArray>& levels = pendingUpload.readyLevels.mipLevels;
    const QSize topLevelSize = pendingUpload.readyLevels.size;
    const QRhiTexture::Format format = pendingUpload.readyLevels.format;

    if (!pendingUpload.stagingTexture) {
        QRhi* rhi = data.renderData.cb->rhi();
        // Built alongside the resident texture rather than over it: the item
        // keeps drawing what it has until the whole chain has landed.
        pendingUpload.stagingTexture = rhi->newTexture(format, topLevelSize, 1,
                                                       QRhiTexture::MipMapped);
        if (!pendingUpload.stagingTexture->create()) {
            qWarning() << "Creating a streamed texture of format" << int(format)
                       << "at size" << topLevelSize << "failed, keeping the resident texture";
            failPendingUpload();
            return false;
        }
    }

    // The chain comes off disk, so a missing level or a short one is corruption
    // rather than a programming error — upload it and the backend reads past
    // the end of what it was given.
    const int expectedLevelCount = data.renderData.cb->rhi()->mipLevelsForSize(topLevelSize);
    if (levels.size() != expectedLevelCount) {
        qWarning() << "Streamed chain of format" << int(format)
                   << "at size" << topLevelSize << "holds" << levels.size()
                   << "levels, expected" << expectedLevelCount
                   << "levels, keeping the resident texture";
        failPendingUpload();
        return false;
    }

    for (int level = pendingUpload.nextLevelToUpload; level < levels.size(); level++) {
        const QSize levelSize = cw::mip::mipLevelSize(topLevelSize, level);
        if (levels.at(level).size() != cw::mip::mipLevelBytes(format, levelSize)) {
            qWarning() << "Streamed level" << level << "of format" << int(format)
                       << "at size" << levelSize << "holds" << levels.at(level).size()
                       << "bytes, keeping the resident texture";
            failPendingUpload();
            return false;
        }
    }

    if (format == QRhiTexture::RGBA8) {
        // create() allocated every level of an uncompressed texture, so the
        // chain can land one level per frame under the budget.
        while (pendingUpload.nextLevelToUpload < levels.size()) {
            const int level = pendingUpload.nextLevelToUpload;
            const QByteArray& levelBytes = levels.at(level);
            const QSize levelSize = cw::mip::mipLevelSize(topLevelSize, level);

            if (!cw::residency::takeFromBudget(remainingUploadBytes, levelBytes.size(),
                                               anythingUploadedThisFrame)) {
                return true;
            }
            anythingUploadedThisFrame = true;

            // The level's bytes are wrapped in an image of its size. copy()
            // owns them: the level payload is dropped before the batch is
            // submitted.
            const QImage levelImage(reinterpret_cast<const uchar*>(levelBytes.constData()),
                                    levelSize.width(), levelSize.height(),
                                    QImage::Format_RGBA8888);
            data.resourceUpdateBatch->uploadTexture(
                pendingUpload.stagingTexture,
                QRhiTextureUploadDescription(
                    QRhiTextureUploadEntry(0, level,
                                           QRhiTextureSubresourceUploadDescription(levelImage.copy()))));

            pendingUpload.nextLevelToUpload++;
        }
    } else {
        // QRhiGles2 allocates compressed storage only from the first
        // uploadTexture() call that names a level, and treats every later call
        // as a sub-image update of storage it assumes exists. Levels sent in
        // separate calls therefore land in unallocated storage, the chain is
        // mip-incomplete, and the sampler returns opaque black. The whole chain
        // goes up in one call, charged to the budget as a whole; it costs about
        // 4/3 of level 0.
        qint64 chainBytes = 0;
        for (const QByteArray& levelBytes : levels) {
            chainBytes += levelBytes.size();
        }

        if (!cw::residency::takeFromBudget(remainingUploadBytes, chainBytes,
                                           anythingUploadedThisFrame)) {
            return true;
        }
        anythingUploadedThisFrame = true;

        QVector<QRhiTextureUploadEntry> entries;
        entries.reserve(levels.size());
        for (int level = 0; level < levels.size(); level++) {
            entries.append(QRhiTextureUploadEntry(
                0, level, QRhiTextureSubresourceUploadDescription(levels.at(level))));
        }

        QRhiTextureUploadDescription description;
        description.setEntries(entries.cbegin(), entries.cend());
        data.resourceUpdateBatch->uploadTexture(pendingUpload.stagingTexture, description);
        pendingUpload.nextLevelToUpload = levels.size();
    }

    // The last level landed: swap the finished chain in atomically (render thread
    // only), so the item never goes a frame without a texture.
    delete texture;
    texture = pendingUpload.stagingTexture;
    residentTopLevel = pendingUpload.readyTopLevel;
    demotionInFlight = false;
    textureBytes.setBytes(cw::mip::chainBytes(format, streamSource.size, residentTopLevel));

    pendingUpload.stagingTexture = nullptr;   // ownership moved to `texture`
    clearPendingUpload();

    createShaderResourceBindings(data, sharedData);
    return false;
}

void cwRhiTexturedItems::Item::initializeResources(const ResourceUpdateData& data, const SharedItemData&)
{
    QRhi* rhi = data.renderData.cb->rhi();

    // Geometry changes only on re-triangulation, so Immutable keeps it device-local.
    vertexBuffer = rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, 0);
    vertexBuffer->create();

    indexBuffer = rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::IndexBuffer, 0);
    indexBuffer->create();

    resourcesInitialized = true;
}

/**
 * Recomputes localBounds from the geometry payload and refreshes worldBounds.
 *
 * Only synchronize can call this — updateGeometryBuffers drops the geometry
 * once it has been uploaded. Geometry without positions leaves the bounds
 * invalid, so the item keeps drawing.
 */
void cwRhiTexturedItems::Item::updateBoundsFromGeometry()
{
    boundsValid = false;
    localBounds = QBox3D();
    worldBounds = QBox3D();
    uvPerMeter = 0.0;

    const auto* positionAttribute = geometry.attribute(cwGeometry::Semantic::Position);
    const qsizetype vertexCount = geometry.vertexCount();
    if (!positionAttribute || vertexCount == 0) {
        return;
    }

    for (qsizetype index = 0; index < vertexCount; index++) {
        localBounds.unite(geometry.value<QVector3D>(positionAttribute, index));
    }

    boundsValid = true;
    updateWorldBounds();

    // Selection needs the geometry's texel density, and this is the one moment
    // the geometry is on the render thread — updateGeometryBuffers drops it once
    // uploaded. The sampling is bounded to a fixed triangle count, so it costs
    // the same whatever the mesh size.
    uvPerMeter = cw::residency::uvPerMeter(geometry, modelMatrix);
}

/**
 * Refreshes worldBounds from localBounds through the current modelMatrix. LiDAR
 * geometry arrives already morphed to world space with an identity matrix; the
 * transform is identity-safe, so scraps with real matrices share this path.
 */
void cwRhiTexturedItems::Item::updateWorldBounds()
{
    if (!boundsValid) {
        return;
    }

    worldBounds = transformedBounds(localBounds, modelMatrix);
}

void cwRhiTexturedItems::Item::updateGeometryBuffers(const ResourceUpdateData& data)
{
    if (!vertexBuffer || !indexBuffer) {
        return;
    }

    QRhiResourceUpdateBatch* batch = data.resourceUpdateBatch;
    // Textured items use a single Interleaved buffer (validated by
    // cwRenderTexturedItems::geometryMatchesLayout) — bind 0 only.
    const auto bufferViews = geometry.vertexBuffers();
    Q_ASSERT(bufferViews.size() <= 1);
    const QByteArray vertexData = bufferViews.isEmpty() ? QByteArray() : *bufferViews.at(0).data;
    const auto indices = geometry.indices();

    const bool narrowIndices = geometry.vertexCount() <= kMaxUInt16VertexCount;
    indexFormat = narrowIndices ? QRhiCommandBuffer::IndexUInt16 : QRhiCommandBuffer::IndexUInt32;

    QByteArray indexData;
    if (narrowIndices) {
        indexData.resize(indices.size() * qsizetype(sizeof(quint16)));
        auto* narrowed = reinterpret_cast<quint16*>(indexData.data());
        for (qsizetype i = 0; i < indices.size(); i++) {
            narrowed[i] = static_cast<quint16>(indices.at(i));
        }
    } else {
        indexData = QByteArray::fromRawData(reinterpret_cast<const char*>(indices.constData()),
                                            indices.size() * qsizetype(sizeof(uint32_t)));
    }
    const quint32 indexBytes = quint32(indexData.size());

    // A QRhiBuffer size is a quint32, so a mesh past 4 GiB would wrap silently.
    const int vertexStride = bufferViews.isEmpty() ? 0 : bufferViews.at(0).stride;
    const quint32 vertexBytes = quint32(cw::clampedVertexBytes(vertexData.size(), vertexStride));
    const bool vertexBytesClamped = vertexBytes < vertexData.size();
    if (vertexBytesClamped) {
        qWarning() << "Textured item vertex buffer of" << vertexData.size()
                   << "bytes exceeds the" << cw::kMaxRhiBufferBytes
                   << "byte QRhiBuffer limit; the item is too large to draw";
    }

    if (vertexBuffer->size() != vertexBytes) {
        vertexBuffer->setSize(vertexBytes);
        vertexBuffer->create();
    }
    if (vertexBytesClamped) {
        batch->uploadStaticBuffer(vertexBuffer, 0, vertexBytes, vertexData.constData());
    } else if (!vertexData.isEmpty()) {
        // By-value QByteArray: a refcount bump instead of a deep copy.
        batch->uploadStaticBuffer(vertexBuffer, vertexData);
    }

    if (indexBuffer->size() != indexBytes) {
        indexBuffer->setSize(indexBytes);
        indexBuffer->create();
    }
    if (indexBytes > 0) {
        if (narrowIndices) {
            batch->uploadStaticBuffer(indexBuffer, indexData);
        } else {
            // The wide indexData is a QByteArray::fromRawData view over
            // geometry.indices(), which `geometry = {}` below frees before the
            // batch is consumed. The by-value overload would refcount the
            // wrapper, not the borrowed bytes, so it must deep-copy here.
            batch->uploadStaticBuffer(indexBuffer, 0, indexBytes, indexData.constData());
        }
    }

    geometryBytes.setBytes(qint64(vertexBuffer->size()) + qint64(indexBuffer->size()));

    // A half-clamped indexed mesh would reference dropped vertices and render
    // garbage, so draw nothing instead.
    numberOfIndices = vertexBytesClamped ? 0 : indices.size();
    geometry = {};
    geometryNeedsUpdate = false;
}

void cwRhiTexturedItems::Item::updateTextureResource(const ResourceUpdateData& data, const SharedItemData& sharedData)
{
    auto* rhi = data.renderData.cb->rhi();
    const QSize size = image.size();

    if (!texture || texture->format() != QRhiTexture::RGBA8 || texture->pixelSize() != size) {
        delete texture;
        texture = nullptr;

        if (!size.isEmpty()) {
            texture = rhi->newTexture(QRhiTexture::RGBA8, size, 1,
                                      QRhiTexture::MipMapped | QRhiTexture::UsedWithGenerateMips);
            texture->create();
        }

        textureBytes.setBytes(cw::mip::chainBytes(QRhiTexture::RGBA8, size, kWholeImageTopLevel));
    }

    if (texture && !image.isNull()) {
        data.resourceUpdateBatch->uploadTexture(texture, image);
        data.resourceUpdateBatch->generateMips(texture);
    }

    image = {};
    textureNeedsUpdate = false;
    Q_UNUSED(sharedData);
}

void cwRhiTexturedItems::Item::updateUniformBuffer(const ResourceUpdateData& data)
{
    if (!material.wantsPerDrawUniform()) {
        uniformNeedsUpdate = false;
        modelMatrixNeedsUpdate = false;
        return;
    }

    QRhi* rhi = data.renderData.cb->rhi();
    QByteArray payload = buildPerDrawUniformPayload();
    if (payload.isEmpty()) {
        payload.resize(sizeof(float) * 16);
        std::memcpy(payload.data(), modelMatrix.constData(), sizeof(float) * 16);
    }

    const qsizetype alignedSize = rhi->ubufAligned(payload.size());
    if (payload.size() < alignedSize) {
        const qsizetype originalSize = payload.size();
        payload.resize(alignedSize);
        std::memset(payload.data() + originalSize, 0, alignedSize - originalSize);
    }

    if (!uniformBuffer || uniformBuffer->size() != alignedSize) {
        delete uniformBuffer;
        uniformBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, alignedSize);
        uniformBuffer->create();
    }

    data.resourceUpdateBatch->updateDynamicBuffer(uniformBuffer, 0, alignedSize, payload.constData());
    uniformNeedsUpdate = false;
    modelMatrixNeedsUpdate = false;
}

void cwRhiTexturedItems::Item::ensurePipeline(const RenderData& renderData,
                                              const SharedItemData& sharedData,
                                              const QRhiVertexInputLayout& layout)
{
    if (!owner) {
        pipelineNeedsUpdate = false;
        return;
    }

    auto* renderer = renderData.renderer;
    // Precondition: renderData is this item's pass's routed target (gather()
    // filters items to its pass; updateResources() selects the item's entry from
    // perPassRenderData). The key self-adjusts when the routing flips.
    Q_ASSERT(renderData.renderPassDescriptor);

    const cwRhiPipelineKey key = owner->makePipelineKey(renderData.renderPassDescriptor,
                                                        renderData.sampleCount, material);

    if (!pipelineNeedsUpdate && pipelineRecord && pipelineRecord->key == key) {
        return;
    }

    // Keep a reference to every target's pipeline (in `pipelines`), so toggling
    // between the live swap chain and an offscreen target reuses the cached
    // record instead of releasing and rebuilding it each frame.
    pipelineRecord = pipelines.acquire(owner->m_frame, key, [&]() {
        return owner->acquirePipeline(key, material, renderData.cb->rhi(), layout, sharedData);
    });
    pipelineNeedsUpdate = (pipelineRecord == nullptr);

    if (!material.wantsPerDrawUniform()) {
        delete uniformBuffer;
        uniformBuffer = nullptr;
        uniformNeedsUpdate = false;
        modelMatrixNeedsUpdate = false;
    } else {
        uniformNeedsUpdate = true;
        modelMatrixNeedsUpdate = true;
    }
}

void cwRhiTexturedItems::Item::purgePipelinesFor(QRhiRenderPassDescriptor* descriptor)
{
    pipelines.purgeFor(descriptor);
    pipelineRecord = nullptr;
    pipelineNeedsUpdate = true; // re-acquired in the next ensurePipeline
}

void cwRhiTexturedItems::Item::createShaderResourceBindings(const ResourceUpdateData& data,
                                                             const SharedItemData& sharedData)
{
    // The bindings are rebuilt even while pipelineRecord is null (purged by
    // evictPipelinesFor). Skipping would leave the old srb bound to a texture
    // that a streamed mip swap just deleted.
    QRhi* rhi = data.renderData.cb->rhi();
    auto* renderer = data.renderData.renderer;

    if (srb) {
        delete srb;
        srb = nullptr;
    }

    auto* sampledTexture = texture ? texture : sharedData.loadingTexture;
    auto* sampler = owner ? owner->sharedSampler(rhi) : nullptr;
    if (!sampler) {
        return;
    }

    QVector<QRhiShaderResourceBinding> bindings;
    bindings.append(QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(material.globalUniformBinding,
                                                             toRhiStages(material.globalUniformStages),
                                                             renderer->globalUniformBuffer(),
                                                             renderer->globalUniformBufferStride()));

    if (material.wantsPerDrawUniform()) {
        if (!uniformBuffer) {
            QByteArray zero(kFallbackUniformSize, '\0');
            const qsizetype aligned = rhi->ubufAligned(zero.size());
            uniformBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, aligned);
            uniformBuffer->create();
            data.resourceUpdateBatch->updateDynamicBuffer(uniformBuffer, 0, zero.size(), zero.constData());
        }
        bindings.append(QRhiShaderResourceBinding::uniformBuffer(material.perDrawUniformBinding,
                                                                 toRhiStages(material.perDrawUniformStages),
                                                                 uniformBuffer));
    }

    bindings.append(QRhiShaderResourceBinding::sampledTexture(material.textureBinding,
                                                              toRhiStages(material.textureStages),
                                                              sampledTexture,
                                                              sampler));

    srb = rhi->newShaderResourceBindings();
    srb->setBindings(bindings.cbegin(), bindings.cend());
    srb->create();
    Q_ASSERT(!pipelineRecord || !pipelineRecord->layout
             || pipelineRecord->layout->isLayoutCompatible(srb));
}

QByteArray cwRhiTexturedItems::Item::buildPerDrawUniformPayload() const
{
    QByteArray payload;
    const int matrixBytes = sizeof(float) * 16;
    payload.resize(matrixBytes + uniformBlock.size());

    std::memcpy(payload.data(), modelMatrix.constData(), matrixBytes);
    if (!uniformBlock.isEmpty()) {
        std::memcpy(payload.data() + matrixBytes, uniformBlock.constData(), uniformBlock.size());
    }

    return payload;
}

quint8 cwRhiTexturedItems::toStageMask(cwRenderMaterialState::ShaderStages stages)
{
    return cwShaderStageMask(stages);
}

cwRhiPipelineKey cwRhiTexturedItems::makePipelineKey(QRhiRenderPassDescriptor* renderPass,
                                                     int sampleCount,
                                                     const cwRenderMaterialState& material) const
{
    cwRhiPipelineKey key;
    key.renderPass = renderPass;
    key.sampleCount = sampleCount;
    key.vertexShader = material.vertexShader;
    key.fragmentShader = material.fragmentShader;
    key.cullMode = static_cast<quint8>(material.cullMode);
    key.frontFace = static_cast<quint8>(material.frontFace);
    key.blendMode = static_cast<quint8>(material.blendMode);
    key.depthTest = material.depthTest ? 1 : 0;
    key.depthWrite = material.depthWrite ? 1 : 0;
    key.globalBinding = static_cast<quint8>(material.globalUniformBinding);
    key.perDrawBinding = material.wantsPerDrawUniform() ? static_cast<quint8>(material.perDrawUniformBinding) : 0xFF;
    key.textureBinding = static_cast<quint8>(material.textureBinding);
    key.globalStages = toStageMask(material.globalUniformStages);
    key.perDrawStages = toStageMask(material.perDrawUniformStages);
    key.textureStages = toStageMask(material.textureStages);
    key.hasPerDraw = material.wantsPerDrawUniform() ? 1 : 0;
    key.topology = static_cast<quint8>(QRhiGraphicsPipeline::Triangles);
    return key;
}

cwRhiPipelineRecord *cwRhiTexturedItems::acquirePipeline(const cwRhiPipelineKey& key,
                                                                const cwRenderMaterialState& material,
                                                                QRhi* rhi,
                                                                const QRhiVertexInputLayout& layout,
                                                                const SharedItemData& sharedData)
{
    Q_UNUSED(sharedData);

    auto* sampler = sharedSampler(rhi);
    if (!sampler) {
        return nullptr;
    }

    const quint32 globalStride = m_frame->globalUniformBufferStride();

    auto createFn = [material, layout, sampler, key, globalStride](QRhi* localRhi) -> cwRhiPipelineRecord* {
        if (!localRhi) {
            return nullptr;
        }

        auto* record = new cwRhiPipelineRecord;
        record->pipeline = localRhi->newGraphicsPipeline();

        QShader vertex = loadShader(material.vertexShader);
        QShader fragment = loadShader(material.fragmentShader);

        record->pipeline->setShaderStages({
            { QRhiShaderStage::Vertex, vertex },
            { QRhiShaderStage::Fragment, fragment }
        });
        record->pipeline->setCullMode(toRhiCullMode(material.cullMode));
        record->pipeline->setFrontFace(toRhiFrontFace(material.frontFace));
        record->pipeline->setTopology(static_cast<QRhiGraphicsPipeline::Topology>(key.topology));
        record->pipeline->setDepthTest(material.depthTest);
        record->pipeline->setDepthWrite(material.depthWrite);
        record->pipeline->setSampleCount(key.sampleCount);

        if (material.requiresBlending()) {
            record->pipeline->setTargetBlends({ toBlendState(material) });
        } else {
            record->pipeline->setTargetBlends({});
        }

        QVector<QRhiShaderResourceBinding> layoutBindings;
        layoutBindings.append(QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(material.globalUniformBinding,
                                                                       toRhiStages(material.globalUniformStages),
                                                                       nullptr,
                                                                       globalStride));

        if (material.wantsPerDrawUniform()) {
            layoutBindings.append(QRhiShaderResourceBinding::uniformBuffer(material.perDrawUniformBinding,
                                                                           toRhiStages(material.perDrawUniformStages),
                                                                           nullptr));
        }

        layoutBindings.append(QRhiShaderResourceBinding::sampledTexture(material.textureBinding,
                                                                        toRhiStages(material.textureStages),
                                                                        nullptr,
                                                                        sampler));

        record->layout = localRhi->newShaderResourceBindings();
        record->layout->setBindings(layoutBindings.cbegin(), layoutBindings.cend());
        record->layout->create();

        record->pipeline->setVertexInputLayout(layout);
        record->pipeline->setShaderResourceBindings(record->layout);
        record->pipeline->setRenderPassDescriptor(key.renderPass);
        record->pipeline->create();

        return record;
    };

    return m_frame->acquirePipeline(key, rhi, createFn);
}

void cwRhiTexturedItems::purgePipelinesFor(QRhiRenderPassDescriptor* descriptor)
{
    for (auto* item : std::as_const(m_items)) {
        item->purgePipelinesFor(descriptor);
    }
}

QRhiShaderResourceBinding::StageFlags cwRhiTexturedItems::toRhiStages(cwRenderMaterialState::ShaderStages stages)
{
    using Stage = cwRenderMaterialState::ShaderStage;
    QRhiShaderResourceBinding::StageFlags flags = {};
    if (stages.testFlag(Stage::Vertex)) {
        flags |= QRhiShaderResourceBinding::VertexStage;
    }
    if (stages.testFlag(Stage::Fragment)) {
        flags |= QRhiShaderResourceBinding::FragmentStage;
    }
    return flags;
}

QRhiGraphicsPipeline::CullMode cwRhiTexturedItems::toRhiCullMode(cwRenderMaterialState::CullMode mode)
{
    switch (mode) {
    case cwRenderMaterialState::CullMode::None:  return QRhiGraphicsPipeline::None;
    case cwRenderMaterialState::CullMode::Front: return QRhiGraphicsPipeline::Front;
    case cwRenderMaterialState::CullMode::Back:  return QRhiGraphicsPipeline::Back;
    }
    return QRhiGraphicsPipeline::Back;
}

QRhiGraphicsPipeline::FrontFace cwRhiTexturedItems::toRhiFrontFace(cwRenderMaterialState::FrontFace face)
{
    return face == cwRenderMaterialState::FrontFace::CW
           ? QRhiGraphicsPipeline::CW
           : QRhiGraphicsPipeline::CCW;
}

QRhiGraphicsPipeline::TargetBlend cwRhiTexturedItems::toBlendState(const cwRenderMaterialState& material)
{
    QRhiGraphicsPipeline::TargetBlend blend;
    blend.enable = true;

    switch (material.blendMode) {
    case cwRenderMaterialState::BlendMode::None:
        blend.enable = false;
        break;
    case cwRenderMaterialState::BlendMode::Alpha:
        blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
        blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        blend.srcAlpha = QRhiGraphicsPipeline::One;
        blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        break;
    case cwRenderMaterialState::BlendMode::Additive:
        blend.srcColor = QRhiGraphicsPipeline::One;
        blend.dstColor = QRhiGraphicsPipeline::One;
        blend.srcAlpha = QRhiGraphicsPipeline::One;
        blend.dstAlpha = QRhiGraphicsPipeline::One;
        break;
    case cwRenderMaterialState::BlendMode::PremultipliedAlpha:
        blend.srcColor = QRhiGraphicsPipeline::One;
        blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        blend.srcAlpha = QRhiGraphicsPipeline::One;
        blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        break;
    }

    return blend;
}

cwRHIObject::RenderPass cwRhiTexturedItems::toRenderPass(cwRenderMaterialState::RenderPass pass)
{
    using MaterialPass = cwRenderMaterialState::RenderPass;
    switch (pass) {
    case MaterialPass::Opaque:      return cwRHIObject::RenderPass::Opaque;
    case MaterialPass::Transparent: return cwRHIObject::RenderPass::Transparent;
    case MaterialPass::Overlay:     return cwRHIObject::RenderPass::Overlay;
    case MaterialPass::ShadowMap:   return cwRHIObject::RenderPass::ShadowMap;
    }
    return cwRHIObject::RenderPass::Opaque;
}

QRhiSampler* cwRhiTexturedItems::sharedSampler(QRhi* rhi)
{
    return m_frame->sharedLinearClampSampler(rhi);
}
