/* gbp-meson-toolchain-edition-preferences.c
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

#define G_LOG_DOMAIN "gbp-meson-toolchain-edition-preferences-addin"

#include <glib/gi18n.h>
#include <libide-gui.h>

#include "gbp-meson-toolchain-edition-preferences-addin.h"
#include "gbp-meson-toolchain-edition-preferences-row.h"

struct _GbpMesonToolchainEditionPreferencesAddin
{
  GObject         parent_instance;

  GArray         *ids;
  DzlPreferences *preferences;
  GCancellable   *cancellable;
};

static void
meson_toolchain_edition_preferences_add_new (GbpMesonToolchainEditionPreferencesAddin *self,
                                             GtkWidget                                *emitter)
{
  g_autofree gchar *user_folder_path = NULL;
  g_autofree gchar *new_target = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GFileOutputStream) output_stream = NULL;
  GbpMesonToolchainEditionPreferencesRow *pref_row;
  guint id = 0;

  g_assert (GBP_IS_MESON_TOOLCHAIN_EDITION_PREFERENCES_ADDIN (self));
  g_assert (DZL_IS_PREFERENCES_BIN (emitter));

  user_folder_path = g_build_filename (g_get_user_data_dir (), "meson", "cross", NULL);
  if (g_mkdir_with_parents (user_folder_path, 0700) != 0)
    {
      g_critical ("Can't create %s", user_folder_path);
      return;
    }

  for (uint i = 0; i < UINT_MAX; i++)
    {
      g_autofree gchar *possible_target = g_strdup_printf ("new_file%u", i);
      g_autofree gchar *possible_path = g_build_filename (user_folder_path, possible_target, NULL);
      if (!g_file_test (possible_path, G_FILE_TEST_EXISTS)) {
        new_target = g_steal_pointer (&possible_path);
        break;
      }
    }

  pref_row = g_object_new (GBP_TYPE_MESON_TOOLCHAIN_EDITION_PREFERENCES_ROW,
                           "toolchain-path", new_target,
                           "visible", TRUE,
                           NULL);

  file = g_file_new_for_path (new_target);

  if ((output_stream = g_file_create (file, G_FILE_CREATE_NONE, NULL, &error)))
    g_output_stream_close (G_OUTPUT_STREAM (output_stream), NULL, NULL);

  id = dzl_preferences_add_custom (self->preferences, "sdk", "toolchain", GTK_WIDGET (pref_row), "", 1);
  g_array_append_val (self->ids, id);

  gbp_meson_toolchain_edition_preferences_row_show_popup (pref_row);
}

static GtkWidget *
toolchain_edition_preferences_get_add_widget (GbpMesonToolchainEditionPreferencesAddin *self)
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
                        "label", _("Add toolchain"),
                        "xalign", 0.0f,
                        "hexpand", TRUE,
                        NULL);

  subtitle = g_object_new (GTK_TYPE_LABEL,
                           "visible", TRUE,
                           "label", g_markup_printf_escaped ("<small>%s</small>", _("Define a new custom toolchain targeting a specific platform")),
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
                           G_CALLBACK (meson_toolchain_edition_preferences_add_new),
                           self,
                           G_CONNECT_SWAPPED);

  return bin;
}

static void
gbp_meson_toolchain_edition_preferences_addin_load_finish (GObject      *object,
                                                           GAsyncResult *result,
                                                           gpointer      user_data)
{
  GFile *folder = (GFile *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(GPtrArray) ret = NULL;
  GbpMesonToolchainEditionPreferencesAddin *self = (GbpMesonToolchainEditionPreferencesAddin *)user_data;

  g_assert (G_IS_FILE (folder));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_MESON_TOOLCHAIN_EDITION_PREFERENCES_ADDIN (self));

  ret = ide_g_file_find_finish (folder, result, &error);
  IDE_PTR_ARRAY_SET_FREE_FUNC (ret, g_object_unref);

  if (ret == NULL)
    return;

  for (guint i = 0; i < ret->len; i++)
    {
      guint id = 0;
      GFile *file = g_ptr_array_index (ret, i);
      g_autoptr(GError) load_error = NULL;
      g_autoptr(GbpMesonToolchainEditionPreferencesRow) pref_row = NULL;
      g_autofree gchar *path = NULL;

      pref_row = g_object_new (GBP_TYPE_MESON_TOOLCHAIN_EDITION_PREFERENCES_ROW,
                               "visible", TRUE,
                               NULL);

      path = g_file_get_path (file);
      if (!gbp_meson_toolchain_edition_preferences_row_load_file (g_object_ref_sink (pref_row), path, &load_error))
        continue;

      id = dzl_preferences_add_custom (self->preferences, "sdk", "toolchain", GTK_WIDGET (pref_row), NULL, i);
      g_array_append_val (self->ids, id);
    }

}

static void
gbp_meson_toolchain_edition_preferences_addin_load (IdePreferencesAddin *addin,
                                                    DzlPreferences      *preferences)
{
  GbpMesonToolchainEditionPreferencesAddin *self = (GbpMesonToolchainEditionPreferencesAddin *)addin;
  GtkWidget *widget = NULL;
  guint id = 0;
  g_autofree gchar *user_folder_path = NULL;
  g_autoptr(GFile) user_folder = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_MESON_TOOLCHAIN_EDITION_PREFERENCES_ADDIN (self));
  g_assert (DZL_IS_PREFERENCES (preferences));

  self->ids = g_array_new (FALSE, FALSE, sizeof (guint));
  self->preferences = preferences;
  self->cancellable = g_cancellable_new ();

  dzl_preferences_add_list_group (preferences, "sdk", "toolchain", _("Toolchain"), GTK_SELECTION_NONE, 0);

  widget = toolchain_edition_preferences_get_add_widget (self);
  id = dzl_preferences_add_custom (preferences, "sdk", "toolchain", widget, "", 0);

  g_array_append_val (self->ids, id);

  user_folder_path = g_build_filename (g_get_user_data_dir (), "meson", "cross", NULL);
  user_folder = g_file_new_for_path (user_folder_path);
  ide_g_file_find_async (user_folder,
                         "*",
                         self->cancellable,
                         gbp_meson_toolchain_edition_preferences_addin_load_finish,
                         self);

  IDE_EXIT;
}

static void
gbp_meson_toolchain_edition_preferences_addin_unload (IdePreferencesAddin *addin,
                                      DzlPreferences      *preferences)
{
  GbpMesonToolchainEditionPreferencesAddin *self = (GbpMesonToolchainEditionPreferencesAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_MESON_TOOLCHAIN_EDITION_PREFERENCES_ADDIN (self));
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
  g_clear_pointer (&self->cancellable, g_object_unref);

  IDE_EXIT;
}

static void
preferences_addin_iface_init (IdePreferencesAddinInterface *iface)
{
  iface->load = gbp_meson_toolchain_edition_preferences_addin_load;
  iface->unload = gbp_meson_toolchain_edition_preferences_addin_unload;
}

G_DEFINE_TYPE_EXTENDED (GbpMesonToolchainEditionPreferencesAddin, gbp_meson_toolchain_edition_preferences_addin, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_PREFERENCES_ADDIN, preferences_addin_iface_init))

static void
gbp_meson_toolchain_edition_preferences_addin_class_init (GbpMesonToolchainEditionPreferencesAddinClass *klass)
{
}

static void
gbp_meson_toolchain_edition_preferences_addin_init (GbpMesonToolchainEditionPreferencesAddin *self)
{
}
