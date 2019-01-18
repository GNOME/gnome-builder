/* gbp-meson-config-view-addin.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-meson-config-view-addin"

#include "config.h"

#include <dazzle.h>
#include <glib/gi18n.h>
#include <libide-foundry.h>
#include <libide-gui.h>

#include "gbp-meson-build-system.h"
#include "gbp-meson-config-view-addin.h"

struct _GbpMesonConfigViewAddin
{
  GObject parent_instance;
};

static void
gbp_meson_config_view_addin_load (IdeConfigViewAddin *addin,
                                  DzlPreferences     *preferences,
                                  IdeConfig          *config)
{
  IdeBuildSystem *build_system;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CONFIG_VIEW_ADDIN (addin));
  g_assert (DZL_IS_PREFERENCES (preferences));
  g_assert (IDE_IS_CONFIG (config));

  context = ide_object_get_context (IDE_OBJECT (config));
  build_system = ide_build_system_from_context (context);

  if (!GBP_IS_MESON_BUILD_SYSTEM (build_system))
    return;

  dzl_preferences_add_page (preferences, "meson", _("Meson"), 20);
  dzl_preferences_add_list_group (preferences, "meson", "options", _("Meson Options"), GTK_SELECTION_NONE, 0);
}

static void
config_view_addin_iface_init (IdeConfigViewAddinInterface *iface)
{
  iface->load = gbp_meson_config_view_addin_load;
}

G_DEFINE_TYPE_WITH_CODE (GbpMesonConfigViewAddin, gbp_meson_config_view_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_CONFIG_VIEW_ADDIN, config_view_addin_iface_init))

static void
gbp_meson_config_view_addin_class_init (GbpMesonConfigViewAddinClass *klass)
{
}

static void
gbp_meson_config_view_addin_init (GbpMesonConfigViewAddin *self)
{
}
