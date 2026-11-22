#include "GuidQt.hpp"
#include "AdminDialog.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTabWidget>
#include <QTableView>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include "AccessRuleModel.hpp"
#include "AdminGrantModel.hpp"
#include "PermissionLabels.hpp"
#include "UiTheme.hpp"
namespace {
QPushButton* Button(const char* text, const char* name, QWidget* p) {
  auto* b = new QPushButton(QString::fromUtf8(text), p);
  b->setObjectName(name);
  b->setAutoDefault(false);
  return b;
}
void Table(QTableView* t) {
  t->setSelectionBehavior(QAbstractItemView::SelectRows);
  t->setSelectionMode(QAbstractItemView::SingleSelection);
  t->setEditTriggers(QAbstractItemView::NoEditTriggers);
  t->setShowGrid(false);
  t->verticalHeader()->hide();
  t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  t->horizontalHeader()->setStretchLastSection(true);
}
}  // namespace
AdminDialog::AdminDialog(ProjectService& p, NodeId id, UserId actor,
                         QWidget* parent)
    : QDialog(parent),
      project_(p),
      node_id_(id ? id : p.root_id()),
      actor_id_(actor) {
  if (project_.user(actor).role == UserRole::User)
    throw AccessDenied("Administrator required");
  setObjectName("admin_dialog");
  setWindowTitle("Администрирование");
  setPalette(ui::DarkPalette());
  setStyleSheet(ui::DarkStyleSheet());
  resize(1180, 780);
  setMinimumSize(940, 640);
  auto* outer = new QVBoxLayout(this);
  outer->setContentsMargins(20, 18, 20, 16);
  auto* title = new QLabel("Управление доступом", this);
  title->setObjectName("dialog_title");
  outer->addWidget(title);
  auto* badge = new QLabel(RoleLabel(project_.user(actor)), this);
  outer->addWidget(badge);
  auto* split = new QSplitter(this);
  scopes_ = new QTreeWidget(split);
  scopes_->setObjectName("admin_scopes");
  scopes_->setHeaderLabel("Область управления");
  scopes_->setMinimumWidth(190);
  tabs_ = new QTabWidget(split);
  split->setStretchFactor(1, 4);
  outer->addWidget(split, 1);
  auto* page = new QWidget(tabs_);
  auto* l = new QVBoxLayout(page);
  path_ = new QLabel(page);
  path_->setObjectName("admin_path");
  path_->setTextFormat(Qt::PlainText);
  path_->setWordWrap(true);
  l->addWidget(path_);
  rule_model_ = new AccessRuleModel(project_, node_id_, actor_id_, this);
  rules_ = new QTableView(page);
  rules_->setObjectName("access_rules");
  rules_->setModel(rule_model_);
  Table(rules_);
  l->addWidget(rules_, 1);
  auto* form = new QFormLayout;
  kind_ = new QComboBox(page);
  kind_->setObjectName("principal_kind");
  kind_->addItems({"Пользователь", "Группа"});
  search_ = new QLineEdit(page);
  search_->setObjectName("principal_search");
  search_->setPlaceholderText("Поиск получателя по имени или ID");
  principal_ = new QComboBox(page);
  principal_->setObjectName("principal_choice");
  form->addRow("Получатель", kind_);
  form->addRow("Найти", search_);
  form->addRow("Имя", principal_);
  auto* decisions = new QWidget(page);
  auto* dl = new QHBoxLayout(decisions);
  dl->setContentsMargins(0, 0, 0, 0);
  read_ = new QComboBox(decisions);
  write_ = new QComboBox(decisions);
  read_->setObjectName("read_decision");
  write_->setObjectName("write_decision");
  for (auto* c : {read_, write_})
    c->addItems({"Не задано", "Разрешено", "Запрещено"});
  dl->addWidget(new QLabel("Чтение", decisions));
  dl->addWidget(read_, 1);
  dl->addWidget(new QLabel("Запись", decisions));
  dl->addWidget(write_, 1);
  form->addRow("Решения", decisions);
  rule_source_ = new QComboBox(page);
  rule_source_->setObjectName("rule_source");
  form->addRow("На основании", rule_source_);
  l->addLayout(form);
  rule_hint_ = new QLabel(page);
  rule_hint_->setObjectName("rule_hint");
  rule_hint_->setTextFormat(Qt::PlainText);
  rule_hint_->setWordWrap(true);
  l->addWidget(rule_hint_);
  auto* actions = new QHBoxLayout;
  auto* add = Button("Новое правило", "new_rule", page);
  apply_ = Button("Применить правило", "apply_rule", page);
  remove_ = Button("Удалить правило", "remove_rule", page);
  actions->addWidget(add);
  actions->addWidget(apply_);
  actions->addWidget(remove_);
  actions->addStretch();
  l->addLayout(actions);
  auto* inh = new QHBoxLayout;
  inherit_ = new QCheckBox("Наследовать разрешения", page);
  inherit_->setObjectName("inherit_permissions");
  inherit_->setToolTip(
      "Запреты предков продолжают действовать при отключении разрешений");
  inherit_apply_ = Button("Применить наследование", "apply_inheritance", page);
  inh->addWidget(inherit_);
  inh->addStretch();
  inh->addWidget(inherit_apply_);
  l->addLayout(inh);
  auto* e = new QHBoxLayout;
  effective_user_ = new QComboBox(page);
  effective_user_->setObjectName("effective_user");
  e->addWidget(new QLabel("Итог для", page));
  e->addWidget(effective_user_, 1);
  l->addLayout(e);
  effective_ = new QLabel(page);
  effective_->setObjectName("effective_rights");
  effective_->setTextFormat(Qt::PlainText);
  effective_->setWordWrap(true);
  l->addWidget(effective_);
  tabs_->addTab(page, "Доступ");
  auto* grantpage = new QWidget(tabs_);
  auto* gl = new QVBoxLayout(grantpage);
  auto* hint = new QLabel(
      "Полномочия выдаются на выбранную слева папку и её "
      "поддерево.\nПолучатель должен быть ниже уровнем; передавать можно "
      "только возможности одного источника.",
      grantpage);
  hint->setWordWrap(true);
  gl->addWidget(hint);
  grant_model_ = new AdminGrantModel(project_, actor_id_, this);
  grants_ = new QTableView(grantpage);
  grants_->setObjectName("admin_grants");
  grants_->setModel(grant_model_);
  Table(grants_);
  gl->addWidget(grants_, 1);
  grant_user_ = new QComboBox(grantpage);
  grant_user_->setObjectName("grant_recipient");
  grant_source_ = new QComboBox(grantpage);
  grant_source_->setObjectName("grant_source");
  auto* gf = new QFormLayout;
  gf->addRow("Кому", grant_user_);
  gf->addRow("На основании", grant_source_);
  gl->addLayout(gf);
  const char* labels[] = {"Чтение содержимого", "Запись содержимого",
                          "Настройка доступа", "Дальнейшее делегирование"};
  for (unsigned i = 0; i < 4; ++i) {
    capabilities_[i] = new QCheckBox(QString::fromUtf8(labels[i]), grantpage);
    capabilities_[i]->setObjectName(QString("grant_cap_%1").arg(i));
    gl->addWidget(capabilities_[i]);
  }
  auto* ga = new QHBoxLayout;
  issue_ = Button("Выдать полномочия", "issue_grant", grantpage);
  revoke_ = Button("Отозвать назначение…", "revoke_grant", grantpage);
  ga->addWidget(issue_);
  ga->addWidget(revoke_);
  ga->addStretch();
  gl->addLayout(ga);
  tabs_->addTab(grantpage, "Полномочия");
  if (project_.user(actor_id_).role == UserRole::SuperAdmin)
    tabs_->addTab(accounts_page(), "Пользователи и группы");
  status_ = new QLabel(this);
  status_->setObjectName("admin_status");
  status_->setWordWrap(true);
  status_->setTextFormat(Qt::PlainText);
  outer->addWidget(status_);
  auto* close = Button("Закрыть", "admin_close", this);
  outer->addWidget(close, 0, Qt::AlignRight);
  connect(close, &QPushButton::clicked, this, &QDialog::reject);
  connect(scopes_, &QTreeWidget::currentItemChanged, this,
          [this](QTreeWidgetItem* i) {
            if (i) {
              node_id_ = i->data(0, Qt::UserRole).value<Guid>();
              refresh();
            }
          });
  connect(search_, &QLineEdit::textChanged, this,
          [this] { fill_principals(); });
  connect(kind_, &QComboBox::currentIndexChanged, this,
          [this] { fill_principals(); });
  connect(principal_, &QComboBox::currentIndexChanged, this, [this] {
    if (kind_->currentIndex() == 0) {
      auto at = effective_user_->findData(principal_->currentData());
      if (at >= 0) effective_user_->setCurrentIndex(at);
    }
    effective();
  });
  connect(effective_user_, &QComboBox::currentIndexChanged, this,
          [this] { effective(); });
  connect(add, &QPushButton::clicked, this, [this] { new_rule(); });
  connect(apply_, &QPushButton::clicked, this, [this] { apply_rule(); });
  connect(remove_, &QPushButton::clicked, this, [this] { delete_rule(); });
  connect(rules_->selectionModel(), &QItemSelectionModel::currentChanged, this,
          [this] { select_rule(); });
  connect(inherit_apply_, &QPushButton::clicked, this, [this] {
    try {
      auto before = project_.revision();
      project_.set_inheritance(node_id_, inherit_->isChecked(), actor_id_,
                               rule_source_->currentData().value<Guid>());
      changed(before);
    } catch (const std::exception& e) {
      error(e);
    }
  });
  connect(grant_source_, &QComboBox::currentIndexChanged, this,
          [this] { grant_options(); });
  connect(issue_, &QPushButton::clicked, this, [this] { issue_grant(); });
  connect(revoke_, &QPushButton::clicked, this, [this] { revoke_grant(); });
  connect(grants_->selectionModel(), &QItemSelectionModel::currentChanged, this,
          [this] {
            auto g = grant_model_->row(grants_->currentIndex().row());
            bool ok = false;
            if (g) try {
                project_.preview_revoke(g->id, actor_id_);
                ok = true;
              } catch (const std::exception&) {
              }
            revoke_->setEnabled(ok);
          });
  fill_tree();
  refresh();
}
void AdminDialog::set_change_handler(std::function<void()> h) {
  change_handler_ = std::move(h);
}
void AdminDialog::fill_tree() {
  QSignalBlocker blocker(scopes_);
  scopes_->clear();
  auto* root = new QTreeWidgetItem(scopes_, QStringList("/"));
  root->setData(0, Qt::UserRole,
                QVariant::fromValue(project_.root_id()));
  root->setData(0, Qt::UserRole + 1, true);
  std::function<void(QTreeWidgetItem*, NodeId)> append = [&](auto* parent,
                                                             NodeId id) {
    try {
      auto children = project_.administrative_children(id, actor_id_);
      for (std::size_t i = 0; i < children.size(); ++i) {
        const auto& n = children.get(i);
        auto* item = new QTreeWidgetItem(
            parent, QStringList(QString::fromStdString(n.name)));
        item->setData(0, Qt::UserRole, QVariant::fromValue(n.id));
        item->setData(0, Qt::UserRole + 1, n.type == FSType::Folder);
        item->setIcon(0,
                      ui::LineIcon(n.type == FSType::Folder ? ui::Icon::Folder
                                                            : ui::Icon::File));
        if (n.type == FSType::Folder) append(item, n.id);
      }
    } catch (const AccessDenied&) {
    }
  };
  append(root, project_.root_id());
  scopes_->expandAll();
  QTreeWidgetItem* selected = root;
  for (QTreeWidgetItemIterator i(scopes_); *i; ++i)
    if ((*i)->data(0, Qt::UserRole).value<Guid>() == node_id_) {
      selected = *i;
      break;
    }
  scopes_->setCurrentItem(selected);
  node_id_ = selected->data(0, Qt::UserRole).value<Guid>();
}
void AdminDialog::fill_sources(QComboBox* c, AdminCapability cap) {
  QSignalBlocker block(c);
  auto keep = c->currentData();
  c->clear();
  if (project_.user(actor_id_).role == UserRole::SuperAdmin)
    c->addItem("Суперадминистратор", QVariant::fromValue(Guid{}));
  else {
    auto gs = project_.admin_sources(node_id_, actor_id_, cap);
    for (std::size_t i = 0; i < gs.size(); ++i) {
      auto& g = gs.get(i);
      c->addItem(QString("#%1 · %2")
                     .arg(GuidLabel(g.id))
                     .arg(QString::fromStdString(project_.administrative_path(
                         g.scope_node_id, actor_id_))),
                 QVariant::fromValue(g.id));
    }
  }
  auto at = c->findData(keep);
  if (at >= 0) c->setCurrentIndex(at);
}
void AdminDialog::refresh() {
  try {
    path_->setText(QString::fromStdString(
        project_.administrative_path(node_id_, actor_id_)));
  } catch (const AccessDenied&) {
    path_->setText("Выберите выданную область управления");
  }
  rule_model_->reload(node_id_);
  grant_model_->reload();
  fill_sources(rule_source_, AdminCapability::ManageAccess);
  fill_sources(grant_source_, AdminCapability::DelegateAdministration);
  {
    QSignalBlocker b(effective_user_);
    auto keep = effective_user_->currentData();
    effective_user_->clear();
    auto ids = project_.user_ids();
    for (std::size_t i = 0; i < ids.size(); ++i) {
      auto u = project_.user(ids.get(i));
      effective_user_->addItem(
          QString::fromStdString(u.name) + QString(" · #%1").arg(GuidLabel(u.id)),
          QVariant::fromValue(u.id));
    }
    auto at = effective_user_->findData(keep);
    if (at >= 0) effective_user_->setCurrentIndex(at);
  }
  {
    QSignalBlocker b(grant_user_);
    auto keep = grant_user_->currentData();
    grant_user_->clear();
    auto actor = project_.user(actor_id_);
    auto ids = project_.user_ids();
    for (std::size_t i = 0; i < ids.size(); ++i) {
      auto u = project_.user(ids.get(i));
      if (u.role == UserRole::Admin && (actor.role == UserRole::SuperAdmin ||
                                        actor.admin_level > u.admin_level))
        grant_user_->addItem(
            QString::fromStdString(u.name) +
                QString(" · ур. %1 · #%2").arg(u.admin_level).arg(GuidLabel(u.id)),
            QVariant::fromValue(u.id));
    }
    auto at = grant_user_->findData(keep);
    if (at >= 0) grant_user_->setCurrentIndex(at);
  }
  bool manage = project_.can_administer(node_id_, actor_id_,
                                        AdminCapability::ManageAccess);
  inherit_->setEnabled(manage);
  inherit_apply_->setEnabled(manage);
  try {
    inherit_->setChecked(project_.policy_view(node_id_, actor_id_).inherits);
  } catch (const AccessDenied&) {
    inherit_->setChecked(false);
  }
  fill_principals();
  new_rule();
  grant_options();
  revoke_->setEnabled(false);
  effective();
  for (auto* choices : findChildren<QComboBox*>()) GuidChoiceTooltips(choices);
}
void AdminDialog::fill_principals() {
  auto keep = principal_->currentData();
  QSignalBlocker b(principal_);
  principal_->clear();
  auto ids =
      kind_->currentIndex() == 0 ? project_.user_ids() : project_.group_ids();
  for (std::size_t i = 0; i < ids.size(); ++i) {
    auto id = ids.get(i);
    if (kind_->currentIndex() == 0) {
      auto candidate = project_.user(id);
      auto actor = project_.user(actor_id_);
      if (candidate.role == UserRole::SuperAdmin || candidate.id == actor.id ||
          (actor.role != UserRole::SuperAdmin &&
           candidate.role == UserRole::Admin &&
           candidate.admin_level >= actor.admin_level))
        continue;
    }
    QString text = QString::fromStdString(kind_->currentIndex() == 0
                                              ? project_.user(id).name
                                              : project_.group(id).name) +
                   QString(" · #%1").arg(GuidLabel(id));
    if (text.contains(search_->text(), Qt::CaseInsensitive) ||
        GuidText(id).contains(search_->text(), Qt::CaseInsensitive))
      principal_->addItem(text, QVariant::fromValue(id));
  }
  auto at = principal_->findData(keep);
  if (at >= 0) principal_->setCurrentIndex(at);
  effective();
  GuidChoiceTooltips(principal_);
}
void AdminDialog::new_rule() {
  editing_ = {};
  fill_principals();
  read_->setCurrentIndex(0);
  write_->setCurrentIndex(0);
  bool ok = project_.can_administer(node_id_, actor_id_,
                                    AdminCapability::ManageAccess) &&
            rule_source_->count() > 0;
  for (QWidget* w : std::initializer_list<QWidget*>{
           kind_, principal_, search_, read_, write_, rule_source_})
    w->setEnabled(ok);
  apply_->setEnabled(ok);
  remove_->setEnabled(false);
  rule_hint_->setText(
      ok ? "Новое локальное правило. «Не задано» не создаёт запрет."
         : "На этот объект не выданы полномочия настройки доступа.");
}
void AdminDialog::select_rule() {
  auto view = rule_model_->row(rules_->currentIndex().row());
  if (!view) return;
  auto& r = view->rule;
  editing_ = r.id;
  search_->clear();
  kind_->setCurrentIndex(r.principal.kind == PrincipalKind::Group ? 1 : 0);
  fill_principals();
  auto selected =
      principal_->findData(QVariant::fromValue(r.principal.id));
  if (selected < 0) {
    principal_->addItem(
        QString::fromStdString(r.principal.kind == PrincipalKind::User
                                   ? project_.user(r.principal.id).name
                                   : project_.group(r.principal.id).name),
        QVariant::fromValue(r.principal.id));
    selected = principal_->count() - 1;
  }
  principal_->setCurrentIndex(selected);
  if (r.issuer_id == actor_id_) {
    auto source = rule_source_->findData(
        QVariant::fromValue(r.source_grant_id));
    if (source >= 0) rule_source_->setCurrentIndex(source);
  }
  read_->setCurrentIndex(static_cast<int>(r.read));
  write_->setCurrentIndex(static_cast<int>(r.write));
  bool ok = view->editable && rule_source_->count() > 0;
  for (QWidget* w : std::initializer_list<QWidget*>{
           kind_, principal_, search_, read_, write_, rule_source_})
    w->setEnabled(ok);
  apply_->setEnabled(ok);
  remove_->setEnabled(ok);
  rule_hint_->setText(
      QString("%1 · автор: %2 · источник: #%3")
          .arg(QString::fromStdString(view->reason),
               QString::fromStdString(project_.user(r.issuer_id).name))
          .arg(GuidLabel(r.source_grant_id)));
}
void AdminDialog::apply_rule() {
  try {
    if (principal_->currentIndex() < 0)
      throw std::invalid_argument("Выберите получателя");
    auto before = project_.revision();
    project_.upsert_rule(
        node_id_, editing_,
        {{kind_->currentIndex() == 0 ? PrincipalKind::User
                                     : PrincipalKind::Group,
          principal_->currentData().value<Guid>()},
         static_cast<PermissionDecision>(read_->currentIndex()),
         static_cast<PermissionDecision>(write_->currentIndex())},
        actor_id_, rule_source_->currentData().value<Guid>());
    changed(before);
  } catch (const std::exception& e) {
    error(e);
  }
}
void AdminDialog::delete_rule() {
  try {
    auto before = project_.revision();
    project_.remove_rule(node_id_, editing_, actor_id_,
                         rule_source_->currentData().value<Guid>());
    changed(before);
  } catch (const std::exception& e) {
    error(e);
  }
}
void AdminDialog::effective() {
  if (!effective_ || !effective_user_ || effective_user_->currentIndex() < 0)
    return;
  try {
    auto e = project_.explain(
        node_id_, effective_user_->currentData().value<Guid>(), actor_id_);
    effective_->setText(
        QString("Чтение: %1 · Запись: %2\n%3")
            .arg(e.access.read ? "да" : "нет", e.access.write ? "да" : "нет",
                 (e.reason == "Explicit or inherited deny"
                      ? QString("Действует явный или унаследованный запрет")
                  : e.reason == "Allowed by rule or assignment"
                      ? QString("Разрешено правилом доступа или назначением")
                      : QString("Подходящих разрешений нет"))));
  } catch (const AccessDenied&) {
    effective_->setText(
        "Для просмотра итоговых прав нужно полномочие настройки доступа.");
  }
}
void AdminDialog::grant_options() {
  unsigned mask = 15;
  if (project_.user(actor_id_).role != UserRole::SuperAdmin) {
    mask = 0;
    auto gs = project_.admin_sources(node_id_, actor_id_,
                                     AdminCapability::DelegateAdministration);
    for (std::size_t i = 0; i < gs.size(); ++i)
      if (gs.get(i).id == grant_source_->currentData().value<Guid>())
        mask = gs.get(i).capabilities;
  }
  for (unsigned i = 0; i < 4; ++i) {
    bool ok = (mask & (1U << i)) != 0;
    capabilities_[i]->setEnabled(ok);
    if (!ok) capabilities_[i]->setChecked(false);
  }
  // Core validates that the selected scope is a folder; empty folders are valid
  // too.
  issue_->setEnabled(
      grant_source_->count() > 0 && grant_user_->count() > 0 &&
      scopes_->currentItem() &&
      scopes_->currentItem()->data(0, Qt::UserRole + 1).toBool());
}
void AdminDialog::issue_grant() {
  try {
    unsigned mask = 0;
    for (unsigned i = 0; i < 4; ++i)
      if (capabilities_[i]->isChecked()) mask |= 1U << i;
    auto before = project_.revision();
    project_.grant_admin({grant_user_->currentData().value<Guid>(), node_id_,
                          static_cast<std::uint8_t>(mask),
                          grant_source_->currentData().value<Guid>()},
                         actor_id_);
    changed(before);
  } catch (const std::exception& e) {
    error(e);
  }
}
void AdminDialog::revoke_grant() {
  try {
    auto g = grant_model_->row(grants_->currentIndex().row());
    if (!g) return;
    auto impact = project_.preview_revoke(g->id, actor_id_);
    if (QMessageBox::question(
            this, "Отзыв полномочий",
            QString(
                "Будет удалено назначений: %1, правил доступа: %2. Продолжить?")
                .arg(impact.grants.size())
                .arg(impact.rules.size()),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No) != QMessageBox::Yes)
      return;
    auto before = project_.revision();
    project_.revoke(g->id, actor_id_, impact.revision);
    changed(before);
  } catch (const std::exception& e) {
    error(e);
  }
}
void AdminDialog::changed(std::uint64_t before) {
  if (before != project_.revision() && change_handler_) change_handler_();
  refresh();
  status_->setText("Изменения применены");
}
void AdminDialog::error(const std::exception& e) {
  status_->setText(QString::fromUtf8(e.what()));
  effective();
}
