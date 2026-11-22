#include "GuidQt.hpp"
#include "AutosaveController.hpp"

#include <stdexcept>
#include <utility>

AutosaveController::AutosaveController(int interval, Writer writer,
                                       QObject* parent)
    : QObject(parent), timer_(this), writer_(std::move(writer)) {
  if (interval <= 0 || !writer_)
    throw std::invalid_argument("Invalid autosave configuration");
  timer_.setInterval(interval);
  connect(&timer_, &QTimer::timeout, this, [this] {
    if (dirty_ && !path_.isEmpty()) flush();
  });
  timer_.start();
}
void AutosaveController::mark_dirty() {
  dirty_ = true;
  if (status_handler_) status_handler_();
}
void AutosaveController::mark_saved(const QString& path) {
  path_ = path;
  dirty_ = false;
  error_.clear();
  if (status_handler_) status_handler_();
}
void AutosaveController::set_enabled(bool enabled) {
  enabled_ = enabled;
  if (enabled && pause_depth_ == 0)
    timer_.start();
  else
    timer_.stop();
  if (status_handler_) status_handler_();
}
void AutosaveController::pause() {
  ++pause_depth_;
  timer_.stop();
}
void AutosaveController::resume() {
  if (pause_depth_ == 0) return;
  --pause_depth_;
  if (pause_depth_ == 0 && enabled_) timer_.start();
}
bool AutosaveController::enabled() const noexcept { return enabled_; }
bool AutosaveController::dirty() const noexcept { return dirty_; }
const QString& AutosaveController::path() const noexcept { return path_; }
const QString& AutosaveController::error() const noexcept { return error_; }
bool AutosaveController::flush() {
  if (!dirty_) return true;
  if (path_.isEmpty()) return false;
  try {
    writer_(path_);
    dirty_ = false;
    error_.clear();
  } catch (const std::exception& e) {
    error_ = QString::fromUtf8(e.what());
    if (status_handler_) status_handler_();
    return false;
  }
  if (status_handler_) status_handler_();
  return true;
}
void AutosaveController::set_status_handler(std::function<void()> handler) {
  status_handler_ = std::move(handler);
}
