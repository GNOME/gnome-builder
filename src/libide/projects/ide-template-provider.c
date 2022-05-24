/* ide-template-provider.c
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

#define G_LOG_DOMAIN "ide-template-provider"

#include "config.h"

#include "ide-template-provider.h"

G_DEFINE_INTERFACE (IdeTemplateProvider, ide_template_provider, G_TYPE_OBJECT)

static GList *
ide_template_provider_real_get_project_templates (IdeTemplateProvider *self)
{
  return NULL;
}

static void
ide_template_provider_default_init (IdeTemplateProviderInterface *iface)
{
  iface->get_project_templates = ide_template_provider_real_get_project_templates;
}

/**
 * ide_template_provider_get_project_templates:
 * @self: An #IdeTemplateProvider
 *
 * Gets a list of templates for this provider.
 *
 * Plugins should implement this interface to feed #IdeProjectTemplate's into
 * the project creation workflow.
 *
 * Returns: (transfer full) (element-type Ide.ProjectTemplate): a #GList of
 *   #IdeProjectTemplate instances.
 */
GList *
ide_template_provider_get_project_templates (IdeTemplateProvider *self)
{
  g_return_val_if_fail (IDE_IS_TEMPLATE_PROVIDER (self), NULL);

  return IDE_TEMPLATE_PROVIDER_GET_IFACE (self)->get_project_templates (self);
}
