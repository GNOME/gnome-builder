/* ide-template-locator.h
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

#define IDE_TYPE_TEMPLATE_LOCATOR (ide_template_locator_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeTemplateLocator, ide_template_locator, IDE, TEMPLATE_LOCATOR, TmplTemplateLocator)

struct _IdeTemplateLocatorClass
{
  TmplTemplateLocatorClass parent_class;
};

IDE_AVAILABLE_IN_ALL
IdeTemplateLocator *ide_template_locator_new              (void);
IDE_AVAILABLE_IN_ALL
const char         *ide_template_locator_get_license_text (IdeTemplateLocator *self);
IDE_AVAILABLE_IN_ALL
void                ide_template_locator_set_license_text (IdeTemplateLocator *self,
                                                           const char         *license_text);

G_END_DECLS
