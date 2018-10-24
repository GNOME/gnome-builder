/* gbp-grep-panel.c
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

#define G_LOG_DOMAIN "gbp-grep-panel"

#include <glib/gi18n.h>

#include "gbp-grep-panel.h"

struct _GbpGrepPanel
{
  DzlDockWidget  parent_instance;
  GtkTreeView   *tree_view;
};

enum {
  PROP_0,
  PROP_MODEL,
  N_PROPS
};

G_DEFINE_TYPE (GbpGrepPanel, gbp_grep_panel, DZL_TYPE_DOCK_WIDGET)

static GParamSpec *properties [N_PROPS];

static void
match_data_func (GtkCellLayout   *layout,
                 GtkCellRenderer *cell,
                 GtkTreeModel    *model,
                 GtkTreeIter     *iter,
                 gpointer         user_data)
{
  g_auto(GValue) src = G_VALUE_INIT;
  g_auto(GValue) dst = G_VALUE_INIT;
  const gchar *str;
  const gchar *tmp;
  guint count = 0;

  g_assert (GTK_IS_CELL_LAYOUT (layout));
  g_assert (GTK_IS_CELL_RENDERER_TEXT (cell));
  g_assert (GTK_IS_TREE_MODEL (model));
  g_assert (iter != NULL);

  gtk_tree_model_get_value (model, iter, 0, &src);
  str = g_value_get_string (&src);
  g_value_init (&dst, G_TYPE_STRING);

  for (tmp = str; *tmp; tmp = g_utf8_next_char (tmp))
    {
      if (*tmp == ':')
        {
          if (count == 1)
            {
              tmp++;
              /* We can use static string because we control
               * the lifetime of the GValue here. Let's us avoid
               * an unnecessary copy.
               */
              g_value_set_static_string (&dst, tmp);
              break;
            }
          count++;
        }
    }

  g_object_set_property (G_OBJECT (cell), "text", &dst);
}

static void
path_data_func (GtkCellLayout   *layout,
                GtkCellRenderer *cell,
                GtkTreeModel    *model,
                GtkTreeIter     *iter,
                gpointer         user_data)
{
  g_auto(GValue) src = G_VALUE_INIT;
  g_auto(GValue) dst = G_VALUE_INIT;
  const gchar *str;
  const gchar *tmp;
  guint count = 0;

  g_assert (GTK_IS_CELL_LAYOUT (layout));
  g_assert (GTK_IS_CELL_RENDERER_TEXT (cell));
  g_assert (GTK_IS_TREE_MODEL (model));
  g_assert (iter != NULL);

  gtk_tree_model_get_value (model, iter, 0, &src);
  str = g_value_get_string (&src);
  g_value_init (&dst, G_TYPE_STRING);

  for (tmp = str; *tmp; tmp = g_utf8_next_char (tmp))
    {
      if (*tmp == ':')
        {
          if (count == 1)
            {
              g_value_take_string (&dst, g_strndup (str, tmp - str));
              break;
            }
          count++;
        }
    }

  g_object_set_property (G_OBJECT (cell), "text", &dst);
}

static void
filename_data_func (GtkCellLayout   *layout,
                    GtkCellRenderer *cell,
                    GtkTreeModel    *model,
                    GtkTreeIter     *iter,
                    gpointer         user_data)
{
  g_auto(GValue) src = G_VALUE_INIT;
  g_auto(GValue) dst = G_VALUE_INIT;
  const gchar *str;
  const gchar *tmp;
  const gchar *slash;
  guint count = 0;

  g_assert (GTK_IS_CELL_LAYOUT (layout));
  g_assert (GTK_IS_CELL_RENDERER_TEXT (cell));
  g_assert (GTK_IS_TREE_MODEL (model));
  g_assert (iter != NULL);

  gtk_tree_model_get_value (model, iter, 0, &src);
  slash = str = g_value_get_string (&src);
  g_value_init (&dst, G_TYPE_STRING);

  for (tmp = str; *tmp; tmp = g_utf8_next_char (tmp))
    {
      if (*tmp == ':')
        {
          if (count == 1)
            {
              g_value_take_string (&dst, g_strndup (slash, tmp - slash));
              break;
            }
          count++;
        }
      else if (*tmp == G_DIR_SEPARATOR)
        {
          slash = tmp + 1;
        }
    }

  g_object_set_property (G_OBJECT (cell), "text", &dst);
}

static void
gbp_grep_panel_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GbpGrepPanel *self = GBP_GREP_PANEL (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, gbp_grep_panel_get_model (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_grep_panel_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GbpGrepPanel *self = GBP_GREP_PANEL (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      gbp_grep_panel_set_model (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_grep_panel_class_init (GbpGrepPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = gbp_grep_panel_get_property;
  object_class->set_property = gbp_grep_panel_set_property;

  properties [PROP_MODEL] =
    g_param_spec_object ("model", NULL, NULL,
                         GBP_TYPE_GREP_MODEL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "gbpgreppanel");
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/grep/gbp-grep-panel.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpGrepPanel, tree_view);
}

static void
gbp_grep_panel_init (GbpGrepPanel *self)
{
  GtkTreeViewColumn *column;
  GtkCellRenderer *cell;

  gtk_widget_init_template (GTK_WIDGET (self));

  column = gtk_tree_view_column_new ();
  cell = gtk_cell_renderer_toggle_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), cell, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (column), cell, "active", 1);
  gtk_tree_view_column_set_expand (column, FALSE);
  gtk_tree_view_append_column (self->tree_view, column);

  column = gtk_tree_view_column_new ();
  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), cell, TRUE);
  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (column), cell, filename_data_func, NULL, NULL);
  gtk_tree_view_column_set_title (column, _("Filename"));
  gtk_tree_view_column_set_expand (column, FALSE);
  gtk_tree_view_column_set_resizable (column, TRUE);
  gtk_tree_view_append_column (self->tree_view, column);

  column = gtk_tree_view_column_new ();
  cell = g_object_new (GTK_TYPE_CELL_RENDERER_TEXT,
                       "ellipsize", PANGO_ELLIPSIZE_END,
                       NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), cell, TRUE);
  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (column), cell, match_data_func, NULL, NULL);
  gtk_tree_view_column_set_title (column, _("Match"));
  gtk_tree_view_column_set_expand (column, TRUE);
  gtk_tree_view_column_set_resizable (column, TRUE);
  gtk_tree_view_append_column (self->tree_view, column);

  column = gtk_tree_view_column_new ();
  cell = g_object_new (GTK_TYPE_CELL_RENDERER_TEXT,
                       "ellipsize", PANGO_ELLIPSIZE_END,
                       "width-chars", 20,
                       NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), cell, TRUE);
  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (column), cell, path_data_func, NULL, NULL);
  gtk_tree_view_column_set_title (column, _("Path"));
  gtk_tree_view_column_set_expand (column, FALSE);
  gtk_tree_view_column_set_resizable (column, TRUE);
  gtk_tree_view_append_column (self->tree_view, column);
}

void
gbp_grep_panel_set_model (GbpGrepPanel *self,
                          GbpGrepModel *model)
{
  g_return_if_fail (GBP_IS_GREP_PANEL (self));
  g_return_if_fail (!model || GBP_IS_GREP_MODEL (model));

  gtk_tree_view_set_model (self->tree_view, GTK_TREE_MODEL (model));
}

/**
 * gbp_grep_panel_get_model:
 * @self: a #GbpGrepPanel
 *
 * Returns: (transfer none) (nullable): a #GbpGrepModel
 */
GbpGrepModel *
gbp_grep_panel_get_model (GbpGrepPanel *self)
{
  return GBP_GREP_MODEL (gtk_tree_view_get_model (self->tree_view));
}

GtkWidget *
gbp_grep_panel_new (void)
{
  return g_object_new (GBP_TYPE_GREP_PANEL, NULL);
}
