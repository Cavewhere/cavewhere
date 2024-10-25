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

        if(m_pendingChanges.contains(scrap)) {
            PendingScrapCommand existingCommand = m_pendingChanges.value(scrap);
            if(existingCommand.type() == PendingScrapCommand::RemoveScrap) {
                // Replace with an add command
                m_pendingChanges.insert(scrap, command);
                update();
            }
        } else {
            m_pendingChanges.insert(scrap, command);
            update();
        }
    }
}

void cwRenderScraps::removeScrap(cwScrap *scrap)
{
    PendingScrapCommand command = PendingScrapCommand(PendingScrapCommand::RemoveScrap,
                                                      scrap,
                                                      cwTriangulatedData());

    if(m_pendingChanges.contains(scrap)) {
        PendingScrapCommand existingCommand = m_pendingChanges.value(scrap);
        if(existingCommand.type() == PendingScrapCommand::AddScrap) {
            // Scrap has been removed
            m_pendingChanges.insert(scrap, command);
            update();
        }
    } else {
        m_pendingChanges.insert(scrap, command);
        update();
    }
}

cwRHIObject* cwRenderScraps::createRHIObject() {
    return new cwRhiScraps();
}
