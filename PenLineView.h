#ifndef PENLINEVIEW_H
#define PENLINEVIEW_H

//Qt includes
#include <QQuickItem>

//Our includes
#include <PenLineModel.h>

class PenLineView : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(PenLineModel* model READ model WRITE setModel NOTIFY modelChanged)

public:
    explicit PenLineView(QQuickItem* parent = nullptr);
    ~PenLineView();

    PenLineModel* model() const;
    void setModel(PenLineModel* model);

signals:
    void modelChanged();

protected:

private:
    QPointer<PenLineModel> m_model;
    // Using QQuickShape from Qt’s private API for custom drawing.
    class QQuickShape* m_shape;
};

#endif // PENLINEVIEW_H
