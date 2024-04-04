/*
 * gbp-manuals-panel.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include <libide-tree.h>

#include "gbp-manuals-panel.h"

#include "manuals-navigatable.h"
#include "manuals-navigatable-model.h"

struct _GbpManualsPanel
{
  IdePane            parent_instance;

  ManualsRepository *repository;

  IdeTree           *tree;
};

enum {
  PROP_0,
  PROP_REPOSITORY,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpManualsPanel, gbp_manuals_panel, IDE_TYPE_PANE)

static GParamSpec *properties[N_PROPS];

static void
gbp_manuals_panel_dispose (GObject *object)
{
  GbpManualsPanel *self = (GbpManualsPanel *)object;

  gtk_widget_dispose_template (GTK_WIDGET (self), GBP_TYPE_MANUALS_PANEL);

  g_clear_object (&self->repository);

  G_OBJECT_CLASS (gbp_manuals_panel_parent_class)->dispose (object);
}

static void
gbp_manuals_panel_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GbpManualsPanel *self = GBP_MANUALS_PANEL (object);

  switch (prop_id)
    {
    case PROP_REPOSITORY:
      g_value_set_object (value, self->repository);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_manuals_panel_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GbpManualsPanel *self = GBP_MANUALS_PANEL (object);

  switch (prop_id)
    {
    case PROP_REPOSITORY:
      gbp_manuals_panel_set_repository (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_manuals_panel_class_init (GbpManualsPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gbp_manuals_panel_dispose;
  object_class->get_property = gbp_manuals_panel_get_property;
  object_class->set_property = gbp_manuals_panel_set_property;

  properties[PROP_REPOSITORY] =
    g_param_spec_object ("repository", NULL, NULL,
                         MANUALS_TYPE_REPOSITORY,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/manuals/gbp-manuals-panel.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpManualsPanel, tree);
}

static void
gbp_manuals_panel_init (GbpManualsPanel *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

void
gbp_manuals_panel_set_repository (GbpManualsPanel   *self,
                                  ManualsRepository *repository)
{
  g_autoptr(IdeTreeNode) root = NULL;

  g_return_if_fail (GBP_IS_MANUALS_PANEL (self));
  g_return_if_fail (MANUALS_IS_REPOSITORY (repository));

  if (!g_set_object (&self->repository, repository))
    return;

  root = ide_tree_node_new ();
  ide_tree_node_set_item (root, repository);
  ide_tree_set_root (self->tree, root);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_REPOSITORY]);
}

GbpManualsPanel *
gbp_manuals_panel_new (void)
{
  return g_object_new (GBP_TYPE_MANUALS_PANEL, NULL);
}
