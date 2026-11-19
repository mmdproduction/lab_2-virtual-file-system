#include "MainWindow.hpp"

#include <QAbstractItemView>
#include <QFont>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QTimer>
#include <QToolButton>
#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QCheckBox>
#include <QCloseEvent>
#include <QKeySequence>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStatusBar>
#include <QTableView>
#include <QTreeView>
#include <QVBoxLayout>

#include <stdexcept>
#include <utility>

#include "Dictionary/AVLDictionary.hpp"
#include "Gui/UiTheme.hpp"
#include "Gui/AdminDialog.hpp"
#include "Gui/UserChoiceModel.hpp"
#include "Gui/FileIconDialog.hpp"
#include "Gui/ProjectStore.hpp"
#include "Gui/AutosaveController.hpp"
#include "Gui/SearchDialog.hpp"

namespace {
class AutosavePause {
public:
    explicit AutosavePause(AutosaveController& controller) : controller_(controller) { controller_.pause(); }
    ~AutosavePause() { controller_.resume(); }
    AutosavePause(const AutosavePause&) = delete;
    AutosavePause& operator=(const AutosavePause&) = delete;
private:
    AutosaveController& controller_;
};
}  // namespace

MainWindow::MainWindow(QWidget* parent, const MainWindowOptions& options)
    : QMainWindow(parent), options_(options),
      project_(std::make_unique<ProjectState>(ProjectState{
          UserManager(std::make_unique<AvlDictionary<UserId, User>>(),
                      std::make_unique<AvlDictionary<GroupId, Group>>()),
          VirtualFileSystem(std::make_unique<AvlDictionary<NodeId, FSNode>>(),
                            std::make_unique<AvlDictionary<std::string, ArraySequence<NodeId>>>())})) {
    current_user_id_ = project_->users.admin_id();
    const User& admin = current_user();
    const NodeId assets = project_->vfs.create_directory(project_->vfs.root_id(), "assets", admin);
    project_->vfs.create_directory(assets, "textures", admin);
    project_->vfs.create_directory(assets, "scenes", admin);
    project_->vfs.create_directory(project_->vfs.root_id(), "documents", admin);
    model_ = std::make_unique<VfsTreeModel>(project_->vfs, admin);

    autosave_ = std::make_unique<AutosaveController>(options_.autosave_interval_ms,
        [this](const QString& path) { ProjectStore::save(path, project_->users, project_->vfs); });
    setWindowTitle("VFS — Новый проект[*]");
    resize(1120, 720);
    setMinimumSize(820, 520);
    setFont(QFont("Sans Serif", 10));
    setPalette(ui::DarkPalette());
    setStyleSheet(ui::DarkStyleSheet());

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(24, 22, 24, 16);
    layout->setSpacing(16);
    auto* header = new QHBoxLayout;
    auto* titles = new QVBoxLayout;
    auto* title = new QLabel("Рабочее пространство", central);
    title->setObjectName("title");
    auto* subtitle = new QLabel("Виртуальные папки и ссылки на файлы", central);
    subtitle->setObjectName("subtitle");
    titles->addWidget(title);
    titles->addWidget(subtitle);
    header->addLayout(titles);
    header->addStretch();
    auto* user_badge = new QWidget(central);
    user_badge->setObjectName("user_badge");
    auto* badge_layout = new QHBoxLayout(user_badge);
    badge_layout->setContentsMargins(12, 9, 12, 9);
    badge_layout->setSpacing(9);
    auto* user_icon = new QLabel(user_badge);
    user_icon->setPixmap(ui::LineIcon(ui::Icon::User).pixmap(18, 18));
    user_selector_ = new QComboBox(user_badge);
    user_selector_->setObjectName("user_selector");
    user_selector_->setMinimumWidth(160);
    user_selector_->setToolTip("Переключить пользователя для проверки прав доступа");
    user_choices_ = std::make_unique<UserChoiceModel>(project_->users);
    user_selector_->setModel(user_choices_.get());
    user_label_ = new QLabel("Администратор", user_badge);
    user_label_->setObjectName("user_label");
    badge_layout->addWidget(user_icon);
    badge_layout->addWidget(user_selector_);
    badge_layout->addWidget(user_label_);
    header->addWidget(user_badge, 0, Qt::AlignVCenter);
    layout->addLayout(header);
    auto* project_line = new QHBoxLayout;
    project_status_ = new QLabel("Новый проект · сначала сохраните файл", central);
    project_status_->setObjectName("project_status");
    project_status_->setTextFormat(Qt::PlainText);
    project_line->addWidget(project_status_, 1);
    autosave_checkbox_ = new QCheckBox("Автосохранение", central);
    autosave_checkbox_->setObjectName("autosave_checkbox");
    autosave_checkbox_->setChecked(true);
    autosave_checkbox_->setToolTip("Обновлять выбранный файл проекта каждые 30 секунд при наличии изменений");
    project_line->addWidget(autosave_checkbox_);
    layout->addLayout(project_line);
    autosave_->set_status_handler([this] { update_project_status(); });
    connect(autosave_checkbox_, &QCheckBox::toggled, this, [this](bool enabled) { autosave_->set_enabled(enabled); });
    auto* project_menu = menuBar()->addMenu("Проект");
    load_project_ = project_menu->addAction("Открыть проект…");
    load_project_->setObjectName("load_project_action");
    load_project_->setShortcut(QKeySequence::Open);
    save_project_ = project_menu->addAction("Сохранить");
    save_project_->setObjectName("save_project_action");
    save_project_->setShortcut(QKeySequence::Save);
    save_project_as_ = project_menu->addAction("Сохранить как…");
    save_project_as_->setObjectName("save_project_as_action");
    save_project_as_->setShortcut(QKeySequence::SaveAs);
    connect(load_project_, &QAction::triggered, this, [this] { load_project(); });
    connect(save_project_, &QAction::triggered, this, [this] { save_project(false); });
    connect(save_project_as_, &QAction::triggered, this, [this] { save_project(true); });

    auto* toolbar = new QHBoxLayout;
    up_ = new QPushButton("Вверх", central);
    up_->setObjectName("up_button");
    up_->setIcon(ui::LineIcon(ui::Icon::ArrowUp));
    up_->setIconSize(QSize(18, 18));
    auto* create = create_directory_ = new QPushButton("Новая папка", central);
    create->setObjectName("create_directory_button");
    create->setIcon(ui::LineIcon(ui::Icon::Plus, QColor("#161b23")));
    create->setIconSize(QSize(18, 18));
    toolbar->addWidget(up_);
    toolbar->addWidget(create);
    create_file_ = new QPushButton("Новый файл", central);
    create_file_->setObjectName("create_file_button");
    create_file_->setIcon(ui::LineIcon(ui::Icon::File));
    attach_file_ = new QPushButton("Подключить файл", central);
    attach_file_->setObjectName("attach_file_button");
    attach_file_->setIcon(ui::LineIcon(ui::Icon::Plus));
    toolbar->addWidget(create_file_);
    toolbar->addWidget(attach_file_);
    auto* search = new QPushButton("Поиск", central);
    search->setObjectName("search_files_button");
    search->setIcon(ui::LineIcon(ui::Icon::Search));
    search->setToolTip("Поиск файлов по полному имени (Ctrl+F)");
    toolbar->addWidget(search);
    connect(search, &QPushButton::clicked, this, [this] { search_files(); });
    auto* search_action = new QAction(this);
    search_action->setObjectName("search_files_action");
    search_action->setShortcut(QKeySequence::Find);
    addAction(search_action);
    connect(search_action, &QAction::triggered, this, [this] { search_files(); });
    toolbar->addStretch();
    auto* manage = manage_ = new QPushButton("Администрирование", central);
    manage->setObjectName("manage_accounts_button");
    manage->setIcon(ui::LineIcon(ui::Icon::Group));
    manage->setEnabled(admin.is_admin);
    manage->setToolTip("Пользователи, группы и права выбранного объекта");
    toolbar->addWidget(manage);
    layout->addLayout(toolbar);

    auto* splitter = new QSplitter(Qt::Horizontal, central);
    auto* navigation = new QWidget(splitter);
    auto* navigation_layout = new QVBoxLayout(navigation);
    navigation_layout->setContentsMargins(0, 0, 0, 0);
    auto* navigation_title = new QLabel("ОБЗОР", navigation);
    navigation_title->setObjectName("section_title");
    navigation_layout->addWidget(navigation_title);
    tree_ = new QTreeView(navigation);
    tree_->setObjectName("vfs_tree");
    tree_->setModel(model_.get());
    tree_->hideColumn(1);
    tree_->hideColumn(2);
    tree_->setHeaderHidden(true);
    tree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tree_->setUniformRowHeights(true);
    tree_->setIconSize(QSize(20, 20));
    tree_->setIndentation(21);
    tree_->expandAll();
    navigation_layout->addWidget(tree_);

    auto* browser = new QWidget(splitter);
    auto* browser_layout = new QVBoxLayout(browser);
    browser_layout->setContentsMargins(0, 0, 0, 0);
    path_ = new QLabel(browser);
    path_->setObjectName("current_path");
    path_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto* path_row = new QHBoxLayout;
    path_row->addWidget(path_, 1);
    const auto edit_button = [browser, path_row](const char* name, const QString& text, ui::Icon icon) {
        auto* button = new QToolButton(browser);
        button->setObjectName(QString::fromLatin1(name));
        button->setToolTip(text);
        button->setAccessibleName(text);
        button->setIcon(ui::LineIcon(icon));
        button->setIconSize(QSize(18, 18));
        path_row->addWidget(button);
        return button;
    };
    rename_node_ = edit_button("rename_node_button", "Переименовать выбранный объект", ui::Icon::Rename);
    choose_icon_ = edit_button("choose_icon_button", "Выбрать значок файла", ui::Icon::Palette);
    delete_node_ = edit_button("delete_node_button", "Удалить выбранный объект из VFS", ui::Icon::Trash);
    browser_layout->addLayout(path_row);
    contents_ = new QTableView(browser);
    contents_->setObjectName("directory_contents");
    contents_->setModel(model_.get());
    contents_->setSelectionBehavior(QAbstractItemView::SelectRows);
    contents_->setSelectionMode(QAbstractItemView::SingleSelection);
    contents_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    contents_->setAlternatingRowColors(false);
    contents_->setIconSize(QSize(20, 20));
    contents_->setShowGrid(false);
    contents_->verticalHeader()->hide();
    contents_->verticalHeader()->setDefaultSectionSize(40);
    contents_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    contents_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    contents_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    contents_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    browser_layout->addWidget(contents_);
    empty_ = new QLabel("Нет доступа к корневой папке. Администратор должен выдать вашей группе право чтения корня.", browser);
    empty_->setObjectName("empty_state");
    empty_->setWordWrap(true);
    empty_->setAlignment(Qt::AlignCenter);
    empty_->hide();
    browser_layout->addWidget(empty_, 1);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    layout->addWidget(splitter, 1);
    setCentralWidget(central);
    QAbstractItemView* views[] = {tree_, contents_};
    for (auto* view : views) {
        view->setDragDropMode(QAbstractItemView::DragDrop);
        view->setDragEnabled(true);
        view->setAcceptDrops(true);
        view->setDropIndicatorShown(true);
        view->setDefaultDropAction(Qt::MoveAction);
        view->setDragDropOverwriteMode(false);
    }
    configure_model();
    connect(user_selector_, &QComboBox::currentIndexChanged, this, [this](int row) {
        const UserId id = user_choices_->id_at(row);
        if (id != kInvalidUserId && id != current_user_id_) change_user(id);
    });
    connect(rename_node_, &QToolButton::clicked, this, [this] { rename_node(); });
    connect(delete_node_, &QToolButton::clicked, this, [this] { delete_node(); });
    connect(choose_icon_, &QToolButton::clicked, this, [this] { choose_icon(); });
    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    contents_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tree_, &QWidget::customContextMenuRequested, this, [this](const QPoint& point) { context_menu(tree_, point); });
    connect(contents_, &QWidget::customContextMenuRequested, this, [this](const QPoint& point) { context_menu(contents_, point); });
    connect(create, &QPushButton::clicked, this, [this] { create_directory(); });
    connect(create_file_, &QPushButton::clicked, this, [this] { file_action(true); });
    connect(attach_file_, &QPushButton::clicked, this, [this] { file_action(false); });
    connect(manage, &QPushButton::clicked, this, [this] {
        const NodeId target = selected_node_;
        try {
            AdminDialog dialog(project_->users, project_->vfs, target, current_user_id_, this);
            dialog.set_change_handler([this] { autosave_->mark_dirty(); });
            dialog.exec();
            reload_users();
            model_->refresh();
            refresh_directory();
        } catch (const std::exception& error) {
            QMessageBox::warning(this, "Не удалось открыть настройки", QString::fromUtf8(error.what()));
        }
    });
    connect(up_, &QPushButton::clicked, this, [this] {
        const FSNode& node = project_->vfs.get_node(current_directory_, current_user());
        if (node.parent_id != kInvalidNodeId) show_directory(node.parent_id);
    });
    show_directory(assets);
    update_project_status();
}

MainWindow::~MainWindow() {
    // Destroy views and their connections while the model and VFS still exist.
    delete takeCentralWidget();
}

QString MainWindow::directory_path(NodeId id) const {
    const User& user = current_user();
    QString path;
    while (id != project_->vfs.root_id()) {
        const FSNode& node = project_->vfs.get_node(id, user);
        path.prepend('/' + QString::fromStdString(node.name));
        id = node.parent_id;
    }
    return path.isEmpty() ? QString("/") : path;
}

void MainWindow::show_directory(NodeId id) {
    const FSNode& node = project_->vfs.get_node(id, current_user());
    if (node.type != FSType::Folder) return;
    current_directory_ = id;
    selected_node_ = id;
    const QModelIndex index = model_->index_for_node(id);
    contents_->show();
    empty_->hide();
    contents_->setRootIndex(index);
    contents_->clearSelection();
    contents_->setCurrentIndex(QModelIndex());
    {
        const QSignalBlocker blocker(tree_->selectionModel());
        tree_->setCurrentIndex(index);
    }
    for (QModelIndex ancestor = index; ancestor.isValid(); ancestor = ancestor.parent()) {
        tree_->expand(ancestor);
    }
    path_->setText(directory_path(id));
    update_actions();
    statusBar()->showMessage(QString("Доступных объектов в папке: %1")
        .arg(model_->rowCount(index)));
}

void MainWindow::create_directory() {
    bool accepted = false;
    const QString name = QInputDialog::getText(this, "Новая папка", "Имя папки:",
                                              QLineEdit::Normal, "Новая папка", &accepted);
    if (!accepted) return;
    const NodeId parent_id = current_directory_;
    try {
        model_->create_directory(parent_id, name.toUtf8().toStdString());
        show_directory(parent_id);
        statusBar()->showMessage("Папка создана: " + name, 5000);
    } catch (const std::exception& error) {
        show_directory(parent_id);
        QMessageBox::warning(this, "Не удалось создать папку", QString::fromUtf8(error.what()));
    }
}

const User& MainWindow::current_user() const { return project_->users.get_user(current_user_id_); }

void MainWindow::reload_users() {
    const QSignalBlocker blocker(user_selector_);
    user_choices_->reload();
    user_selector_->setCurrentIndex(user_choices_->row_for_id(current_user_id_));
    user_label_->setText(current_user().is_admin ? "Администратор" : "Пользователь");
}

void MainWindow::change_user(UserId id) {
    const User& user = project_->users.get_user(id);
    current_user_id_ = id;
    model_->set_user(user);
    user_label_->setText(user.is_admin ? "Администратор" : "Пользователь");
    refresh_directory();
}

void MainWindow::update_actions() {
    if (project_loading_) return;
    save_project_->setEnabled(current_user().is_admin);
    save_project_as_->setEnabled(current_user().is_admin);
    load_project_->setEnabled(current_user().is_admin);
    const bool visible = model_->index_for_node(current_directory_).isValid();
    const bool writable = visible && project_->vfs.get_access_rights(current_directory_, current_user()).write;
    create_directory_->setEnabled(writable);
    create_file_->setEnabled(writable);
    attach_file_->setEnabled(writable);
    up_->setEnabled(visible && current_directory_ != project_->vfs.root_id());
    manage_->setVisible(current_user().is_admin);
    const bool selected_visible = model_->index_for_node(selected_node_).isValid();
    manage_->setEnabled(current_user().is_admin && selected_visible);
    bool editable = false;
    bool file_icon = false;
    if (selected_visible) {
        const FSNode& node = project_->vfs.get_node(selected_node_, current_user());
        const bool write_node = project_->vfs.get_access_rights(node.id, current_user()).write;
        editable = node.id != project_->vfs.root_id() && write_node &&
                   project_->vfs.get_access_rights(node.parent_id, current_user()).write;
        file_icon = node.type == FSType::File && write_node;
    }
    rename_node_->setEnabled(editable);
    delete_node_->setEnabled(editable);
    choose_icon_->setEnabled(file_icon);
}

void MainWindow::refresh_directory() {
    if (project_loading_) return;
    NodeId target = current_directory_;
    if (!model_->index_for_node(target).isValid()) target = project_->vfs.root_id();
    if (model_->index_for_node(target).isValid()) {
        const NodeId selection = selected_node_;
        show_directory(target);
        const QModelIndex index = model_->index_for_node(selection);
        if (index.isValid() && selection != target) {
            {
                const QSignalBlocker blocker(tree_->selectionModel());
                tree_->setCurrentIndex(index);
            }
            for (QModelIndex ancestor = index.parent(); ancestor.isValid(); ancestor = ancestor.parent()) {
                tree_->expand(ancestor);
            }
            tree_->scrollTo(index);
            if (index.parent() == contents_->rootIndex()) contents_->setCurrentIndex(index);
            selected_node_ = selection;
            update_actions();
        }
        return;
    }
    current_directory_ = selected_node_ = kInvalidNodeId;
    contents_->setRootIndex(QModelIndex());
    contents_->hide();
    empty_->show();
    path_->setText("Нет доступной папки");
    statusBar()->showMessage("Нет права чтения корневой папки");
    update_actions();
}

void MainWindow::file_action(bool creating) {
    const NodeId parent = current_directory_;
    const auto options = QFileDialog::DontUseNativeDialog | QFileDialog::DontConfirmOverwrite;
    const QString path = creating
        ? QFileDialog::getSaveFileName(this, "Новый пустой файл", QDir::homePath(), "Все файлы (*)", nullptr, options)
        : QFileDialog::getOpenFileName(this, "Подключить существующий файл", QDir::homePath(), "Все файлы (*)", nullptr, QFileDialog::DontUseNativeDialog);
    if (path.isEmpty()) return;
    try {
        const std::string name = QFileInfo(path).fileName().toUtf8().toStdString();
        if (creating) model_->create_file(parent, name, path);
        else model_->attach_file(parent, name, path);
        refresh_directory();
        statusBar()->showMessage(creating ? "Пустой файл создан" : "Файл подключён", 5000);
    } catch (const std::exception& error) {
        refresh_directory();
        QMessageBox::warning(this, "Не удалось добавить файл", QString::fromUtf8(error.what()));
    }
}

void MainWindow::rename_node() {
    const NodeId id = selected_node_;
    if (!rename_node_->isEnabled()) return;
    const QString previous = QString::fromStdString(project_->vfs.get_node(id, current_user()).name);
    bool accepted = false;
    const QString name = QInputDialog::getText(this, "Переименование", "Новое имя:", QLineEdit::Normal, previous, &accepted);
    if (!accepted) return;
    try {
        model_->rename(id, name.toUtf8().toStdString());
        refresh_directory();
        statusBar()->showMessage("Объект переименован", 5000);
    } catch (const std::exception& error) {
        refresh_directory();
        QMessageBox::warning(this, "Не удалось переименовать", QString::fromUtf8(error.what()));
    }
}

void MainWindow::delete_node() {
    if (!delete_node_->isEnabled()) return;
    const NodeId id = selected_node_;
    const FSNode& node = project_->vfs.get_node(id, current_user());
    const NodeId parent = node.parent_id;
    const QString name = QString::fromStdString(node.name);
    const bool folder = node.type == FSType::Folder;
    QMessageBox confirm(QMessageBox::Question, "Удаление из VFS",
        folder ? QString("Удалить папку «%1» со всем содержимым из VFS?\nФизические файлы останутся на диске.").arg(name)
               : QString("Удалить файл «%1» из VFS?\nФизический файл останется на диске.").arg(name),
        QMessageBox::Yes | QMessageBox::No, this);
    confirm.setTextFormat(Qt::PlainText);
    confirm.setDefaultButton(QMessageBox::No);
    confirm.button(QMessageBox::Yes)->setText("Удалить");
    confirm.button(QMessageBox::No)->setText("Отмена");
    if (confirm.exec() != QMessageBox::Yes) return;
    bool deleting_current = false;
    for (NodeId ancestor = current_directory_; ancestor != kInvalidNodeId;) {
        if (ancestor == id) { deleting_current = true; break; }
        ancestor = project_->vfs.get_node(ancestor, current_user()).parent_id;
    }
    try {
        model_->remove(id);
        if (deleting_current) current_directory_ = parent;
        selected_node_ = current_directory_;
        refresh_directory();
        statusBar()->showMessage("Объект удалён из VFS", 5000);
    } catch (const std::exception& error) {
        refresh_directory();
        QMessageBox::warning(this, "Не удалось удалить объект", QString::fromUtf8(error.what()));
    }
}

void MainWindow::choose_icon() {
    if (!choose_icon_->isEnabled()) return;
    const NodeId id = selected_node_;
    FileIconDialog dialog(project_->vfs.get_node(id, current_user()).file_icon, this);
    if (dialog.exec() != QDialog::Accepted) return;
    try {
        model_->set_file_icon(id, dialog.selected_icon());
        statusBar()->showMessage("Значок файла изменён", 5000);
    } catch (const std::exception& error) {
        QMessageBox::warning(this, "Не удалось изменить значок", QString::fromUtf8(error.what()));
    }
}

void MainWindow::context_menu(QAbstractItemView* view, const QPoint& point) {
    const QModelIndex index = view->indexAt(point);
    if (!index.isValid()) return;
    const NodeId id = model_->node_id(index);
    view->setCurrentIndex(index);
    selected_node_ = id;
    update_actions();
    QMenu menu(this);
    menu.setObjectName("node_context_menu");
    if (rename_node_->isEnabled()) {
        auto* action = menu.addAction(ui::LineIcon(ui::Icon::Rename), "Переименовать");
        action->setObjectName("rename_node_action");
        connect(action, &QAction::triggered, this, [this] { rename_node(); });
    }
    if (choose_icon_->isEnabled()) {
        auto* action = menu.addAction(ui::LineIcon(ui::Icon::Palette), "Выбрать значок…");
        action->setObjectName("choose_icon_action");
        connect(action, &QAction::triggered, this, [this] { choose_icon(); });
    }
    if (delete_node_->isEnabled()) {
        if (!menu.isEmpty()) menu.addSeparator();
        auto* action = menu.addAction(ui::LineIcon(ui::Icon::Trash), "Удалить из VFS");
        action->setObjectName("delete_node_action");
        connect(action, &QAction::triggered, this, [this] { delete_node(); });
    }
    if (!menu.isEmpty()) menu.exec(view->viewport()->mapToGlobal(point));
}

void MainWindow::configure_model() {
    model_->set_error_handler([this](const QString& error) {
        QMessageBox::warning(this, "Не удалось переместить объект", error);
    });
    model_->set_move_handler([this](NodeId) {
        QTimer::singleShot(0, this, [this] { statusBar()->showMessage("Объект перемещён", 5000); });
    });
    connect(model_.get(), &QAbstractItemModel::modelAboutToBeReset, this, [this] {
        model_resetting_ = true;
    });
    connect(model_.get(), &QAbstractItemModel::modelReset, this, [this] {
        model_resetting_ = false;
        QTimer::singleShot(0, this, [this] { refresh_directory(); });
    });

    connect(tree_->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex& index, const QModelIndex&) {
        const NodeId id = model_->node_id(index);
        if (id == kInvalidNodeId) return;
        if (project_->vfs.get_node(id, current_user()).type == FSType::Folder) {
            show_directory(id);
        } else {
            selected_node_ = id;
            update_actions();
        }
    });

    connect(contents_->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex& index) {
        if (model_resetting_ || project_loading_) return;
        const NodeId id = model_->node_id(index);
        selected_node_ = id == kInvalidNodeId ? current_directory_ : id;
        update_actions();
    });
    const auto select_object = [this](const QModelIndex& index) {
        const NodeId id = model_->node_id(index);
        if (id != kInvalidNodeId) selected_node_ = id;
        update_actions();
    };
    connect(tree_, &QTreeView::clicked, model_.get(), select_object);
    connect(tree_, &QTreeView::activated, model_.get(), select_object);
    connect(contents_, &QTableView::clicked, model_.get(), select_object);
    connect(contents_, &QTableView::activated, model_.get(), select_object);
    model_->set_change_handler([this] { autosave_->mark_dirty(); });
    connect(contents_, &QTableView::doubleClicked, model_.get(), [this](const QModelIndex& index) { open_node(index); });
    connect(tree_, &QTreeView::doubleClicked, model_.get(), [this](const QModelIndex& index) { open_node(index); });
    tree_->hideColumn(1);
    tree_->hideColumn(2);
    contents_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    contents_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    contents_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
}

void MainWindow::open_node(const QModelIndex& index) {
    const NodeId id = model_->node_id(index);
    if (id == kInvalidNodeId) return;
    try {
        const FSNode& node = project_->vfs.get_node(id, current_user());
        if (node.type == FSType::Folder) show_directory(id);
        else FileOpener::open(project_->vfs, id, current_user(), options_.file_launcher);
    } catch (const std::exception& error) {
        QMessageBox::warning(this, "Не удалось открыть файл", QString::fromUtf8(error.what()));
    }
}

bool MainWindow::save_project(bool save_as) {
    const AutosavePause pause(*autosave_);
    if (!current_user().is_admin) return false;
    QString path = autosave_->path();
    if (save_as || path.isEmpty()) {
        QFileDialog dialog(this, "Сохранить проект", path.isEmpty() ? QDir::homePath() : QFileInfo(path).absolutePath());
        dialog.setObjectName("save_project_dialog");
        dialog.setAcceptMode(QFileDialog::AcceptSave);
        dialog.setNameFilter("Проект VFS (*.vfsp)");
        dialog.setDefaultSuffix("vfsp");
        dialog.setOption(QFileDialog::DontUseNativeDialog);
        dialog.selectFile(path.isEmpty() ? "project.vfsp" : QFileInfo(path).fileName());
        if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) return false;
        path = dialog.selectedFiles().first();
    }
    try {
        ProjectStore::save(path, project_->users, project_->vfs);
        autosave_->mark_saved(path);
        statusBar()->showMessage("Проект сохранён", 5000);
        return true;
    } catch (const std::exception& error) {
        QMessageBox::warning(this, "Не удалось сохранить проект", QString::fromUtf8(error.what()));
        return false;
    }
}

bool MainWindow::confirm_replacement() {
    const AutosavePause pause(*autosave_);
    if (!autosave_->dirty()) return true;
    QString text = "В проекте есть несохранённые изменения. Сохранить их?";
    if (!autosave_->error().isEmpty()) text += "\nОшибка автосохранения: " + autosave_->error();
    QMessageBox box(QMessageBox::Question, "Несохранённые изменения", text,
                    QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, this);
    box.setTextFormat(Qt::PlainText);
    box.setDefaultButton(QMessageBox::Cancel);
    box.button(QMessageBox::Save)->setText("Сохранить");
    box.button(QMessageBox::Save)->setEnabled(current_user().is_admin);
    box.button(QMessageBox::Discard)->setText("Не сохранять");
    box.button(QMessageBox::Cancel)->setText("Отмена");
    const int action = box.exec();
    if (action == QMessageBox::Discard) return true;
    return action == QMessageBox::Save && save_project(false);
}

void MainWindow::load_project() {
    const AutosavePause pause(*autosave_);
    if (!current_user().is_admin) return;
    const QString path = QFileDialog::getOpenFileName(this, "Открыть проект", QDir::homePath(),
        "Проект VFS (*.vfsp);;Все файлы (*)", nullptr, QFileDialog::DontUseNativeDialog);
    if (path.isEmpty() || !confirm_replacement()) return;
    try {
        auto project = std::make_unique<ProjectState>(ProjectStore::load(path));
        install_project(std::move(project), path);
        statusBar()->showMessage("Проект загружен", 5000);
    } catch (const std::exception& error) {
        QMessageBox::warning(this, "Не удалось загрузить проект", QString::fromUtf8(error.what()));
    }
}

void MainWindow::install_project(std::unique_ptr<ProjectState> project, const QString& path) {
    const UserId admin = project->users.admin_id();
    auto model = std::make_unique<VfsTreeModel>(project->vfs, project->users.get_user(admin));
    auto choices = std::make_unique<UserChoiceModel>(project->users);
    // Both new adapters are fully prepared while the old project is still alive.
    const QSignalBlocker chooser_blocker(user_selector_);
    project_loading_ = true;
    disconnect(tree_->selectionModel(), nullptr, this, nullptr);
    disconnect(contents_->selectionModel(), nullptr, this, nullptr);
    tree_->setModel(nullptr);
    contents_->setModel(nullptr);
    user_selector_->setModel(choices.get());
    model_.reset();
    user_choices_ = std::move(choices);
    project_ = std::move(project);
    model_ = std::move(model);
    current_user_id_ = admin;
    current_directory_ = selected_node_ = project_->vfs.root_id();
    model_resetting_ = false;
    tree_->setModel(model_.get());
    contents_->setModel(model_.get());
    configure_model();
    user_selector_->setCurrentIndex(user_choices_->row_for_id(admin));
    user_label_->setText("Администратор");
    project_loading_ = false;
    refresh_directory();
    autosave_->mark_saved(path);
}

void MainWindow::update_project_status() {
    const QString path = autosave_->path();
    const QString name = path.isEmpty() ? "Новый проект" : QFileInfo(path).fileName();
    QString state;
    if (!autosave_->error().isEmpty()) state = "Ошибка сохранения";
    else if (path.isEmpty()) state = "сначала сохраните файл";
    else state = autosave_->dirty() ? "есть изменения" : "сохранено";
    project_status_->setText(name + " · " + state);
    project_status_->setToolTip(path + (autosave_->error().isEmpty() ? QString() : "\n" + autosave_->error()));
    project_status_->setStyleSheet(autosave_->error().isEmpty() ? "color: #89929f" : "color: #d38a8a");
    setWindowTitle("VFS — " + name + "[*]");
    setWindowModified(autosave_->dirty());
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (autosave_->dirty() && autosave_->enabled() && !autosave_->path().isEmpty() && autosave_->flush()) {
        event->accept();
        return;
    }
    if (confirm_replacement()) event->accept(); else event->ignore();
}

void MainWindow::search_files() {
    SearchDialog dialog(project_->vfs, current_user(), options_.file_launcher, this);
    if (dialog.exec() != QDialog::Accepted) return;
    const NodeId id = dialog.selected_node();
    try {
        const QModelIndex index = model_->index_for_node(id);
        if (!index.isValid()) return;
        const FSNode& node = project_->vfs.get_node(id, current_user());
        show_directory(node.parent_id);
        contents_->setCurrentIndex(index);
        contents_->scrollTo(index);
        {
            const QSignalBlocker blocker(tree_->selectionModel());
            tree_->setCurrentIndex(index);
        }
        tree_->scrollTo(index);
        selected_node_ = id;
        update_actions();
    } catch (const std::exception& error) {
        QMessageBox::warning(this, "Не удалось показать файл", QString::fromUtf8(error.what()));
    }
}
