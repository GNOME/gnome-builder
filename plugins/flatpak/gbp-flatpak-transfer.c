/* gbp-flatpak-transfer.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-flatpak-transfer"

#include <flatpak.h>
#include <glib/gi18n.h>

#include "gbp-flatpak-transfer.h"

struct _GbpFlatpakTransfer
{
  IdeObject parent_instance;

  gchar   *id;
  gchar   *arch;
  gchar   *branch;

  guint    force_update : 1;

  GMutex   mutex;
  gchar   *status;
  gdouble  progress;
};

enum {
  PROP_0,
  PROP_ID,
  PROP_ARCH,
  PROP_BRANCH,
  PROP_FORCE_UPDATE,
  PROP_TITLE,
  PROP_ICON_NAME,
  PROP_PROGRESS,
  PROP_STATUS,
  N_PROPS
};

static void transfer_iface_init (IdeTransferInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GbpFlatpakTransfer, gbp_flatpak_transfer, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_TRANSFER, transfer_iface_init))

static GParamSpec *properties [N_PROPS];

static void
progress_callback (const gchar *status,
                   guint        progress,
                   gboolean     estimating,
                   gpointer     user_data)
{
  GbpFlatpakTransfer *self = user_data;

  g_assert (GBP_IS_FLATPAK_TRANSFER (self));

  g_mutex_lock (&self->mutex);
  g_free (self->status);
  self->status = g_strdup (status);
  self->progress = progress / 100.0;
  g_mutex_unlock (&self->mutex);

  ide_object_notify_in_main (self, properties[PROP_PROGRESS]);
  ide_object_notify_in_main (self, properties[PROP_STATUS]);
}

static gboolean
update_installation (GbpFlatpakTransfer   *self,
                     FlatpakInstallation  *installation,
                     GCancellable         *cancellable,
                     GError              **error)
{
  g_autoptr(FlatpakInstalledRef) ref = NULL;

  g_assert (GBP_IS_FLATPAK_TRANSFER (self));
  g_assert (FLATPAK_IS_INSTALLATION (installation));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  ref = flatpak_installation_update (installation,
                                     FLATPAK_UPDATE_FLAGS_NONE,
                                     FLATPAK_REF_KIND_RUNTIME,
                                     self->id,
                                     self->arch,
                                     self->branch,
                                     progress_callback,
                                     self,
                                     cancellable,
                                     error);

  return ref != NULL;
}

static gboolean
install_from_remote (GbpFlatpakTransfer   *self,
                     FlatpakInstallation  *installation,
                     FlatpakRemote        *remote,
                     GCancellable         *cancellable,
                     GError              **error)
{
  g_autoptr(FlatpakInstalledRef) ref = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_TRANSFER (self));
  g_assert (FLATPAK_IS_INSTALLATION (installation));
  g_assert (FLATPAK_IS_REMOTE (remote));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_debug ("Installing %s/%s/%s from remote %s",
           self->id, self->arch, self->branch,
           flatpak_remote_get_name (remote));

  ref = flatpak_installation_install (installation,
                                      flatpak_remote_get_name (remote),
                                      FLATPAK_REF_KIND_RUNTIME,
                                      self->id,
                                      self->arch,
                                      self->branch,
                                      progress_callback,
                                      self,
                                      cancellable,
                                      error);

  IDE_TRACE_MSG ("ref = %p", ref);

  if (ref != NULL)
    g_debug ("%s/%s/%s was installed from remote %s",
             self->id, self->arch, self->branch,
             flatpak_remote_get_name (remote));

  IDE_RETURN (ref != NULL);
}

static void
gbp_flatpak_transfer_execute_worker (GTask        *task,
                                     gpointer      source_object,
                                     gpointer      task_data,
                                     GCancellable *cancellable)
{
  GbpFlatpakTransfer *self = source_object;
  FlatpakInstallation *installations[2] = { NULL };
  g_autoptr(FlatpakInstallation) user = NULL;
  g_autoptr(FlatpakInstallation) system = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_TRANSFER (self));
  g_assert (G_IS_TASK (task));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /*
   * Load the installations.
   */

  installations[0] = user = flatpak_installation_new_user (cancellable, NULL);
  installations[1] = system = flatpak_installation_new_system (cancellable, NULL);

  /*
   * Locate the id within a previous installation;
   */

  for (guint i = 0; i < G_N_ELEMENTS (installations); i++)
    {
      FlatpakInstallation *installation = installations[i];
      g_autoptr(GError) error = NULL;
      g_autoptr(GPtrArray) refs = NULL;

      if (installation == NULL)
        continue;

      refs = flatpak_installation_list_installed_refs (installation, cancellable, &error);

      if (error != NULL)
        {
          g_task_return_error (task, g_steal_pointer (&error));
          IDE_EXIT;
        }

      for (guint j = 0; j < refs->len; j++)
        {
          FlatpakInstalledRef *ref = g_ptr_array_index (refs, j);
          const gchar *id;
          const gchar *arch;
          const gchar *branch;

          g_assert (FLATPAK_IS_INSTALLED_REF (ref));

          id = flatpak_ref_get_name (FLATPAK_REF (ref));
          arch = flatpak_ref_get_arch (FLATPAK_REF (ref));
          branch = flatpak_ref_get_branch (FLATPAK_REF (ref));

          IDE_TRACE_MSG ("Found %s/%s/%s installed in installation[%u]",
                         id, arch, branch, i);

          if (g_strcmp0 (self->id, id) == 0 &&
              g_strcmp0 (self->branch, branch) == 0 &&
              g_strcmp0 (self->arch, arch) == 0)
            {
              if (!self->force_update)
                {
                  IDE_TRACE_MSG ("Force update unset, considering transfer complete");
                  g_task_return_boolean (task, TRUE);
                  IDE_EXIT;
                }

              if (!update_installation (self, installation, cancellable, &error))
                g_task_return_error (task, g_steal_pointer (&error));
              else
                g_task_return_boolean (task, TRUE);

              IDE_EXIT;
            }
        }
    }

  /*
   * We didn't locate the id under a previous installation, so we need to
   * locate a remote that has the matching ref and install it from that.
   */
  g_debug ("%s was not found, locating within remote", self->id);

  for (guint i = 0; i < G_N_ELEMENTS (installations); i++)
    {
      FlatpakInstallation *installation = installations[i];
      g_autoptr(GPtrArray) remotes = NULL;
      g_autoptr(GError) error = NULL;

      if (installation == NULL)
        continue;

      remotes = flatpak_installation_list_remotes (installation, cancellable, &error);

      if (error != NULL)
        {
          g_task_return_error (task, g_steal_pointer (&error));
          IDE_EXIT;
        }

      for (guint j = 0; j < remotes->len; j++)
        {
          FlatpakRemote *remote = g_ptr_array_index (remotes, j);
          g_autoptr(GPtrArray) refs = NULL;

          g_assert (FLATPAK_IS_REMOTE (remote));

          refs = flatpak_installation_list_remote_refs_sync (installation,
                                                             flatpak_remote_get_name (remote),
                                                             cancellable,
                                                             &error);

          if (error != NULL)
            {
              g_task_return_error (task, g_steal_pointer (&error));
              IDE_EXIT;
            }

          for (guint k = 0; k < refs->len; k++)
            {
              FlatpakRemoteRef *ref = g_ptr_array_index (refs, k);
              const gchar *id;
              const gchar *arch;
              const gchar *branch;

              g_assert (FLATPAK_IS_REMOTE_REF (ref));

              id = flatpak_ref_get_name (FLATPAK_REF (ref));
              arch = flatpak_ref_get_arch (FLATPAK_REF (ref));
              branch = flatpak_ref_get_branch (FLATPAK_REF (ref));

              if (g_strcmp0 (self->id, id) == 0 &&
                  g_strcmp0 (self->branch, branch) == 0 &&
                  g_strcmp0 (self->arch, arch) == 0)
                {
                  if (install_from_remote (self, installation, remote, cancellable, &error))
                    g_task_return_boolean (task, TRUE);
                  else
                    g_task_return_error (task, g_steal_pointer (&error));

                  IDE_EXIT;
                }
            }
        }
    }

  g_task_return_new_error (task,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_FOUND,
                           /* Translators: %s is the id of the runtime such as org.gnome.Sdk */
                           _("Failed to locate %s"),
                           self->id);

  IDE_EXIT;
}

static void
gbp_flatpak_transfer_execute_async (IdeTransfer         *transfer,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  GbpFlatpakTransfer *self = (GbpFlatpakTransfer *)transfer;
  g_autoptr(GTask) task = NULL;

  g_assert (GBP_IS_FLATPAK_TRANSFER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_flatpak_transfer_execute_async);
  g_task_run_in_thread (task, gbp_flatpak_transfer_execute_worker);
}

static gboolean
gbp_flatpak_transfer_execute_finish (IdeTransfer   *transfer,
                                     GAsyncResult  *result,
                                     GError       **error)
{
  g_assert (GBP_IS_FLATPAK_TRANSFER (transfer));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
transfer_iface_init (IdeTransferInterface *iface)
{
  iface->execute_async = gbp_flatpak_transfer_execute_async;
  iface->execute_finish = gbp_flatpak_transfer_execute_finish;
}

static void
gbp_flatpak_transfer_finalize (GObject *object)
{
  GbpFlatpakTransfer *self = (GbpFlatpakTransfer *)object;

  g_clear_pointer (&self->id, g_free);
  g_clear_pointer (&self->arch, g_free);
  g_clear_pointer (&self->branch, g_free);
  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (gbp_flatpak_transfer_parent_class)->finalize (object);
}

static void
gbp_flatpak_transfer_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GbpFlatpakTransfer *self = GBP_FLATPAK_TRANSFER (object);

  switch (prop_id)
    {
    case PROP_STATUS:
      g_mutex_lock (&self->mutex);
      g_value_set_string (value, self->status);
      g_mutex_unlock (&self->mutex);
      break;

    case PROP_TITLE:
      g_value_take_string (value, g_strdup_printf (_("Installing %s"), self->id));
      break;

    case PROP_ICON_NAME:
      g_value_set_string (value, "folder-download-symbolic");
      break;

    case PROP_PROGRESS:
      g_mutex_lock (&self->mutex);
      g_value_set_double (value, self->progress);
      g_mutex_unlock (&self->mutex);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_flatpak_transfer_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GbpFlatpakTransfer *self = GBP_FLATPAK_TRANSFER (object);

  switch (prop_id)
    {
    case PROP_ID:
      self->id = g_value_dup_string (value);
      break;

    case PROP_ARCH:
      self->arch = g_value_dup_string (value);
      if (self->arch == NULL)
        self->arch = g_strdup (flatpak_get_default_arch ());
      break;

    case PROP_BRANCH:
      self->branch = g_value_dup_string (value);
      if (self->branch == NULL)
        self->branch = g_strdup ("stable");
      break;

    case PROP_FORCE_UPDATE:
      self->force_update = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_flatpak_transfer_class_init (GbpFlatpakTransferClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_flatpak_transfer_finalize;
  object_class->get_property = gbp_flatpak_transfer_get_property;
  object_class->set_property = gbp_flatpak_transfer_set_property;

  properties [PROP_ID] =
    g_param_spec_string ("id", NULL, NULL, NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties [PROP_ARCH] =
    g_param_spec_string ("arch", NULL, NULL, NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties [PROP_BRANCH] =
    g_param_spec_string ("branch", NULL, NULL, NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  
  properties [PROP_FORCE_UPDATE] =
    g_param_spec_boolean ("force-update", NULL, NULL, FALSE,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties [PROP_STATUS] =
    g_param_spec_string ("status", NULL, NULL, NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties [PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL, NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties [PROP_ICON_NAME] =
    g_param_spec_string ("icon-name", NULL, NULL, NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties [PROP_PROGRESS] =
    g_param_spec_double ("progress", NULL, NULL, 0.0, 100.0, 0.0,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_flatpak_transfer_init (GbpFlatpakTransfer *self)
{
  g_mutex_init (&self->mutex);

  self->arch = g_strdup (flatpak_get_default_arch ());
  self->branch = g_strdup ("master");
}

GbpFlatpakTransfer *
gbp_flatpak_transfer_new (IdeContext  *context,
                          const gchar *id,
                          const gchar *arch,
                          const gchar *branch,
                          gboolean     force_update)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (id != NULL, NULL);

  if (arch == NULL)
    arch = flatpak_get_default_arch ();

  if (branch == NULL)
    branch = "stable";

  return g_object_new (GBP_TYPE_FLATPAK_TRANSFER,
                       "context", context,
                       "id", id,
                       "arch", arch,
                       "branch", branch,
                       "force-update", force_update,
                       NULL);
}

gboolean
gbp_flatpak_transfer_is_installed (GbpFlatpakTransfer *self,
                                   GCancellable       *cancellable)
{
  FlatpakInstallation *installations[2] = { NULL };
  g_autoptr(FlatpakInstallation) user = NULL;
  g_autoptr(FlatpakInstallation) system = NULL;

  IDE_ENTRY;

  g_return_val_if_fail (GBP_IS_FLATPAK_TRANSFER (self), FALSE);

  installations[0] = user = flatpak_installation_new_user (cancellable, NULL);
  installations[1] = system = flatpak_installation_new_system (cancellable, NULL);

  for (guint i = 0; i < G_N_ELEMENTS (installations); i++)
    {
      FlatpakInstallation *installation = installations[i];
      g_autoptr(GError) error = NULL;
      g_autoptr(GPtrArray) refs = NULL;

      if (installation == NULL)
        continue;

      refs = flatpak_installation_list_installed_refs (installation, cancellable, &error);

      if (refs == NULL)
        continue;

      for (guint j = 0; j < refs->len; j++)
        {
          FlatpakInstalledRef *ref = g_ptr_array_index (refs, j);
          const gchar *id;
          const gchar *arch;
          const gchar *branch;

          g_assert (FLATPAK_IS_INSTALLED_REF (ref));

          id = flatpak_ref_get_name (FLATPAK_REF (ref));
          arch = flatpak_ref_get_arch (FLATPAK_REF (ref));
          branch = flatpak_ref_get_branch (FLATPAK_REF (ref));

          if (g_strcmp0 (self->id, id) == 0 &&
              g_strcmp0 (self->branch, branch) == 0 &&
              g_strcmp0 (self->arch, arch) == 0)
            return TRUE;
        }
    }

  return FALSE;
}
