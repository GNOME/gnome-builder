/* gbp-projectui-tweaks-addin.c
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

#define G_LOG_DOMAIN "gbp-projectui-tweaks-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <gtk/gtk.h>

#include <libide-foundry.h>

#include "gbp-projectui-tweaks-addin.h"

struct _GbpProjectuiTweaksAddin
{
  IdeTweaksAddin parent_instance;
};

G_DEFINE_FINAL_TYPE (GbpProjectuiTweaksAddin, gbp_projectui_tweaks_addin, IDE_TYPE_TWEAKS_ADDIN)

static void
gbp_projectui_tweaks_addin_load (IdeTweaksAddin *addin,
                                 IdeTweaks      *tweaks)
{
  GbpProjectuiTweaksAddin *self = (GbpProjectuiTweaksAddin *)addin;
  g_autoptr(GtkFlattenListModel) configs = NULL;
  g_autoptr(GListStore) store = NULL;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_PROJECTUI_TWEAKS_ADDIN (self));
  g_assert (IDE_IS_TWEAKS (tweaks));

  ide_tweaks_addin_set_resource_paths (IDE_TWEAKS_ADDIN (self),
                                       IDE_STRV_INIT ("/plugins/projectui/tweaks.ui"));

  store = g_list_store_new (G_TYPE_LIST_MODEL);
  configs = gtk_flatten_list_model_new (G_LIST_MODEL (g_object_ref (store)));
  ide_tweaks_expose_object (tweaks, "Configurations", G_OBJECT (configs));

  if ((context = ide_tweaks_get_context (tweaks)))
    {
      IdeConfigManager *config_manager = ide_config_manager_from_context (context);
      g_list_store_append (store, config_manager);
    }

  IDE_TWEAKS_ADDIN_CLASS (gbp_projectui_tweaks_addin_parent_class)->load (addin, tweaks);
}

static void
gbp_projectui_tweaks_addin_class_init (GbpProjectuiTweaksAddinClass *klass)
{
  IdeTweaksAddinClass *tweaks_addin_class = IDE_TWEAKS_ADDIN_CLASS (klass);

  tweaks_addin_class->load = gbp_projectui_tweaks_addin_load;
}

static void
gbp_projectui_tweaks_addin_init (GbpProjectuiTweaksAddin *self)
{
}
