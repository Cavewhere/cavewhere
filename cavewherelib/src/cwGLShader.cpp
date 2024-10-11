/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwGLShader.h"

//Qt includes
#include <QFileInfo>
#include <QFile>

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

        QFile file(filename);
        bool canOpen = file.open(QFile::ReadOnly);
        if(!canOpen) {
            qDebug() << "Can't open shader: " << qPrintable(filename);
        }

        //Read the shader code from the file
        QString fileContent = file.readAll();

        //Add defines to the shader
        foreach(QString define, Defines) {
            fileContent.prepend(QString("#define %1\n").arg(define));
        }

        bool compilingError = compileSourceCode(fileContent);
        if(!compilingError) {
            qDebug() << "Compiling error in " << qPrintable(filename) << log();
        }

        emit SourceFileChanged(SourceFile);
    }
}

/**
 * @brief cwGLShader::addDefine
 * @param define
 *
 * This adds a #define to the shader, such that macros work
 * in the shader
 */
void cwGLShader::addDefine(QString define)
{
    Defines.append(define);
}
