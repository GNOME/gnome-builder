/* ide-object.h
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

#include <gio/gio.h>

#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_OBJECT (ide_object_get_type())

typedef enum
{
  IDE_OBJECT_START,
  IDE_OBJECT_END,
  IDE_OBJECT_BEFORE_SIBLING,
  IDE_OBJECT_AFTER_SIBLING,
} IdeObjectLocation;

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeObject, ide_object, IDE, OBJECT, GObject)

struct _IdeObjectClass
{
  GObjectClass parent_class;

  void     (*destroy)    (IdeObject         *self);
  void     (*add)        (IdeObject         *self,
                          IdeObject         *sibling,
                          IdeObject         *child,
                          IdeObjectLocation  location);
  void     (*remove)     (IdeObject         *self,
                          IdeObject         *child);
  void     (*parent_set) (IdeObject         *self,
                          IdeObject         *parent);
  char    *(*repr)       (IdeObject         *self);

  /*< private */
  gpointer _reserved[16];
};

IDE_AVAILABLE_IN_ALL
gpointer      ide_object_new                    (GType               type,
                                                 IdeObject          *parent) G_GNUC_WARN_UNUSED_RESULT;
IDE_AVAILABLE_IN_ALL
GCancellable *ide_object_ref_cancellable        (IdeObject          *self) G_GNUC_WARN_UNUSED_RESULT;
IDE_AVAILABLE_IN_ALL
IdeObject    *ide_object_get_parent             (IdeObject          *self) G_GNUC_WARN_UNUSED_RESULT;
IDE_AVAILABLE_IN_ALL
IdeObject    *ide_object_ref_parent             (IdeObject          *self) G_GNUC_WARN_UNUSED_RESULT;
IDE_AVAILABLE_IN_ALL
IdeObject    *ide_object_ref_root               (IdeObject          *self) G_GNUC_WARN_UNUSED_RESULT;
IDE_AVAILABLE_IN_ALL
gboolean      ide_object_is_root                (IdeObject          *self);
IDE_AVAILABLE_IN_ALL
void          ide_object_lock                   (IdeObject          *self);
IDE_AVAILABLE_IN_ALL
void          ide_object_unlock                 (IdeObject          *self);
IDE_AVAILABLE_IN_ALL
void          ide_object_add                    (IdeObject          *self,
                                                 IdeObject          *sibling,
                                                 IdeObject          *child,
                                                 IdeObjectLocation   location);
IDE_AVAILABLE_IN_ALL
void          ide_object_append                 (IdeObject          *self,
                                                 IdeObject          *child);
IDE_AVAILABLE_IN_ALL
void          ide_object_prepend                (IdeObject          *self,
                                                 IdeObject          *child);
IDE_AVAILABLE_IN_ALL
void          ide_object_insert_before          (IdeObject          *self,
                                                 IdeObject          *sibling,
                                                 IdeObject          *child);
IDE_AVAILABLE_IN_ALL
void          ide_object_insert_after           (IdeObject          *self,
                                                 IdeObject          *sibling,
                                                 IdeObject          *child);
IDE_AVAILABLE_IN_ALL
void          ide_object_insert_sorted          (IdeObject          *self,
                                                 IdeObject          *child,
                                                 GCompareDataFunc    func,
                                                 gpointer            user_data);
IDE_AVAILABLE_IN_ALL
void          ide_object_remove                 (IdeObject          *self,
                                                 IdeObject          *child);
IDE_AVAILABLE_IN_ALL
void          ide_object_foreach                (IdeObject          *self,
                                                 GFunc               callback,
                                                 gpointer            user_data);
IDE_AVAILABLE_IN_ALL
gboolean      ide_object_set_error_if_destroyed (IdeObject          *self,
                                                 GError            **error);
IDE_AVAILABLE_IN_ALL
void          ide_object_destroy                (IdeObject          *self);
IDE_AVAILABLE_IN_ALL
void          ide_object_destroyed              (IdeObject         **self);
IDE_AVAILABLE_IN_ALL
guint         ide_object_get_position           (IdeObject          *self);
IDE_AVAILABLE_IN_ALL
guint         ide_object_get_n_children         (IdeObject          *self);
IDE_AVAILABLE_IN_ALL
IdeObject    *ide_object_get_nth_child          (IdeObject          *self,
                                                 guint               nth);
IDE_AVAILABLE_IN_ALL
gpointer      ide_object_get_child_typed        (IdeObject          *self,
                                                 GType               type);
IDE_AVAILABLE_IN_ALL
GPtrArray    *ide_object_get_children_typed     (IdeObject          *self,
                                                 GType               type);
IDE_AVAILABLE_IN_ALL
gpointer      ide_object_ensure_child_typed     (IdeObject          *self,
                                                 GType               type);
IDE_AVAILABLE_IN_ALL
void          ide_object_notify_in_main         (gpointer            instance,
                                                 GParamSpec         *pspec);
IDE_AVAILABLE_IN_ALL
void          ide_object_notify_by_pspec        (gpointer            instance,
                                                 GParamSpec         *pspec);
IDE_AVAILABLE_IN_ALL
gboolean      ide_object_in_destruction         (IdeObject          *self);
IDE_AVAILABLE_IN_ALL
gchar        *ide_object_repr                   (IdeObject          *self);
IDE_AVAILABLE_IN_ALL
void          ide_object_log                    (gpointer            instance,
                                                 GLogLevelFlags      level,
                                                 const gchar        *domain,
                                                 const gchar        *format,
                                                 ...) G_GNUC_PRINTF (4, 5);
IDE_AVAILABLE_IN_ALL
gboolean      ide_object_check_ready            (IdeObject          *self,
                                                 GError            **error);

#ifdef __cplusplus
# define ide_object_message(instance, format, ...) ide_object_log(instance, G_LOG_LEVEL_MESSAGE, G_LOG_DOMAIN, format __VA_OPT__(,) __VA_ARGS__)
# define ide_object_warning(instance, format, ...) ide_object_log(instance, G_LOG_LEVEL_WARNING, G_LOG_DOMAIN, format __VA_OPT__(,) __VA_ARGS__)
# define ide_object_debug(instance, format, ...)   ide_object_log(instance, G_LOG_LEVEL_DEBUG,   G_LOG_DOMAIN, format __VA_OPT__(,) __VA_ARGS__)
#else
# define ide_object_message(instance, format, ...) ide_object_log(instance, G_LOG_LEVEL_MESSAGE, G_LOG_DOMAIN, format, ##__VA_ARGS__)
# define ide_object_warning(instance, format, ...) ide_object_log(instance, G_LOG_LEVEL_WARNING, G_LOG_DOMAIN, format, ##__VA_ARGS__)
# define ide_object_debug(instance, format, ...)   ide_object_log(instance, G_LOG_LEVEL_DEBUG,   G_LOG_DOMAIN, format, ##__VA_ARGS__)
#endif

G_END_DECLS
