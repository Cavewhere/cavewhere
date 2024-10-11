#ifndef CWABSTRACTSCRAPVIEWMATRIX_H
#define CWABSTRACTSCRAPVIEWMATRIX_H

//Our includes
#include "cwGlobals.h"
#include "cwScrap.h"

//Qt includes
#include <QObject>
#include <QMatrix4x4>

class CAVEWHERE_LIB_EXPORT cwAbstractScrapViewMatrix : public QObject {

    Q_OBJECT

    Q_PROPERTY(QMatrix4x4 matrix READ matrix NOTIFY matrixChanged)
    Q_PROPERTY(cwScrap::ScrapType type READ type CONSTANT)

public:
    class Data {
    public:
        Data() = default;
        virtual ~Data() = default;
        virtual QMatrix4x4 matrix() const = 0;
        virtual Data* clone() const = 0;
        virtual cwScrap::ScrapType type() const = 0;
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

    virtual cwAbstractScrapViewMatrix* clone() const = 0;

    cwScrap::ScrapType type() const {
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
