#include "cwRhiPipelineSet.h"
#include "cwRhiFrameRenderer.h"

void cwRhiPipelineSet::purgeFor(QRhiRenderPassDescriptor* descriptor)
{
    if (!m_frame) {
        return;
    }
    for (auto it = m_records.begin(); it != m_records.end(); ) {
        if (it.key().renderPass == descriptor) {
            m_frame->releasePipeline(it.value());
            it = m_records.erase(it);
        } else {
            ++it;
        }
    }
}

void cwRhiPipelineSet::releaseAll()
{
    if (m_frame) {
        for (auto* record : std::as_const(m_records)) {
            m_frame->releasePipeline(record);
        }
    }
    m_records.clear();
}
