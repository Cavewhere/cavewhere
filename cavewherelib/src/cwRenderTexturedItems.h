#ifndef CWRENDERTEXTUREDITEMS_H
#define CWRENDERTEXTUREDITEMS_H

#include "cwRenderMaterialState.h"
#include "cwRenderObject.h"
#include "cwGeometry.h"
#include "CaveWhereLibExport.h"
#include <QHash>
#include <QByteArray>
#include <QtGui/qimage.h>
#include <QMatrix4x4>

class CAVEWHERE_LIB_EXPORT cwRenderTexturedItems : public cwRenderObject
{
    Q_OBJECT

public:
    using CullMode = cwRenderMaterialState::CullMode;

    cwRenderTexturedItems(QObject* parent = nullptr);

    static QVector<cwGeometry::AttributeDesc> geometryLayout();

    struct Item {
        cwGeometry geometry;
        QImage texture;
        cwRenderMaterialState material;
        QByteArray uniformBlock;
        QMatrix4x4 modelMatrix;
        bool visible = true;
        bool storeGeometry = false; // Keep CPU-side geometry when tests need it
        bool storeTexture = false;  // Keep CPU-side texture when tests need it
    };

    uint32_t addItem(const Item& item);
    void updateGeometry(uint32_t id, const cwGeometry& geometry);
    void updateTexture(uint32_t id, const QImage& image);
    // Named setItemVisible, not an overload of setVisible: a same-name
    // overload would hide cwRenderObject::setVisible(bool) and make the
    // whole-object toggle unreachable without qualification.
    void setItemVisible(uint32_t id, bool visible);
    void setCulling(uint32_t id, CullMode culling);
    void setMaterial(uint32_t id, const cwRenderMaterialState& material);
    void setUniformBlock(uint32_t id, const QByteArray& uniformBlock);
    void setModelMatrix(uint32_t id, const QMatrix4x4& modelMatrix);
    void removeItem(uint32_t id);

    //For testing
    Item item(uint32_t id) const;
    bool hasItem(uint32_t id) const;


protected:
    cwRHIObject *createRHIObject() override;
    void updateVisibility() override;

private:
    // What travels to the render thread. Item additionally carries
    // visible/storeGeometry/storeTexture, which are authoring state only —
    // visibility publishes through the scene visibility store, never on a
    // command — so payloads carry just the render data.
    struct ItemPayload {
        cwGeometry geometry;
        QImage texture;
        cwRenderMaterialState material;
        QByteArray uniformBlock;
        QMatrix4x4 modelMatrix;
    };

    // Pending data to update
    class PendingCommand {
    public:
        enum Type {
            Add,
            Remove,
            UpdateGeometry,
            UpdateTexture,
            UpdateMaterial,
            UpdateUniformBlock,
            UpdateModelMatrix,
            Unknown
        };

        PendingCommand() :
            m_commandType(Unknown)
        {

        }

        PendingCommand(Type type, uint32_t id, ItemPayload payload) :
            m_commandType(type),
            m_id(id),
            m_payload(payload)
        { }

        Type type() const { return m_commandType; }
        uint32_t id() const { return m_id; }
        const ItemPayload& payload() const { return m_payload; }


    private:
        ItemPayload m_payload;

        Type m_commandType;
        uint32_t m_id = 0;

    };

    QVector<PendingCommand> m_pendingChanges;

    // Simple ID generator for items
    uint32_t m_nextId = 1;
    QHash<uint32_t, Item> m_frontState;

    void addCommand(const PendingCommand &&command);

    // Publish one item's effective sub-item visibility: authored visibility
    // ANDed with its pick-ready gate, so an item stays hidden until its
    // sub-BVH publishes (issue #505 Phase 4).
    void publishItemVisibility(uint32_t id, bool authoredVisible);

    friend class cwRhiTexturedItems;
};


#endif // CWRENDERTEXTUREDITEMS_H
