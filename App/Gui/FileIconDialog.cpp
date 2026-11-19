#include "FileIconDialog.hpp"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "Gui/UiTheme.hpp"

FileIconDialog::FileIconDialog(FileIcon current, QWidget* parent) : QDialog(parent) {
    setWindowTitle("Значок файла");
    setPalette(ui::DarkPalette());
    setStyleSheet(ui::DarkStyleSheet());
    setMinimumWidth(440);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(22, 20, 22, 18);
    layout->setSpacing(16);
    auto* title = new QLabel("Выберите значок файла", this);
    title->setObjectName("dialog_title");
    layout->addWidget(title);
    auto* grid = new QGridLayout;
    choices_ = new QButtonGroup(this);
    struct Option { FileIcon icon; const char* name; ui::Icon glyph; };
    const Option options[] = {
        {FileIcon::Default, "Файл", ui::Icon::File},
        {FileIcon::Text, "Текст", ui::Icon::Text},
        {FileIcon::Image, "Изображение", ui::Icon::Image},
        {FileIcon::Audio, "Аудио", ui::Icon::Audio},
        {FileIcon::Video, "Видео", ui::Icon::Video},
        {FileIcon::Code, "Код", ui::Icon::Code}
    };
    for (int i = 0; i < 6; ++i) {
        const auto& option = options[i];
        auto* button = new QPushButton(QString::fromUtf8(option.name), this);
        button->setObjectName(QString("file_icon_%1").arg(i));
        button->setIcon(ui::LineIcon(option.glyph));
        button->setIconSize(QSize(28, 28));
        button->setMinimumHeight(68);
        button->setCheckable(true);
        button->setAutoDefault(false);
        choices_->addButton(button, static_cast<int>(option.icon));
        button->setChecked(option.icon == current);
        grid->addWidget(button, i / 3, i % 3);
    }
    layout->addLayout(grid);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->setObjectName("icon_dialog_buttons");
    buttons->button(QDialogButtonBox::Ok)->setText("Применить");
    buttons->button(QDialogButtonBox::Cancel)->setText("Отмена");
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

FileIcon FileIconDialog::selected_icon() const {
    const int id = choices_->checkedId();
    return id >= 0 && id <= static_cast<int>(FileIcon::Code)
        ? static_cast<FileIcon>(id) : FileIcon::Default;
}
