#include "GuidQt.hpp"
#include "ProjectStore.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include "Project/ProjectCodec.hpp"
#include "Project/ProjectState.hpp"
namespace {
constexpr qint64 kMaxBytes = 64 * 1024 * 1024;
QString Normal(const QString& path) {
  QFileInfo f(path);
  auto c = f.canonicalFilePath();
  return c.isEmpty() ? QDir::cleanPath(f.absoluteFilePath()) : c;
}
}  // namespace
QByteArray ProjectStore::encode(const ProjectService& p, UserId a) {
  return QByteArray::fromStdString(p.save(a));
}
std::unique_ptr<ProjectService> ProjectStore::decode(const QByteArray& bytes,
                                                     const ProjectService& p,
                                                     UserId a) {
  p.require_superadmin(a);
  if (bytes.size() > kMaxBytes)
    throw std::length_error("Project exceeds 64 MiB");
  return p.load(bytes.toStdString(), a);
}
void ProjectStore::write(const QString& path, const ProjectService& p) {
  if (path.isEmpty()) throw std::invalid_argument("Empty project path");
  auto out = Normal(path);
  p.state_->vfs.nodes_->for_each([&](auto, const FSNode& n) {
    if (n.type == FSType::File &&
        Normal(QString::fromStdString(n.physical_path)) == out)
      throw std::runtime_error(
          "Project output cannot overwrite an attached physical file");
  });
  auto bytes = QByteArray::fromStdString(ProjectCodec::encode(p));
  QSaveFile f(path);
  f.setDirectWriteFallback(false);
  if (!f.open(QIODevice::WriteOnly))
    throw std::runtime_error(f.errorString().toStdString());
  if (f.write(bytes) != bytes.size() || !f.commit())
    throw std::runtime_error(f.errorString().toStdString());
}
void ProjectStore::save(const QString& path, const ProjectService& p,
                        UserId a) {
  p.require_superadmin(a);
  write(path, p);
}
std::unique_ptr<ProjectService> ProjectStore::load(const QString& path,
                                                   const ProjectService& p,
                                                   UserId a) {
  p.require_superadmin(a);
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly))
    throw std::runtime_error(f.errorString().toStdString());
  if (f.size() > kMaxBytes) throw std::length_error("Project exceeds 64 MiB");
  auto bytes = f.readAll();
  if (f.error() != QFileDevice::NoError)
    throw std::runtime_error(f.errorString().toStdString());
  return decode(bytes, p, a);
}
std::function<void(const QString&)> ProjectStore::autosave_writer(
    std::function<const ProjectService&()> provider) {
  return [provider = std::move(provider)](const QString& path) {
    write(path, provider());
  };
}
