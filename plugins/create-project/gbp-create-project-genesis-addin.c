/* gbp-create-project-genesis-addin.c
 *
 * Copyright (C) 2016 Christian Hergert <christian@hergert.me>
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

  guint                   is_ready : 1;
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

  ide_clear_weak_pointer (&self->widget);

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
      g_value_set_boolean (value, self->is_ready);
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
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_create_project_genesis_addin_init (GbpCreateProjectGenesisAddin *self)
{
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
      ide_set_weak_pointer (&self->widget, widget);

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
    }

  return GTK_WIDGET (self->widget);
}

static gchar *
gbp_create_project_genesis_addin_get_icon_name (IdeGenesisAddin *addin)
{
  return g_strdup ("gtk-missing");
}

static gchar *
gbp_create_project_genesis_addin_get_title (IdeGenesisAddin *addin)
{
  return g_strdup (_("From a project template"));
}

static void
gbp_create_project_genesis_addin_run_async (IdeGenesisAddin     *addin,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data)
{
  GbpCreateProjectGenesisAddin *self = (GbpCreateProjectGenesisAddin *)addin;
  g_autoptr(GTask) task = NULL;

  g_assert (GBP_IS_CREATE_PROJECT_GENESIS_ADDIN (self));

  task = g_task_new (self, cancellable, callback, user_data);

  /*
   * TODO: Generate the project from information found in the widget.
   *       See gbp-create-project-tool.c for examples.
   */

  g_task_return_boolean (task, TRUE);
}

static gboolean
gbp_create_project_genesis_addin_run_finish (IdeGenesisAddin  *addin,
                                             GAsyncResult     *result,
                                             GError          **error)
{
  g_assert (GBP_IS_CREATE_PROJECT_GENESIS_ADDIN (addin));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
genesis_addin_iface_init (IdeGenesisAddinInterface *iface)
{
  iface->get_icon_name = gbp_create_project_genesis_addin_get_icon_name;
  iface->get_title = gbp_create_project_genesis_addin_get_title;
  iface->get_widget = gbp_create_project_genesis_addin_get_widget;
  iface->run_async = gbp_create_project_genesis_addin_run_async;
  iface->run_finish = gbp_create_project_genesis_addin_run_finish;
}
