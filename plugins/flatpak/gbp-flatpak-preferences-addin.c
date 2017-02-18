/* gbp-flatpak-preferences-addin.c
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-flatpak-preferences-addin"

#include <flatpak.h>
#include <glib/gi18n.h>

#include "gbp-flatpak-application-addin.h"
#include "gbp-flatpak-preferences-addin.h"
#include "gbp-flatpak-transfer.h"

struct _GbpFlatpakPreferencesAddin
{
  GObject         parent_instance;

  GArray         *ids;
  IdePreferences *preferences;

  guint           show_all : 1;
};

static void gbp_flatpak_preferences_addin_reload (GbpFlatpakPreferencesAddin *self,
                                                  IdePreferences             *preferences);

static void
gbp_flatpak_preferences_addin_view_more (GbpFlatpakPreferencesAddin *self,
                                         IdePreferencesBin          *bin)
{
  g_assert (GBP_IS_FLATPAK_PREFERENCES_ADDIN (self));
  g_assert (IDE_IS_PREFERENCES_BIN (bin));

  self->show_all = !self->show_all;
  if (self->preferences != NULL)
    gbp_flatpak_preferences_addin_reload (self, self->preferences);
}

static gboolean
is_ignored (const gchar *name)
{
  return g_str_has_suffix (name, ".Locale") ||
         g_str_has_suffix (name, ".Debug") ||
         g_str_has_suffix (name, ".Var");
}

static GtkWidget *
create_row (GbpFlatpakPreferencesAddin *self,
            const gchar                *name,
            const gchar                *arch,
            const gchar                *branch)
{
  g_autofree gchar *label = NULL;
  g_autoptr(GbpFlatpakTransfer) transfer = NULL;
  GbpFlatpakApplicationAddin *app_addin;
  GtkWidget *box;
  GtkWidget *button;

  app_addin = gbp_flatpak_application_addin_get_default ();
  transfer = gbp_flatpak_transfer_new (name, arch, branch, TRUE);

  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_HORIZONTAL,
                      "visible", TRUE,
                      NULL);

  label = g_strdup_printf ("%s <b>%s</b> <small>%s</small>", name, branch, arch);

  gtk_container_add (GTK_CONTAINER (box),
                     g_object_new (GTK_TYPE_LABEL,
                                   "hexpand", TRUE,
                                   "label", label,
                                   "use-markup", TRUE,
                                   "visible", TRUE,
                                   "xalign", 0.0f,
                                   NULL));

  button = g_object_new (IDE_TYPE_TRANSFER_BUTTON,
                         "hexpand", FALSE,
                         "visible", TRUE,
                         "label", _("Install"),
                         "transfer", transfer,
                         "width-request", 100,
                         NULL);

  if (gbp_flatpak_application_addin_has_runtime (app_addin, name, arch, branch))
    gtk_button_set_label (GTK_BUTTON (button), _("Update"));

  /* TODO: Update label after transfer completes */

  gtk_container_add (GTK_CONTAINER (box), button);

  return box;
}

static gint
compare_refs (gconstpointer a,
              gconstpointer b)
{
  FlatpakRef *refa = *(FlatpakRef **)a;
  FlatpakRef *refb = *(FlatpakRef **)b;
  gint ret;

  ret = g_strcmp0 (flatpak_ref_get_name (refa), flatpak_ref_get_name (refb));
  if (ret != 0)
    return ret;

  /* sort numerically in reverse */
  ret = -g_utf8_collate (flatpak_ref_get_branch (refa), flatpak_ref_get_branch (refb));
  if (ret != 0)
    return ret;

  return g_strcmp0 (flatpak_ref_get_arch (refa), flatpak_ref_get_arch (refb));
}

static gboolean
is_old_gnome_version (const gchar *version)
{
  if (g_str_equal (version, "master"))
    return FALSE;

  if (g_utf8_collate ("3.20", version) > 0)
    return TRUE;

  return FALSE;
}

static void
add_runtimes (GbpFlatpakPreferencesAddin *self,
              IdePreferences             *preferences,
              FlatpakInstallation        *installation)
{
  g_autoptr(GPtrArray) remotes = NULL;
  g_autoptr(GPtrArray) all_refs = g_ptr_array_new_with_free_func (g_object_unref);
  guint ignored = 0;

  remotes = flatpak_installation_list_remotes (installation, NULL, NULL);

  if (remotes != NULL)
    {
      for (guint i = 0; i < remotes->len; i++)
        {
          FlatpakRemote *remote = g_ptr_array_index (remotes, i);
          g_autoptr(GPtrArray) refs = NULL;

          g_assert (FLATPAK_IS_REMOTE (remote));

          refs = flatpak_installation_list_remote_refs_sync (installation,
                                                             flatpak_remote_get_name (remote),
                                                             NULL,
                                                             NULL);

          if (refs == NULL)
            continue;

          for (guint j = 0; j < refs->len; j++)
            g_ptr_array_add (all_refs, g_object_ref (g_ptr_array_index (refs, j)));
        }
    }

  g_ptr_array_sort (all_refs, compare_refs);

  for (guint j = 0; j < all_refs->len; j++)
    {
      FlatpakRemoteRef *ref = g_ptr_array_index (all_refs, j);
      FlatpakRefKind kind = flatpak_ref_get_kind (FLATPAK_REF (ref));
      const gchar *name = flatpak_ref_get_name (FLATPAK_REF (ref));
      const gchar *branch = flatpak_ref_get_branch (FLATPAK_REF (ref));
      const gchar *arch = flatpak_ref_get_arch (FLATPAK_REF (ref));
      g_autofree gchar *keywords = NULL;
      GtkWidget *row;
      guint id;

      /* TODO: handle multi-arch and cross-compile */
      if (g_strcmp0 (arch, flatpak_get_default_arch ()) != 0)
        continue;

      if (kind != FLATPAK_REF_KIND_RUNTIME)
        continue;

      if (is_ignored (name))
        continue;

      /* Don't show this item by default if it's not GNOME or an old branch */
      if (!self->show_all && (!g_str_has_prefix (name, "org.gnome.") || is_old_gnome_version (branch)))
        {
          ignored++;
          continue;
        }

      /* translators: keywords are used to match search keywords in preferences */
      keywords = g_strdup_printf (_("flatpak %s %s %s"), name, branch, arch);

      row = create_row (self, name, arch, branch);
      id = ide_preferences_add_custom (preferences, "sdk", "flatpak-runtimes", row, keywords, j);
      g_array_append_val (self->ids, id);
    }

  if (ignored)
    {
      g_autofree gchar *tooltip = NULL;
      GtkWidget *image;
      GtkWidget *row;
      guint id;

      /* translators: %u is the number of hidden runtimes to be shown */
      tooltip = g_strdup_printf (ngettext ("Show %u more runtime", "show %u more runtimes", ignored), ignored);

      image = g_object_new (GTK_TYPE_IMAGE,
                            "icon-size", GTK_ICON_SIZE_MENU,
                            "icon-name", "view-more-symbolic",
                            "tooltip-text", tooltip,
                            "visible", TRUE,
                            NULL);
      row = g_object_new (IDE_TYPE_PREFERENCES_BIN,
                          "child", image,
                          "visible", TRUE,
                          NULL);
      g_signal_connect_object (row,
                               "preference-activated",
                               G_CALLBACK (gbp_flatpak_preferences_addin_view_more),
                               self,
                               G_CONNECT_SWAPPED);
      id = ide_preferences_add_custom (preferences, "sdk", "flatpak-runtimes", row, NULL, G_MAXINT);
      g_array_append_val (self->ids, id);
    }
}

static void
gbp_flatpak_preferences_addin_reload (GbpFlatpakPreferencesAddin *self,
                                      IdePreferences             *preferences)
{
  g_autoptr(FlatpakInstallation) system = NULL;
  g_autoptr(FlatpakInstallation) user = NULL;
  g_autoptr(GFile) file = NULL;
  g_autofree gchar *path = NULL;
  guint id;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_PREFERENCES_ADDIN (self));
  g_assert (IDE_IS_PREFERENCES (preferences));

  if (self->ids != NULL)
    {
      for (guint i = 0; i < self->ids->len; i++)
        {
          id = g_array_index (self->ids, guint, i);
          ide_preferences_remove_id (preferences, id);
        }
    }

  path = g_build_filename (g_get_home_dir (), ".local", "share", "flatpak", NULL);
  file = g_file_new_for_path (path);
  user = flatpak_installation_new_for_path (file, TRUE, NULL, NULL);
  if (user != NULL)
    add_runtimes (self, preferences, user);

  system = flatpak_installation_new_system (NULL, NULL);
  if (system != NULL)
    add_runtimes (self, preferences, system);

  IDE_EXIT;
}

static void
gbp_flatpak_preferences_addin_load (IdePreferencesAddin *addin,
                                    IdePreferences      *preferences)
{
  GbpFlatpakPreferencesAddin *self = (GbpFlatpakPreferencesAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_PREFERENCES_ADDIN (self));
  g_assert (IDE_IS_PREFERENCES (preferences));

  self->ids = g_array_new (FALSE, FALSE, sizeof (guint));
  self->preferences = preferences;

  ide_preferences_add_list_group (preferences, "sdk", "flatpak-runtimes", _("Flatpak Runtimes"), GTK_SELECTION_NONE, 0);

  gbp_flatpak_preferences_addin_reload (self, preferences);

  IDE_EXIT;
}

static void
gbp_flatpak_preferences_addin_unload (IdePreferencesAddin *addin,
                                      IdePreferences      *preferences)
{
  GbpFlatpakPreferencesAddin *self = (GbpFlatpakPreferencesAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_PREFERENCES_ADDIN (self));
  g_assert (IDE_IS_PREFERENCES (preferences));

  for (guint i = 0; i < self->ids->len; i++)
    {
      guint id = g_array_index (self->ids, guint, i);

      ide_preferences_remove_id (preferences, id);
    }

  g_clear_pointer (&self->ids, g_array_unref);
  self->preferences = NULL;

  IDE_EXIT;
}

static void
preferences_addin_iface_init (IdePreferencesAddinInterface *iface)
{
  iface->load = gbp_flatpak_preferences_addin_load;
  iface->unload = gbp_flatpak_preferences_addin_unload;
}

G_DEFINE_TYPE_EXTENDED (GbpFlatpakPreferencesAddin, gbp_flatpak_preferences_addin, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_PREFERENCES_ADDIN, preferences_addin_iface_init))

static void
gbp_flatpak_preferences_addin_class_init (GbpFlatpakPreferencesAddinClass *klass)
{
}

static void
gbp_flatpak_preferences_addin_init (GbpFlatpakPreferencesAddin *self)
{
}
