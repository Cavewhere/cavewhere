//Our includes
#include "cwGLShader.h"

//Qt includes
#include <QFileInfo>

cwGLShader::cwGLShader(QGLShader::ShaderType type, QObject *parent) :
    QGLShader(type, parent)
{
}

cwGLShader::cwGLShader(QGLShader::ShaderType type, const QGLContext * context, QObject * parent):
    QGLShader(type, context, parent)
{

}

/**
  \brief Sets the shader's source file

  This will watch if the shader files source code has changed.  If it has,
  then it'll automatically try to recompile the shader
  */
void cwGLShader::setSourceFile(QString filename) {
    if(filename != SourceFile) {
        SourceFile = filename;
        bool compilingError = compileSourceFile(filename);
        if(!compilingError) {
            qDebug() << "Compiling error in " << qPrintable(filename) << log();
        }

        emit SourceFileChanged(SourceFile);
    }
}
