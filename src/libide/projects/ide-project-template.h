/* ide-project-template.h
 *
 * Copyright 2015-2019 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_PROJECTS_INSIDE) && !defined (IDE_PROJECTS_COMPILATION)
# error "Only <libide-projects.h> can be included directly."
#endif

#include <gtk/gtk.h>
#include <tmpl-glib.h>

#include <libide-core.h>

#include "ide-template-base.h"
#include "ide-template-input.h"

G_BEGIN_DECLS

#define IDE_TYPE_PROJECT_TEMPLATE (ide_project_template_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeProjectTemplate, ide_project_template, IDE, PROJECT_TEMPLATE, IdeTemplateBase)

struct _IdeProjectTemplateClass
{
  IdeTemplateBaseClass parent_instance;

  gboolean    (*validate_name)   (IdeProjectTemplate   *self,
                                  const char           *name);
  gboolean    (*validate_app_id) (IdeProjectTemplate   *self,
                                  const char           *app_id);
  void        (*expand_async)    (IdeProjectTemplate   *self,
                                  IdeTemplateInput     *input,
                                  TmplScope            *scope,
                                  GCancellable         *cancellable,
                                  GAsyncReadyCallback   callback,
                                  gpointer              user_data);
  gboolean    (*expand_finish)   (IdeProjectTemplate   *self,
                                  GAsyncResult         *result,
                                  GError              **error);
};

IDE_AVAILABLE_IN_ALL
const char         *ide_project_template_get_id          (IdeProjectTemplate   *self);
IDE_AVAILABLE_IN_ALL
int                 ide_project_template_get_priority    (IdeProjectTemplate   *self);
IDE_AVAILABLE_IN_ALL
const char         *ide_project_template_get_name        (IdeProjectTemplate   *self);
IDE_AVAILABLE_IN_ALL
const char         *ide_project_template_get_description (IdeProjectTemplate   *self);
IDE_AVAILABLE_IN_ALL
const char * const *ide_project_template_get_languages   (IdeProjectTemplate   *self);
IDE_AVAILABLE_IN_ALL
void                ide_project_template_expand_async    (IdeProjectTemplate   *self,
                                                          IdeTemplateInput     *input,
                                                          TmplScope            *scope,
                                                          GCancellable         *cancellable,
                                                          GAsyncReadyCallback   callback,
                                                          gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean            ide_project_template_expand_finish   (IdeProjectTemplate   *self,
                                                          GAsyncResult         *result,
                                                          GError              **error);
IDE_AVAILABLE_IN_ALL
int                 ide_project_template_compare         (IdeProjectTemplate   *a,
                                                          IdeProjectTemplate   *b);
IDE_AVAILABLE_IN_ALL
gboolean            ide_project_template_validate_name   (IdeProjectTemplate   *self,
                                                          const char           *name);
IDE_AVAILABLE_IN_ALL
gboolean            ide_project_template_validate_app_id (IdeProjectTemplate   *self,
                                                          const char           *app_id);

G_END_DECLS
