#ifndef CWGLSHADER_H
#define CWGLSHADER_H

#include <QOpenGLShader>
#include <QFileSystemWatcher>
#include <QString>
#include <QStringList>

class cwGLShader : public QOpenGLShader
{
    Q_OBJECT
public:
    explicit cwGLShader(QOpenGLShader::ShaderType type, QObject * parent = 0);

    void setSourceFile(QString filename);
    QString sourceFile() const;

    void addDefine(QString define);


signals:
    void SourceFileChanged(QString sourceFile);

public slots:

private:
    QString SourceFile;
    QStringList Defines;

};

inline QString cwGLShader::sourceFile() const {
    return SourceFile;
}



#endif // CWGLSHADER_H
