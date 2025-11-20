#ifndef CWABSTRACTSCRAPVIEWMATRIX_H
#define CWABSTRACTSCRAPVIEWMATRIX_H

//Our includes
#include "cwGlobals.h"
#include "cwScrapType.h"

//Qt includes
#include <QObject>
#include <QMatrix4x4>
#include <QQmlEngine>

class CAVEWHERE_LIB_EXPORT cwAbstractScrapViewMatrix : public QObject {

    Q_OBJECT
    QML_NAMED_ELEMENT(AbstractScrapViewMatrix)
    QML_UNCREATABLE("Abstract class")

    Q_PROPERTY(QMatrix4x4 matrix READ matrix NOTIFY matrixChanged)
    Q_PROPERTY(Type type READ type CONSTANT)

public:
    enum Type {
        Plan = cwScrapType::Plan,
        RunningProfile = cwScrapType::RunningProfile,
        ProjectedProfile = cwScrapType::ProjectedProfile
    };
    Q_ENUM(Type)

    class Data {
    public:
        Data() = default;
        virtual ~Data() = default;
        virtual QMatrix4x4 matrix() const = 0;
        virtual Data* clone() const = 0;
        virtual Type type() const = 0;
    };

    cwAbstractScrapViewMatrix(QObject* parent = nullptr) :
        QObject(parent)
    {

    }

    QMatrix4x4 matrix() const {
        return  m_data->matrix();
    }

    virtual const cwAbstractScrapViewMatrix::Data* data() const {
        return m_data.get();
    }

    void setData(cwAbstractScrapViewMatrix::Data* data) {
        m_data.reset(data);
        emit matrixChanged();
    }

    virtual cwAbstractScrapViewMatrix* clone() const = 0;

    Type type() const {
        return m_data->type();
    }

signals:
    void matrixChanged();

protected:
    cwAbstractScrapViewMatrix(QObject* parent, Data* data) :
        QObject(parent),
        m_data(data)
    {

    }

    virtual cwAbstractScrapViewMatrix::Data* d() const {
        return m_data.get();
    }

private:
    std::unique_ptr<Data> m_data;

};

#endif // CWABSTRACTSCRAPVIEWMATRIX_H
