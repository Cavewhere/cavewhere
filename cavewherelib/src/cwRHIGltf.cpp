#include "cwRHIGltf.h"
#include "cwRenderGLTF.h" // for pulling SceneCPU from the render object
#include "cwRhiItemRenderer.h"

//Qt includes
#include <QDebug>
#include <QFile>

cwRHIGltf::cwRHIGltf() {}

cwRHIGltf::~cwRHIGltf() {}

void cwRHIGltf::initialize(const ResourceUpdateData& data)
{
    if (m_initialized) {
        return;
    }

    ensureShaders();

    QRhi* rhi = data.renderData.renderer->rhi();
    if (rhi == nullptr) {
        return;
    }

    // Dynamic UBOs (updated every frame)
    m_sceneUbo = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(QMatrix4x4));
    m_sceneUbo->create();

    m_modelUbo = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(QMatrix4x4));
    m_modelUbo->create();

    m_materialUbo = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(QVector4D));
    m_materialUbo->create();

    m_initialized = true;
}

void cwRHIGltf::synchronize(const SynchronizeData& data)
{
    // Pull CPU scene from cwRenderGLTF
    if (auto* ro = dynamic_cast<cwRenderGLTF*>(data.object)) {
        const cw::gltf::SceneCPU& scene = ro->m_data.value(); // cwTracked<T> accessor
        m_sceneCPU = scene;
        m_resourcesDirty = true;
    }
}

void cwRHIGltf::updateResources(const ResourceUpdateData& data)
{
    if (!m_initialized) {
        initialize(data);
    }

    if (!m_resourcesDirty) {
        return;
    }

    QRhi* rhi = data.renderData.renderer->rhi();
    if (rhi == nullptr) {
        return;
    }

    // Clear old GPU state (simple approach; you can make it incremental)
    m_meshes.clear();
    m_textures.clear();
    m_pipelines.clear();

    // Build textures and meshes from CPU snapshot
    buildTextures(rhi, data.resourceUpdateBatch);
    buildMeshes(rhi, data.resourceUpdateBatch);

    m_resourcesDirty = false;
}

void cwRHIGltf::render(const RenderData& data)
{
    if (!isVisible()) {
        return;
    }
    if (!m_initialized) {
        return;
    }

    QRhi* rhi = data.renderer->rhi();
    if (rhi == nullptr) {
        return;
    }

    // Update per-frame scene UBO
    QMatrix4x4 viewProj;
    {
        // Replace with your renderer’s actual API
        // e.g., data.renderer->viewProjectionMatrix();
        viewProj = data.renderer->viewProjectionMatrix();
    }

    QRhiResourceUpdateBatch* rub = rhi->nextResourceUpdateBatch();
    rub->updateDynamicBuffer(m_sceneUbo, 0, sizeof(QMatrix4x4), viewProj.constData());

    // We’ll reuse one SRB per pipeline, but material texture binding can vary.
    // To keep it simple, we rebuild SRB per material use (per draw) here by cloning.
    // If performance matters, move SRB creation to updateResources and keep per-mesh SRBs.

    for (const MeshGPU& mesh : m_meshes) {

        // Update model + material
        rub->updateDynamicBuffer(m_modelUbo, 0, sizeof(QMatrix4x4), mesh.modelMatrix.constData());
        rub->updateDynamicBuffer(m_materialUbo, 0, sizeof(QVector4D), &mesh.baseColorFactor);

        data.cb->resourceUpdate(rub);
        rub = rhi->nextResourceUpdateBatch(); // get a fresh batch for the next draw’s updates

        // Choose texture (optional)
        QRhiTexture* tex = nullptr;
        QRhiSampler* samp = nullptr;
        if (mesh.baseColorTextureIndex >= 0 && mesh.baseColorTextureIndex < m_textures.size()) {
            const TextureGPU& tg = m_textures[mesh.baseColorTextureIndex];
            if (tg.valid()) {
                tex = tg.texture;
                samp = tg.sampler;
            }
        }

        for (const PrimitiveGPU& primitive : mesh.primitives) {
            // Pipeline for this vertex layout
            PipelineKey key;
            key.hasNormal = primitive.hasNormal;
            key.hasTangent = primitive.hasTangent;
            key.hasTexCoord0 = primitive.hasTexCoord0;
            key.stride = primitive.vertexStride;

            //Stride isn't supported by this code currently
            // Q_ASSERT(primitive.vertexStride == 0);

            PipelinePack* pack = ensurePipeline(rhi, key, data.renderer->renderTarget()->renderPassDescriptor());
            if (pack == nullptr || !pack->pipeline) {
                continue;
            }

            // Bind dynamic SRB (we clone the cached SRB and set texture if present)
            QScopedPointer<QRhiShaderResourceBindings> srb(
                rhi->newShaderResourceBindings()
                );
            QVector<QRhiShaderResourceBinding> bindings;
            bindings << QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, m_sceneUbo);
            bindings << QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::VertexStage, m_modelUbo);
            bindings << QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::FragmentStage, m_materialUbo);

            if (tex && samp) {
                bindings << QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage, tex, samp);
            } else {
                // Bind a null texture slot if your shader expects it; or branch in shader.
            }
            srb->setBindings(bindings.cbegin(), bindings.cend());
            srb->create();

            //This makes sure the pipeline matches
            Q_ASSERT(srb->isLayoutCompatible(pack->pipeline->shaderResourceBindings()));

            // Issue draw
            data.cb->setGraphicsPipeline(pack->pipeline);
            data.cb->setShaderResources(srb.data());

            const QRhiCommandBuffer::VertexInput vbind(primitive.vertex, 0);

            // qDebug() << vbind.
            qDebug() << "vbind:" << primitive.vertex << primitive.index->size() << primitive.indexFormat;

            // data.cb->setVertexInput(0, 1, &vbind);
            data.cb->setVertexInput(0, 1, &vbind,
                                    primitive.index,
                                    0, //offset
                                    primitive.indexFormat);

            qDebug() << "Dram primative:" << primitive.indexCount;
           data.cb->drawIndexed(primitive.indexCount);
        }
    }

    // Flush any remaining updates
    if (rub) {
        data.cb->resourceUpdate(rub);
    }
}

// ---------- Helpers ----------

void cwRHIGltf::ensureShaders()
{
    if (m_vertexShader.isValid() && m_fragmentShader.isValid()) {
        return;
    }
    // Assumes you have precompiled .qsb shaders named like these:

    Q_ASSERT(QFile::exists(QStringLiteral(":/shaders/unlit.vert.qsb")));
    Q_ASSERT(QFile::exists(QStringLiteral(":/shaders/unlit.frag.qsb")));

    m_vertexShader = cwRHIObject::loadShader(QStringLiteral(":/shaders/unlit.vert.qsb"));
    m_fragmentShader = cwRHIObject::loadShader(QStringLiteral(":/shaders/unlit.frag.qsb"));
}

QRhiGraphicsPipeline::TargetBlend cwRHIGltf::premultipliedAlphaBlend()
{
    QRhiGraphicsPipeline::TargetBlend b;
    b.enable = true;
    b.srcColor = QRhiGraphicsPipeline::One;
    b.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    b.opColor = QRhiGraphicsPipeline::Add;
    b.srcAlpha = QRhiGraphicsPipeline::One;
    b.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    b.opAlpha = QRhiGraphicsPipeline::Add;
    return b;
}

QRhiVertexInputLayout cwRHIGltf::makeInputLayout(const PipelineKey& key)
{
    QRhiVertexInputLayout layout;

    QVector<QRhiVertexInputAttribute> attrs;
    int offset = 0;

    // location 0: position (float3)
    attrs << QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float3, offset);
    offset += int(sizeof(float) * 3);

    // // location 1: normal (float3)
    // if (key.hasNormal) {
    //     qDebug() << "Has normals!";
    //     attrs << QRhiVertexInputAttribute(0, 1, QRhiVertexInputAttribute::Float3, offset);
    //     offset += int(sizeof(float) * 3);
    // }

    // // location 2: tangent (float4)
    // if (key.hasTangent) {
    //     qDebug() << "Has tangent!";
    //     attrs << QRhiVertexInputAttribute(0, 2, QRhiVertexInputAttribute::Float4, offset);
    //     offset += int(sizeof(float) * 4);
    // }

    // location 3: texcoord0 (float2)
    if (key.hasTexCoord0) {
        qDebug() << "Has texcoord!";
        attrs << QRhiVertexInputAttribute(0, 1, QRhiVertexInputAttribute::Float2, offset);
        offset += int(sizeof(float) * 2);
    }

    qDebug() << "Stride:" << key.stride << offset;

    Q_ASSERT(key.stride == offset);

    layout.setBindings({ QRhiVertexInputBinding(key.stride) });
    layout.setAttributes(attrs.begin(), attrs.end());

    return layout;
}

cwRHIGltf::PipelinePack* cwRHIGltf::ensurePipeline(QRhi* rhi,
                                                   const PipelineKey& key,
                                                   QRhiRenderPassDescriptor* renderPassDesc)
{
    auto it = m_pipelines.find(key);
    if (it != m_pipelines.end() && it->pipeline) {
        return &it.value();
    }

    PipelinePack pack;

    QRhiGraphicsPipeline* pipe = rhi->newGraphicsPipeline();

    QRhiGraphicsPipeline::TargetBlend blend = premultipliedAlphaBlend();
    pipe->setTargetBlends({ blend });
    pipe->setDepthTest(true);
    pipe->setDepthWrite(true);
    pipe->setCullMode(QRhiGraphicsPipeline::Back);
    pipe->setFrontFace(QRhiGraphicsPipeline::CCW);

    //TODO: This needs to be fix, see cwRhiScraps
    pipe->setSampleCount(4);


    //TODO: This not work for all gltf files, but this is for testing
    pipe->setTopology(QRhiGraphicsPipeline::Triangles);

    QRhiVertexInputLayout inputLayout = makeInputLayout(key);
    pipe->setVertexInputLayout(inputLayout);

    pipe->setShaderStages({{ QRhiShaderStage::Vertex, m_vertexShader },
                           { QRhiShaderStage::Fragment, m_fragmentShader }});


    // Bind dynamic SRB (we clone the cached SRB and set texture if present)
    auto srb = rhi->newShaderResourceBindings();
    QVector<QRhiShaderResourceBinding> bindings;
    bindings << QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, nullptr);
    bindings << QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::VertexStage, nullptr);
    bindings << QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::FragmentStage, nullptr);

    // if (tex && samp) {
    bindings << QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage, nullptr, nullptr);
    // } else {
    //     // Bind a null texture slot if your shader expects it; or branch in shader.
    // }
    srb->setBindings(bindings.cbegin(), bindings.cend());
    srb->create();

    pipe->setShaderResourceBindings(srb);

    qDebug() << "glTF shaders:" << m_vertexShader.isValid() << m_fragmentShader.isValid();
    qDebug() << m_vertexShader.description();

    pipe->setRenderPassDescriptor(renderPassDesc);
    if (!pipe->create()) {
        return nullptr;
    }

    pack.pipeline = pipe;
    // We do not create a persistent SRB here since we bind dynamic buffers and optional textures per draw.

    m_pipelines.insert(key, pack);
    return &m_pipelines[key];
}

void cwRHIGltf::buildTextures(QRhi* rhi, QRhiResourceUpdateBatch* rub)
{
    m_textures.resize(m_sceneCPU.textures.size());

    for (int i = 0; i < m_sceneCPU.textures.size(); i++) {
        const auto& cpu = m_sceneCPU.textures[i];
        if (cpu.width <= 0 || cpu.height <= 0 || cpu.pixels.isEmpty()) {
            continue;
        }

        TextureGPU t;

        QRhiTexture::Format fmt = QRhiTexture::RGBA8; // Use SRGB8A8 if your build exposes it for baseColor
        t.texture = rhi->newTexture(fmt,
                                    QSize(cpu.width, cpu.height),
                                    1,
                                    QRhiTexture::MipMapped | QRhiTexture::UsedWithGenerateMips);
                                    // QRhiTexture::UsedAsTransferSource);
                                        // | QRhiTexture::UsedWithTransferDestination
                                        // | QRhiTexture::UsedWithSampled);
        t.texture->create();

        t.sampler = rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::Linear,
                                    QRhiSampler::Repeat, QRhiSampler::Repeat);
        t.sampler->create();

        QRhiTextureSubresourceUploadDescription sub(cpu.pixels);
        QRhiTextureUploadEntry entry(0, 0, sub);
        QRhiTextureUploadDescription desc({ entry });
        rub->uploadTexture(t.texture, desc);
        rub->generateMips(t.texture);

        m_textures[i] = t;
    }
}

void cwRHIGltf::buildMeshes(QRhi* rhi, QRhiResourceUpdateBatch* resources)
{
    m_meshes.clear();
    m_meshes.reserve(m_sceneCPU.meshes.size());

    for (const auto& m : std::as_const(m_sceneCPU.meshes)) {
        MeshGPU mgpu;
        mgpu.modelMatrix = m.modelMatrix;
        mgpu.baseColorFactor = m.material.baseColorFactor;
        mgpu.baseColorTextureIndex = m.material.baseColorTextureIndex;

        mgpu.primitives.reserve(m.primitives.size());
        for (const auto& p : m.primitives) {
            PrimitiveGPU pg;

            const int stride = (p.vertexCount > 0) ? p.vertexInterleaved.size() / p.vertexCount : 0;
            pg.vertexStride = stride;
            pg.indexCount = p.indexCount;
            pg.indexFormat = p.indexFormat;
            pg.hasNormal = p.hasNormal;
            pg.hasTangent = p.hasTangent;
            pg.hasTexCoord0 = p.hasTexCoord0;

            pg.vertex = rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, p.vertexInterleaved.size());
            pg.vertex->create();

            pg.index = rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::IndexBuffer, p.indexData.size());
            pg.index->create();

            resources->uploadStaticBuffer(pg.vertex, 0, p.vertexInterleaved.size(), p.vertexInterleaved.constData());
            resources->uploadStaticBuffer(pg.index, 0, p.indexData.size(), p.indexData.constData());

            mgpu.primitives.push_back(pg);
        }

        m_meshes.push_back(std::move(mgpu));
    }
}
