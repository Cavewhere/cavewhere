#ifndef CWFILEDIALOG_H
#define CWFILEDIALOG_H

#include <QFileDialog>

/**
  This helps load a file dialog from QML
  */
class cwFileDialogHelper : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(QString settingKey READ settingKey WRITE setSettingKey NOTIFY settingKeyChanged)
    Q_PROPERTY(bool multipleFiles READ multipleFiles WRITE setMultipleFiles NOTIFY multipleFilesChanged)
    Q_PROPERTY(QString caption READ caption WRITE setCaption NOTIFY captionChanged)

public:

    explicit cwFileDialogHelper(QObject *parent = 0);

    Q_INVOKABLE void open();

    QString filter() const;
    void setFilter(QString filter);

    QString settingKey() const;
    void setSettingKey(QString settingKey);

    bool multipleFiles() const;
    void setMultipleFiles(bool multipleFiles);

    QString caption() const;
    void setCaption(QString caption);

signals:
    void filesSelected (QStringList selected );
    void filterChanged();
    void settingKeyChanged();
    void multipleFilesChanged();
    void captionChanged();

public slots:

private:
    QString Filter; //!<
    QString SettingKey; //!<
    bool MultipleFiles; //!<   -
    QString Caption; //!<
};

/**
Gets filter
*/
inline QString cwFileDialogHelper::filter() const {
    return Filter;
}

/**
Gets multipleFiles
*/
inline bool cwFileDialogHelper::multipleFiles() const {
    return MultipleFiles;
}

/**
Gets settingKey
*/
inline QString cwFileDialogHelper::settingKey() const {
    return SettingKey;
}

/**
Gets caption
*/
inline QString cwFileDialogHelper::caption() const {
    return Caption;
}
#endif // CWFILEDIALOG_H
