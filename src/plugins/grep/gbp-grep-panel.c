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
  GtkButton         *replace_button;
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
  const GbpGrepModelLine *line = NULL;
  PangoAttrList *attrs = NULL;
  const gchar *begin = NULL;

  g_assert (GTK_IS_CELL_LAYOUT (layout));
  g_assert (GTK_IS_CELL_RENDERER_TEXT (cell));
  g_assert (GTK_IS_TREE_MODEL (model));
  g_assert (iter != NULL);

  gbp_grep_model_get_line (GBP_GREP_MODEL (model), iter, &line);

  if G_LIKELY (line != NULL)
    {
      goffset adjust;

      /* Skip to the beginning of the text */
      begin = line->start_of_message;
      while (*begin && g_unichar_isspace (g_utf8_get_char (begin)))
        begin = g_utf8_next_char (begin);

      /*
       * If any of our matches are for space, we can't skip past the starting
       * space or we will fail to highlight properly.
       */
      adjust = begin - line->start_of_message;
      for (guint i = 0; i < line->matches->len; i++)
        {
          const GbpGrepModelMatch *match = &g_array_index (line->matches, GbpGrepModelMatch, i);

          if (match->match_begin < adjust)
            {
              begin = line->start_of_message;
              adjust = 0;
              break;
            }
        }

      /* Now create pango attributes to draw around the matched text so that
       * the user knows exactly where the match is. We need to adjust for what
       * we chomped off the beginning of the visible message.
       */
      attrs = pango_attr_list_new ();
      for (guint i = 0; i < line->matches->len; i++)
        {
          const GbpGrepModelMatch *match = &g_array_index (line->matches, GbpGrepModelMatch, i);
          PangoAttribute *bg_attr = pango_attr_background_new (64764, 59881, 20303);
          PangoAttribute *alpha_attr = pango_attr_background_alpha_new (32767);
          gint start_index = match->match_begin - adjust;
          gint end_index = match->match_end - adjust;

          bg_attr->start_index = start_index;
          bg_attr->end_index = end_index;

          alpha_attr->start_index = start_index;
          alpha_attr->end_index = end_index;

          pango_attr_list_insert (attrs, g_steal_pointer (&bg_attr));
          pango_attr_list_insert (attrs, g_steal_pointer (&alpha_attr));
        }
    }

  g_object_set (cell,
                "attributes", attrs,
                "text", begin,
                NULL);

  g_clear_pointer (&attrs, pango_attr_list_unref);
}

static void
path_data_func (GtkCellLayout   *layout,
                GtkCellRenderer *cell,
                GtkTreeModel    *model,
                GtkTreeIter     *iter,
                gpointer         user_data)
{
  const GbpGrepModelLine *line = NULL;

  g_assert (GTK_IS_CELL_LAYOUT (layout));
  g_assert (GTK_IS_CELL_RENDERER_TEXT (cell));
  g_assert (GTK_IS_TREE_MODEL (model));
  g_assert (iter != NULL);

  gbp_grep_model_get_line (GBP_GREP_MODEL (model), iter, &line);

  if G_LIKELY (line != NULL)
    {
      const gchar *slash = strrchr (line->path, G_DIR_SEPARATOR);

      if (slash != NULL)
        {
          g_autofree gchar *path = g_strndup (line->path, slash - line->path);
          g_object_set (cell, "text", path, NULL);
          return;
        }
    }

  g_object_set (cell, "text", ".", NULL);
}

static void
filename_data_func (GtkCellLayout   *layout,
                    GtkCellRenderer *cell,
                    GtkTreeModel    *model,
                    GtkTreeIter     *iter,
                    gpointer         user_data)
{
  const GbpGrepModelLine *line = NULL;

  g_assert (GTK_IS_CELL_LAYOUT (layout));
  g_assert (GTK_IS_CELL_RENDERER_TEXT (cell));
  g_assert (GTK_IS_TREE_MODEL (model));
  g_assert (iter != NULL);

  gbp_grep_model_get_line (GBP_GREP_MODEL (model), iter, &line);

  if G_LIKELY (line != NULL)
    {
      const gchar *slash = strrchr (line->path, G_DIR_SEPARATOR);

      if (slash != NULL)
        g_object_set (cell, "text", slash + 1, NULL);
      else
        g_object_set (cell, "text", line->path, NULL);

      return;
    }

  g_object_set (cell, "text", NULL, NULL);
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
      const GbpGrepModelLine *line = NULL;

      gbp_grep_model_get_line (GBP_GREP_MODEL (model), &iter, &line);

      if G_LIKELY (line != NULL)
        {
          g_autoptr(IdeSourceLocation) location = NULL;
          IdePerspective *editor;
          IdeWorkbench *workbench;
          IdeContext *context;
          guint lineno = line->line;

          workbench = ide_widget_get_workbench (GTK_WIDGET (self));
          context = ide_workbench_get_context (workbench);
          editor = ide_workbench_get_perspective_by_name (workbench, "editor");

          if (lineno > 0)
            lineno--;

          location = ide_source_location_new_for_path (context, line->path, lineno, 0);

          ide_editor_perspective_focus_location (IDE_EDITOR_PERSPECTIVE (editor), location);
        }
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
gbp_grep_panel_replace_cb (GbpGrepPanel *self,
                           GtkButton    *button)
{
  g_assert (GBP_IS_GREP_PANEL (self));
  g_assert (GTK_IS_BUTTON (button));

  gbp_grep_model_create_edits (GBP_GREP_MODEL (gtk_tree_view_get_model (self->tree_view)));
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
  gtk_widget_class_bind_template_child (widget_class, GbpGrepPanel, replace_button);
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

  g_signal_connect_object (self->replace_button,
                           "clicked",
                           G_CALLBACK (gbp_grep_panel_replace_cb),
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
