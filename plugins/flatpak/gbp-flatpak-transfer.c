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

#include "gbp-flatpak-application-addin.h"
#include "gbp-flatpak-transfer.h"

struct _GbpFlatpakTransfer
{
  IdeObject    parent_instance;

  gchar       *id;
  gchar       *arch;
  gchar       *branch;
  gchar       *status;
  gdouble      progress;
  guint        force_update : 1;
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

G_DEFINE_TYPE_WITH_CODE (GbpFlatpakTransfer, gbp_flatpak_transfer, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_TRANSFER, transfer_iface_init))

static GParamSpec *properties [N_PROPS];

static void
proxy_notify (GbpFlatpakTransfer *self,
              GParamSpec         *pspec,
              IdeProgress        *progress)
{
  g_assert (GBP_IS_FLATPAK_TRANSFER (self));
  g_assert (pspec != NULL);
  g_assert (IDE_IS_PROGRESS (progress));

  if (g_strcmp0 (pspec->name, "message") == 0)
    {
      g_free (self->status);
      self->status = ide_progress_get_message (progress);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STATUS]);
    }

  if (g_strcmp0 (pspec->name, "fraction") == 0)
    {
      self->progress = ide_progress_get_fraction (progress);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PROGRESS]);
    }
}

static void
gbp_flatpak_transfer_execute_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  GbpFlatpakApplicationAddin *addin = (GbpFlatpakApplicationAddin *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (addin));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!gbp_flatpak_application_addin_install_runtime_finish (addin, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);

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
  g_autoptr(GTask) task = NULL;
  g_autoptr(IdeProgress) progress = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_TRANSFER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_flatpak_transfer_execute_async);

  addin = gbp_flatpak_application_addin_get_default ();

  if (gbp_flatpak_application_addin_has_runtime (addin, self->id, self->arch, self->branch) && !self->force_update)
    {
      g_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  gbp_flatpak_application_addin_install_runtime_async (addin,
                                                       self->id,
                                                       self->arch,
                                                       self->branch,
                                                       cancellable,
                                                       &progress,
                                                       gbp_flatpak_transfer_execute_cb,
                                                       g_steal_pointer (&task));

  g_signal_connect_object (progress,
                           "notify::fraction",
                           G_CALLBACK (proxy_notify),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (progress,
                           "notify::message",
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
  gboolean ret;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_TRANSFER (transfer));
  g_assert (G_IS_TASK (result));

  ret = g_task_propagate_boolean (G_TASK (result), error);

  IDE_RETURN (ret);
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

  IDE_ENTRY;

  g_clear_pointer (&self->id, g_free);
  g_clear_pointer (&self->arch, g_free);
  g_clear_pointer (&self->branch, g_free);

  G_OBJECT_CLASS (gbp_flatpak_transfer_parent_class)->finalize (object);

  IDE_EXIT;
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
      g_value_set_string (value, self->status);
      break;

    case PROP_TITLE:
      if (g_str_equal (self->arch, flatpak_get_default_arch ()))
        g_value_take_string (value, g_strdup_printf (_("Installing %s %s"), self->id, self->branch));
      else
        g_value_take_string (value, g_strdup_printf (_("Installing %s %s for %s"),
                                                     self->id, self->branch, self->arch));
      break;

    case PROP_ICON_NAME:
      g_value_set_static_string (value, "folder-download-symbolic");
      break;

    case PROP_PROGRESS:
      g_value_set_double (value, self->progress);
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
      if (self->arch == NULL)
        self->arch = g_strdup (flatpak_get_default_arch ());
      break;

    case PROP_BRANCH:
      g_free (self->branch);
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
  self->arch = g_strdup (flatpak_get_default_arch ());
  self->branch = g_strdup ("master");
}

GbpFlatpakTransfer *
gbp_flatpak_transfer_new (const gchar *id,
                          const gchar *arch,
                          const gchar *branch,
                          gboolean     force_update)
{
  g_return_val_if_fail (id != NULL, NULL);

  if (arch == NULL)
    arch = flatpak_get_default_arch ();

  if (branch == NULL)
    branch = "stable";

  return g_object_new (GBP_TYPE_FLATPAK_TRANSFER,
                       "id", id,
                       "arch", arch,
                       "branch", branch,
                       "force-update", force_update,
                       NULL);
}
