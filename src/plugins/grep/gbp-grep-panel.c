/* gbp-grep-panel.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-grep-panel"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-code.h>
#include <libide-editor.h>
#include <libide-gui.h>

#include "gbp-grep-panel.h"

#define I_ g_intern_string

struct _GbpGrepPanel
{
  IdePane            parent_instance;

  GCancellable      *cancellable;

  /* Unowned references */
  GtkTreeView       *tree_view;
  GtkTreeViewColumn *toggle_column;
  GtkCheckButton    *check;

  GtkStack          *stack;
  GtkScrolledWindow *scrolled_window;
  AdwSpinner        *spinner;

  GtkButton         *replace_button;
  GtkEditable       *replace_entry;

  GtkButton         *find_button;
  GtkEditable       *find_entry;

  GtkCheckButton    *regex_button;
  GtkCheckButton    *whole_words_button;
  GtkCheckButton    *case_button;
  GtkCheckButton    *recursive_button;

  GtkButton         *close_button;
};

enum {
  PROP_0,
  PROP_MODEL,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpGrepPanel, gbp_grep_panel, IDE_TYPE_PANE)

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
          gint start_index = match->match_begin_bytes - adjust;
          gint end_index = match->match_end_bytes - adjust;

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
      g_autofree gchar *formatted = NULL;
      const gchar *slash = strrchr (line->path, G_DIR_SEPARATOR);
      const gchar *shortpath;

      if (slash != NULL)
        shortpath = slash + 1;
      else
        shortpath = line->path;

      formatted = g_strdup_printf ("%s:%u", shortpath, line->line);
      g_object_set (cell, "text", formatted, NULL);

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
          g_autoptr(IdeLocation) location = NULL;
          g_autoptr(PanelPosition) position = NULL;
          g_autoptr(GFile) child = NULL;
          IdeWorkspace *workspace;
          guint lineno = line->line;

          workspace = ide_widget_get_workspace (GTK_WIDGET (self));

          if (lineno > 0)
            lineno--;

          child = gbp_grep_model_get_file (GBP_GREP_MODEL (model), line->path);
          location = ide_location_new (child, lineno, -1);

          position = panel_position_new ();
          ide_editor_focus_location (workspace, position, location);
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
  GtkCheckButton *toggle;
  GtkTreeModel *model;

  g_assert (GBP_IS_GREP_PANEL (self));
  g_assert (GTK_IS_TREE_VIEW_COLUMN (column));

  toggle = GTK_CHECK_BUTTON (self->check);
  gtk_check_button_set_active (toggle, !gtk_check_button_get_active (toggle));

  model = gtk_tree_view_get_model (self->tree_view);
  gbp_grep_model_toggle_mode (GBP_GREP_MODEL (model));
  gtk_widget_queue_resize (GTK_WIDGET (self->tree_view));
}

static void
gbp_grep_panel_replace_edited_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  IdeBufferManager *bufmgr = (IdeBufferManager *)object;
  g_autoptr(GbpGrepPanel) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_BUFFER_MANAGER (bufmgr));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_GREP_PANEL (self));

  if (!ide_buffer_manager_apply_edits_finish (bufmgr, result, &error))
    ide_object_warning (IDE_OBJECT (bufmgr), "Failed to apply edits: %s", error->message);

  gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->scrolled_window));
}

static void
gbp_grep_panel_replace_clicked_cb (GbpGrepPanel *self,
                                   GtkButton    *button)
{
  g_autoptr(GPtrArray) edits = NULL;
  IdeBufferManager *bufmgr;
  const gchar *text;
  IdeContext *context;

  g_assert (GBP_IS_GREP_PANEL (self));
  g_assert (GTK_IS_BUTTON (button));

  edits = gbp_grep_model_create_edits (GBP_GREP_MODEL (gtk_tree_view_get_model (self->tree_view)));
  if (edits == NULL || edits->len == 0)
    return;

  text = gtk_editable_get_text (GTK_EDITABLE (self->replace_entry));

  for (guint i = 0; i < edits->len; i++)
    {
      IdeTextEdit *edit = g_ptr_array_index (edits, i);
      ide_text_edit_set_text (edit, text);
    }

  g_debug ("Replacing %u edit points with %s", edits->len, text);

  gtk_widget_set_sensitive (GTK_WIDGET (self->replace_button), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->replace_entry), FALSE);
  gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->spinner));

  context = ide_widget_get_context (GTK_WIDGET (self));
  bufmgr = ide_buffer_manager_from_context (context);

  ide_buffer_manager_apply_edits_async (bufmgr,
                                        IDE_PTR_ARRAY_STEAL_FULL (&edits),
                                        NULL,
                                        gbp_grep_panel_replace_edited_cb,
                                        g_object_ref (self));
}

static void
gbp_grep_panel_find_entry_text_changed_cb (GbpGrepPanel *self,
                                           GParamSpec   *pspec,
                                           GtkEntry     *entry)
{
  gboolean is_query_empty;

  g_assert (GBP_IS_GREP_PANEL (self));
  g_assert (GTK_IS_EDITABLE (entry));

  is_query_empty = (g_strcmp0 (gtk_editable_get_text (GTK_EDITABLE (entry)), "") == 0);

  gtk_widget_set_sensitive (GTK_WIDGET (self->find_button), !is_query_empty);
}

static void
gbp_grep_panel_close_panel_action (GSimpleAction *action,
                                   GVariant      *variant,
                                   gpointer       user_data)
{
  GbpGrepPanel *self = (GbpGrepPanel *)user_data;
  gboolean is_project_wide;
  GbpGrepModel *model;

  g_assert (GBP_IS_GREP_PANEL (self));

  model = gbp_grep_panel_get_model (self);
  is_project_wide = (model == NULL || gbp_grep_model_get_directory (model) == NULL);

  if (!is_project_wide)
    ide_pane_destroy (IDE_PANE (self));
}

static void
gbp_grep_panel_close_clicked_cb (GbpGrepPanel *self,
                                 GtkButton    *button)
{
  g_assert (GBP_IS_GREP_PANEL (self));
  g_assert (GTK_IS_BUTTON (button));

  ide_pane_destroy (IDE_PANE (self));
}

static void
gbp_grep_panel_scan_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  GbpGrepModel *model = (GbpGrepModel *)object;
  g_autoptr(GbpGrepPanel) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_GREP_MODEL (model));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_GREP_PANEL (self));

  if (!gbp_grep_model_scan_finish (model, result, &error))
    {
      /* When it's been cancelled, it means a new search has been launched when the previous
       * one was still running. In that case, we don't want to update the UI like we would
       * if the search did end correctly, since that would show back the old search results
       * and hide the spinner, which is confusing because it will be replaced by the new
       * search results later when they arrive. So instead, don't do any of this and just
       * let the next pending search results update the UI accordingly when we get them.
       */
      if (ide_error_ignore (error))
        return;

      /* TODO: For now we warn in the not-very-noticeable messages panel, but when we start
       * depending on libadwaita we'll be able to use a status page here as an error page,
       * in the stack.
       */
      ide_object_warning (ide_widget_get_context (GTK_WIDGET (self)),
                          "Failed to find files: %s", error->message);
    }
  else
    gbp_grep_panel_set_model (self, model);

  g_clear_object (&self->cancellable);

  gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->scrolled_window));

  /* The model defaults to selecting all items, so if the "Select all" header check box was
   * unselected, then we'll end up in an inconsistent state where toggling the header check
   * box will unselect the items when it should have selected all of them. To avoid this,
   * just set back the "Select all" check box to "selected" when starting a new search.
   */
  gtk_check_button_set_active (GTK_CHECK_BUTTON (self->check), TRUE);

  panel_widget_raise (PANEL_WIDGET (self));
  gtk_widget_grab_focus (GTK_WIDGET (self->replace_entry));
}

/**
 * gbp_grep_panel_launch_search:
 * @self: a #GbpGrepPanel
 *
 * Launches the search operation with the settings coming from the model
 * previously set with gbp_grep_panel_set_model ().
 */
void
gbp_grep_panel_launch_search (GbpGrepPanel *self)
{
  GbpGrepModel *model;
  GFile *root_dir = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GREP_PANEL (self));

  model = gbp_grep_panel_get_model (self);

  /* Nothing's really reusable between search operations (and it isn't allowed anyway by
   * gbp_grep_model_scan_async()), so just start from a new one. The only part we keep
   * from it is the search directory because we can't modify it in the UI and so the
   * only place where it's actually stored is the (old) model.
   */
  if (model != NULL)
    root_dir = gbp_grep_model_get_directory (model);

  model = gbp_grep_model_new (ide_widget_get_context (GTK_WIDGET (self)));
  gbp_grep_model_set_directory (model, root_dir);

  gbp_grep_model_set_use_regex (model, gtk_check_button_get_active (GTK_CHECK_BUTTON (self->regex_button)));
  gbp_grep_model_set_at_word_boundaries (model, gtk_check_button_get_active (GTK_CHECK_BUTTON (self->whole_words_button)));
  gbp_grep_model_set_case_sensitive (model, gtk_check_button_get_active (GTK_CHECK_BUTTON (self->case_button)));
  gbp_grep_model_set_query (model, gtk_editable_get_text (GTK_EDITABLE (self->find_entry)));

  gbp_grep_model_set_recursive (model, gtk_check_button_get_active (GTK_CHECK_BUTTON (self->recursive_button)));

  gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->spinner));
  gtk_widget_set_sensitive (GTK_WIDGET (self->replace_button), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->replace_entry), FALSE);

  panel_widget_raise (PANEL_WIDGET (self));
  gtk_widget_grab_focus (GTK_WIDGET (self));

  /* We allow making a new search even if there's already one running, but cancel the previous
   * one to make sure it doesn't needlessly use resources for the grep process that's still
   * running. Useful for example when you realize that your regex is going to match almost
   * every single lines of the source tree and so it will never end…
   */
  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  self->cancellable = g_cancellable_new ();
  gbp_grep_model_scan_async (model,
                             self->cancellable,
                             gbp_grep_panel_scan_cb,
                             g_object_ref (self));

  IDE_EXIT;
}

static void
gbp_grep_panel_find_clicked_cb (GbpGrepPanel *self,
                                GtkButton    *button)
{
  g_assert (GBP_IS_GREP_PANEL (self));
  g_assert (GTK_IS_BUTTON (button));

  gbp_grep_panel_launch_search (self);
}

/* We can't really use the receives-default/grab_default() stuff as that only really
 * works when there's only one entry+button in a popover. So here just chain up the
 * Enter key in the entry to activate the button.
 */
static void
on_entry_activate_toggle_action_button_cb (GtkEntry *entry,
                                           gpointer  user_data)
{
  GtkButton *button = (GtkButton *)user_data;

  g_assert (GTK_IS_EDITABLE (entry));
  g_assert (GTK_IS_BUTTON (button));

  if (gtk_widget_get_sensitive (GTK_WIDGET (button)))
    g_signal_emit_by_name (button, "activate", NULL);
}

static gboolean
gbp_grep_panel_grab_focus (GtkWidget *widget)
{
  GbpGrepPanel *self = (GbpGrepPanel *)widget;

  g_assert (GBP_IS_GREP_PANEL (self));

  return gtk_widget_grab_focus (GTK_WIDGET (self->find_entry));
}

static void
gbp_grep_panel_dispose (GObject *object)
{
  GbpGrepPanel *self = (GbpGrepPanel *)object;

  g_assert (GBP_IS_GREP_PANEL (self));

  if (self->cancellable)
    {
      g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);
    }

  G_OBJECT_CLASS (gbp_grep_panel_parent_class)->dispose (object);
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
  object_class->dispose = gbp_grep_panel_dispose;

  widget_class->grab_focus = gbp_grep_panel_grab_focus;

  properties [PROP_MODEL] =
    g_param_spec_object ("model", NULL, NULL,
                         GBP_TYPE_GREP_MODEL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "gbpgreppanel");
  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/grep/gbp-grep-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, GbpGrepPanel, stack);
  gtk_widget_class_bind_template_child (widget_class, GbpGrepPanel, scrolled_window);
  gtk_widget_class_bind_template_child (widget_class, GbpGrepPanel, tree_view);
  gtk_widget_class_bind_template_child (widget_class, GbpGrepPanel, spinner);

  gtk_widget_class_bind_template_child (widget_class, GbpGrepPanel, replace_button);
  gtk_widget_class_bind_template_child (widget_class, GbpGrepPanel, replace_entry);
  gtk_widget_class_bind_template_child (widget_class, GbpGrepPanel, find_button);
  gtk_widget_class_bind_template_child (widget_class, GbpGrepPanel, find_entry);

  gtk_widget_class_bind_template_child (widget_class, GbpGrepPanel, regex_button);
  gtk_widget_class_bind_template_child (widget_class, GbpGrepPanel, whole_words_button);
  gtk_widget_class_bind_template_child (widget_class, GbpGrepPanel, case_button);
  gtk_widget_class_bind_template_child (widget_class, GbpGrepPanel, recursive_button);
  gtk_widget_class_bind_template_child (widget_class, GbpGrepPanel, close_button);
}

static const GActionEntry actions[] = {
  { "close-panel", gbp_grep_panel_close_panel_action },
};

static void
gbp_grep_panel_init (GbpGrepPanel *self)
{
  g_autoptr(GSimpleActionGroup) group = NULL;
  GtkTreeViewColumn *column;
  GtkCellRenderer *cell;

  gtk_widget_init_template (GTK_WIDGET (self));

  panel_widget_set_id (PANEL_WIDGET (self), "org.gnome.builder.grep.panel");

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "grep",
                                  G_ACTION_GROUP (group));

  g_signal_connect (self->find_entry,
                    "activate",
                    G_CALLBACK (on_entry_activate_toggle_action_button_cb),
                    self->find_button);
  g_signal_connect (self->replace_entry,
                    "activate",
                    G_CALLBACK (on_entry_activate_toggle_action_button_cb),
                    self->replace_button);

  g_signal_connect_object (self->replace_button,
                           "clicked",
                           G_CALLBACK (gbp_grep_panel_replace_clicked_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->find_button,
                           "clicked",
                           G_CALLBACK (gbp_grep_panel_find_clicked_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->find_entry,
                           "notify::text",
                           G_CALLBACK (gbp_grep_panel_find_entry_text_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->close_button,
                           "clicked",
                           G_CALLBACK (gbp_grep_panel_close_clicked_cb),
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
  gtk_tree_view_column_set_title (column, _("Location"));
  gtk_tree_view_column_set_expand (column, FALSE);
  gtk_tree_view_column_set_resizable (column, TRUE);
  gtk_tree_view_append_column (self->tree_view, column);

  column = gtk_tree_view_column_new ();
  cell = g_object_new (GTK_TYPE_CELL_RENDERER_TEXT,
                       "ellipsize", PANGO_ELLIPSIZE_END,
                       NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), cell, TRUE);
  /* translators: the column header for the matches in the 'find in files' results */
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

static char *
sanitize_workdir (GFile *workdir,
                  GFile *search_directory)
{
  g_autofree char *relative_dir = g_file_get_relative_path (workdir, search_directory);
  /* To make it clear that it's just the directory inserted in the "Find in %s" string, ensure
   * the path ends with a directory separator, as g_file_get_relative_path() doesn't do it.
   * That way we won't end up with "Find in data" but instead "Find in data/".
   */
  gboolean is_dir = (g_file_query_file_type (search_directory, G_FILE_QUERY_INFO_NONE, NULL) == G_FILE_TYPE_DIRECTORY);
  g_autofree char *dir_sep_terminated_path = is_dir ?
    g_strconcat (relative_dir, G_DIR_SEPARATOR_S, NULL) : g_strdup (relative_dir);
  /* We want a mnemonic on the buttons but paths can contain underscores, and they would
   * be taken as if they are mnemonics underlines which is not what we want. Instead,
   * escape the underscore by doubling it so that GTK only renders one and doesn't use
   * it as mnemonic.
   */
  g_autoptr(GString) underscore_escaper = g_string_new (dir_sep_terminated_path);
  g_string_replace (underscore_escaper, "_", "__", 0);

  return g_strdup (underscore_escaper->str);
}

void
gbp_grep_panel_set_model (GbpGrepPanel *self,
                          GbpGrepModel *model)
{
  g_return_if_fail (GBP_IS_GREP_PANEL (self));
  g_return_if_fail (!model || GBP_IS_GREP_MODEL (model));

  if (model != NULL)
    {
      GFile *search_directory = gbp_grep_model_get_directory (model);
      IdeContext *context = ide_widget_get_context (GTK_WIDGET (self));
      g_autoptr(GFile) workdir = ide_context_ref_workdir (context);
      /* The project-wide default panel (done in the editor addin) uses NULL to indicate project-wide,
       * but when searching from the top-level "Files" project tree row we'll have the full path
       * even if it's effectively project-wide, not NULL. It's nice to keep the
       * "Find in Project" label while allowing to close the panel, so differentiate both cases.
       */
      gboolean is_initial_panel = (search_directory == NULL);
      gboolean is_project_wide = (is_initial_panel || g_file_equal (workdir, search_directory));

      gboolean has_item = (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (model), NULL) != 0);

      gtk_widget_set_sensitive (GTK_WIDGET (self->replace_button), has_item);
      gtk_widget_set_sensitive (GTK_WIDGET (self->replace_entry), has_item);

      gtk_editable_set_text (GTK_EDITABLE (self->find_entry),
                             gbp_grep_model_get_query (model));

      gtk_widget_set_visible (GTK_WIDGET (self->close_button), !is_initial_panel);

      /* Project wide is done in the UI file directly. */
      if (!is_project_wide)
        {
          g_autofree char *mnemonic_safe_directory = sanitize_workdir (workdir, search_directory);
          /* TRANSLATORS: %s is the directory or file from where the search was started from the project tree. */
          g_autofree char *find_label = g_strdup_printf (_("_Find in %s"), mnemonic_safe_directory);
          /* TRANSLATORS: %s is the directory or file from where the search was started from the project tree. */
          g_autofree char *replace_label = g_strdup_printf (_("_Replace in %s"), mnemonic_safe_directory);
          gboolean is_dir = (g_file_query_file_type (search_directory, G_FILE_QUERY_INFO_NONE, NULL) == G_FILE_TYPE_DIRECTORY);

          gtk_button_set_label (self->find_button, find_label);
          gtk_button_set_label (self->replace_button, replace_label);

          gtk_widget_set_visible (GTK_WIDGET (self->recursive_button), is_dir);
        }
      else
        {
          gtk_button_set_label (self->find_button, _("Find in Project"));
          gtk_button_set_label (self->replace_button, _("Replace in Project"));

          gtk_widget_set_visible (GTK_WIDGET (self->recursive_button), TRUE);
        }

      gtk_check_button_set_active (GTK_CHECK_BUTTON (self->regex_button),
                                   gbp_grep_model_get_use_regex (model));
      gtk_check_button_set_active (GTK_CHECK_BUTTON (self->whole_words_button),
                                   gbp_grep_model_get_at_word_boundaries (model));
      gtk_check_button_set_active (GTK_CHECK_BUTTON (self->case_button),
                                   gbp_grep_model_get_case_sensitive (model));
      gtk_check_button_set_active (GTK_CHECK_BUTTON (self->recursive_button),
                                   gbp_grep_model_get_recursive (model));
    }

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
