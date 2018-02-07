/* ide-cross-compilation-preference-addin.c
 *
 * Copyright (C) 2018 Corentin Noël <corentin.noel@collabora.co.uk>
 * Copyright (C) 2018 Collabora Ltd.
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
 */

#include <dazzle.h>
#include <glib/gi18n.h>

#include "ide-cross-compilation-preferences-addin.h"

struct _IdeCrossCompilationPreferencesAddin
{
  GObject         parent_instance;

  GArray         *ids;
  DzlPreferences *preferences;
};

static void preferences_addin_iface_init (IdePreferencesAddinInterface *iface);

static GtkWidget *create_new_device_row (IdeCrossCompilationPreferencesAddin *self);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (IdeCrossCompilationPreferencesAddin,
                                ide_cross_compilation_preferences_addin,
                                IDE_TYPE_OBJECT,
                                0,
                                G_IMPLEMENT_INTERFACE (IDE_TYPE_PREFERENCES_ADDIN,
                                                       preferences_addin_iface_init))

static void
cross_compilation_preferences_foreach_build_system (PeasExtensionSet *set,
                                                    PeasPluginInfo *info,
                                                    PeasExtension *exten,
                                                    gpointer data)
{
  IdeCrossCompilationPreferencesAddin *self = IDE_CROSS_COMPILATION_PREFERENCES_ADDIN (data);
  IdeBuildSystem *build_system = IDE_BUILD_SYSTEM (exten);
  gchar *build_system_name = ide_build_system_get_display_name (build_system);
  gchar *build_system_id = ide_build_system_get_id (build_system);
  GtkWidget *target_name;
  guint id;

  dzl_preferences_add_list_group (self->preferences, "devices.id", build_system_id, build_system_name, GTK_SELECTION_NONE, 0);

  target_name = g_object_new (DZL_TYPE_PREFERENCES_ENTRY,
                              "visible", TRUE,
                              "title", _("Target Name"),
                              NULL);
  id = dzl_preferences_add_custom (self->preferences, "devices.id", build_system_id, target_name, "device name", 0);
  g_array_append_val (self->ids, id);
}

static void
ide_cross_compilation_preferences_addin_load (IdePreferencesAddin *addin,
                                              DzlPreferences      *preferences)
{
  IdeCrossCompilationPreferencesAddin *self = IDE_CROSS_COMPILATION_PREFERENCES_ADDIN(addin);
  GtkWidget *add_row, *flow, *device_name;
  guint id;
  GObject *context;
  PeasExtensionSet *build_systems;

  IDE_ENTRY;

  g_assert (IDE_IS_CROSS_COMPILATION_PREFERENCES_ADDIN (self));
  g_assert (DZL_IS_PREFERENCES (preferences));

  self->ids = g_array_new (FALSE, FALSE, sizeof (guint));
  self->preferences = preferences;

  dzl_preferences_add_page (preferences, "devices", _("Devices"), 200);

  dzl_preferences_add_list_group (preferences, "devices", "devices", NULL, GTK_SELECTION_SINGLE, 0);

  dzl_preferences_add_list_group (preferences, "devices", "add-device", NULL, GTK_SELECTION_NONE, 1);
  add_row = create_new_device_row (self);
  id = dzl_preferences_add_custom (preferences, "devices", "add-device", add_row, "targets test", 0);
  g_array_append_val (self->ids, id);

  flow = gtk_widget_get_ancestor (add_row, DZL_TYPE_COLUMN_LAYOUT);

  g_assert (flow != NULL);

  dzl_column_layout_set_max_columns (DZL_COLUMN_LAYOUT (flow), 1);

  dzl_preferences_add_page (self->preferences, "devices.id", NULL, 0);
  dzl_preferences_add_list_group (self->preferences, "devices.id", "general", _("General"), GTK_SELECTION_NONE, 0);

  device_name = g_object_new (DZL_TYPE_PREFERENCES_ENTRY,
                              "visible", TRUE,
                              "title", _("Name"),
                              NULL);
  id = dzl_preferences_add_custom (self->preferences, "devices.id", "general", device_name, "device name", 0);
  g_array_append_val (self->ids, id);

  /* List all the build systems to allow the configuration of each.
   */
  context = g_object_new (IDE_TYPE_CONTEXT, NULL);
  build_systems = peas_extension_set_new (NULL, IDE_TYPE_BUILD_SYSTEM, "context", context, NULL);
  peas_extension_set_foreach (build_systems, cross_compilation_preferences_foreach_build_system, self);

  g_object_unref (build_systems);
  g_object_unref (context);

  IDE_EXIT;
}

static void
ide_cross_compilation_preferences_addin_unload (IdePreferencesAddin *addin,
                                                DzlPreferences      *preferences)
{
  IdeCrossCompilationPreferencesAddin *self = IDE_CROSS_COMPILATION_PREFERENCES_ADDIN (addin);

  IDE_ENTRY;

  g_assert (IDE_IS_CROSS_COMPILATION_PREFERENCES_ADDIN (self));
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
  iface->load = ide_cross_compilation_preferences_addin_load;
  iface->unload = ide_cross_compilation_preferences_addin_unload;
}

static void
ide_cross_compilation_preferences_addin_class_init (IdeCrossCompilationPreferencesAddinClass *klass)
{
  
}

static void ide_cross_compilation_preferences_addin_class_finalize (IdeCrossCompilationPreferencesAddinClass *klass) {
  
}

static void
ide_cross_compilation_preferences_addin_init (IdeCrossCompilationPreferencesAddin *self)
{
  
}

static void
ide_cross_compilation_preferences_addin_add_device (IdeCrossCompilationPreferencesAddin *self,
                                                    DzlPreferencesBin                   *bin)
{
  g_assert (IDE_IS_CROSS_COMPILATION_PREFERENCES_ADDIN (self));
  g_assert (DZL_IS_PREFERENCES_BIN (bin));

  if (self->preferences != NULL)
    {
      GHashTable *map = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
      g_hash_table_insert (map, "{id}",0);
      dzl_preferences_set_page (self->preferences, "devices.id", map);
      g_hash_table_unref (map);
    }
}

static GtkWidget *
create_new_device_row (IdeCrossCompilationPreferencesAddin *self)
{
  GtkWidget *grid, *label, *subtitle, *image, *row;
  GtkStyleContext *style;

  grid = g_object_new (GTK_TYPE_GRID,
                       "orientation", GTK_ORIENTATION_HORIZONTAL,
                       "visible", TRUE,
                       NULL);

  label = g_object_new (GTK_TYPE_LABEL,
                        "hexpand", TRUE,
                        "label", _("Add New Device"),
                        "visible", TRUE,
                        "xalign", 0.0f,
                        NULL);

  subtitle = g_object_new (GTK_TYPE_LABEL,
                           "hexpand", TRUE,
                           "label", g_markup_printf_escaped ("<small>%s</small>", _("Add another device if your project targets another architecture or board than the once you are currently using")),
                           "use-markup", TRUE,
                           "wrap", TRUE,
                           "visible", TRUE,
                           "xalign", 0.0f,
                           NULL);
  style = gtk_widget_get_style_context (subtitle);
  gtk_style_context_add_class (style, GTK_STYLE_CLASS_DIM_LABEL);

  image = g_object_new (GTK_TYPE_IMAGE,
                        "icon-name", "list-add-symbolic",
                        "visible", TRUE,
                        NULL);

  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), subtitle, 0, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), image, 1, 0, 1, 2);

  row = g_object_new (DZL_TYPE_PREFERENCES_BIN,
                      "child", grid,
                      "visible", TRUE,
                      NULL);

  g_signal_connect_object (row,
                           "preference-activated",
                           G_CALLBACK (ide_cross_compilation_preferences_addin_add_device),
                           self,
                           G_CONNECT_SWAPPED);
  return row;
}

void
_ide_cross_compilation_preferences_addin_register_type (GTypeModule *module)
{
  ide_cross_compilation_preferences_addin_register_type (module);
}
