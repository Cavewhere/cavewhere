// MovingAverageProxyModel.h
#ifndef MOVINGAVERAGEPROXYMODEL_H
#define MOVINGAVERAGEPROXYMODEL_H

//Qt includes
#include <QIdentityProxyModel>
#include <QQmlEngine>
#include <QObjectBindableProperty>

class MovingAverageProxyModel:public QIdentityProxyModel{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int windowSize READ windowSize WRITE setWindowSize NOTIFY windowSizeChanged BINDABLE bindableWindowSize)

public:
    explicit MovingAverageProxyModel(QObject* parent=nullptr);

    int windowSize() const { return m_windowSize.value(); }
    void setWindowSize(int size) {  m_windowSize = size; }
    QBindable<int> bindableWindowSize() { return &m_windowSize; }

    QVariant data(const QModelIndex &idx, int role) const;

signals:
    void windowSizeChanged();

private:
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(MovingAverageProxyModel, int, m_windowSize, 1, &MovingAverageProxyModel::windowSizeChanged);
};
#endif//MOVINGAVERAGEPROXYMODEL_H
