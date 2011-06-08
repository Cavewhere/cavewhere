#ifndef CWGLSHADER_H
#define CWGLSHADER_H

#include <QGLShader>
#include <QFileSystemWatcher>
#include <QString>

class cwGLShader : public QGLShader
{
    Q_OBJECT
public:
    explicit cwGLShader(QGLShader::ShaderType type, QObject * parent = 0);
    cwGLShader ( QGLShader::ShaderType type, const QGLContext * context, QObject * parent = 0);

    void setSourceFile(QString filename);
    QString sourceFile() const;

signals:
    void SourceFileChanged(QString sourceFile);

public slots:

private:
    QString SourceFile;

};

inline QString cwGLShader::sourceFile() const {
    return SourceFile;
}

#endif // CWGLSHADER_H
