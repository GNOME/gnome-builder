/* ide-template-base.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#include <tmpl-glib.h>

G_BEGIN_DECLS

#define IDE_TYPE_TEMPLATE_BASE (ide_template_base_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeTemplateBase, ide_template_base, IDE, TEMPLATE_BASE, GObject)

struct _IdeTemplateBaseClass
{
  GObjectClass parent_class;
};

TmplTemplateLocator *ide_template_base_get_locator       (IdeTemplateBase       *self);
void                 ide_template_base_set_locator       (IdeTemplateBase       *self,
                                                          TmplTemplateLocator   *locator);
void                 ide_template_base_add_resource      (IdeTemplateBase       *self,
                                                          const gchar           *resource_path,
                                                          GFile                 *destination,
                                                          TmplScope             *scope,
                                                          gint                   mode);
void                 ide_template_base_add_path          (IdeTemplateBase       *self,
                                                          const gchar           *path,
                                                          GFile                 *destination,
                                                          TmplScope             *scope,
                                                          gint                   mode);
void                 ide_template_base_expand_all_async  (IdeTemplateBase       *self,
                                                          GCancellable          *cancellable,
                                                          GAsyncReadyCallback    callback,
                                                          gpointer               user_data);
gboolean             ide_template_base_expand_all_finish (IdeTemplateBase       *self,
                                                          GAsyncResult          *result,
                                                          GError               **error);
void                 ide_template_base_reset             (IdeTemplateBase       *self);

G_END_DECLS
