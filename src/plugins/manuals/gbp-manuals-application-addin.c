/* gbp-manuals-application-addin.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-manuals-application-addin"

#include "config.h"

#include <glib/gi18n.h>

#include "gbp-manuals-application-addin.h"

struct _GbpManualsApplicationAddin
{
  GObject parent_instance;
};

static void
gbp_manuals_application_addin_load (IdeApplicationAddin *addin,
                                    IdeApplication      *application)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_MANUALS_APPLICATION_ADDIN (addin));
  g_assert (IDE_IS_APPLICATION (application));

}

static void
gbp_manuals_application_addin_unload (IdeApplicationAddin *addin,
                                   IdeApplication      *application)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_MANUALS_APPLICATION_ADDIN (addin));
  g_assert (IDE_IS_APPLICATION (application));

}

static void
app_addin_iface_init (IdeApplicationAddinInterface *iface)
{
  iface->load = gbp_manuals_application_addin_load;
  iface->unload = gbp_manuals_application_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpManualsApplicationAddin, gbp_manuals_application_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_APPLICATION_ADDIN, app_addin_iface_init))

static void
gbp_manuals_application_addin_class_init (GbpManualsApplicationAddinClass *klass)
{
}

static void
gbp_manuals_application_addin_init (GbpManualsApplicationAddin *self)
{
}
