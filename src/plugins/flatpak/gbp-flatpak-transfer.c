/* gbp-flatpak-transfer.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-flatpak-transfer"

#include <flatpak.h>
#include <glib/gi18n.h>

#include "gbp-flatpak-application-addin.h"
#include "gbp-flatpak-transfer.h"

struct _GbpFlatpakTransfer
{
  IdeTransfer parent_instance;

  gchar *id;
  gchar *arch;
  gchar *branch;

  guint  has_runtime : 1;
  guint  force_update : 1;
  guint  finished : 1;
  guint  failed : 1;
};

enum {
  PROP_0,
  PROP_ID,
  PROP_ARCH,
  PROP_BRANCH,
  PROP_FORCE_UPDATE,
  N_PROPS
};

G_DEFINE_TYPE (GbpFlatpakTransfer, gbp_flatpak_transfer, IDE_TYPE_TRANSFER)

static GParamSpec *properties [N_PROPS];

static void
gbp_flatpak_transfer_update_title (GbpFlatpakTransfer *self)
{
  g_autofree gchar *triplet = NULL;

  g_return_if_fail (GBP_IS_FLATPAK_TRANSFER (self));

  if (self->id && self->arch && self->branch)
    triplet = g_strdup_printf ("%s/%s/%s", self->id, self->arch, self->branch);
  else if (self->id && self->arch)
    triplet = g_strdup_printf ("%s/%s/", self->id, self->arch);
  else if (self->id && self->branch)
    triplet = g_strdup_printf ("%s//%s", self->id, self->branch);
  else
    triplet = g_strdup_printf ("%s//", self->id);

  if (!self->failed)
    {
      g_autofree gchar *title = NULL;

      if (self->has_runtime)
        {
          if (self->finished)
            /* Translators: %s is replaced with the runtime identifier */
            title = g_strdup_printf (_("Updated %s"), triplet);
          else
            /* Translators: %s is replaced with the runtime identifier */
            title = g_strdup_printf (_("Updating %s"), triplet);
        }
      else
        {
          if (self->finished)
            /* Translators: %s is replaced with the runtime identifier */
            title = g_strdup_printf (_("Installed %s"), triplet);
          else
            /* Translators: %s is replaced with the runtime identifier */
            title = g_strdup_printf (_("Installing %s"), triplet);
        }

      ide_transfer_set_title (IDE_TRANSFER (self), title);
    }
  else
    {
      ide_transfer_set_title (IDE_TRANSFER (self), triplet);
    }
}

static void
task_completed (GbpFlatpakTransfer *self,
                GParamSpec         *pspec,
                IdeTask            *task)
{
  g_assert (GBP_IS_FLATPAK_TRANSFER (self));
  g_assert (IDE_IS_TASK (task));

  self->finished = TRUE;

  gbp_flatpak_transfer_update_title (self);

  ide_transfer_set_progress (IDE_TRANSFER (self), 1.0);

  if (self->failed)
    ide_transfer_set_status (IDE_TRANSFER (self), _("Failed to install runtime"));
  else if (self->finished && self->has_runtime)
    ide_transfer_set_status (IDE_TRANSFER (self), _("Runtime has been updated"));
  else
    ide_transfer_set_status (IDE_TRANSFER (self), _("Runtime has been installed"));
}

static void
proxy_notify (GbpFlatpakTransfer *self,
              GParamSpec         *pspec,
              IdeNotification    *progress)
{
  g_assert (GBP_IS_FLATPAK_TRANSFER (self));
  g_assert (pspec != NULL);
  g_assert (IDE_IS_NOTIFICATION (progress));

  if (g_strcmp0 (pspec->name, "body") == 0)
    {
      g_autofree gchar *message = ide_notification_dup_body (progress);
      ide_transfer_set_status (IDE_TRANSFER (self), message);
    }

  if (g_strcmp0 (pspec->name, "progress") == 0)
    ide_transfer_set_progress (IDE_TRANSFER (self), ide_notification_get_progress (progress));
}

static void
gbp_flatpak_transfer_execute_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  GbpFlatpakApplicationAddin *addin = (GbpFlatpakApplicationAddin *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (addin));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!gbp_flatpak_application_addin_install_runtime_finish (addin, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
gbp_flatpak_transfer_execute_async (IdeTransfer         *transfer,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  GbpFlatpakTransfer *self = (GbpFlatpakTransfer *)transfer;
  GbpFlatpakApplicationAddin *addin;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(IdeNotification) progress = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_TRANSFER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_flatpak_transfer_execute_async);

  g_signal_connect_object (task,
                           "notify::completed",
                           G_CALLBACK (task_completed),
                           self,
                           G_CONNECT_SWAPPED);

  addin = gbp_flatpak_application_addin_get_default ();

  if (self->branch == NULL &&
      gbp_flatpak_application_addin_has_runtime (addin, self->id, self->arch, "stable"))
    self->branch = g_strdup ("stable");

  if (self->branch == NULL &&
      gbp_flatpak_application_addin_has_runtime (addin, self->id, self->arch, "master"))
    self->branch = g_strdup ("master");

  self->failed = FALSE;
  self->finished = FALSE;
  self->has_runtime = gbp_flatpak_application_addin_has_runtime (addin, self->id, self->arch, self->branch);

  if (self->has_runtime && !self->force_update)
    {
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  gbp_flatpak_transfer_update_title (self);

  gbp_flatpak_application_addin_install_runtime_async (addin,
                                                       self->id,
                                                       self->arch,
                                                       self->branch,
                                                       cancellable,
                                                       &progress,
                                                       gbp_flatpak_transfer_execute_cb,
                                                       g_steal_pointer (&task));

  g_signal_connect_object (progress,
                           "notify::progress",
                           G_CALLBACK (proxy_notify),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (progress,
                           "notify::body",
                           G_CALLBACK (proxy_notify),
                           self,
                           G_CONNECT_SWAPPED);

  IDE_EXIT;
}

static gboolean
gbp_flatpak_transfer_execute_finish (IdeTransfer   *transfer,
                                     GAsyncResult  *result,
                                     GError       **error)
{
  GbpFlatpakTransfer *self = (GbpFlatpakTransfer *)transfer;
  gboolean ret;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_TRANSFER (self));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  if (ret == FALSE)
    {
      self->failed = TRUE;
      gbp_flatpak_transfer_update_title (self);
    }

  IDE_RETURN (ret);
}

static void
gbp_flatpak_transfer_finalize (GObject *object)
{
  GbpFlatpakTransfer *self = (GbpFlatpakTransfer *)object;

  g_clear_pointer (&self->id, g_free);
  g_clear_pointer (&self->arch, g_free);
  g_clear_pointer (&self->branch, g_free);

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
    case PROP_ID:
      g_value_set_string (value, self->id);
      break;

    case PROP_ARCH:
      g_value_set_string (value, self->arch);
      break;

    case PROP_BRANCH:
      g_value_set_string (value, self->branch);
      break;

    case PROP_FORCE_UPDATE:
      g_value_set_boolean (value, self->force_update);
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
      g_free (self->id);
      self->id = g_value_dup_string (value);
      break;

    case PROP_ARCH:
      g_free (self->arch);
      self->arch = g_value_dup_string (value);
      break;

    case PROP_BRANCH:
      g_free (self->branch);
      self->branch = g_value_dup_string (value);
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
  IdeTransferClass *transfer_class = IDE_TRANSFER_CLASS (klass);

  object_class->finalize = gbp_flatpak_transfer_finalize;
  object_class->get_property = gbp_flatpak_transfer_get_property;
  object_class->set_property = gbp_flatpak_transfer_set_property;

  transfer_class->execute_async = gbp_flatpak_transfer_execute_async;
  transfer_class->execute_finish = gbp_flatpak_transfer_execute_finish;

  properties [PROP_ID] =
    g_param_spec_string ("id",
                         "Id",
                         "The runtime identifier such as org.gnome.Platform",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_ARCH] =
    g_param_spec_string ("arch",
                         "Arch",
                         "The arch identifier such as x86_64",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_BRANCH] =
    g_param_spec_string ("branch",
                         "Branch",
                         "The branch identifier such as 'stable'",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_FORCE_UPDATE] =
    g_param_spec_boolean ("force-update",
                          "Force Update",
                          "If we should always try to at least update",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_flatpak_transfer_init (GbpFlatpakTransfer *self)
{
}

GbpFlatpakTransfer *
gbp_flatpak_transfer_new (const gchar *id,
                          const gchar *arch,
                          const gchar *branch,
                          gboolean     force_update)
{
  GbpFlatpakTransfer *ret;

  g_return_val_if_fail (id != NULL, NULL);

  if (arch == NULL)
    arch = flatpak_get_default_arch ();

  ret = g_object_new (GBP_TYPE_FLATPAK_TRANSFER,
                      "id", id,
                      "arch", arch,
                      "branch", branch,
                      "force-update", force_update,
                      NULL);
  gbp_flatpak_transfer_update_title (ret);

  return ret;
}
