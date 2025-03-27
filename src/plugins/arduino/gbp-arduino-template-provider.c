/*
 * gbp-arduino-template-provider.c
 *
 * Copyright 2025 Nokse <nokse@posteo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define G_LOG_DOMAIN "gbp-arduino-template-provider"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-projects.h>

#include "gbp-arduino-template-provider.h"
#include "gbp-arduino-template.h"

struct _GbpArduinoTemplateProvider
{
  GObject parent_instance;
};

GList *
gbp_arduino_template_provider_get_project_templates (IdeTemplateProvider *provider)
{
  GList *list = NULL;

  g_assert (GBP_IS_ARDUINO_TEMPLATE_PROVIDER (provider));

  list = g_list_prepend (list,
                         g_object_new (GBP_TYPE_ARDUINO_TEMPLATE,
                                       "id", "arduino-templates:empty",
                                       "name", _ ("Arduino Sketch Project"),
                                       "description", _ ("An empty arduino sketch project"),
                                       "languages", IDE_STRV_INIT ("C", "C++"),
                                       "priority", 2000,
                                       NULL));

  return list;
}

static void
template_provider_iface_init (IdeTemplateProviderInterface *iface)
{
  iface->get_project_templates = gbp_arduino_template_provider_get_project_templates;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpArduinoTemplateProvider, gbp_arduino_template_provider, G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE (IDE_TYPE_TEMPLATE_PROVIDER, template_provider_iface_init))

static void
gbp_arduino_template_provider_class_init (GbpArduinoTemplateProviderClass *klass)
{
}

static void
gbp_arduino_template_provider_init (GbpArduinoTemplateProvider *self)
{
}

