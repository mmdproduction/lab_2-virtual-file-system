#pragma once
#include <QAbstractTableModel>
#include <optional>

#include "Project/ProjectService.hpp"
class AccessRuleModel : public QAbstractTableModel {
  ProjectService& project_;
  UserId actor_;
  NodeId node_;
  ArraySequence<RuleView> rows_;

 public:
  AccessRuleModel(ProjectService&, NodeId, UserId, QObject* parent = nullptr);
  void reload(NodeId);
  std::optional<RuleView> row(int) const;
  int rowCount(const QModelIndex& p = {}) const override;
  int columnCount(const QModelIndex& p = {}) const override;
  QVariant data(const QModelIndex&, int role = Qt::DisplayRole) const override;
  QVariant headerData(int, Qt::Orientation,
                      int role = Qt::DisplayRole) const override;
};
