#ifndef CWRENDERTEXTUREDITEMS_H
#define CWRENDERTEXTUREDITEMS_H

#include "cwRenderObject.h"
#include "cwGeometry.h"
#include <QtGui/qimage.h>

class cwRenderTexturedItems : public cwRenderObject
{
    Q_OBJECT

public:
    cwRenderTexturedItems();

    struct Item {
        cwGeometry geometry;
        QImage texture;
    };

    uint32_t addItem(const Item& item);
    void updateGeometry(uint32_t id, const cwGeometry& geometry);
    void updateTexture(uint32_t id, const QImage& image);
    void removeItem(uint32_t id);

    //For testing


protected:
    cwRHIObject *createRHIObject() override;

private:
    // Pending data to update
    class PendingCommand {
    public:
        enum Type {
            Add,
            Remove,
            UpdateGeometry,
            UpdateTexture,
            Unknown
        };

        PendingCommand() :
            m_commandType(Unknown)
        {

        }

        PendingCommand(Type type, uint32_t id, Item item) :
            m_commandType(type),
            m_id(id),
            m_item(item)
        { }

        Type type() const { return m_commandType; }
        uint32_t id() const { return m_id; }
        const Item& item() const { return m_item; }


    private:
        Type m_commandType;
        uint32_t m_id = 0;
        Item m_item;

    };

    QVector<PendingCommand> m_pendingChanges;

    // Simple ID generator for items
    uint32_t m_nextId = 1;
    QSet<uint32_t> m_ids;

    void addCommand(const PendingCommand& command);

    friend class cwRhiTexturedItems;
};

#endif // CWRENDERTEXTUREDITEMS_H
