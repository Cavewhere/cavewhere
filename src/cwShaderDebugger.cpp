//Our include
#include "cwShaderDebugger.h"
#include "cwDebug.h"
#include "cwGLShader.h"

//Qt includes
#include <QOpenGLContext>

cwShaderDebugger::cwShaderDebugger(QObject *parent) :
    QObject(parent),
    Context(NULL)
{
    ShaderSourceWatcher = new QFileSystemWatcher(this);
    connect(ShaderSourceWatcher, SIGNAL(fileChanged(QString)), SLOT(recompileShader(QString)));
}

/**
  \brief Adds the programe for debugging

  The shaders in the program need to be cwGLShader instead of QOpenGLShader for the
  debugging to work correctly
  */
void cwShaderDebugger::addShaderProgram(QOpenGLShaderProgram* program) {
    QList<QOpenGLShader*> shaders = program->shaders();

    //Update the lookup
    foreach(QOpenGLShader* shader, shaders) {
        cwGLShader* ourShader = qobject_cast<cwGLShader*>(shader);
        if(ourShader == NULL) {
            qDebug() << "Can't debug shader " << shader << "because it's not a cwGLShader" << LOCATION;
            continue;
        }

        QString shaderSource = ourShader->sourceFile();
        if(!shaderSource.isEmpty()) {
            FileNameToShader[shaderSource] = ourShader;

            //Add the file to the watcher
            ShaderSourceWatcher->addPath(shaderSource);
        }

        ShaderToShaderProgram[ourShader] = program;
    }
}

/**
  \brief Recompiles the shader at path
  */
void cwShaderDebugger::recompileShader(QString path) {

    if(Context != NULL) {
        Context->makeCurrent(Context->surface());
    } else {
        if(QOpenGLContext::currentContext() == NULL) {
            qDebug() << "Can't recompileShader: " << path << "because no OpenGL context is current";
        }
    }

   // qDebug() << "Sauce:" << ShaderSourceWatcher->files();

    cwGLShader* shader = FileNameToShader.value(path, NULL);
    QOpenGLShaderProgram* program = NULL;
    if(shader != NULL) {
        program = ShaderToShaderProgram.value(shader, NULL);
    }

    if(shader == NULL || program == NULL) {
        qDebug() << "Can't file shader or program in the debugger" << LOCATION;
    }

    //Recompile the shader and relink the programe
    bool shaderCompiled = shader->compileSourceFile(shader->sourceFile());
    bool programLinked = program->link();

    qDebug() << "Compiling shader" << qPrintable(shader->sourceFile());
    if(shaderCompiled) {
        qDebug() << "\t Successful.";
    } else {
        qDebug() << "\t" << qPrintable(shader->log());
    }

    qDebug() << "Linking program";
    if(programLinked) {
        qDebug() << "\t Successful.";
    } else {
        qDebug() << "\t" << qPrintable(program->log());
    }

    //There's a bug in the Qfilesystemwatcher, this is a work around
    ShaderSourceWatcher->removePath(path);
    ShaderSourceWatcher->addPath(path);

    emit shaderReloaded(shader);

}
