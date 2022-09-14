/* ide-action-group.h
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define IDE_DEFINE_ACTION_GROUP(Type, prefix, ...)                                \
struct _##Type##ActionEntry {                                                     \
  const gchar *name;                                                              \
  void (*activate) (Type *self, GVariant *param);                                 \
  const gchar *parameter_type;                                                    \
  const gchar *state;                                                             \
  void (*change_state) (Type *self, GVariant *state);                             \
} prefix##_actions[] = __VA_ARGS__;                                               \
                                                                                  \
typedef struct {                                                                  \
  GVariant *state;                                                                \
  GVariant *state_hint;                                                           \
  guint enabled : 1;                                                              \
} Type##ActionInfo;                                                               \
                                                                                  \
static gboolean                                                                   \
_##prefix##_has_action (GActionGroup *group,                                      \
                        const gchar *name)                                        \
{                                                                                 \
  for (guint i = 0; i < G_N_ELEMENTS(prefix##_actions); i++)                      \
    {                                                                             \
      if (g_strcmp0 (name, prefix##_actions[i].name) == 0)                        \
        return TRUE;                                                              \
    }                                                                             \
  return FALSE;                                                                   \
}                                                                                 \
                                                                                  \
static gchar **                                                                   \
_##prefix##_list_actions (GActionGroup *group)                                    \
{                                                                                 \
  GPtrArray *ar = g_ptr_array_new ();                                             \
                                                                                  \
  for (guint i = 0; i < G_N_ELEMENTS(prefix##_actions); i++)                      \
    g_ptr_array_add (ar, g_strdup (prefix##_actions[i].name));                    \
  g_ptr_array_add (ar, NULL);                                                     \
                                                                                  \
  return (gchar **)g_ptr_array_free (ar, FALSE);                                  \
}                                                                                 \
                                                                                  \
static void                                                                       \
_##prefix##_action_info_free (gpointer data)                                      \
{                                                                                 \
  Type##ActionInfo *info = data;                                                  \
  g_clear_pointer (&info->state, g_variant_unref);                                \
  g_clear_pointer (&info->state_hint, g_variant_unref);                           \
  g_slice_free (Type##ActionInfo, info);                                          \
}                                                                                 \
                                                                                  \
static Type##ActionInfo *                                                         \
_##prefix##_get_action_info (GActionGroup *group,                                 \
                             const gchar *name)                                   \
{                                                                                 \
  g_autofree gchar *fullname = g_strdup_printf ("ACTION-INFO:%s", name);          \
  Type##ActionInfo *info = g_object_get_data (G_OBJECT (group), fullname);        \
  if (info == NULL)                                                               \
    {                                                                             \
      info = g_slice_new0 (Type##ActionInfo);                                     \
      info->enabled = TRUE;                                                       \
      for (guint i = 0; i < G_N_ELEMENTS(prefix##_actions); i++)                  \
        {                                                                         \
          if (g_strcmp0 (prefix##_actions[i].name, name) == 0)                    \
            {                                                                     \
              if (prefix##_actions[i].state != NULL)                              \
                info->state = g_variant_parse (                                   \
                  NULL, prefix##_actions[i].state, NULL, NULL, NULL);             \
              break;                                                              \
            }                                                                     \
        }                                                                         \
      g_object_set_data_full (G_OBJECT (group), fullname, info,                   \
                              _##prefix##_action_info_free);                      \
    }                                                                             \
  return info;                                                                    \
}                                                                                 \
                                                                                  \
G_GNUC_UNUSED static inline GVariant *                                            \
prefix##_get_action_state (Type *self,                                            \
                           const gchar *name)                                     \
{                                                                                 \
  Type##ActionInfo *info = _##prefix##_get_action_info (G_ACTION_GROUP (self),    \
                                                        name);                    \
  return info->state;                                                             \
}                                                                                 \
                                                                                  \
G_GNUC_UNUSED static inline void                                                  \
prefix##_set_action_state (Type *self,                                            \
                           const gchar *name,                                     \
                           GVariant *state)                                       \
{                                                                                 \
  Type##ActionInfo *info = _##prefix##_get_action_info (G_ACTION_GROUP (self),    \
                                                        name);                    \
  if (state != info->state)                                                       \
    {                                                                             \
      g_clear_pointer (&info->state, g_variant_unref);                            \
      info->state = state ? g_variant_ref_sink (state) : NULL;                    \
      g_action_group_action_state_changed (G_ACTION_GROUP (self), name, state);   \
    }                                                                             \
}                                                                                 \
                                                                                  \
G_GNUC_UNUSED static inline void                                                  \
prefix##_set_action_enabled (Type *self,                                          \
                             const gchar *name,                                   \
                             gboolean enabled)                                    \
{                                                                                 \
  Type##ActionInfo *info = _##prefix##_get_action_info (G_ACTION_GROUP (self),    \
                                                        name);                    \
  if (enabled != info->enabled)                                                   \
    {                                                                             \
      info->enabled = !!enabled;                                                  \
      g_action_group_action_enabled_changed (G_ACTION_GROUP (self),               \
                                             name, enabled);                      \
    }                                                                             \
}                                                                                 \
                                                                                  \
static void                                                                       \
_##prefix##_change_action_state (GActionGroup *group,                             \
                                 const gchar *name,                               \
                                 GVariant *state)                                 \
{                                                                                 \
  for (guint i = 0; i < G_N_ELEMENTS(prefix##_actions); i++)                      \
    {                                                                             \
      if (g_strcmp0 (name, prefix##_actions[i].name) == 0)                        \
        {                                                                         \
          if (prefix##_actions[i].change_state)                                   \
            prefix##_actions[i].change_state ((Type*)group, state);               \
          return;                                                                 \
        }                                                                         \
    }                                                                             \
}                                                                                 \
                                                                                  \
static void                                                                       \
_##prefix##_activate_action (GActionGroup *group,                                 \
                             const gchar *name,                                   \
                             GVariant *param)                                     \
{                                                                                 \
  for (guint i = 0; i < G_N_ELEMENTS(prefix##_actions); i++)                      \
    {                                                                             \
      if (g_strcmp0 (name, prefix##_actions[i].name) == 0)                        \
        {                                                                         \
          if (prefix##_actions[i].activate)                                       \
            prefix##_actions[i].activate ((Type*)group, param);                   \
          return;                                                                 \
        }                                                                         \
    }                                                                             \
}                                                                                 \
                                                                                  \
static gboolean                                                                   \
_##prefix##_query_action (GActionGroup *group,                                    \
                          const gchar *name,                                      \
                          gboolean *enabled,                                      \
                          const GVariantType **parameter_type,                    \
                          const GVariantType **state_type,                        \
                          GVariant **state_hint,                                  \
                          GVariant **state)                                       \
{                                                                                 \
  if (enabled) *enabled = FALSE;                                                  \
  if (parameter_type) *parameter_type = NULL ;                                    \
  if (state_type) *state_type = NULL ;                                            \
  if (state_hint) *state_hint = NULL ;                                            \
  if (state) *state = NULL ;                                                      \
  for (guint i = 0; i < G_N_ELEMENTS(prefix##_actions); i++)                      \
    {                                                                             \
      if (g_strcmp0 (name, prefix##_actions[i].name) == 0)                        \
        {                                                                         \
          Type##ActionInfo *info = _##prefix##_get_action_info(group, name);      \
          if (prefix##_actions[i].change_state && state_type)                     \
            *state_type = prefix##_actions[i].parameter_type ?                    \
                          G_VARIANT_TYPE(prefix##_actions[i].parameter_type) :    \
                          NULL;                                                   \
          else if (prefix##_actions[i].activate && parameter_type)                \
            *parameter_type = prefix##_actions[i].parameter_type ?                \
                              G_VARIANT_TYPE(prefix##_actions[i].parameter_type) :\
                              NULL;                                               \
          if (state_hint)                                                         \
            *state_hint = info->state_hint != NULL ?                              \
                          g_variant_ref (info->state_hint) : NULL;                \
          if (state)                                                              \
            *state = info->state != NULL ?                                        \
                     g_variant_ref (info->state) : NULL;                          \
          if (enabled)                                                            \
            *enabled = info->enabled;                                             \
          return TRUE;                                                            \
        }                                                                         \
    }                                                                             \
  return FALSE;                                                                   \
}                                                                                 \
                                                                                  \
static void                                                                       \
prefix##_init_action_group (GActionGroupInterface *iface)                         \
{                                                                                 \
  iface->has_action = _##prefix##_has_action;                                     \
  iface->list_actions = _##prefix##_list_actions;                                 \
  iface->change_action_state = _##prefix##_change_action_state;                   \
  iface->activate_action = _##prefix##_activate_action;                           \
  iface->query_action = _##prefix##_query_action;                                 \
}

G_END_DECLS
