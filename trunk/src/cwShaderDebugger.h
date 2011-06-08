#ifndef CWSHADERDEBUGGER_H
#define CWSHADERDEBUGGER_H

//Qt includes
#include <QObject>
#include <QGLShaderProgram>
#include <QFileSystemWatcher>

//Our includes
class cwGLShader;

class cwShaderDebugger : public QObject
{
    Q_OBJECT
public:
    explicit cwShaderDebugger(QObject *parent = 0);

    void addShaderProgram(QGLShaderProgram* program);

    void setGLContext(QGLContext* widget);
    QGLContext* GLContext() const;

signals:
    void shaderReloaded(cwGLShader* shader); //Called when ever the file has been reloaded

public slots:

private slots:
    void recompileShader(QString path);

private:
    QGLContext* Context;

    //Reverse lookups
    QMap<QString, cwGLShader*> FileNameToShader;
    QMap<cwGLShader*, QGLShaderProgram*> ShaderToShaderProgram;

    QList<QGLShaderProgram*> Programs;

    QFileSystemWatcher* ShaderSourceWatcher;

};

inline void cwShaderDebugger::setGLContext(QGLContext* context) {
    Context = context;
}

inline QGLContext* cwShaderDebugger::GLContext() const {
    return Context;
}

#endif // CWSHADERDEBUGGER_H
