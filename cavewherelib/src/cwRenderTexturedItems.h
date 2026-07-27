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
    // Push every render field of an existing item in one call — the symmetric
    // partner of addItem. Keeps add/update field-complete by construction: a
    // field added to Item is wired into both here and addItem, side by side,
    // instead of being easy to forget in a caller's hand-rolled setter loop.
    void updateItem(uint32_t id, const Item& item);
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

    // Which field a caller-facing edit touches; addCommand() folds it into the
    // target id's PendingItemState.
    enum class CommandType {
        Add,
        Remove,
        UpdateGeometry,
        UpdateTexture,
        UpdateMaterial,
        UpdateUniformBlock,
        UpdateModelMatrix
    };

    // The coalesced pending state for one item id. Keying the queue by id makes
    // "one command per item per field" a data-structure invariant rather than
    // logic maintained by hand: repeated edits between frames land on the same
    // entry (last-writer-wins), so the queue stays bounded by the number of live
    // items regardless of render cadence — the fix for the issue #629 leak, where
    // editing while the 3D view was hidden stacked a full-mesh payload per edit.
    struct PendingItemState {
        enum class Lifecycle {
            Update, // item already on the render thread; apply the dirty fields
            Add,    // create a new render item from payload
            Remove  // tear the render item down
        };
        Lifecycle lifecycle = Lifecycle::Update;

        // Which payload fields an Update touched since the last sync. Ignored for
        // Add (which uses the whole payload) and Remove (which uses none).
        bool geometryDirty = false;
        bool textureDirty = false;
        bool materialDirty = false;
        bool uniformBlockDirty = false;
        bool modelMatrixDirty = false;

        ItemPayload payload;
    };

    QHash<uint32_t, PendingItemState> m_pendingChanges;

    // Simple ID generator for items
    uint32_t m_nextId = 1;
    QHash<uint32_t, Item> m_frontState;

    void addCommand(CommandType type, uint32_t id, const ItemPayload& payload);

    // Publish one item's effective sub-item visibility: authored visibility
    // ANDed with its pick-ready gate, so an item stays hidden until its
    // sub-BVH publishes (issue #505 Phase 4).
    void publishItemVisibility(uint32_t id, bool authoredVisible);

    friend class cwRhiTexturedItems;
};


#endif // CWRENDERTEXTUREDITEMS_H
