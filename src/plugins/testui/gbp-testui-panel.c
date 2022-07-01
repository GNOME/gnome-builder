/* gbp-testui-panel.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-testui-panel"

#include "config.h"

#include <libide-foundry.h>

#include "gbp-testui-panel.h"

struct _GbpTestuiPanel
{
  IdePane          parent_instance;

  GtkListView    *list_view;
  GtkNoSelection *selection;

  GListModel      *model;
};

G_DEFINE_FINAL_TYPE (GbpTestuiPanel, gbp_testui_panel, IDE_TYPE_PANE)

enum {
  PROP_0,
  PROP_MODEL,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
gbp_testui_panel_activate_cb (GbpTestuiPanel *self,
                              guint           position,
                              GtkListView    *list_view)
{
  GtkSelectionModel *model;
  g_autoptr(IdeTest) test = NULL;
  IdeTestManager *test_manager;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TESTUI_PANEL (self));
  g_assert (GTK_IS_LIST_VIEW (list_view));

  model = gtk_list_view_get_model (list_view);
  test = g_list_model_get_item (G_LIST_MODEL (model), position);

  g_assert (IDE_IS_TEST (test));

  g_debug ("Activating test \"%s\"", ide_test_get_id (test));

  context = ide_widget_get_context (GTK_WIDGET (self));
  test_manager = ide_test_manager_from_context (context);
  ide_test_manager_run_async (test_manager, test, NULL, NULL, NULL);

  IDE_EXIT;
}

static void
gbp_testui_panel_set_model (GbpTestuiPanel *self,
                            GListModel     *model)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TESTUI_PANEL (self));
  g_assert (!model || G_IS_LIST_MODEL (model));

  if (g_set_object (&self->model, model))
    {
      gtk_no_selection_set_model (self->selection, model);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
    }
}

static void
gbp_testui_panel_dispose (GObject *object)
{
  GbpTestuiPanel *self = (GbpTestuiPanel *)object;

  g_clear_object (&self->model);

  G_OBJECT_CLASS (gbp_testui_panel_parent_class)->dispose (object);
}

static void
gbp_testui_panel_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GbpTestuiPanel *self = GBP_TESTUI_PANEL (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_testui_panel_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GbpTestuiPanel *self = GBP_TESTUI_PANEL (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      gbp_testui_panel_set_model (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_testui_panel_class_init (GbpTestuiPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gbp_testui_panel_dispose;
  object_class->get_property = gbp_testui_panel_get_property;
  object_class->set_property = gbp_testui_panel_set_property;

  properties [PROP_MODEL] =
    g_param_spec_object ("model", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/testui/gbp-testui-panel.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpTestuiPanel, list_view);
  gtk_widget_class_bind_template_child (widget_class, GbpTestuiPanel, selection);
  gtk_widget_class_bind_template_callback (widget_class, gbp_testui_panel_activate_cb);
}

static void
gbp_testui_panel_init (GbpTestuiPanel *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GbpTestuiPanel *
gbp_testui_panel_new (void)
{
  return g_object_new (GBP_TYPE_TESTUI_PANEL, NULL);
}
