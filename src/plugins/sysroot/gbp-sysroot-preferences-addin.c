/* gbp-sysroot-preferences.c
 *
 * Copyright 2018 Corentin Noël <corentin.noel@collabora.com>
 * Copyright 2018 Collabora Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, eitIher version 3 of the License, or
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

#define G_LOG_DOMAIN "gbp-sysroot-preferences-addin"

#include <glib/gi18n.h>
#include <libide-gui.h>

#include "gbp-sysroot-preferences-addin.h"
#include "gbp-sysroot-preferences-row.h"
#include "gbp-sysroot-manager.h"

struct _GbpSysrootPreferencesAddin
{
  GObject         parent_instance;

  GArray         *ids;
  DzlPreferences *preferences;
};

static void
sysroot_preferences_add_new (GbpSysrootPreferencesAddin *self,
                             GtkWidget                  *emitter)
{
  GtkWidget *pref_row = NULL;
  guint id = 0;
  g_autofree gchar *new_target = NULL;
  GbpSysrootManager *sysroot_manager = NULL;

  g_assert (GBP_IS_SYSROOT_PREFERENCES_ADDIN (self));
  g_assert (DZL_IS_PREFERENCES_BIN (emitter));

  sysroot_manager = gbp_sysroot_manager_get_default ();
  new_target = gbp_sysroot_manager_create_target (sysroot_manager);
  pref_row = g_object_new (GBP_TYPE_SYSROOT_PREFERENCES_ROW,
                           "visible", TRUE,
                           "sysroot-id", new_target,
                           NULL);

  id = dzl_preferences_add_custom (self->preferences, "sdk", "sysroot", pref_row, "", 1);
  g_array_append_val (self->ids, id);

  gbp_sysroot_preferences_row_show_popup (GBP_SYSROOT_PREFERENCES_ROW (pref_row));
}

static GtkWidget *
sysroot_preferences_get_add_widget (GbpSysrootPreferencesAddin *self)
{
  GtkWidget *bin = NULL;
  GtkWidget *grid = NULL;
  GtkWidget *label = NULL;
  GtkWidget *subtitle = NULL;
  GtkWidget *image = NULL;

  bin = g_object_new (DZL_TYPE_PREFERENCES_BIN,
                      "visible", TRUE,
                      NULL);

  grid = g_object_new (GTK_TYPE_GRID,
                       "visible", TRUE,
                       NULL);

  label = g_object_new (GTK_TYPE_LABEL,
                        "visible", TRUE,
                        "label", _("Add sysroot"),
                        "xalign", 0.0f,
                        "hexpand", TRUE,
                        NULL);

  subtitle = g_object_new (GTK_TYPE_LABEL,
                           "visible", TRUE,
                           "label", g_markup_printf_escaped ("<small>%s</small>", _("Define a new sysroot target to build against a different target")),
                           "use-markup", TRUE,
                           "xalign", 0.0f,
                           "hexpand", TRUE,
                           NULL);

  gtk_style_context_add_class (gtk_widget_get_style_context (subtitle), GTK_STYLE_CLASS_DIM_LABEL);

  image = g_object_new (GTK_TYPE_IMAGE,
                        "visible", TRUE,
                        "icon-name", "list-add-symbolic",
                        "valign", GTK_ALIGN_CENTER,
                        NULL);

  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), subtitle, 0, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), image, 1, 0, 1, 2);

  gtk_container_add (GTK_CONTAINER (bin), grid);

  g_signal_connect_object (bin,
                           "preference-activated",
                           G_CALLBACK (sysroot_preferences_add_new),
                           self,
                           G_CONNECT_SWAPPED);

  return bin;
}

static void
gbp_sysroot_preferences_addin_load (IdePreferencesAddin *addin,
                                    DzlPreferences      *preferences)
{
  GbpSysrootPreferencesAddin *self = (GbpSysrootPreferencesAddin *)addin;
  GtkWidget *widget = NULL;
  GbpSysrootManager *sysroot_manager = NULL;
  g_auto(GStrv) sysroots = NULL;
  guint sysroots_length = 0;
  guint id = 0;

  IDE_ENTRY;

  g_assert (GBP_IS_SYSROOT_PREFERENCES_ADDIN (self));
  g_assert (DZL_IS_PREFERENCES (preferences));

  self->ids = g_array_new (FALSE, FALSE, sizeof (guint));
  self->preferences = preferences;

  dzl_preferences_add_list_group (preferences, "sdk", "sysroot", _("Sysroots"), GTK_SELECTION_NONE, 0);

  widget = sysroot_preferences_get_add_widget (self);
  id = dzl_preferences_add_custom (preferences, "sdk", "sysroot", widget, "", 0);

  g_array_append_val (self->ids, id);

  sysroot_manager = gbp_sysroot_manager_get_default ();
  sysroots = gbp_sysroot_manager_list (sysroot_manager);
  sysroots_length = g_strv_length (sysroots);
  for (guint i = 0; i < sysroots_length; i++)
    {
      GtkWidget *pref_row = g_object_new (GBP_TYPE_SYSROOT_PREFERENCES_ROW,
                                          "visible", TRUE,
                                          "sysroot-id", sysroots[i],
                                          NULL);

      id = dzl_preferences_add_custom (self->preferences, "sdk", "sysroot", pref_row, NULL, i);
      g_array_append_val (self->ids, id);
    }

  IDE_EXIT;
}

static void
gbp_sysroot_preferences_addin_unload (IdePreferencesAddin *addin,
                                      DzlPreferences      *preferences)
{
  GbpSysrootPreferencesAddin *self = (GbpSysrootPreferencesAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_SYSROOT_PREFERENCES_ADDIN (self));
  g_assert (DZL_IS_PREFERENCES (preferences));

  /* Clear preferences so reload code doesn't try to
   * make forward progress updating items.
   */
  self->preferences = NULL;

  for (guint i = 0; i < self->ids->len; i++)
    {
      guint id = g_array_index (self->ids, guint, i);

      dzl_preferences_remove_id (preferences, id);
    }

  g_clear_pointer (&self->ids, g_array_unref);

  IDE_EXIT;
}

static void
preferences_addin_iface_init (IdePreferencesAddinInterface *iface)
{
  iface->load = gbp_sysroot_preferences_addin_load;
  iface->unload = gbp_sysroot_preferences_addin_unload;
}

G_DEFINE_TYPE_EXTENDED (GbpSysrootPreferencesAddin, gbp_sysroot_preferences_addin, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_PREFERENCES_ADDIN, preferences_addin_iface_init))

static void
gbp_sysroot_preferences_addin_class_init (GbpSysrootPreferencesAddinClass *klass)
{
}

static void
gbp_sysroot_preferences_addin_init (GbpSysrootPreferencesAddin *self)
{
}
