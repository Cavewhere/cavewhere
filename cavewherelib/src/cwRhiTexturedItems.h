#ifndef CWRHITEXTUREDITEMS_H
#define CWRHITEXTUREDITEMS_H

//Our includes
#include "cwRHIObject.h"
#include "cwRenderMemoryLedger.h"
#include "cwRenderTexturedItems.h"
#include "cwRhiPipelineSet.h"
#include "cwRhiFrameRenderer.h"
#include "cwStreamedItemState.h"
#include "cwStreamedTexture.h"
#include "cwTextureStreamer.h"
#include <QMatrix4x4>
#include <optional>

class cwFrustum;

class cwRhiTexturedItems : public cwRHIObject
{
public:
    cwRhiTexturedItems();
    ~cwRhiTexturedItems();

    void initialize(const ResourceUpdateData& data) override;
    void synchronize(const SynchronizeData& data) override;
    void updateResources(const ResourceUpdateData& data) override;
    /**
     * Drains finished loads, uploads what this frame's budget allows, and then
     * enforces the GPU budget by demoting streamed items back to their pinned
     * base.
     *
     * Enforcement reads the process-wide ledger, which every 3D view reports
     * into, so convergence is joint: each view demotes its own items against the
     * shared total and every demotion shrinks what the other views measure.
     */
    bool streamResources(ResourceUpdateData& data, qint64& remainingUploadBytes) override;
    /**
     * Runs selection for every item the offscreen job's camera can see, at the
     * job's output size, asking for whatever detail is missing at export priority.
     * False while any of those items is coarser than the job wants.
     */
    bool residencyReady(const RenderData& jobRenderData) override;
    /**
     * Cancels every streamed load and frees every resident streamed chain, so
     * the ledger gets the whole scene's texture bytes back. Only for a view that
     * has stopped drawing; the next frame after it is shown again rebinds the
     * loading texture and re-requests the pinned base.
     */
    void releaseStreamedResources() override;
    bool gather(const GatherContext& context, QVector<PipelineBatch>& batches) override;
    void purgePipelinesFor(QRhiRenderPassDescriptor* descriptor) override;
    std::optional<QBox3D> worldBounds() const override;

private:
    using StreamPriority = cwStreamedItemState::StreamPriority;

    struct SharedItemData {
        QRhiTexture* loadingTexture = nullptr;
    };

    struct Item {
        Item();
        ~Item();

        QRhiBuffer* vertexBuffer = nullptr;
        QRhiBuffer* indexBuffer = nullptr;
        QRhiBuffer* uniformBuffer = nullptr;
        QRhiTexture* texture = nullptr;
        QRhiShaderResourceBindings* srb = nullptr;
        // One pipeline per render target this item draws into, kept resident so
        // live↔offscreen toggling never rebuilds them. pipelineRecord is the
        // record for the pass currently being gathered/rendered. The srb above
        // is render-pass-independent and shared across all of them.
        cwRhiPipelineSet pipelines;
        cwRhiPipelineRecord* pipelineRecord = nullptr;

        int numberOfIndices = 0;
        QRhiCommandBuffer::IndexFormat indexFormat = QRhiCommandBuffer::IndexUInt32;

        cwGeometry geometry;
        // The whole-texture fallback, waiting to upload; dropped once the
        // upload is recorded. Only a producer with no cache entry to stream
        // sends one.
        QImage image;

        // The streamed alternative: a descriptor the render thread pulls mip
        // levels from, one budgeted upload at a time. `streaming` owns which
        // level is resident, which load is open and the chain being uploaded;
        // uvPerMeter is the geometry's texel density, measured while the
        // geometry is still on hand (setLocalBounds).
        cwStreamedTexture streamSource;
        cwStreamedItemState streaming;
        double uvPerMeter = 0.0;
        quint64 lastVisibleFrame = 0;

        QByteArray uniformBlock;
        cwRenderMaterialState material;
        QMatrix4x4 modelMatrix;

        // localBounds comes from the Position attribute, worldBounds from
        // localBounds through modelMatrix. An item with invalid bounds always
        // draws rather than risking a wrong cull.
        QBox3D localBounds;
        QBox3D worldBounds;
        bool boundsValid = false;

        bool resourcesInitialized = false;
        bool geometryNeedsUpdate = false;
        bool textureNeedsUpdate = false;
        bool uniformNeedsUpdate = false;
        bool pipelineNeedsUpdate = true;
        bool modelMatrixNeedsUpdate = true;

        cwLedgeredBytes geometryBytes {cwRenderMemoryLedger::Category::TexturedItemGeometry,
                                       cwRenderMemoryLedger::Residency::Gpu};
        cwLedgeredBytes textureBytes {cwRenderMemoryLedger::Category::TexturedItemTexture,
                                      cwRenderMemoryLedger::Residency::Gpu};

        cwRhiTexturedItems* owner = nullptr;

        void initializeResources(const ResourceUpdateData &data, const SharedItemData &sharedData);
        void ensurePipeline(const RenderData& renderData, const SharedItemData &sharedData, const QRhiVertexInputLayout& layout);
        void updateGeometryBuffers(const ResourceUpdateData& data);
        void updateTextureResource(const ResourceUpdateData& data, const SharedItemData &sharedData);
        void updateUniformBuffer(const ResourceUpdateData& data);
        void createShaderResourceBindings(const ResourceUpdateData& data, const SharedItemData &sharedData);
        void purgePipelinesFor(QRhiRenderPassDescriptor* descriptor);
        QByteArray buildPerDrawUniformPayload() const;
        //! Adopts the GUI thread's local bounds, then refreshes worldBounds and
        //! the texel density selection needs
        void setLocalBounds(const std::optional<QBox3D>& bounds);
        void updateWorldBounds();
        //! True when @a frustum is on and this item's box falls outside it. An
        //! item with invalid bounds always draws rather than risking a wrong cull.
        bool isCulledBy(const cwFrustum* frustum) const;
        //! Uploads pending levels while the frame's budget allows; true while
        //! levels remain
        bool uploadPendingLevels(const ResourceUpdateData& data,
                                 const SharedItemData& sharedData,
                                 qint64& remainingUploadBytes,
                                 bool& anythingUploadedThisFrame);
    };

    //! Picks the mip level @a item should hold this frame and asks the streamer
    //! for it. Arithmetic only in the common no-change case.
    void selectStreamLevel(uint32_t id, Item* item, const GatherContext& context,
                           StreamPriority priority = StreamPriority::Live);

    //! Turns a state-machine transition into the streamer call it asks for
    void applyStreamAction(uint32_t id, Item* item, const cwStreamedItemState::Action& action);

    //! Demotes streamed items back to their pinned base until the ledger's GPU
    //! total fits @a budgets.gpuBudgetBytes. A no-op while under budget.
    void enforceGpuBudget(const cwRenderBudgets& budgets);

    //! The compressed format streamed levels transcode to, or RGBA8 when the
    //! backend accepts no compressed format
    static QRhiTexture::Format streamTargetFormat();

    //! Publishes this frame's streamed-texture residency counts for the HUD
    void publishStreamingStats() const;

    QHash<uint32_t, Item*> m_items;
    cwTextureStreamer m_streamer;
    bool m_resourcesInitialized = false;
    // True while the budget is over and no streamed item has detail it can give
    // back. Warning on the transition into that state keeps a budget set below
    // the floor from warning every frame.
    bool m_atResidencyFloor = false;
    SharedItemData m_sharedData;
    QRhiVertexInputLayout m_inputLayout;

    static QRhiShaderResourceBinding::StageFlags toRhiStages(cwRenderMaterialState::ShaderStages stages);
    static QRhiGraphicsPipeline::CullMode toRhiCullMode(cwRenderMaterialState::CullMode mode);
    static QRhiGraphicsPipeline::FrontFace toRhiFrontFace(cwRenderMaterialState::FrontFace face);
    static QRhiGraphicsPipeline::TargetBlend toBlendState(const cwRenderMaterialState& material);
    static quint8 toStageMask(cwRenderMaterialState::ShaderStages stages);

    cwRhiPipelineKey makePipelineKey(QRhiRenderPassDescriptor* renderPass,
                                     int sampleCount,
                                     const cwRenderMaterialState& material) const;
    cwRhiPipelineRecord* acquirePipeline(const cwRhiPipelineKey& key,
                                                const cwRenderMaterialState& material,
                                                QRhi* rhi,
                                                const QRhiVertexInputLayout& layout,
                                                const SharedItemData& sharedData);
    QRhiSampler* sharedSampler(QRhi* rhi);
    static cwRHIObject::RenderPass toRenderPass(cwRenderMaterialState::RenderPass pass);

    // Per-item render state is private and needs no production accessor; the
    // sync-time streaming tests read it through this friend, the same seam
    // CwRhiSceneTestAccess uses for cwRhiScene.
    friend struct CwRhiTexturedItemsTestAccess;
};



#endif // CWRHITEXTUREDITEMS_H
