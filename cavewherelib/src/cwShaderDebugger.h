/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSHADERDEBUGGER_H
#define CWSHADERDEBUGGER_H

//Qt includes
#include <QObject>
#include <QOpenGLShaderProgram>
#include <QFileSystemWatcher>

//Our includes
class cwGLShader;

class cwShaderDebugger : public QObject
{
    Q_OBJECT
public:
    explicit cwShaderDebugger(QObject *parent = 0);

    void addShaderProgram(QOpenGLShaderProgram* program);

    void setGLContext(QOpenGLContext* widget);
    QOpenGLContext* GLContext() const;

signals:
    void shaderReloaded(cwGLShader* shader); //Called when ever the file has been reloaded

public slots:

private slots:
    void recompileShader(QString path);

private:
    QOpenGLContext* Context;

    //Reverse lookups
    QMap<QString, cwGLShader*> FileNameToShader;
    QMap<cwGLShader*, QOpenGLShaderProgram*> ShaderToShaderProgram;

    QList<QOpenGLShaderProgram*> Programs;

    QFileSystemWatcher* ShaderSourceWatcher;

};

inline void cwShaderDebugger::setGLContext(QOpenGLContext* context) {
    Context = context;
}

inline QOpenGLContext* cwShaderDebugger::GLContext() const {
    return Context;
}

#endif // CWSHADERDEBUGGER_H
