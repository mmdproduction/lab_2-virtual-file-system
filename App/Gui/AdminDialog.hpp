#pragma once
#include <QDialog>
#include <functional>

#include "Project/ProjectService.hpp"
class QTreeWidget;
class QComboBox;
class QLineEdit;
class QCheckBox;
class QPushButton;
class QLabel;
class QTableView;
class QTabWidget;
class QSpinBox;
class AccessRuleModel;
class AdminGrantModel;
class AccountListModel;
class UserChoiceModel;
class AdminDialog : public QDialog {
  ProjectService& project_;
  NodeId node_id_;
  UserId actor_id_;
  AccessRuleId editing_ = {};
  std::function<void()> change_handler_;
  QTreeWidget* scopes_;
  QTabWidget* tabs_;
  QLabel *path_, *status_, *effective_, *rule_hint_;
  QTableView *rules_, *grants_;
  AccessRuleModel* rule_model_;
  AdminGrantModel* grant_model_;
  QComboBox *kind_, *principal_, *read_, *write_, *rule_source_,
      *effective_user_, *grant_user_, *grant_source_;
  QLineEdit* search_;
  QCheckBox* inherit_;
  QPushButton *apply_, *remove_, *inherit_apply_, *issue_, *revoke_;
  QCheckBox* capabilities_[4]{};
  UserChoiceModel* account_users_ = nullptr;
  QComboBox *account_user_ = nullptr, *account_group_ = nullptr,
            *role_ = nullptr;
  QSpinBox* level_ = nullptr;
  AccountListModel* memberships_ = nullptr;
  void fill_tree();
  void refresh();
  void fill_principals();
  void new_rule();
  void select_rule();
  void apply_rule();
  void delete_rule();
  void effective();
  void fill_sources(QComboBox*, AdminCapability);
  void grant_options();
  void issue_grant();
  void revoke_grant();
  QWidget* accounts_page();
  void reload_accounts();
  void account_details();
  void create_account(bool);
  void rename_account(bool);
  void apply_role();
  void changed(std::uint64_t before);
  void error(const std::exception&);

 public:
  AdminDialog(ProjectService&, NodeId, UserId, QWidget* parent = nullptr);
  void set_change_handler(std::function<void()>);
};
