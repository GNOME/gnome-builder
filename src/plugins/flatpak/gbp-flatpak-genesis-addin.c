/* gbp-flatpak-genesis-addin.c
 *
 * Copyright © 2016 Endless Mobile, Inc.
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

#include <glib/gi18n.h>
#include <ide.h>

#include "gbp-flatpak-clone-widget.h"
#include "gbp-flatpak-genesis-addin.h"

struct _GbpFlatpakGenesisAddin
{
  GObject                parent_instance;
  GbpFlatpakCloneWidget *clone_widget;
};

static void genesis_addin_iface_init (IdeGenesisAddinInterface *iface);

G_DEFINE_TYPE_EXTENDED (GbpFlatpakGenesisAddin, gbp_flatpak_genesis_addin, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_GENESIS_ADDIN, genesis_addin_iface_init))

enum {
  PROP_0,
  PROP_IS_READY
};

static void
gbp_flatpak_genesis_addin_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GbpFlatpakGenesisAddin *self = GBP_FLATPAK_GENESIS_ADDIN(object);

  switch (prop_id)
    {
    case PROP_IS_READY:
      if (self->clone_widget != NULL)
        g_object_get_property (G_OBJECT (self->clone_widget), "is-ready", value);
      else
        g_value_set_boolean (value, FALSE);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
gbp_flatpak_genesis_addin_class_init (GbpFlatpakGenesisAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gbp_flatpak_genesis_addin_get_property;

  g_object_class_install_property (object_class,
                                   PROP_IS_READY,
                                   g_param_spec_boolean ("is-ready",
                                                         "Is Ready",
                                                         "If the widget is ready to continue.",
                                                         FALSE,
                                                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));
}

static void
gbp_flatpak_genesis_addin_init (GbpFlatpakGenesisAddin *self)
{
}

static gchar *
gbp_flatpak_genesis_addin_get_icon_name (IdeGenesisAddin *addin)
{
  return g_strdup ("gitg-symbolic");
}

static gchar *
gbp_flatpak_genesis_addin_get_title (IdeGenesisAddin *addin)
{
  return g_strdup (_("Clone App"));
}

static void
widget_is_ready (GtkWidget              *widget,
                 GParamSpec             *pspec,
                 GbpFlatpakGenesisAddin *self)
{
  g_assert (GBP_IS_FLATPAK_GENESIS_ADDIN (self));

  g_object_notify (G_OBJECT (self), "is-ready");
}

static GtkWidget *
gbp_flatpak_genesis_addin_get_widget (IdeGenesisAddin *addin)
{
  GbpFlatpakGenesisAddin *self = (GbpFlatpakGenesisAddin *)addin;

  g_assert (GBP_IS_FLATPAK_GENESIS_ADDIN (self));

  if (self->clone_widget == NULL)
    {
      self->clone_widget = g_object_new (GBP_TYPE_FLATPAK_CLONE_WIDGET,
                                         "visible", TRUE,
                                         NULL);
      g_signal_connect (self->clone_widget,
                        "notify::is-ready",
                        G_CALLBACK (widget_is_ready),
                        self);
    }

  return GTK_WIDGET (self->clone_widget);
}

static void
gbp_flatpak_genesis_addin_run_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  GbpFlatpakCloneWidget *widget = (GbpFlatpakCloneWidget *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (GBP_IS_FLATPAK_CLONE_WIDGET (widget));

  if (!gbp_flatpak_clone_widget_clone_finish (widget, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);
}

static void
gbp_flatpak_genesis_addin_run_async (IdeGenesisAddin     *addin,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  GbpFlatpakGenesisAddin *self = (GbpFlatpakGenesisAddin *)addin;
  GTask *task;

  g_return_if_fail (GBP_IS_FLATPAK_GENESIS_ADDIN (addin));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  gbp_flatpak_clone_widget_clone_async (self->clone_widget,
                                        cancellable,
                                        gbp_flatpak_genesis_addin_run_cb,
                                        task);
}

static gboolean
gbp_flatpak_genesis_addin_run_finish (IdeGenesisAddin  *addin,
                                      GAsyncResult     *result,
                                      GError          **error)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_GENESIS_ADDIN (addin), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static gint
gbp_flatpak_genesis_addin_get_priority (IdeGenesisAddin *addin)
{
  return 100;
}

static gchar *
gbp_flatpak_genesis_addin_get_label (IdeGenesisAddin *addin)
{
  return NULL;
}

static gchar *
gbp_flatpak_genesis_addin_get_next_label (IdeGenesisAddin *addin)
{
  return g_strdup (_("Clone"));
}

static void
genesis_addin_iface_init (IdeGenesisAddinInterface *iface)
{
  iface->get_title = gbp_flatpak_genesis_addin_get_title;
  iface->get_icon_name = gbp_flatpak_genesis_addin_get_icon_name;
  iface->get_widget = gbp_flatpak_genesis_addin_get_widget;
  iface->run_async = gbp_flatpak_genesis_addin_run_async;
  iface->run_finish = gbp_flatpak_genesis_addin_run_finish;
  iface->get_priority = gbp_flatpak_genesis_addin_get_priority;
  iface->get_label = gbp_flatpak_genesis_addin_get_label;
  iface->get_next_label = gbp_flatpak_genesis_addin_get_next_label;
}
