/* gbp-todo-panel.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-todo-panel"

#include <glib/gi18n.h>

#include <libide-code.h>
#include <libide-gui.h>

#include "gbp-todo-item.h"
#include "gbp-todo-panel.h"

struct _GbpTodoPanel
{
  IdePane             parent_instance;

  GbpTodoModel       *model;
  GtkFilterListModel *filter_model;
  GtkCustomFilter    *filter;

  GtkNoSelection     *selection;
  GtkStack           *stack;
  GtkSearchEntry     *search;
};

G_DEFINE_FINAL_TYPE (GbpTodoPanel, gbp_todo_panel, IDE_TYPE_PANE)

enum {
  PROP_0,
  PROP_MODEL,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
gbp_todo_panel_activate_cb (GbpTodoPanel *self,
                            guint         position,
                            GtkListView  *list_view)
{
  g_autoptr(GbpTodoItem) item = NULL;
  g_autoptr(GFile) file = NULL;
  IdeWorkbench *workbench;
  GListModel *model;
  const char *path;
  guint lineno;

  g_assert (GBP_IS_TODO_PANEL (self));
  g_assert (GTK_IS_LIST_VIEW (list_view));

  model = G_LIST_MODEL (gtk_list_view_get_model (list_view));
  g_assert (G_IS_LIST_MODEL (model));

  item = g_list_model_get_item (model, position);
  g_assert (GBP_IS_TODO_ITEM (item));

  workbench = ide_widget_get_workbench (GTK_WIDGET (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  path = gbp_todo_item_get_path (item);
  g_assert (path != NULL);

  if (g_path_is_absolute (path))
    {
      file = g_file_new_for_path (path);
    }
  else
    {
      IdeContext *context;
      IdeVcs *vcs;
      GFile *workdir;

      context = ide_workbench_get_context (workbench);
      vcs = ide_vcs_from_context (context);
      workdir = ide_vcs_get_workdir (vcs);
      file = g_file_get_child (workdir, path);
    }

  /* Set lineno info so that the editor can jump to the location of the TODO
   * item. Our line number from the model is 1-based, and we need 0-based for
   * our API to open files.
   */
  lineno = gbp_todo_item_get_lineno (item);
  if (lineno > 0)
    lineno--;

  ide_workbench_open_at_async (workbench,
                               file,
                               "editorui",
                               lineno,
                               -1,
                               IDE_BUFFER_OPEN_FLAGS_NONE,
                               NULL, NULL, NULL, NULL);
}

static gboolean
filter_func (gpointer itemptr,
             gpointer user_data)
{
  GbpTodoItem *item = itemptr;
  const char *str = user_data;
  guint prio;

  if (ide_str_empty0 (str))
    return TRUE;

  if (item->path && gtk_source_completion_fuzzy_match (item->path, str, &prio))
    return TRUE;

  for (guint i = 0; i < G_N_ELEMENTS (item->lines); i++)
    {
      if (item->lines[i] == NULL)
        break;

      if (strcasestr (item->lines[i], str) != NULL)
        return TRUE;
    }

  return FALSE;
}

static void
gbp_todo_panel_notify_text_cb (GbpTodoPanel   *self,
                               GParamSpec     *pspec,
                               GtkSearchEntry *search)
{
  const char *text;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TODO_PANEL (self));
  g_assert (GTK_IS_SEARCH_ENTRY (search));

  text = gtk_editable_get_text (GTK_EDITABLE (search));

  gtk_custom_filter_set_filter_func (self->filter,
                                     filter_func,
                                     text[0] ? g_utf8_casefold (text, -1) : NULL,
                                     g_free);

  /* You could check the previous value here if you'd like
   * to make this code faster.
   */
  gtk_filter_changed (GTK_FILTER (self->filter),
                      GTK_FILTER_CHANGE_DIFFERENT);

  if (text[0] == 0)
    gtk_no_selection_set_model (self->selection, G_LIST_MODEL (self->model));
  else
    gtk_no_selection_set_model (self->selection, G_LIST_MODEL (self->filter_model));
}

static void
gbp_todo_panel_dispose (GObject *object)
{
  GbpTodoPanel *self = (GbpTodoPanel *)object;

  g_assert (GBP_IS_TODO_PANEL (self));

  g_clear_object (&self->filter_model);
  g_clear_object (&self->filter);
  g_clear_object (&self->model);

  G_OBJECT_CLASS (gbp_todo_panel_parent_class)->dispose (object);
}

static void
gbp_todo_panel_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GbpTodoPanel *self = GBP_TODO_PANEL (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, gbp_todo_panel_get_model (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_todo_panel_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GbpTodoPanel *self = GBP_TODO_PANEL (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      gbp_todo_panel_set_model (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_todo_panel_class_init (GbpTodoPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gbp_todo_panel_dispose;
  object_class->get_property = gbp_todo_panel_get_property;
  object_class->set_property = gbp_todo_panel_set_property;

  properties [PROP_MODEL] =
    g_param_spec_object ("model",
                         "Model",
                         "The model for the TODO list",
                         GBP_TYPE_TODO_MODEL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/todo/gbp-todo-panel.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpTodoPanel, selection);
  gtk_widget_class_bind_template_child (widget_class, GbpTodoPanel, stack);
  gtk_widget_class_bind_template_child (widget_class, GbpTodoPanel, search);
  gtk_widget_class_bind_template_callback (widget_class, gbp_todo_panel_activate_cb);
  gtk_widget_class_bind_template_callback (widget_class, gbp_todo_panel_notify_text_cb);
}

static void
gbp_todo_panel_init (GbpTodoPanel *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->filter = gtk_custom_filter_new (filter_func, NULL, NULL);
  self->filter_model = gtk_filter_list_model_new (NULL, g_object_ref (GTK_FILTER (self->filter)));
}

/**
 * gbp_todo_panel_get_model:
 * @self: a #GbpTodoPanel
 *
 * Gets the model being displayed by the treeview.
 *
 * Returns: (transfer none) (nullable): a #GbpTodoModel.
 */
GbpTodoModel *
gbp_todo_panel_get_model (GbpTodoPanel *self)
{
  g_return_val_if_fail (GBP_IS_TODO_PANEL (self), NULL);

  return self->model;
}

void
gbp_todo_panel_set_model (GbpTodoPanel *self,
                          GbpTodoModel *model)
{
  g_return_if_fail (GBP_IS_TODO_PANEL (self));
  g_return_if_fail (!model || GBP_IS_TODO_MODEL (model));

  if (g_set_object (&self->model, model))
    {
      gtk_no_selection_set_model (self->selection, G_LIST_MODEL (model));
      gtk_filter_list_model_set_model (self->filter_model, G_LIST_MODEL (model));
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MODEL]);
    }
}

void
gbp_todo_panel_make_ready (GbpTodoPanel *self)
{
  g_return_if_fail (GBP_IS_TODO_PANEL (self));

  gtk_stack_set_visible_child_name (self->stack, "todos");
}
