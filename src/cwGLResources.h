#ifndef CWGLRESOURCES_H
#define CWGLRESOURCES_H

//Qt includes
#include <QObject>
#include <QOpenGLContext>

class cwGLResources : public QObject
{
    Q_OBJECT
public:
    explicit cwGLResources(QObject *parent = 0);

    void setContext(QOpenGLContext *context);
    QOpenGLContext *context() const;

signals:
    
public slots:
    
private:
    QOpenGLContext* Context;

};

#endif // CWGLRESOURCES_H
