/* gbp-make-template-provider.c
 *
 * Copyright 2017 Matthew Leeds <mleeds@redhat.com>
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

#define G_LOG_DOMAIN "gbp-make-template-provider"

#include "config.h"

#include <libide-projects.h>

#include "gbp-make-template-provider.h"

struct _GbpMakeTemplateProvider
{
  GObject parent_instance;
};

static void
template_provider_iface_init (IdeTemplateProviderInterface *iface)
{
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpMakeTemplateProvider, gbp_make_template_provider, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_TEMPLATE_PROVIDER, template_provider_iface_init))

static void
gbp_make_template_provider_class_init (GbpMakeTemplateProviderClass *klass)
{
}

static void
gbp_make_template_provider_init (GbpMakeTemplateProvider *self)
{
}
