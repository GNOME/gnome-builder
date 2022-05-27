/* ide-template-provider.h
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#include <libide-core.h>

#include "ide-project-template.h"

G_BEGIN_DECLS

#define IDE_TYPE_TEMPLATE_PROVIDER (ide_template_provider_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (IdeTemplateProvider, ide_template_provider, IDE, TEMPLATE_PROVIDER, GObject)

struct _IdeTemplateProviderInterface
{
  GTypeInterface parent_iface;

  GList *(*get_project_templates) (IdeTemplateProvider *self);
};

IDE_AVAILABLE_IN_ALL
GList *ide_template_provider_get_project_templates (IdeTemplateProvider *self);

G_END_DECLS
