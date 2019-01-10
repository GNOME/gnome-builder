/* ide-golang-preferences-addin.h
 *
 * Copyright 2018 Loïc BLOT <loic.blot@unix-experience.fr>
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

#include <libide-gui.h>

#include "ide-golang-preferences-addin.h"
#include "ide-golang-application-addin.h"

#include <glib/gi18n.h>

struct _IdeGolangPreferencesAddin
{
  GObject         parent_instance;

  GArray         *ids;
  DzlPreferences *preferences;
  GCancellable   *cancellable;
};

static GtkWidget *
ide_golang_create_preferences_page (IdePreferencesAddin *addin)
{
  GtkWidget *box = g_object_new (GTK_TYPE_BOX,
                                 "orientation", GTK_ORIENTATION_HORIZONTAL,
                                 "expand", TRUE,
                                 "spacing", 12,
                                 "visible", TRUE,
                                 NULL);

  GtkWidget *vbox = g_object_new (GTK_TYPE_BOX,
                                 "orientation", GTK_ORIENTATION_VERTICAL,
                                 "expand", TRUE,
                                 "visible", TRUE,
                                 NULL);

  GtkWidget *version_label = g_object_new (GTK_TYPE_LABEL,
                                            "halign", GTK_ALIGN_START,
                                           "expand", TRUE,
                                           "visible", TRUE,
                                           "label", "Version");

  GtkWidget *version_value_label = g_object_new (GTK_TYPE_LABEL,
                                                 "halign", GTK_ALIGN_START,
                                                 "expand", TRUE,
                                                 "visible", TRUE,
                                                 "label", "unknown");
  GtkStyleContext *context;

  GString *version_str = g_string_new ("<small>");
  g_string_append (version_str, golang_get_go_version());
  g_string_append(version_str, "</small>");

  context = gtk_widget_get_style_context (version_value_label);
  gtk_style_context_add_class(context, "dim-label");

  gtk_label_set_markup (GTK_LABEL(version_value_label), version_str->str);

  gtk_box_pack_start (GTK_BOX(vbox), version_label, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX(vbox), version_value_label, TRUE, TRUE, 0);

  gtk_box_pack_start (GTK_BOX(box), vbox, TRUE, TRUE, 0);

  g_string_free(version_str, TRUE);
  return box;
}

static void
ide_golang_preferences_addin_load (IdePreferencesAddin *addin,
                                    DzlPreferences      *preferences)
{
  IdeGolangPreferencesAddin *self = (IdeGolangPreferencesAddin *)addin;
  guint id;

  IDE_ENTRY;

  g_assert (IDE_IS_GOLANG_PREFERENCES_ADDIN (self));
  g_assert (DZL_IS_PREFERENCES (preferences));

  self->ids = g_array_new (FALSE, FALSE, sizeof (guint));
  self->preferences = preferences;

  dzl_preferences_add_list_group (preferences, "sdk", "golang", _("Golang"), GTK_SELECTION_NONE, 100);

  id = dzl_preferences_add_file_chooser (preferences, "sdk",
            "go",
            "org.gnome.builder.plugins.golang",
            "goroot-path",
            "/org/gnome/builder/plugins/golang/",
            _("GOROOT"),
            _("Go ROOT library path"),
            GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
            "",
            150);
  g_array_append_val (self->ids, id);

  id = dzl_preferences_add_custom (preferences, "sdk", "golang", ide_golang_create_preferences_page (addin), NULL, 1000);
  g_array_append_val (self->ids, id);

  IDE_EXIT;
}

static void
ide_golang_preferences_addin_unload (IdePreferencesAddin *addin,
                                      DzlPreferences      *preferences)
{
  IdeGolangPreferencesAddin *self = (IdeGolangPreferencesAddin *)addin;

  IDE_ENTRY;

  g_assert (IDE_IS_GOLANG_PREFERENCES_ADDIN (self));
  g_assert (DZL_IS_PREFERENCES (preferences));

  /* Clear preferences so reload code doesn't try to
   * make forward progress updating items.
   */
  self->preferences = NULL;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

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
  iface->load = ide_golang_preferences_addin_load;
  iface->unload = ide_golang_preferences_addin_unload;
}

G_DEFINE_TYPE_EXTENDED (IdeGolangPreferencesAddin, ide_golang_preferences_addin, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_PREFERENCES_ADDIN, preferences_addin_iface_init))

static void
ide_golang_preferences_addin_class_init (IdeGolangPreferencesAddinClass *klass)
{
}

static void
ide_golang_preferences_addin_init (IdeGolangPreferencesAddin *self)
{
}
