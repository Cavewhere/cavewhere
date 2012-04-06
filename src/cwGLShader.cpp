//Our includes
#include "cwGLShader.h"

//Qt includes
#include <QFileInfo>

cwGLShader::cwGLShader(QOpenGLShader::ShaderType type, QObject *parent) :
    QOpenGLShader(type, parent)
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
