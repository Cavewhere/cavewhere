#ifndef CWRENDERSCRAPS_H
#define CWRENDERSCRAPS_H

// Our includes
#include "cwRenderObject.h"
#include "cwTriangulatedData.h"
#include "cwImageTexture.h"
#include "cwGeometryItersecter.h"
#include "cwFutureManagerToken.h"
#include "cwProject.h"
class cwCavingRegion;
class cwScrap;

#include <QSharedPointer>
#include <QQmlEngine>
#include <QHash>

class cwRenderScraps : public cwRenderObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RenderScraps)

    Q_PROPERTY(cwProject* project READ project WRITE setProject NOTIFY projectChanged)
    // Q_PROPERTY(bool visible READ visible WRITE setVisible NOTIFY visibleChanged)

public:
    explicit cwRenderScraps(QObject *parent = nullptr);

    cwProject* project() const;
    void setProject(cwProject* project);

    void setCavingRegion(cwCavingRegion* region);
    void setFutureManagerToken(cwFutureManagerToken token);

    // bool visible() const;
    // void setVisible(bool visible);

    void addScrapToUpdate(cwScrap* scrap);
    void removeScrap(cwScrap* scrap);

    void addScrapTextureToUpdate(cwScrap* scrap);

    cwRHIObject* createRHIObject() override;

signals:
    void projectChanged();
    // void visibleChanged();

private:
    cwProject* m_project = nullptr; //!< The project file for loading textures
    cwFutureManagerToken m_futureManagerToken;

    // Pending data to update
    class PendingScrapCommand {
    public:
        enum Type {
            AddScrap,
            RemoveScrap,
            UpdateScrapTexture,
            Unknown
        };

        PendingScrapCommand() :
            m_commandType(Unknown),
            m_scrap(nullptr)
        {

        }

        PendingScrapCommand(Type type, cwScrap* scrap, cwTriangulatedData data) :
            m_commandType(type),
            m_scrap(scrap),
            m_data(data)
        { }

        Type type() const { return m_commandType; }
        cwScrap* scrap() const { return m_scrap; }
        cwTriangulatedData triangulatedData() const { return m_data; }

    private:
        Type m_commandType;
        cwScrap* m_scrap;
        cwTriangulatedData m_data; //We need triangle data because scrap might get delete duing a frame

    };

    QHash<cwScrap*, PendingScrapCommand> m_pendingChanges;

    bool m_visible = true; //!< True if the scraps are visible and false if they're not

    void addCommand(const PendingScrapCommand& command);

    friend class cwRhiScraps;
};

#endif // CWRENDERSCRAPS_H
