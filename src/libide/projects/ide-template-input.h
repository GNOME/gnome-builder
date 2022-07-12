/* ide-template-input.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#include <tmpl-glib.h>

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_TEMPLATE_INPUT (ide_template_input_get_type())

typedef enum
{
  IDE_TEMPLATE_INPUT_VALID          = 0,
  IDE_TEMPLATE_INPUT_INVAL_NAME     = 1 << 0,
  IDE_TEMPLATE_INPUT_INVAL_APP_ID   = 1 << 1,
  IDE_TEMPLATE_INPUT_INVAL_LOCATION = 1 << 2,
  IDE_TEMPLATE_INPUT_INVAL_LANGUAGE = 1 << 3,
  IDE_TEMPLATE_INPUT_INVAL_TEMPLATE = 1 << 4,
} IdeTemplateInputValidation;

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeTemplateInput, ide_template_input, IDE, TEMPLATE_INPUT, GObject)

IDE_AVAILABLE_IN_ALL
IdeTemplateInput           *ide_template_input_new                     (void);
IDE_AVAILABLE_IN_ALL
const char                 *ide_template_input_get_author              (IdeTemplateInput     *self);
IDE_AVAILABLE_IN_ALL
void                        ide_template_input_set_author              (IdeTemplateInput     *self,
                                                                        const char           *author);
IDE_AVAILABLE_IN_ALL
GFile                      *ide_template_input_get_directory           (IdeTemplateInput     *self);
IDE_AVAILABLE_IN_ALL
void                        ide_template_input_set_directory           (IdeTemplateInput     *self,
                                                                        GFile                *directory);
IDE_AVAILABLE_IN_ALL
const char                 *ide_template_input_get_language            (IdeTemplateInput     *self);
IDE_AVAILABLE_IN_ALL
void                        ide_template_input_set_language            (IdeTemplateInput     *self,
                                                                        const char           *language);
IDE_AVAILABLE_IN_ALL
gboolean                    ide_template_input_get_use_version_control (IdeTemplateInput     *self);
IDE_AVAILABLE_IN_ALL
void                        ide_template_input_set_use_version_control (IdeTemplateInput     *self,
                                                                        gboolean              use_version_control);
IDE_AVAILABLE_IN_ALL
const char                 *ide_template_input_get_name                (IdeTemplateInput     *self);
IDE_AVAILABLE_IN_ALL
void                        ide_template_input_set_name                (IdeTemplateInput     *self,
                                                                        const char           *name);
IDE_AVAILABLE_IN_ALL
const char                 *ide_template_input_get_app_id              (IdeTemplateInput     *self);
IDE_AVAILABLE_IN_ALL
void                        ide_template_input_set_app_id              (IdeTemplateInput     *self,
                                                                        const char           *app_id);
IDE_AVAILABLE_IN_ALL
const char                 *ide_template_input_get_project_version     (IdeTemplateInput     *self);
IDE_AVAILABLE_IN_ALL
void                        ide_template_input_set_project_version     (IdeTemplateInput     *self,
                                                                        const char           *project_version);
IDE_AVAILABLE_IN_ALL
const char                 *ide_template_input_get_license_name        (IdeTemplateInput     *self);
IDE_AVAILABLE_IN_ALL
void                        ide_template_input_set_license_name        (IdeTemplateInput     *self,
                                                                        const char           *license_name);
IDE_AVAILABLE_IN_ALL
const char                 *ide_template_input_get_template            (IdeTemplateInput     *self);
IDE_AVAILABLE_IN_ALL
void                        ide_template_input_set_template            (IdeTemplateInput     *self,
                                                                        const char           *template);
IDE_AVAILABLE_IN_ALL
GListModel                 *ide_template_input_get_templates_model     (IdeTemplateInput     *self);
IDE_AVAILABLE_IN_ALL
GListModel                 *ide_template_input_get_languages_model     (IdeTemplateInput     *self);
IDE_AVAILABLE_IN_ALL
GListModel                 *ide_template_input_get_licenses_model      (IdeTemplateInput     *self);
IDE_AVAILABLE_IN_ALL
IdeTemplateInputValidation  ide_template_input_validate                (IdeTemplateInput     *self);
IDE_AVAILABLE_IN_ALL
char                       *ide_template_input_get_license_path        (IdeTemplateInput     *self);
IDE_AVAILABLE_IN_ALL
void                        ide_template_input_expand_async            (IdeTemplateInput     *self,
                                                                        IdeContext           *context,
                                                                        GCancellable         *cancellable,
                                                                        GAsyncReadyCallback   callback,
                                                                        gpointer              user_data);
IDE_AVAILABLE_IN_ALL
GFile                      *ide_template_input_expand_finish           (IdeTemplateInput     *self,
                                                                        GAsyncResult         *result,
                                                                        GError              **error);

G_END_DECLS
