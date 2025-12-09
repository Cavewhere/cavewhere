#ifndef CWRENDERTEXTUREDITEMS_H
#define CWRENDERTEXTUREDITEMS_H

#include "cwRenderMaterialState.h"
#include "cwRenderObject.h"
#include "cwGeometry.h"
#include <QHash>
#include <QByteArray>
#include <QtGui/qimage.h>
#include <QMatrix4x4>

class cwRenderTexturedItems : public cwRenderObject
{
    Q_OBJECT
    Q_PROPERTY(double gridAzimuth READ gridAzimuth WRITE setGridAzimuth NOTIFY gridAzimuthChanged FINAL)
    Q_PROPERTY(double gridPitch READ gridPitch WRITE setGridPitch NOTIFY gridPitchChanged FINAL)

public:
    using CullMode = cwRenderMaterialState::CullMode;

    cwRenderTexturedItems(QObject* parent = nullptr);

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
    void setVisible(uint32_t id, bool visible);
    void setCulling(uint32_t id, CullMode culling);
    void setMaterial(uint32_t id, const cwRenderMaterialState& material);
    void setUniformBlock(uint32_t id, const QByteArray& uniformBlock);
    void setModelMatrix(uint32_t id, const QMatrix4x4& modelMatrix);
    void removeItem(uint32_t id);

    double gridAzimuth() const { return m_gridAzimuth; }
    void setGridAzimuth(double azimuth);

    double gridPitch() const { return m_gridPitch; }
    void setGridPitch(double pitch);

    //For testing
    Item item(uint32_t id) const;
    bool hasItem(uint32_t id) const;


signals:
    void gridAzimuthChanged();
    void gridPitchChanged();

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
            UpdateMaterial,
            UpdateUniformBlock,
            UpdateVisiblity,
            UpdateModelMatrix,
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
        Item m_item;

        Type m_commandType;
        uint32_t m_id = 0;

    };

    QVector<PendingCommand> m_pendingChanges;

    // Simple ID generator for items
    uint32_t m_nextId = 1;
    QHash<uint32_t, Item> m_frontState;

    void addCommand(const PendingCommand &&command);
    QByteArray buildGridUniformBlock() const;
    void updateGridUniforms();

    double m_gridAzimuth = 0.0;
    double m_gridPitch = -70.0;

    friend class cwRhiTexturedItems;
};


#endif // CWRENDERTEXTUREDITEMS_H
