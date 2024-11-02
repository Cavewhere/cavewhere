#include "cwRenderScraps.h"
#include "cwScrap.h"
#include "cwRhiScraps.h"
#include "cwScene.h"

cwRenderScraps::cwRenderScraps(QObject *parent) :
    cwRenderObject(parent),
    m_project(nullptr),
    m_visible(true)
{
}

cwProject* cwRenderScraps::project() const {
    return m_project;
}

void cwRenderScraps::setProject(cwProject* project) {
    if(m_project != project) {
        m_project = project;
        emit projectChanged();
    }
}

void cwRenderScraps::setFutureManagerToken(cwFutureManagerToken token)
{
    m_futureManagerToken = token;
}

bool cwRenderScraps::visible() const {
    return m_visible;
}

void cwRenderScraps::setVisible(bool visible) {
    if(m_visible != visible) {
        m_visible = visible;
        emit visibleChanged();
    }
}

void cwRenderScraps::addScrapToUpdate(cwScrap *scrap)
{
    if(!scrap->triangulationData().isNull()) {
        PendingScrapCommand command = PendingScrapCommand(PendingScrapCommand::AddScrap,
                                                          scrap,
                                                          scrap->triangulationData());

        addCommand(command);
    }
}

void cwRenderScraps::removeScrap(cwScrap *scrap)
{
    PendingScrapCommand command = PendingScrapCommand(PendingScrapCommand::RemoveScrap,
                                                      scrap,
                                                      cwTriangulatedData());

    addCommand(command);
}

void cwRenderScraps::addScrapTextureToUpdate(cwScrap *scrap)
{
    PendingScrapCommand command = PendingScrapCommand(PendingScrapCommand::UpdateScrapTexture,
                                                      scrap,
                                                      scrap->triangulationData());

    addCommand(command);

}

cwRHIObject* cwRenderScraps::createRHIObject() {
    return new cwRhiScraps();
}

void cwRenderScraps::addCommand(const PendingScrapCommand &command)
{
    m_pendingChanges.insert(command.scrap(), command);
    update();
}
