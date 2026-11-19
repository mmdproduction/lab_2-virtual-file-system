#pragma once
#include <QAbstractTableModel>
#include <QString>
#include "VirtualFileSystem/VirtualFileSystem.hpp"

class FileSearchModel : public QAbstractTableModel {
public:
    FileSearchModel(const VirtualFileSystem& vfs, const User& user, QObject* parent = nullptr);
    void search(const QString& name);
    NodeId node_id(const QModelIndex& index) const noexcept;
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
private:
    struct Result { NodeId id = kInvalidNodeId; QString name; QString path; FileIcon icon = FileIcon::Default; };
    const VirtualFileSystem& vfs_;
    const User& user_;
    ArraySequence<Result> results_;
};
