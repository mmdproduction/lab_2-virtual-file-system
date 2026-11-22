#include "GuidQt.hpp"
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

#include "AccountListModel.hpp"
#include "AdminDialog.hpp"
#include "PermissionLabels.hpp"
#include "UserChoiceModel.hpp"
QWidget* AdminDialog::accounts_page() {
  auto* page = new QWidget;
  auto* l = new QVBoxLayout(page);
  auto* hint = new QLabel(
      "Учётные записи и членство в группах общие для проекта. Менять их может "
      "только суперадминистратор.",
      page);
  hint->setWordWrap(true);
  l->addWidget(hint);
  account_users_ = new UserChoiceModel(project_, this);
  account_user_ = new QComboBox(page);
  account_user_->setObjectName("account_user");
  account_user_->setModel(account_users_);
  auto* uf = new QFormLayout;
  uf->addRow("Пользователь", account_user_);
  l->addLayout(uf);
  auto* user_actions = new QHBoxLayout;
  auto* create = new QPushButton("Создать пользователя…", page);
  create->setObjectName("create_user");
  auto* rename = new QPushButton("Переименовать…", page);
  rename->setObjectName("rename_user");
  for (auto* b : {create, rename}) b->setAutoDefault(false);
  user_actions->addWidget(create);
  user_actions->addWidget(rename);
  user_actions->addStretch();
  l->addLayout(user_actions);
  auto* rf = new QHBoxLayout;
  role_ = new QComboBox(page);
  role_->setObjectName("account_role");
  role_->addItems({"Пользователь", "Администратор"});
  level_ = new QSpinBox(page);
  level_->setObjectName("admin_level");
  level_->setRange(1, 255);
  auto* apply = new QPushButton("Применить роль…", page);
  apply->setAutoDefault(false);
  apply->setObjectName("apply_role");
  rf->addWidget(new QLabel("Роль", page));
  rf->addWidget(role_, 1);
  rf->addWidget(new QLabel("Уровень", page));
  rf->addWidget(level_);
  rf->addWidget(apply);
  l->addLayout(rf);
  l->addWidget(new QLabel("Группы выбранного пользователя", page));
  memberships_ = new AccountListModel(project_, actor_id_,
                                      AccountListModel::Kind::Groups, this);
  auto* groups = new QListView(page);
  groups->setObjectName("user_memberships");
  groups->setModel(memberships_);
  l->addWidget(groups, 1);
  memberships_->set_error_handler(
      [this](const QString& e) { status_->setText(e); });
  memberships_->set_change_handler([this] {
    if (change_handler_) change_handler_();
    refresh();
  });
  auto* gf = new QFormLayout;
  account_group_ = new QComboBox(page);
  account_group_->setObjectName("account_group");
  gf->addRow("Группа", account_group_);
  l->addLayout(gf);
  auto* ga = new QHBoxLayout;
  auto* cg = new QPushButton("Создать группу…", page);
  cg->setObjectName("create_group");
  auto* rg = new QPushButton("Переименовать группу…", page);
  rg->setObjectName("rename_group");
  for (auto* b : {cg, rg}) b->setAutoDefault(false);
  ga->addWidget(cg);
  ga->addWidget(rg);
  ga->addStretch();
  l->addLayout(ga);
  connect(create, &QPushButton::clicked, this,
          [this] { create_account(false); });
  connect(cg, &QPushButton::clicked, this, [this] { create_account(true); });
  connect(rename, &QPushButton::clicked, this,
          [this] { rename_account(false); });
  connect(rg, &QPushButton::clicked, this, [this] { rename_account(true); });
  connect(apply, &QPushButton::clicked, this, [this] { apply_role(); });
  connect(account_user_, &QComboBox::currentIndexChanged, this,
          [this] { account_details(); });
  connect(role_, &QComboBox::currentIndexChanged, this, [this] {
    auto id = account_user_->currentData(Qt::UserRole).value<Guid>();
    const bool admin = role_->currentIndex() == 1;
    level_->setEnabled(id != project_.superadmin_id() && admin);
    level_->setMinimum(admin ? 1 : 0);
    level_->setSpecialValueText(admin ? QString() : QString("—"));
    if (!admin) level_->setValue(0);
  });
  reload_accounts();
  return page;
}
void AdminDialog::reload_accounts() {
  if (!account_users_) return;
  auto user_id = account_users_->id_at(account_user_->currentIndex());
  {
    QSignalBlocker b(account_user_);
    account_users_->reload();
    account_user_->setCurrentIndex(account_users_->row_for_id(user_id));
    if (account_user_->currentIndex() < 0 && account_users_->rowCount())
      account_user_->setCurrentIndex(0);
  }
  auto keep = account_group_->currentData();
  account_group_->clear();
  auto groups = project_.group_ids();
  for (std::size_t i = 0; i < groups.size(); ++i) {
    auto g = project_.group(groups.get(i));
    account_group_->addItem(
        QString::fromStdString(g.name) + QString(" · #%1").arg(GuidLabel(g.id)),
        QVariant::fromValue(g.id));
  }
  auto at = account_group_->findData(keep);
  if (at >= 0) account_group_->setCurrentIndex(at);
  memberships_->reload();
  account_details();
}
void AdminDialog::account_details() {
  if (!account_users_ || account_user_->currentIndex() < 0) return;
  auto id = account_users_->id_at(account_user_->currentIndex());
  auto u = project_.user(id);
  memberships_->set_selected_user(id);
  bool fixed = u.role == UserRole::SuperAdmin;
  QSignalBlocker b(role_);
  role_->clear();
  role_->addItems({"Пользователь", "Администратор"});
  if (fixed) role_->addItem("Суперадминистратор");
  role_->setCurrentIndex(fixed ? 2 : u.role == UserRole::Admin ? 1 : 0);
  role_->setEnabled(!fixed);
  role_->setToolTip(RoleLabel(u));
  level_->setMinimum(u.role == UserRole::Admin ? 1 : 0);
  level_->setSpecialValueText(u.role == UserRole::Admin ? QString()
                                                        : QString("—"));
  level_->setValue(u.admin_level);
  level_->setEnabled(!fixed && u.role == UserRole::Admin);
  if (auto* apply = findChild<QPushButton*>("apply_role"))
    apply->setEnabled(!fixed);
}
void AdminDialog::create_account(bool group) {
  bool ok = false;
  auto name = QInputDialog::getText(
      this, group ? "Создать группу" : "Создать пользователя",
      "Имя:", QLineEdit::Normal, {}, &ok);
  if (!ok) return;
  try {
    auto before = project_.revision();
    if (group)
      project_.create_group(name.toStdString(), actor_id_);
    else
      project_.create_user(name.toStdString(), actor_id_);
    reload_accounts();
    changed(before);
  } catch (const std::exception& e) {
    error(e);
  }
}
void AdminDialog::rename_account(bool group) {
  auto id = group ? account_group_->currentData().value<Guid>()
                  : account_users_->id_at(account_user_->currentIndex());
  if (!id) return;
  bool ok = false;
  auto old = QString::fromStdString(group ? project_.group(id).name
                                          : project_.user(id).name);
  auto name = QInputDialog::getText(this, "Переименование",
                                    "Имя:", QLineEdit::Normal, old, &ok);
  if (!ok) return;
  try {
    auto before = project_.revision();
    if (group)
      project_.rename_group(id, name.toStdString(), actor_id_);
    else
      project_.rename_user(id, name.toStdString(), actor_id_);
    reload_accounts();
    changed(before);
  } catch (const std::exception& e) {
    error(e);
  }
}
void AdminDialog::apply_role() {
  auto id = account_users_->id_at(account_user_->currentIndex());
  if (!id) return;
  try {
    auto role = role_->currentIndex() == 1 ? UserRole::Admin : UserRole::User;
    unsigned level =
        role == UserRole::Admin ? static_cast<unsigned>(level_->value()) : 0;
    auto impact = project_.preview_role_change(id, role, level, actor_id_);
    if ((impact.grants.size() || impact.rules.size()) &&
        QMessageBox::question(
            this, "Изменение роли",
            QString("Будет отозвано назначений: %1, правил: %2. Продолжить?")
                .arg(impact.grants.size())
                .arg(impact.rules.size()),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No) != QMessageBox::Yes)
      return;
    auto before = project_.revision();
    project_.set_role(id, role, level, actor_id_, impact.revision);
    reload_accounts();
    changed(before);
  } catch (const std::exception& e) {
    error(e);
  }
}
