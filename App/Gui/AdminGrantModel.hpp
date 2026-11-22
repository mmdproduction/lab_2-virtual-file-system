#pragma once
#include <QAbstractTableModel>
#include <optional>

#include "Project/ProjectService.hpp"
class AdminGrantModel : public QAbstractTableModel {
  ProjectService& project_;
  UserId actor_;
  ArraySequence<AdminGrant> rows_;

 public:
  AdminGrantModel(ProjectService&, UserId, QObject* parent = nullptr);
  void reload();
  std::optional<AdminGrant> row(int) const;
  int rowCount(const QModelIndex& p = {}) const override;
  int columnCount(const QModelIndex& p = {}) const override;
  QVariant data(const QModelIndex&, int role = Qt::DisplayRole) const override;
  QVariant headerData(int, Qt::Orientation,
                      int role = Qt::DisplayRole) const override;
};
