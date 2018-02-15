/* ide-sysroot-preferences.c
 *
 * Copyright (C) 2018 Corentin Noël <corentin.noel@collabora.com>
 * Copyright (C) 2018 Collabora Ltd.
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
 */

#define G_LOG_DOMAIN "ide-sysroot-preferences-addin"

#include <glib/gi18n.h>

#include "ide-sysroot-preferences-addin.h"
#include "ide-sysroot-preferences-row.h"
#include "ide-sysroot-manager.h"

struct _IdeSysrootPreferencesAddin
{
  GObject         parent_instance;

  GArray         *ids;
  DzlPreferences *preferences;
};

static void sysroot_preferences_add_new (IdeSysrootPreferencesAddin *self,
                                         GtkWidget *emitter)
{
  GtkWidget *pref_row = NULL;
  guint id = 0;
  gchar *new_target;
  IdeSysrootManager *sysroot_manager = NULL;

  g_assert (IDE_IS_SYSROOT_PREFERENCES_ADDIN (self));
  g_assert (DZL_IS_PREFERENCES_BIN (emitter));

  sysroot_manager = ide_sysroot_manager_get_default ();
  new_target = ide_sysroot_manager_create_target (sysroot_manager);
  pref_row = g_object_new (IDE_TYPE_SYSROOT_PREFERENCES_ROW,
                           "visible", TRUE,
                           "sysroot-id", new_target,
                           NULL);

  id = dzl_preferences_add_custom (self->preferences, "sdk", "sysroot", pref_row, "", 1);
  g_array_append_val (self->ids, id);

  ide_sysroot_preferences_row_show_popup (IDE_SYSROOT_PREFERENCES_ROW (pref_row));
}

GtkWidget *
sysroot_preferences_get_add_widget (IdeSysrootPreferencesAddin *self)
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

  g_signal_connect_swapped (bin, "preference-activated", G_CALLBACK(sysroot_preferences_add_new), self);

  return bin;
}

static void
ide_sysroot_preferences_addin_load (IdePreferencesAddin *addin,
                                    DzlPreferences      *preferences)
{
  IdeSysrootPreferencesAddin *self = IDE_SYSROOT_PREFERENCES_ADDIN (addin);
  GtkWidget *widget = NULL;
  IdeSysrootManager *sysroot_manager = NULL;
  GArray *sysroots = NULL;
  guint id = 0;

  IDE_ENTRY;

  g_assert (IDE_IS_SYSROOT_PREFERENCES_ADDIN (self));
  g_assert (DZL_IS_PREFERENCES (preferences));

  self->ids = g_array_new (FALSE, FALSE, sizeof (guint));
  self->preferences = preferences;

  dzl_preferences_add_list_group (preferences, "sdk", "sysroot", _("Sysroots"), GTK_SELECTION_NONE, 0);

  widget = sysroot_preferences_get_add_widget (self);
  id = dzl_preferences_add_custom (preferences, "sdk", "sysroot", widget, "", 0);

  g_array_append_val (self->ids, id);

  sysroot_manager = ide_sysroot_manager_get_default ();
  sysroots = ide_sysroot_manager_list (sysroot_manager);
  for (guint i = 0; i < sysroots->len; i++)
    {
      gchar *sysroot_id = g_array_index (sysroots, gchar*, i);
      GtkWidget *pref_row = g_object_new (IDE_TYPE_SYSROOT_PREFERENCES_ROW,
                                          "visible", TRUE,
                                          "sysroot-id", sysroot_id,
                                          NULL);

      id = dzl_preferences_add_custom (self->preferences, "sdk", "sysroot", pref_row, NULL, 1);
      g_array_append_val (self->ids, id);
    }

  g_array_free (sysroots, TRUE);

  IDE_EXIT;
}

static void
ide_sysroot_preferences_addin_unload (IdePreferencesAddin *addin,
                                      DzlPreferences      *preferences)
{
  IdeSysrootPreferencesAddin *self = IDE_SYSROOT_PREFERENCES_ADDIN (addin);

  IDE_ENTRY;

  g_assert (IDE_IS_SYSROOT_PREFERENCES_ADDIN (self));
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
  iface->load = ide_sysroot_preferences_addin_load;
  iface->unload = ide_sysroot_preferences_addin_unload;
}

G_DEFINE_TYPE_EXTENDED (IdeSysrootPreferencesAddin, ide_sysroot_preferences_addin, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_PREFERENCES_ADDIN, preferences_addin_iface_init))

static void
ide_sysroot_preferences_addin_class_init (IdeSysrootPreferencesAddinClass *klass)
{
}

static void
ide_sysroot_preferences_addin_init (IdeSysrootPreferencesAddin *self)
{
}
