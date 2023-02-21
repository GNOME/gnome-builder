/* ide-context.h
 *
 * Copyright 2014-2019 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if !defined (IDE_CORE_INSIDE) && !defined (IDE_CORE_COMPILATION)
# error "Only <libide-core.h> can be included directly."
#endif

#include "ide-action-muxer.h"
#include "ide-object.h"
#include "ide-settings.h"

G_BEGIN_DECLS

#define IDE_TYPE_CONTEXT (ide_context_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeContext, ide_context, IDE, CONTEXT, IdeObject)

IDE_AVAILABLE_IN_ALL
IdeContext     *ide_context_new                  (void);
IDE_AVAILABLE_IN_ALL
gboolean        ide_context_has_project          (IdeContext     *self);
IDE_AVAILABLE_IN_ALL
gpointer        ide_context_peek_child_typed     (IdeContext     *self,
                                                  GType           type);
IDE_AVAILABLE_IN_ALL
gchar          *ide_context_dup_project_id       (IdeContext     *self);
IDE_AVAILABLE_IN_ALL
void            ide_context_set_project_id       (IdeContext     *self,
                                                  const gchar    *project_id);
IDE_AVAILABLE_IN_ALL
gchar          *ide_context_dup_title            (IdeContext     *self);
IDE_AVAILABLE_IN_ALL
void            ide_context_set_title            (IdeContext     *self,
                                                  const gchar    *title);
IDE_AVAILABLE_IN_ALL
GFile          *ide_context_ref_workdir          (IdeContext     *self);
IDE_AVAILABLE_IN_ALL
void            ide_context_set_workdir          (IdeContext     *self,
                                                  GFile          *workdir);
IDE_AVAILABLE_IN_ALL
GFile          *ide_context_build_file           (IdeContext     *self,
                                                  const gchar    *path);
IDE_AVAILABLE_IN_ALL
gchar          *ide_context_build_filename       (IdeContext     *self,
                                                  const gchar    *first_part,
                                                  ...) G_GNUC_NULL_TERMINATED;
IDE_AVAILABLE_IN_ALL
GFile          *ide_context_cache_file           (IdeContext     *self,
                                                  const gchar    *first_part,
                                                  ...) G_GNUC_NULL_TERMINATED;
IDE_AVAILABLE_IN_ALL
gchar          *ide_context_cache_filename       (IdeContext     *self,
                                                  const gchar    *first_part,
                                                  ...) G_GNUC_NULL_TERMINATED;
IDE_AVAILABLE_IN_ALL
GSettings      *ide_context_ref_project_settings (IdeContext     *self);
IDE_AVAILABLE_IN_ALL
IdeContext     *ide_object_ref_context           (IdeObject      *self);
IDE_AVAILABLE_IN_ALL
IdeContext     *ide_object_get_context           (IdeObject      *object);
IDE_AVAILABLE_IN_ALL
void            ide_object_set_context           (IdeObject      *object,
                                                  IdeContext     *context);
IDE_AVAILABLE_IN_ALL
void            ide_context_log                  (IdeContext     *self,
                                                  GLogLevelFlags  level,
                                                  const gchar    *domain,
                                                  const gchar    *message);
IDE_AVAILABLE_IN_44
GListModel     *ide_context_ref_logs             (IdeContext     *self);
IDE_AVAILABLE_IN_ALL
IdeActionMuxer *ide_context_ref_action_muxer     (IdeContext     *self);
IDE_AVAILABLE_IN_ALL
void            ide_context_register_settings    (IdeContext     *self,
                                                  const char     *schema_id);
IDE_AVAILABLE_IN_ALL
void            ide_context_unregister_settings  (IdeContext     *self,
                                                  const char     *schema_id);
IDE_AVAILABLE_IN_ALL
IdeSettings    *ide_context_ref_settings         (IdeContext     *self,
                                                  const char     *schema_id);

#ifdef __cplusplus
#define ide_context_warning(instance, format, ...) \
  ide_object_log(instance, G_LOG_LEVEL_WARNING, G_LOG_DOMAIN, format __VA_OPT__(,) __VA_ARGS__)
#else
#define ide_context_warning(instance, format, ...) \
  ide_object_log(instance, G_LOG_LEVEL_WARNING, G_LOG_DOMAIN, format, ##__VA_ARGS__)
#endif

G_END_DECLS
