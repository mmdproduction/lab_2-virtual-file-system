#pragma once

#include <QDialog>

#include "Gui/FileOpener.hpp"
#include "Gui/FileSearchModel.hpp"

class QLineEdit;
class QTableView;
class QPushButton;
class QLabel;

class SearchDialog : public QDialog {
 public:
  SearchDialog(const ProjectService& vfs, UserId user,
               FileOpener::Launcher launcher, QWidget* parent = nullptr);
  NodeId selected_node() const noexcept;

 private:
  void search();
  void update_actions();
  void open_selected();

  const ProjectService& vfs_;
  UserId user_;
  FileOpener::Launcher launcher_;
  FileSearchModel model_;
  QLineEdit* query_ = nullptr;
  QTableView* results_ = nullptr;
  QLabel* status_ = nullptr;
  QPushButton* open_ = nullptr;
  QPushButton* show_ = nullptr;
  NodeId selected_node_ = kInvalidNodeId;
};
