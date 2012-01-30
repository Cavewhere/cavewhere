#ifndef CWFILEDIALOG_H
#define CWFILEDIALOG_H

#include <QFileDialog>

/**
  This helps load a file dialog from QML
  */
class cwFileDialogHelper : public QObject
{
    Q_OBJECT
public:

    explicit cwFileDialogHelper(QObject *parent = 0);

    Q_INVOKABLE void open();


signals:
    void filesSelected ( const QStringList & selected );


public slots:

};

#endif // CWFILEDIALOG_H
