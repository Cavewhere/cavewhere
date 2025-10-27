#ifndef CWRENDERSCRAPS_H
#define CWRENDERSCRAPS_H

// Our includes
#include "cwRenderObject.h"
#include "cwTriangulatedData.h"
#include "cwGeometryItersecter.h"
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

public:
    explicit cwRenderScraps(QObject *parent = nullptr);

    void addScrapToUpdate(cwScrap* scrap);
    void removeScrap(cwScrap* scrap);

    void addScrapTextureToUpdate(cwScrap* scrap);

    cwRHIObject* createRHIObject() override;

private:

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

    QVector<PendingScrapCommand> m_pendingChanges;

    void addCommand(const PendingScrapCommand& command);

    friend class cwRhiScraps;
};

#endif // CWRENDERSCRAPS_H
