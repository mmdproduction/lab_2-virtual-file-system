#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <functional>
#include <cstddef>

class AutosaveController : public QObject {
public:
    using Writer = std::function<void(const QString&)>;
    AutosaveController(int interval_ms, Writer writer, QObject* parent = nullptr);
    void mark_dirty();
    void mark_saved(const QString& path);
    void set_enabled(bool enabled);
    void pause();
    void resume();
    bool enabled() const noexcept;
    bool dirty() const noexcept;
    const QString& path() const noexcept;
    const QString& error() const noexcept;
    bool flush();
    void set_status_handler(std::function<void()> handler);
private:
    QTimer timer_;
    Writer writer_;
    std::function<void()> status_handler_;
    QString path_;
    QString error_;
    std::size_t pause_depth_ = 0;
    bool enabled_ = true;
    bool dirty_ = false;
};
