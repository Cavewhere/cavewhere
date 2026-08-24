#ifndef CWRHITEXTUREDITEMS_H
#define CWRHITEXTUREDITEMS_H

//Our includes
#include "cwRHIObject.h"
#include "cwRenderMemoryLedger.h"
#include "cwRenderTexturedItems.h"
#include "cwRhiPipelineSet.h"
#include "cwRhiFrameRenderer.h"
#include "cwStreamedTexture.h"
#include "cwTextureStreamer.h"
#include <QMatrix4x4>

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
    bool gather(const GatherContext& context, QVector<PipelineBatch>& batches) override;
    void purgePipelinesFor(QRhiRenderPassDescriptor* descriptor) override;
    std::optional<QBox3D> worldBounds() const override;

private:
    //! residentTopLevel / requestedTopLevel when the item holds neither
    static constexpr int kNoResidentLevel = -1;

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
        // Whichever whole-texture representation is waiting to upload; both are
        // dropped once the upload is recorded.
        QImage image;
        cwCompressedTexture compressedTexture;

        // The streamed alternative: a descriptor the render thread pulls mip
        // levels from, one budgeted upload at a time. residentTopLevel is the
        // most detailed level `texture` holds, requestedTopLevel the level a
        // load is running for, and uvPerMeter the geometry's texel density,
        // measured while the geometry is still on hand (updateBoundsFromGeometry).
        cwStreamedTexture streamSource;
        int residentTopLevel = kNoResidentLevel;
        int requestedTopLevel = kNoResidentLevel;
        // The level the camera asked for the last time the item was gathered,
        // so the planner can tell a demotion that would stick from one
        // selection undoes on the next frame.
        int desiredTopLevel = kNoResidentLevel;
        double uvPerMeter = 0.0;
        quint64 lastVisibleFrame = 0;
        // True while the open request is a budget demotion rather than a
        // refinement, so the next frame's planner leaves the item alone. A
        // finer request from selection clears it — the streamer's generation
        // drops the demotion that is already running.
        bool demotionInFlight = false;

        // Levels that have landed on the render thread and are being uploaded
        // into stagingTexture, one budgeted level per frame. Nothing samples
        // stagingTexture until the last level lands, so a half-built chain can
        // straddle frames; the swap onto `texture` is what makes it visible.
        struct PendingUpload {
            cwCompressedTexture readyLevels;
            int readyTopLevel = kNoResidentLevel;
            int nextLevelToUpload = 0;
            QRhiTexture* stagingTexture = nullptr;
        };
        PendingUpload pendingUpload;

        // The newest stream generation this item has accepted, so a load that
        // finishes after a newer one is dropped instead of overwriting it.
        quint64 acceptedGeneration = 0;
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
        //! Uploads compressedTexture; false means the backend rejected the
        //! format or the texture and the caller must use the QImage path
        bool uploadCompressedTexture(const ResourceUpdateData& data);
        void updateUniformBuffer(const ResourceUpdateData& data);
        void createShaderResourceBindings(const ResourceUpdateData& data, const SharedItemData &sharedData);
        void purgePipelinesFor(QRhiRenderPassDescriptor* descriptor);
        QByteArray buildPerDrawUniformPayload() const;
        void updateBoundsFromGeometry();
        void updateWorldBounds();

        //! Drops the half-built chain and its staging texture, keeping `texture`
        void clearPendingUpload();
        //! Drops the half-built chain and reopens the request slot, so selection
        //! can ask for the level again instead of the item stalling forever
        void failPendingUpload();
        //! Forgets what is resident without touching `texture` — a replacement
        //! must land before the item stops drawing what it has
        void resetResidency();
        //! Uploads pending levels while the frame's budget allows; true while
        //! levels remain
        bool uploadPendingLevels(const ResourceUpdateData& data,
                                 const SharedItemData& sharedData,
                                 qint64& remainingUploadBytes,
                                 bool& anythingUploadedThisFrame);
    };

    //! Picks the mip level @a item should hold this frame and asks the streamer
    //! for it. Arithmetic only in the common no-change case.
    void selectStreamLevel(uint32_t id, Item* item, const GatherContext& context);

    //! Demotes streamed items back to their pinned base until the ledger's GPU
    //! total fits @a budgets.gpuBudgetBytes. A no-op while under budget.
    void enforceGpuBudget(const cwRenderBudgets& budgets);

    //! The compressed format streamed levels transcode to, or RGBA8 when the
    //! backend accepts no compressed format
    static QRhiTexture::Format streamTargetFormat();

    //! Adds this object's item counts to the frame's culled/total tally
    void tallyCullingStats(const GatherContext& context) const;

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
