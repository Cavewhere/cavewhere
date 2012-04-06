#ifndef CWGLSHADER_H
#define CWGLSHADER_H

#include <QOpenGLShader>
#include <QFileSystemWatcher>
#include <QString>

class cwGLShader : public QOpenGLShader
{
    Q_OBJECT
public:
    explicit cwGLShader(QOpenGLShader::ShaderType type, QObject * parent = 0);

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
