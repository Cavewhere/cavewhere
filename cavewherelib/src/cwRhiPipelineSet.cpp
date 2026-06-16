#include "cwRhiPipelineSet.h"
#include "cwRhiScene.h"

void cwRhiPipelineSet::purgeFor(QRhiRenderPassDescriptor* descriptor)
{
    if (!m_scene) {
        return;
    }
    for (auto it = m_records.begin(); it != m_records.end(); ) {
        if (it.key().renderPass == descriptor) {
            m_scene->releasePipeline(it.value());
            it = m_records.erase(it);
        } else {
            ++it;
        }
    }
}

void cwRhiPipelineSet::releaseAll()
{
    if (m_scene) {
        for (auto* record : std::as_const(m_records)) {
            m_scene->releasePipeline(record);
        }
    }
    m_records.clear();
}
