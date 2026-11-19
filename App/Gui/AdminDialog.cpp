#include "AdminDialog.hpp"

#include <QCheckBox>
#include <QHeaderView>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QPushButton>
#include <QTableView>
#include <QTabWidget>
#include <QVBoxLayout>

#include <stdexcept>
#include <utility>

#include "Gui/UiTheme.hpp"

AdminDialog::AdminDialog(UserManager& users, VirtualFileSystem& vfs, NodeId node,
                         UserId actor, QWidget* parent)
    : QDialog(parent), users_(users), vfs_(vfs), node_id_(node), actor_id_(actor) {
    if (!users_.get_user(actor_id_).is_admin) {
        throw std::runtime_error("Administrator access required");
    }
    const FSNode& target = vfs_.get_node(node_id_, users_.get_user(actor_id_));
    setWindowTitle("Администрирование");
    resize(1000, 690);
    setMinimumSize(860, 580);
    setPalette(ui::DarkPalette());
    setStyleSheet(ui::DarkStyleSheet());
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 22, 24, 20);
    layout->setSpacing(16);
    auto* title = new QLabel("Администрирование", this);
    title->setObjectName("dialog_title");
    layout->addWidget(title);
    auto* subtitle = new QLabel("Пользователи, группы и доступ к объектам", this);
    subtitle->setObjectName("subtitle");
    layout->addWidget(subtitle);
    auto* tabs = new QTabWidget(this);
    tabs->setObjectName("admin_tabs");
    layout->addWidget(tabs, 1);

    user_model_ = new AccountListModel(users_, actor_id_, AccountListModel::Kind::Users, this);
    group_model_ = new AccountListModel(users_, actor_id_, AccountListModel::Kind::Groups, this);
    policy_model_ = new GroupPolicyModel(users_, vfs_, node_id_, actor_id_, this);
    auto* accounts = new QWidget(tabs);
    auto* accounts_layout = new QVBoxLayout(accounts);
    accounts_layout->setContentsMargins(16, 18, 16, 12);
    accounts_layout->setSpacing(12);
    auto* columns = new QGridLayout;
    columns->setSpacing(18);
    columns->addWidget(new QLabel("Пользователи", accounts), 0, 0);
    group_title_ = new QLabel("Группы", accounts);
    columns->addWidget(group_title_, 0, 1);
    user_list_ = new QListView(accounts);
    user_list_->setObjectName("users_list");
    user_list_->setModel(user_model_);
    user_list_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    user_list_->setIconSize(QSize(20, 20));
    group_list_ = new QListView(accounts);
    group_list_->setObjectName("groups_list");
    group_list_->setModel(group_model_);
    group_list_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    group_list_->setIconSize(QSize(20, 20));
    columns->addWidget(user_list_, 1, 0);
    columns->addWidget(group_list_, 1, 1);
    columns->setRowStretch(1, 1);
    columns->setColumnStretch(0, 1);
    columns->setColumnStretch(1, 1);
    auto* user_buttons = new QHBoxLayout;
    auto* group_buttons = new QHBoxLayout;
    auto* create_user = new QPushButton("Создать", accounts);
    create_user->setObjectName("create_user_button");
    create_user->setIcon(ui::LineIcon(ui::Icon::Plus));
    rename_user_ = new QPushButton("Переименовать", accounts);
    rename_user_->setObjectName("rename_user_button");
    auto* create_group = new QPushButton("Создать", accounts);
    create_group->setObjectName("create_group_button");
    create_group->setIcon(ui::LineIcon(ui::Icon::Plus));
    rename_group_ = new QPushButton("Переименовать", accounts);
    rename_group_->setObjectName("rename_group_button");
    user_buttons->addWidget(create_user);
    user_buttons->addWidget(rename_user_);
    group_buttons->addWidget(create_group);
    group_buttons->addWidget(rename_group_);
    columns->addLayout(user_buttons, 2, 0);
    columns->addLayout(group_buttons, 2, 1);
    user_details_ = new QLabel(accounts);
    user_details_->setObjectName("muted");
    user_details_->setWordWrap(true);
    columns->addWidget(user_details_, 3, 0);
    empty_groups_ = new QLabel("Создайте группу, чтобы назначить ей пользователей.", accounts);
    empty_groups_->setObjectName("muted");
    empty_groups_->setWordWrap(true);
    columns->addWidget(empty_groups_, 3, 1);
    accounts_layout->addLayout(columns, 1);
    auto* membership_note = new QLabel("Флажок у группы включает выбранного пользователя в её состав. Изменения применяются сразу.", accounts);
    membership_note->setObjectName("muted");
    membership_note->setWordWrap(true);
    accounts_layout->addWidget(membership_note);
    account_status_ = new QLabel(accounts);
    account_status_->setObjectName("muted");
    account_status_->setTextFormat(Qt::PlainText);
    accounts_layout->addWidget(account_status_);
    tabs->addTab(accounts, "Пользователи и группы");

    auto* permissions = new QWidget(tabs);
    auto* permissions_layout = new QVBoxLayout(permissions);
    permissions_layout->setContentsMargins(16, 18, 16, 12);
    permissions_layout->setSpacing(14);
    auto* object = new QLabel(QString("%1: %2  ·  #%3")
        .arg(target.type == FSType::Folder ? "Папка" : "Файл")
        .arg(QString::fromStdString(target.name)).arg(static_cast<qulonglong>(node_id_)), permissions);
    object->setTextFormat(Qt::PlainText);
    permissions_layout->addWidget(object);
    inherit_ = new QCheckBox("Наследовать разрешения родительской папки", permissions);
    inherit_->setObjectName("inherit_policy");
    inherit_->setChecked(policy_model_->inherits());
    inherit_->setEnabled(node_id_ != vfs_.root_id());
    permissions_layout->addWidget(inherit_);
    auto* table = new QTableView(permissions);
    table->setObjectName("group_policy_table");
    table->setModel(policy_model_);
    table->setShowGrid(false);
    table->setSelectionBehavior(QAbstractItemView::SelectItems);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->hide();
    table->verticalHeader()->setDefaultSectionSize(48);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    permissions_layout->addWidget(table, 1);
    auto* note = new QLabel("Здесь показаны локальные разрешения. Снятый флажок не запрещает доступ, полученный через другую группу или наследование. Чтение и запись назначаются независимо.", permissions);
    note->setObjectName("muted");
    note->setWordWrap(true);
    permissions_layout->addWidget(note);
    auto* policy_footer = new QHBoxLayout;
    policy_status_ = new QLabel("Изменения прав сохраняются кнопкой «Применить».", permissions);
    policy_status_->setObjectName("muted");
    policy_status_->setWordWrap(true);
    policy_footer->addWidget(policy_status_, 1);
    auto* apply = new QPushButton("Применить", permissions);
    apply->setObjectName("apply_policy_button");
    policy_footer->addWidget(apply);
    permissions_layout->addLayout(policy_footer);
    tabs->addTab(permissions, "Права доступа");
    auto* footer = new QHBoxLayout;
    footer->addStretch();
    auto* close = new QPushButton("Закрыть", this);
    footer->addWidget(close);
    layout->addLayout(footer);

    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    connect(create_user, &QPushButton::clicked, this, [this] { edit_account(false, true); });
    connect(rename_user_, &QPushButton::clicked, this, [this] { edit_account(false, false); });
    connect(create_group, &QPushButton::clicked, this, [this] { edit_account(true, true); });
    connect(rename_group_, &QPushButton::clicked, this, [this] { edit_account(true, false); });
    connect(apply, &QPushButton::clicked, this, [this] { apply_policy(); });
    connect(user_list_->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this] { group_model_->set_selected_user(selected_user()); update_user_details(); });
    connect(group_list_->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this] { rename_group_->setEnabled(selected_group() != kInvalidGroupId); });
    connect(group_model_, &QAbstractItemModel::dataChanged, this, [this] { update_user_details(); });
    connect(policy_model_, &QAbstractItemModel::dataChanged, this, [this] {
        policy_status_->setText("Есть несохранённые изменения прав.");
    });
    connect(inherit_, &QCheckBox::toggled, this, [this] {
        policy_status_->setText("Есть несохранённые изменения прав.");
    });
    group_model_->set_error_handler([this](const QString& error) {
        QMessageBox::warning(this, "Не удалось изменить состав группы", error);
    });
    group_model_->set_change_handler([this] { if (change_handler_) change_handler_(); });
    refresh(actor_id_, kInvalidGroupId);
}

UserId AdminDialog::selected_user() const { return user_model_->id_at(user_list_->currentIndex()); }
GroupId AdminDialog::selected_group() const { return group_model_->id_at(group_list_->currentIndex()); }

void AdminDialog::refresh(UserId user_id, GroupId group_id) {
    user_model_->reload();
    group_model_->reload();
    policy_model_->reload_groups();
    QModelIndex user = user_model_->index_for_id(user_id);
    if (!user.isValid() && user_model_->rowCount() != 0) user = user_model_->index(0);
    user_list_->setCurrentIndex(user);
    group_model_->set_selected_user(selected_user());
    group_list_->setCurrentIndex(group_model_->index_for_id(group_id));
    rename_group_->setEnabled(selected_group() != kInvalidGroupId);
    empty_groups_->setVisible(group_model_->rowCount() == 0);
    update_user_details();
}

void AdminDialog::update_user_details() {
    const UserId id = selected_user();
    rename_user_->setEnabled(id != kInvalidUserId);
    if (id == kInvalidUserId) {
        user_details_->clear();
        group_title_->setText("Группы");
        return;
    }
    const User& user = users_.get_user(id);
    user_details_->setText(QString("#%1 · %2 · Групп: %3")
        .arg(static_cast<qulonglong>(id))
        .arg(user.is_admin ? "Администратор" : "Пользователь")
        .arg(static_cast<qulonglong>(user.group_ids.size())));
    group_title_->setTextFormat(Qt::PlainText);
    group_title_->setText("Группы для " + QString::fromStdString(user.name));
}

void AdminDialog::edit_account(bool group, bool creating) {
    UserId user_id = selected_user();
    GroupId group_id = selected_group();
    if (!creating && (group ? group_id == kInvalidGroupId : user_id == kInvalidUserId)) return;
    const QString existing = creating ? QString() : QString::fromStdString(
        group ? users_.get_group(group_id).name : users_.get_user(user_id).name);
    bool accepted = false;
    const QString name = QInputDialog::getText(this,
        creating ? (group ? "Новая группа" : "Новый пользователь") : "Переименование",
        "Имя:", QLineEdit::Normal, existing, &accepted);
    if (!accepted) return;
    try {
        const std::string value = name.toUtf8().toStdString();
        if (group) {
            if (creating) group_id = users_.create_group(value, actor_id_);
            else users_.rename_group(group_id, value, actor_id_);
        } else {
            if (creating) user_id = users_.create_user(value, actor_id_);
            else users_.rename_user(user_id, value, actor_id_);
        }
        if (change_handler_) change_handler_();
        refresh(user_id, group_id);
        account_status_->setText(creating ? "Создано: " + name : "Имя изменено: " + name);
    } catch (const std::exception& error) {
        QMessageBox::warning(this, "Не удалось изменить настройки", QString::fromUtf8(error.what()));
    }
}

void AdminDialog::apply_policy() {
    try {
        policy_model_->apply(inherit_->isChecked());
        if (change_handler_) change_handler_();
        policy_status_->setText("Права доступа сохранены.");
    } catch (const std::exception& error) {
        QMessageBox::warning(this, "Не удалось сохранить права", QString::fromUtf8(error.what()));
    }
}

void AdminDialog::set_change_handler(std::function<void()> handler) { change_handler_ = std::move(handler); }
