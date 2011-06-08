//Our include
#include "cwShaderDebugger.h"
#include "cwDebug.h"
#include "cwGLShader.h"

cwShaderDebugger::cwShaderDebugger(QObject *parent) :
    QObject(parent),
    Context(NULL)
{
    ShaderSourceWatcher = new QFileSystemWatcher(this);
    connect(ShaderSourceWatcher, SIGNAL(fileChanged(QString)), SLOT(recompileShader(QString)));
}

/**
  \brief Adds the programe for debugging

  The shaders in the program need to be cwGLShader instead of QGLShader for the
  debugging to work correctly
  */
void cwShaderDebugger::addShaderProgram(QGLShaderProgram* program) {
    QList<QGLShader*> shaders = program->shaders();

    //Update the lookup
    foreach(QGLShader* shader, shaders) {
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
        Context->makeCurrent();
    } else {
        if(QGLContext::currentContext() == NULL) {
            qDebug() << "Can't recompileShader: " << path << "because no OpenGL context is current";
        }
    }

    cwGLShader* shader = FileNameToShader.value(path, NULL);
    QGLShaderProgram* program = NULL;
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

    emit shaderReloaded(shader);

}
