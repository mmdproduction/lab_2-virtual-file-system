#include "SearchDialog.hpp"

#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>
#include <exception>
#include <utility>

#include "Gui/UiTheme.hpp"

SearchDialog::SearchDialog(const VirtualFileSystem& vfs, const User& user,
                           FileOpener::Launcher launcher, QWidget* parent)
    : QDialog(parent), vfs_(vfs), user_(user), launcher_(std::move(launcher)), model_(vfs, user) {
    setObjectName("file_search_dialog");
    setWindowTitle("Поиск по имени файла");
    setPalette(ui::DarkPalette());
    setStyleSheet(ui::DarkStyleSheet());
    resize(740, 460);
    setMinimumSize(580, 360);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(22, 20, 22, 18);
    layout->setSpacing(14);
    auto* title = new QLabel("Поиск файлов", this);
    title->setObjectName("dialog_title");
    layout->addWidget(title);
    auto* hint = new QLabel("Полное имя с расширением · с учётом регистра · весь проект", this);
    hint->setObjectName("subtitle");
    layout->addWidget(hint);
    auto* search_line = new QHBoxLayout;
    query_ = new QLineEdit(this);
    query_->setObjectName("search_query");
    query_->setPlaceholderText("Например, texture.png");
    query_->setAccessibleName("Полное имя файла");
    query_->setClearButtonEnabled(true);
    auto* submit = new QPushButton("Найти", this);
    submit->setObjectName("search_submit");
    submit->setIcon(ui::LineIcon(ui::Icon::Search));
    submit->setAutoDefault(false);
    submit->setEnabled(false);
    search_line->addWidget(query_, 1);
    search_line->addWidget(submit);
    layout->addLayout(search_line);
    status_ = new QLabel("Введите имя файла и нажмите Enter", this);
    status_->setObjectName("search_status");
    status_->setTextFormat(Qt::PlainText);
    status_->setWordWrap(true);
    layout->addWidget(status_);
    results_ = new QTableView(this);
    results_->setObjectName("search_results");
    results_->setModel(&model_);
    results_->setSelectionBehavior(QAbstractItemView::SelectRows);
    results_->setSelectionMode(QAbstractItemView::SingleSelection);
    results_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    results_->setShowGrid(false);
    results_->setIconSize(QSize(20, 20));
    results_->verticalHeader()->hide();
    results_->verticalHeader()->setDefaultSectionSize(36);
    results_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    results_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    results_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    layout->addWidget(results_, 1);
    auto* actions = new QHBoxLayout;
    open_ = new QPushButton("Открыть", this);
    open_->setObjectName("search_open_button");
    show_ = new QPushButton("Показать в папке", this);
    show_->setObjectName("search_show_button");
    show_->setIcon(ui::LineIcon(ui::Icon::Folder));
    auto* close = new QPushButton("Закрыть", this);
    close->setObjectName("search_close_button");
    for (auto* button : {open_, show_, close}) button->setAutoDefault(false);
    actions->addWidget(open_);
    actions->addWidget(show_);
    actions->addStretch();
    actions->addWidget(close);
    layout->addLayout(actions);
    connect(query_, &QLineEdit::returnPressed, this, [this] { search(); });
    connect(submit, &QPushButton::clicked, this, [this] { search(); });
    connect(query_, &QLineEdit::textChanged, this, [this, submit](const QString& text) {
        model_.search({});
        status_->setText("Введите имя файла и нажмите Enter");
        submit->setEnabled(!text.isEmpty());
        update_actions();
    });
    connect(results_->selectionModel(), &QItemSelectionModel::currentChanged, this, [this] { update_actions(); });
    connect(open_, &QPushButton::clicked, this, [this] { open_selected(); });
    connect(results_, &QTableView::doubleClicked, this, [this](const QModelIndex& index) {
        results_->setCurrentIndex(index);
        open_selected();
    });
    connect(show_, &QPushButton::clicked, this, [this] {
        const NodeId id = model_.node_id(results_->currentIndex());
        if (id != kInvalidNodeId) { selected_node_ = id; accept(); }
    });
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    update_actions();
    query_->setFocus();
}

void SearchDialog::search() {
    try {
        model_.search(query_->text());
        status_->setText(query_->text().isEmpty() ? "Введите имя файла и нажмите Enter" :
            model_.rowCount() == 0 ? "Ничего не найдено среди доступных файлов" :
            QString("Найдено файлов: %1").arg(model_.rowCount()));
        if (model_.rowCount() > 0) results_->setCurrentIndex(model_.index(0, 0));
    } catch (const std::exception& error) {
        model_.search({});
        status_->setText("Не удалось выполнить поиск: " + QString::fromUtf8(error.what()));
    }
    update_actions();
}

void SearchDialog::update_actions() {
    const bool selected = model_.node_id(results_->currentIndex()) != kInvalidNodeId;
    open_->setEnabled(selected);
    show_->setEnabled(selected);
}

void SearchDialog::open_selected() {
    const NodeId id = model_.node_id(results_->currentIndex());
    if (id == kInvalidNodeId) return;
    try { FileOpener::open(vfs_, id, user_, launcher_); }
    catch (const std::exception& error) {
        QMessageBox::warning(this, "Не удалось открыть файл", QString::fromUtf8(error.what()));
    }
}

NodeId SearchDialog::selected_node() const noexcept { return selected_node_; }
