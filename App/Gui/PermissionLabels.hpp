#pragma once
#include <QString>
#include <QStringList>

#include "Project/ProjectService.hpp"
inline QString RoleLabel(const User& u) {
  if (u.role == UserRole::SuperAdmin) return "Суперадминистратор";
  if (u.role == UserRole::Admin)
    return QString("Администратор · уровень %1").arg(u.admin_level);
  return "Пользователь";
}
inline QString DecisionLabel(PermissionDecision d) {
  return d == PermissionDecision::Allow  ? "Разрешено"
         : d == PermissionDecision::Deny ? "Запрещено"
                                         : "Не задано";
}
inline QString CapabilityLabel(std::uint8_t c) {
  QStringList out;
  if (HasCapability(c, AdminCapability::ReadContent)) out << "Чтение";
  if (HasCapability(c, AdminCapability::WriteContent)) out << "Запись";
  if (HasCapability(c, AdminCapability::ManageAccess)) out << "Доступ";
  if (HasCapability(c, AdminCapability::DelegateAdministration))
    out << "Делегирование";
  return out.join(" · ");
}
