#pragma once

#include <QDialog>

#include "VirtualFileSystem/FSNode.hpp"

class QButtonGroup;

class FileIconDialog : public QDialog {
public:
    explicit FileIconDialog(FileIcon current, QWidget* parent = nullptr);
    FileIcon selected_icon() const;
private:
    QButtonGroup* choices_ = nullptr;
};
