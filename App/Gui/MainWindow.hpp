#pragma once

#include <QMainWindow>
#include <memory>

#include "Gui/FileOpener.hpp"
#include "Gui/VfsTreeModel.hpp"
#include "VirtualFileSystem/UserManager.hpp"

class QLabel;
class QComboBox;
class UserChoiceModel;
class QPushButton;
class QTableView;
class QTreeView;
class QToolButton;
class QAbstractItemView;
class QCheckBox;
class QCloseEvent;
class QAction;
class AutosaveController;
class ProjectService;

struct MainWindowOptions {
  int autosave_interval_ms = 30000;
  FileOpener::Launcher file_launcher;
};

class MainWindow : public QMainWindow {
 public:
  explicit MainWindow(QWidget* parent = nullptr,
                      const MainWindowOptions& options = {});
  ~MainWindow() override;

 private:
  void show_directory(NodeId id);
  void create_directory();
  void file_action(bool creating);
  void rename_node();
  void delete_node();
  void choose_icon();
  void context_menu(QAbstractItemView* view, const QPoint& point);
  void change_user(UserId id);
  void reload_users();
  void refresh_directory();
  void update_actions();
  User current_user() const;
  QString directory_path(NodeId id) const;
  void configure_model();
  void search_files();
  void open_node(const QModelIndex& index);
  bool save_project(bool save_as);
  void load_project();
  void install_project(std::unique_ptr<ProjectService> project,
                       const QString& path);
  bool confirm_replacement();
  void update_project_status();
  void closeEvent(QCloseEvent* event) override;

  MainWindowOptions options_;
  std::unique_ptr<ProjectService> project_;
  std::unique_ptr<VfsTreeModel> model_;
  std::unique_ptr<AutosaveController> autosave_;
  UserId current_user_id_ = kInvalidUserId;
  NodeId current_directory_ = kInvalidNodeId;
  NodeId selected_node_ = kInvalidNodeId;
  bool model_resetting_ = false;
  bool project_loading_ = false;

  QTreeView* tree_ = nullptr;
  QTableView* contents_ = nullptr;
  QLabel* path_ = nullptr;
  QPushButton* up_ = nullptr;
  QPushButton* create_directory_ = nullptr;
  QPushButton* create_file_ = nullptr;
  QPushButton* attach_file_ = nullptr;
  QPushButton* manage_ = nullptr;
  QComboBox* user_selector_ = nullptr;
  std::unique_ptr<UserChoiceModel> user_choices_;
  QLabel* user_label_ = nullptr;
  QLabel* empty_ = nullptr;
  QToolButton* rename_node_ = nullptr;
  QToolButton* delete_node_ = nullptr;
  QToolButton* choose_icon_ = nullptr;
  QAction* save_project_ = nullptr;
  QAction* save_project_as_ = nullptr;
  QAction* load_project_ = nullptr;
  QCheckBox* autosave_checkbox_ = nullptr;
  QLabel* project_status_ = nullptr;
};
