#include "PenLineView.h"

// Include the private API header for QQuickShape.
#include <QtQuickShapes/private/qquickshape_p.h>

PenLineView::PenLineView(QQuickItem* parent)
    : QQuickItem(parent), m_model(nullptr)
{
    // Create QQuickShape instance (using the private API).
    m_shape = new QQuickShape(this);
    // Enable the item to have its own scene graph content.
    setFlag(ItemHasContents, true);
}

PenLineView::~PenLineView() {
    // m_shape is a child of this item and will be deleted automatically.
}

PenLineModel* PenLineView::model() const {
    return m_model;
}

void PenLineView::setModel(PenLineModel* model) {
    if (m_model == model) {
        return;
    }
    m_model = model;

    if(m_model) {
        connect(m_model, &PenLineModel::rowsInserted, this, [this](const QModelIndex& parent, int begin, int end){

        });
        connect(m_model, &PenLineModel::dataChanged, this, [this](const QModelIndex& begin, const QModelIndex& end, const QVector<int>& roles) {

        });
    }

    emit modelChanged();
}
