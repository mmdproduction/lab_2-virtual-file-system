#pragma once

#include <QDialog>

#include "Gui/AccountListModel.hpp"
#include "Gui/GroupPolicyModel.hpp"

class QCheckBox;
class QLabel;
class QListView;
class QPushButton;

class AdminDialog : public QDialog {
public:
    AdminDialog(UserManager& users, VirtualFileSystem& vfs, NodeId node_id,
                 UserId actor_id, QWidget* parent = nullptr);
    void set_change_handler(std::function<void()> handler);

private:
    UserId selected_user() const;
    GroupId selected_group() const;
    void refresh(UserId user_id, GroupId group_id);
    void update_user_details();
    void edit_account(bool group, bool creating);
    void apply_policy();

    std::function<void()> change_handler_;
    UserManager& users_;
    VirtualFileSystem& vfs_;
    NodeId node_id_;
    UserId actor_id_;
    AccountListModel* user_model_ = nullptr;
    AccountListModel* group_model_ = nullptr;
    GroupPolicyModel* policy_model_ = nullptr;
    QListView* user_list_ = nullptr;
    QListView* group_list_ = nullptr;
    QLabel* user_details_ = nullptr;
    QLabel* group_title_ = nullptr;
    QLabel* empty_groups_ = nullptr;
    QLabel* account_status_ = nullptr;
    QLabel* policy_status_ = nullptr;
    QCheckBox* inherit_ = nullptr;
    QPushButton* rename_user_ = nullptr;
    QPushButton* rename_group_ = nullptr;
};
