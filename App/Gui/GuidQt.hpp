#pragma once
#include <QComboBox>
#include <QMetaType>
#include <QString>
#include "Common/Guid.hpp"
Q_DECLARE_METATYPE(Guid)
inline QString GuidText(Guid id) {
  return QString::fromStdString(GuidToString(id));
}
inline QString GuidLabel(Guid id) {
  return id ? GuidText(id).left(8) + QString("…") : QString("—");
}
inline void UpdateGuidChoiceTooltip(QComboBox* choices) {
  const auto value = choices->currentData();
  choices->setToolTip(value.metaType() == QMetaType::fromType<Guid>()
                          ? GuidText(value.value<Guid>()) : QString());
}
inline void GuidChoiceTooltips(QComboBox* choices) {
  if (!choices->property("vfs_guid_tooltip_bound").toBool()) {
    choices->setProperty("vfs_guid_tooltip_bound", true);
    QObject::connect(choices, &QComboBox::currentIndexChanged, choices,
                     [choices](int) { UpdateGuidChoiceTooltip(choices); });
  }
  for (int i = 0; i < choices->count(); ++i) {
    const auto value = choices->itemData(i);
    if (value.metaType() == QMetaType::fromType<Guid>())
      choices->setItemData(i, GuidText(value.value<Guid>()), Qt::ToolTipRole);
  }
  UpdateGuidChoiceTooltip(choices);
}
