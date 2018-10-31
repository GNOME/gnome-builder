/* gbp-ls-view.c
 *
 * Copyright © 2018 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-ls-view"

#include "gbp-ls-view.h"

struct _GbpLsView
{
  IdeLayoutView      parent_instance;

  GtkScrolledWindow *scroller;
  GtkTreeView       *tree_view;
};

enum {
  PROP_0,
  PROP_DIRECTORY,
  N_PROPS
};

G_DEFINE_TYPE (GbpLsView, gbp_ls_view, IDE_TYPE_LAYOUT_VIEW)

static GParamSpec *properties [N_PROPS];

static void
gbp_ls_view_finalize (GObject *object)
{
  GbpLsView *self = (GbpLsView *)object;

  G_OBJECT_CLASS (gbp_ls_view_parent_class)->finalize (object);
}

static void
gbp_ls_view_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GbpLsView *self = GBP_LS_VIEW (object);

  switch (prop_id)
    {
    case PROP_DIRECTORY:
      g_value_set_object (value, gbp_ls_view_get_directory (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_ls_view_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GbpLsView *self = GBP_LS_VIEW (object);

  switch (prop_id)
    {
    case PROP_DIRECTORY:
      gbp_ls_view_set_directory (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_ls_view_class_init (GbpLsViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gbp_ls_view_finalize;
  object_class->get_property = gbp_ls_view_get_property;
  object_class->set_property = gbp_ls_view_set_property;

  properties [PROP_DIRECTORY] =
    g_param_spec_object ("directory",
                         "Directory",
                         "The directory to be displayed",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/ls/gbp-ls-view.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpLsView, scroller);
  gtk_widget_class_bind_template_child (widget_class, GbpLsView, tree_view);
}

static void
gbp_ls_view_init (GbpLsView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
gbp_ls_view_new (void)
{
  return g_object_new (GBP_TYPE_LS_VIEW, NULL);
}

GFile *
gbp_ls_view_get_directory (GbpLsView *self)
{
  g_return_val_if_fail (GBP_IS_LS_VIEW (self), NULL);

  return NULL;
}

void
gbp_ls_view_set_directory (GbpLsView *self,
                           GFile     *directory)
{
  g_return_if_fail (GBP_IS_LS_VIEW (self));
  g_return_if_fail (!directory || G_IS_FILE (directory));

}
