/* gbp-create-project-genesis-addin.c
 *
 * Copyright 2016 Christian Hergert <christian@hergert.me>
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

#include "gbp-create-project-genesis-addin.h"
#include "gbp-create-project-widget.h"

static void genesis_addin_iface_init (IdeGenesisAddinInterface *iface);

struct _GbpCreateProjectGenesisAddin
{
  GObject                 parent;
  GbpCreateProjectWidget *widget;
};

G_DEFINE_TYPE_EXTENDED (GbpCreateProjectGenesisAddin,
                        gbp_create_project_genesis_addin,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_GENESIS_ADDIN,
                                               genesis_addin_iface_init))

enum {
  PROP_0,
  PROP_IS_READY,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
gbp_create_project_genesis_addin_finalize (GObject *object)
{
  GbpCreateProjectGenesisAddin *self = (GbpCreateProjectGenesisAddin *)object;

  g_clear_weak_pointer (&self->widget);

  G_OBJECT_CLASS (gbp_create_project_genesis_addin_parent_class)->finalize (object);
}

static void
gbp_create_project_genesis_addin_get_property (GObject    *object,
                                               guint       prop_id,
                                               GValue     *value,
                                               GParamSpec *pspec)
{
  GbpCreateProjectGenesisAddin *self = GBP_CREATE_PROJECT_GENESIS_ADDIN (object);

  switch (prop_id)
    {
    case PROP_IS_READY:
      if (self->widget != NULL)
        g_object_get_property (G_OBJECT (self->widget), "is-ready", value);
      else
        g_value_set_boolean (value, FALSE);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_create_project_genesis_addin_class_init (GbpCreateProjectGenesisAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_create_project_genesis_addin_finalize;
  object_class->get_property = gbp_create_project_genesis_addin_get_property;

  properties [PROP_IS_READY] =
    g_param_spec_boolean ("is-ready",
                          "Is Ready",
                          "Is Ready",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_create_project_genesis_addin_init (GbpCreateProjectGenesisAddin *self)
{
}

static void
widget_is_ready (GtkWidget                    *widget,
                 GParamSpec                   *pspec,
                 GbpCreateProjectGenesisAddin *self)
{
  g_assert (GBP_IS_CREATE_PROJECT_GENESIS_ADDIN (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_IS_READY]);
}

static GtkWidget *
gbp_create_project_genesis_addin_get_widget (IdeGenesisAddin *addin)
{
  GbpCreateProjectGenesisAddin *self = (GbpCreateProjectGenesisAddin *)addin;

  g_assert (GBP_IS_CREATE_PROJECT_GENESIS_ADDIN (self));

  if (self->widget == NULL)
    {
      GbpCreateProjectWidget *widget;

      widget = g_object_new (GBP_TYPE_CREATE_PROJECT_WIDGET,
                             "visible", TRUE,
                             NULL);
      g_set_weak_pointer (&self->widget, widget);

      /*
       * TODO: You need to watch for changes in the widget's entries, and when
       *       the widget satisfies "valid input", you need to:
       *
       *       self->is_ready = TRUE;
       *       g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_IS_READY]);
       *
       *       In the past, i've added an "is-ready" property to the widget
       *       and just g_object_bind_property() to self. That would require
       *       making the GbpCreateProjectGenesisAddin:is-ready property
       *       writable though.
       */
      g_signal_connect (self->widget,
                        "notify::is-ready",
                        G_CALLBACK (widget_is_ready),
                        self);
    }

  return GTK_WIDGET (self->widget);
}

static gchar *
gbp_create_project_genesis_addin_get_icon_name (IdeGenesisAddin *addin)
{
  return g_strdup ("folder-templates-symbolic");
}

static gchar *
gbp_create_project_genesis_addin_get_title (IdeGenesisAddin *addin)
{
  return g_strdup (_("New Project"));
}

static void
gbp_create_project_genesis_addin_run_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  GbpCreateProjectWidget *widget = (GbpCreateProjectWidget *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_TASK (task));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (widget));

  if (!gbp_create_project_widget_create_finish (widget, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);
}

static void
gbp_create_project_genesis_addin_run_async (IdeGenesisAddin     *addin,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data)
{
  GbpCreateProjectGenesisAddin *self = (GbpCreateProjectGenesisAddin *)addin;
  g_autoptr(IdeTask) task = NULL;

  g_assert (GBP_IS_CREATE_PROJECT_GENESIS_ADDIN (self));

  task = ide_task_new (self, cancellable, callback, user_data);

  /*
   * TODO: Generate the project from information found in the widget.
   *       See gbp-create-project-tool.c for examples.
   */

  gbp_create_project_widget_create_async (self->widget,
                                          cancellable,
                                          gbp_create_project_genesis_addin_run_cb,
                                          g_object_ref (task));
}

static gboolean
gbp_create_project_genesis_addin_run_finish (IdeGenesisAddin  *addin,
                                             GAsyncResult     *result,
                                             GError          **error)
{
  g_return_val_if_fail (GBP_IS_CREATE_PROJECT_GENESIS_ADDIN (addin), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static gint
gbp_create_project_genesis_addin_get_priority (IdeGenesisAddin *addin)
{
  return -100;
}

static gchar *
gbp_create_project_genesis_addin_get_label (IdeGenesisAddin *addin)
{
  return g_strdup (_("New…"));
}

static gchar *
gbp_create_project_genesis_addin_get_next_label (IdeGenesisAddin *addin)
{
  return g_strdup (_("Create"));
}

static void
genesis_addin_iface_init (IdeGenesisAddinInterface *iface)
{
  iface->get_icon_name = gbp_create_project_genesis_addin_get_icon_name;
  iface->get_title = gbp_create_project_genesis_addin_get_title;
  iface->get_widget = gbp_create_project_genesis_addin_get_widget;
  iface->run_async = gbp_create_project_genesis_addin_run_async;
  iface->run_finish = gbp_create_project_genesis_addin_run_finish;
  iface->get_priority = gbp_create_project_genesis_addin_get_priority;
  iface->get_label = gbp_create_project_genesis_addin_get_label;
  iface->get_next_label = gbp_create_project_genesis_addin_get_next_label;
}
