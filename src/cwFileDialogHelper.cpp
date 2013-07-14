/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwFileDialogHelper.h"
#include "cwGlobals.h"

//Qt includes
#include <QFileDialog>

cwFileDialogHelper::cwFileDialogHelper(QObject *parent) :
    QObject(parent),
    MultipleFiles(false)
{
}

/**
  \brief Open's a new file dialog
  */
void cwFileDialogHelper::open() {
    QStringList files;
    if(multipleFiles()) {
        files = cwGlobals::openFiles(caption(), filter(), settingKey());
    } else {
        QString file = cwGlobals::openFile(caption(), filter(), settingKey());
        files.append(file);
    }

    if(!files.isEmpty()) {
        emit filesSelected(files);
    }
}

/**
Sets filter
*/
void cwFileDialogHelper::setFilter(QString filter) {
    if(Filter != filter) {
        Filter = filter;
        emit filterChanged();
    }
}

/**
Sets multipleFiles
*/
void cwFileDialogHelper::setMultipleFiles(bool multipleFiles) {
    if(MultipleFiles != multipleFiles) {
        MultipleFiles = multipleFiles;
        emit multipleFilesChanged();
    }
}

/**
Sets settingKey
*/
void cwFileDialogHelper::setSettingKey(QString settingKey) {
    if(SettingKey != settingKey) {
        SettingKey = settingKey;
        emit settingKeyChanged();
    }
}

/**
Sets caption
*/
void cwFileDialogHelper::setCaption(QString caption) {
    if(Caption != caption) {
        Caption = caption;
        emit captionChanged();
    }
}
