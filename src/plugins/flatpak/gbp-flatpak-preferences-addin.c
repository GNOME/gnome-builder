/* gbp-flatpak-preferences-addin.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-flatpak-preferences-addin"
#define MIN_GNOME_VERSION "3.26"

#include <flatpak.h>
#include <glib/gi18n.h>
#include <libide-gui.h>

#include "gbp-flatpak-application-addin.h"
#include "gbp-flatpak-preferences-addin.h"
#include "gbp-flatpak-transfer.h"
#include "gbp-flatpak-util.h"

struct _GbpFlatpakPreferencesAddin
{
  GObject         parent_instance;

  GArray         *ids;
  DzlPreferences *preferences;
  GCancellable   *cancellable;

  gulong          reload_handler;

  guint           show_all : 1;
};

static void gbp_flatpak_preferences_addin_reload (GbpFlatpakPreferencesAddin *self);

static void
gbp_flatpak_preferences_addin_view_more (GbpFlatpakPreferencesAddin *self,
                                         DzlPreferencesBin          *bin)
{
  g_assert (GBP_IS_FLATPAK_PREFERENCES_ADDIN (self));
  g_assert (DZL_IS_PREFERENCES_BIN (bin));

  self->show_all = !self->show_all;
  if (self->preferences != NULL)
    gbp_flatpak_preferences_addin_reload (self);
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

  return g_utf8_collate (MIN_GNOME_VERSION, version) > 0;
}

static gboolean
contains_runtime (GPtrArray  *runtimes,
                  FlatpakRef *ref)
{
  for (guint i = 0; i < runtimes->len; i++)
    {
      FlatpakRef *existing_ref = g_ptr_array_index (runtimes, i);
      g_autofree gchar *existing_ref_name = NULL;
      g_autofree gchar *ref_name = NULL;

      existing_ref_name = flatpak_ref_format_ref (existing_ref);
      ref_name = flatpak_ref_format_ref (ref);
      if (g_strcmp0 (existing_ref_name, ref_name) == 0)
        return TRUE;
    }

  return FALSE;
}

static void
populate_runtimes (GbpFlatpakPreferencesAddin *self,
                   FlatpakInstallation        *installation,
                   GPtrArray                  *runtimes)
{
  g_autoptr(GPtrArray) remotes = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_PREFERENCES_ADDIN (self));
  g_assert (FLATPAK_IS_INSTALLATION (installation));
  g_assert (runtimes != NULL);

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
            {
              FlatpakRef *ref = g_ptr_array_index (refs, j);
              FlatpakRefKind kind = flatpak_ref_get_kind (ref);
              const gchar *arch = flatpak_ref_get_arch (ref);

              if (kind != FLATPAK_REF_KIND_RUNTIME)
                continue;

              /* Ignore other arches for now */
              if (g_strcmp0 (arch, flatpak_get_default_arch ()) != 0)
                continue;

              if (!contains_runtime (runtimes, ref))
                g_ptr_array_add (runtimes, g_object_ref (ref));
            }
        }
    }

  IDE_EXIT;
}

static void
gbp_flatpak_preferences_addin_reload_worker (IdeTask      *task,
                                             gpointer      source_object,
                                             gpointer      task_data,
                                             GCancellable *cancellable)
{
  GbpFlatpakPreferencesAddin *self = (GbpFlatpakPreferencesAddin *)source_object;
  g_autoptr(GPtrArray) runtimes = NULL;
  GbpFlatpakApplicationAddin *app_addin;
  GPtrArray *installations = task_data;

  IDE_ENTRY;

  g_assert (IDE_IS_TASK (task));
  g_assert (GBP_IS_FLATPAK_PREFERENCES_ADDIN (self));
  g_assert (installations != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  runtimes = g_ptr_array_new_with_free_func (g_object_unref);
  app_addin = gbp_flatpak_application_addin_get_default ();

  /*
   * If our application addin has not yet been loaded, we won't have any
   * runtimes loaded yet.
   */
  if (app_addin != NULL)
    {
      for (guint i = 0; i < installations->len; i++)
        {
          FlatpakInstallation *installation = g_ptr_array_index (installations, i);
          populate_runtimes (self, installation, runtimes);
        }

      g_ptr_array_sort (runtimes, compare_refs);
    }

  ide_task_return_pointer (task, g_steal_pointer (&runtimes), g_ptr_array_unref);

  IDE_EXIT;
}

static void
gbp_flatpak_preferences_addin_reload_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  GbpFlatpakPreferencesAddin *self = (GbpFlatpakPreferencesAddin *)object;
  g_autoptr(GPtrArray) runtimes = NULL;
  g_autoptr(GError) error = NULL;
  guint ignored = 0;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_PREFERENCES_ADDIN (self));
  g_assert (IDE_IS_TASK (result));

  if (NULL == (runtimes = ide_task_propagate_pointer (IDE_TASK (result), &error)))
    {
      g_warning ("%s", error->message);
      IDE_EXIT;
    }

  if (self->preferences == NULL)
    IDE_EXIT;

  IDE_TRACE_MSG ("Found %u runtimes", runtimes->len);

  for (guint j = 0; j < runtimes->len; j++)
    {
      FlatpakRemoteRef *ref = g_ptr_array_index (runtimes, j);
      const gchar *name = flatpak_ref_get_name (FLATPAK_REF (ref));
      const gchar *branch = flatpak_ref_get_branch (FLATPAK_REF (ref));
      const gchar *arch = flatpak_ref_get_arch (FLATPAK_REF (ref));
      g_autofree gchar *keywords = NULL;
      GtkWidget *row;
      guint id;

      if (gbp_flatpak_is_ignored (name))
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
      id = dzl_preferences_add_custom (self->preferences, "sdk", "flatpak-runtimes", row, keywords, j);
      g_array_append_val (self->ids, id);
    }

  if (ignored != 0)
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
      row = g_object_new (DZL_TYPE_PREFERENCES_BIN,
                          "child", image,
                          "visible", TRUE,
                          NULL);
      g_signal_connect_object (row,
                               "preference-activated",
                               G_CALLBACK (gbp_flatpak_preferences_addin_view_more),
                               self,
                               G_CONNECT_SWAPPED);
      id = dzl_preferences_add_custom (self->preferences, "sdk", "flatpak-runtimes", row, NULL, G_MAXINT);
      g_array_append_val (self->ids, id);
    }

  IDE_EXIT;
}

static void
gbp_flatpak_preferences_addin_reload (GbpFlatpakPreferencesAddin *self)
{
  GbpFlatpakApplicationAddin *addin;
  g_autoptr(GPtrArray) installations = NULL;
  g_autoptr(IdeTask) task = NULL;
  guint id;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_FLATPAK_PREFERENCES_ADDIN (self));
  g_assert (DZL_IS_PREFERENCES (self->preferences));

  g_clear_object (&self->cancellable);
  self->cancellable = g_cancellable_new ();

  if (self->ids != NULL)
    {
      for (guint i = 0; i < self->ids->len; i++)
        {
          id = g_array_index (self->ids, guint, i);
          dzl_preferences_remove_id (self->preferences, id);
        }

      g_array_remove_range (self->ids, 0, self->ids->len);
    }

  addin = gbp_flatpak_application_addin_get_default ();
  installations = gbp_flatpak_application_addin_get_installations (addin);

  task = ide_task_new (self, self->cancellable, gbp_flatpak_preferences_addin_reload_cb, NULL);
  ide_task_set_source_tag (task, gbp_flatpak_preferences_addin_reload);
  ide_task_set_task_data (task, g_steal_pointer (&installations), g_ptr_array_unref);
  ide_task_run_in_thread (task, gbp_flatpak_preferences_addin_reload_worker);

  IDE_EXIT;
}

static void
app_addin_reload (GbpFlatpakPreferencesAddin *self,
                  GbpFlatpakApplicationAddin *app_addin)
{
  g_assert (GBP_IS_FLATPAK_PREFERENCES_ADDIN (self));
  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (app_addin));

  if (self->preferences != NULL)
    gbp_flatpak_preferences_addin_reload (self);
}

static void
gbp_flatpak_preferences_addin_load (IdePreferencesAddin *addin,
                                    DzlPreferences      *preferences)
{
  GbpFlatpakPreferencesAddin *self = (GbpFlatpakPreferencesAddin *)addin;
  GbpFlatpakApplicationAddin *app_addin;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_PREFERENCES_ADDIN (self));
  g_assert (DZL_IS_PREFERENCES (preferences));

  self->ids = g_array_new (FALSE, FALSE, sizeof (guint));
  self->preferences = preferences;

  dzl_preferences_add_list_group (preferences, "sdk", "flatpak-runtimes", _("Flatpak Runtimes"), GTK_SELECTION_NONE, 0);

  app_addin = gbp_flatpak_application_addin_get_default ();

  self->reload_handler =
    g_signal_connect_object (app_addin,
                             "reload",
                             G_CALLBACK (app_addin_reload),
                             self,
                             G_CONNECT_SWAPPED);

  gbp_flatpak_preferences_addin_reload (self);

  IDE_EXIT;
}

static void
gbp_flatpak_preferences_addin_unload (IdePreferencesAddin *addin,
                                      DzlPreferences      *preferences)
{
  GbpFlatpakPreferencesAddin *self = (GbpFlatpakPreferencesAddin *)addin;
  GbpFlatpakApplicationAddin *app_addin;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_PREFERENCES_ADDIN (self));
  g_assert (DZL_IS_PREFERENCES (preferences));

  /* Clear preferences so reload code doesn't try to
   * make forward progress updating items.
   */
  self->preferences = NULL;

  app_addin = gbp_flatpak_application_addin_get_default ();
  dzl_clear_signal_handler (app_addin, &self->reload_handler);

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
