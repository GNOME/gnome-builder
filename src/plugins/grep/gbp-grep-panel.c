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
  DzlDockWidget      parent_instance;

  /* Unowned references */
  GtkTreeView       *tree_view;
  GtkTreeViewColumn *toggle_column;
  GtkCheckButton    *check;
  GtkButton         *close_button;
};

enum {
  PROP_0,
  PROP_MODEL,
  N_PROPS
};

G_DEFINE_TYPE (GbpGrepPanel, gbp_grep_panel, DZL_TYPE_DOCK_WIDGET)

static GParamSpec *properties [N_PROPS];

static gchar *
get_path_and_line (const gchar *str,
                   guint       *line)
{
  static GRegex *regex;
  g_autoptr(GMatchInfo) match = NULL;

  if (regex == NULL)
    {
      g_autoptr(GError) error = NULL;

      regex = g_regex_new ("([a-zA-Z0-9\\+\\-\\.\\/_]+):(\\d+):(.*)", 0, 0, &error);
      g_assert_no_error (error);
    }

  if (g_regex_match_full (regex, str, strlen (str), 0, 0, &match, NULL))
    {
      gchar *pathstr = g_match_info_fetch (match, 1);
      g_autofree gchar *linestr = g_match_info_fetch (match, 2);

      *line = g_ascii_strtoll (linestr, NULL, 10);

      return g_steal_pointer (&pathstr);
    }

  *line = 0;

  return NULL;
}

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
gbp_grep_panel_row_activated_cb (GbpGrepPanel      *self,
                                 GtkTreePath       *path,
                                 GtkTreeViewColumn *column,
                                 GtkTreeView       *tree_view)
{
  GtkTreeModel *model;
  GtkTreeIter iter;

  g_assert (GBP_IS_GREP_PANEL (self));
  g_assert (path != NULL);
  g_assert (GTK_IS_TREE_VIEW_COLUMN (column));
  g_assert (GTK_IS_TREE_VIEW (tree_view));

  /* Ignore if this is the toggle checkbox column */
  if (column == self->toggle_column)
    return;

  if ((model = gtk_tree_view_get_model (tree_view)) &&
      gtk_tree_model_get_iter (model, &iter, path))
    {
      g_autofree gchar *str = NULL;
      g_autofree gchar *filename = NULL;
      g_autoptr(IdeSourceLocation) location = NULL;
      IdePerspective *editor;
      IdeWorkbench *workbench;
      IdeContext *context;
      guint line = 0;

      gtk_tree_model_get (model, &iter,
                          0, &str,
                          -1);

      filename = get_path_and_line (str, &line);
      workbench = ide_widget_get_workbench (GTK_WIDGET (self));
      context = ide_workbench_get_context (workbench);
      editor = ide_workbench_get_perspective_by_name (workbench, "editor");

      if (line > 0)
        line--;

      location = ide_source_location_new_for_path (context, filename, line, 0);

      ide_editor_perspective_focus_location (IDE_EDITOR_PERSPECTIVE (editor), location);
    }
}

static void
gbp_grep_panel_row_toggled_cb (GbpGrepPanel          *self,
                               const gchar           *pathstr,
                               GtkCellRendererToggle *toggle)
{
  GtkTreeModel *model;
  GtkTreePath *path;
  GtkTreeIter iter;

  g_assert (GBP_IS_GREP_PANEL (self));
  g_assert (pathstr != NULL);
  g_assert (GTK_IS_CELL_RENDERER_TOGGLE (toggle));

  path = gtk_tree_path_new_from_string (pathstr);
  model = gtk_tree_view_get_model (self->tree_view);

  if (gtk_tree_model_get_iter (model, &iter, path))
    {
      gbp_grep_model_toggle_row (GBP_GREP_MODEL (model), &iter);
      gtk_widget_queue_resize (GTK_WIDGET (self->tree_view));
    }

  g_clear_pointer (&path, gtk_tree_path_free);
}

static void
gbp_grep_panel_toggle_all_cb (GbpGrepPanel      *self,
                              GtkTreeViewColumn *column)
{
  GtkToggleButton *toggle;
  GtkTreeModel *model;

  g_assert (GBP_IS_GREP_PANEL (self));
  g_assert (GTK_IS_TREE_VIEW_COLUMN (column));

  toggle = GTK_TOGGLE_BUTTON (self->check);
  gtk_toggle_button_set_active (toggle, !gtk_toggle_button_get_active (toggle));

  model = gtk_tree_view_get_model (self->tree_view);
  gbp_grep_model_toggle_mode (GBP_GREP_MODEL (model));
  gtk_widget_queue_resize (GTK_WIDGET (self->tree_view));
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
  gtk_widget_class_bind_template_child (widget_class, GbpGrepPanel, close_button);
  gtk_widget_class_bind_template_child (widget_class, GbpGrepPanel, tree_view);
}

static void
gbp_grep_panel_init (GbpGrepPanel *self)
{
  GtkTreeViewColumn *column;
  GtkCellRenderer *cell;

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->close_button,
                           "clicked",
                           G_CALLBACK (gtk_widget_destroy),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->tree_view,
                           "row-activated",
                           G_CALLBACK (gbp_grep_panel_row_activated_cb),
                           self,
                           G_CONNECT_SWAPPED);

  self->check = g_object_new (GTK_TYPE_CHECK_BUTTON,
                              "margin-bottom", 3,
                              "margin-end", 6,
                              "margin-start", 6,
                              "margin-top", 3,
                              "visible", TRUE,
                              "active", TRUE,
                              NULL);
  self->toggle_column = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN,
                                      "visible", TRUE,
                                      "clickable", TRUE,
                                      "widget", self->check,
                                      NULL);
  g_signal_connect_object (self->toggle_column,
                           "clicked",
                           G_CALLBACK (gbp_grep_panel_toggle_all_cb),
                           self,
                           G_CONNECT_SWAPPED);
  cell = g_object_new (GTK_TYPE_CELL_RENDERER_TOGGLE,
                       "activatable", TRUE,
                       NULL);
  g_signal_connect_object (cell,
                           "toggled",
                           G_CALLBACK (gbp_grep_panel_row_toggled_cb),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (self->toggle_column), cell, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (self->toggle_column), cell, "active", 1);
  gtk_tree_view_column_set_expand (self->toggle_column, FALSE);
  gtk_tree_view_append_column (self->tree_view, self->toggle_column);

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
