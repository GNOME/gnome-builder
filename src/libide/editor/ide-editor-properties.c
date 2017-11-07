/* ide-editor-properties.c
 *
 * Copyright © 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-editor-properties"

#include <dazzle.h>

#include "ide-macros.h"

#include "buffers/ide-buffer.h"
#include "editor/ide-editor-properties.h"

/**
 * SECTION:ide-editor-properties
 * @title: IdeEditorProperties
 * @short_description: property editor for IdeEditorView
 *
 * This widget is a property editor to tweak settings of an #IdeEditorView.
 * It should be used in a transient panel when the user needs to tweak the
 * settings of a view.
 *
 * Since: 3.26
 */

struct _IdeEditorProperties
{
  DzlDockWidget        parent_instance;

  DzlSignalGroup      *buffer_signals;

  /* Unowned references */
  IdeEditorView       *view;

  /* Template references */
  GtkCheckButton      *show_line_numbers;
  GtkCheckButton      *show_right_margin;
  GtkCheckButton      *highlight_current_line;
  GtkCheckButton      *insert_trailing_newline;
  GtkCheckButton      *overwrite_braces;
  GtkCheckButton      *auto_indent;
  GtkCheckButton      *smart_backspace;
  GtkTreeView         *tree_view;
  GtkTreeViewColumn   *language_column;
  GtkCellRendererText *language_cell;
  GtkListStore        *languages;
  GtkSearchEntry      *entry;
};

enum {
  PROP_0,
  PROP_VIEW,
  N_PROPS
};

G_DEFINE_TYPE (IdeEditorProperties, ide_editor_properties, DZL_TYPE_DOCK_WIDGET)

static GParamSpec *properties [N_PROPS];

static void
ide_editor_properties_cell_data_func (GtkCellLayout   *cell_layout,
                                      GtkCellRenderer *cell,
                                      GtkTreeModel    *tree_model,
                                      GtkTreeIter     *iter,
                                      gpointer         data)
{
  g_autoptr(GtkSourceLanguage) language = NULL;
  const gchar *text = NULL;

  gtk_tree_model_get (tree_model, iter, 0, &language, -1);

  if (language != NULL)
    text = gtk_source_language_get_name (language);

  g_object_set (cell, "text", text, NULL);
}

static void
ide_editor_properties_language_activated (IdeEditorProperties *self,
                                          GtkTreePath         *path,
                                          GtkTreeViewColumn   *column,
                                          GtkTreeView         *tree_view)
{
  GtkTreeModel *model;
  GtkTreeIter iter;

  g_assert (IDE_IS_EDITOR_PROPERTIES (self));
  g_assert (path != NULL);
  g_assert (column != NULL);
  g_assert (GTK_IS_TREE_VIEW (tree_view));

  model = gtk_tree_view_get_model (tree_view);

  if (gtk_tree_model_get_iter (model, &iter, path))
    {
      g_autoptr(GtkSourceLanguage) language = NULL;

      gtk_tree_model_get (model, &iter, 0, &language, -1);
      if (language != NULL && self->view != NULL)
        ide_editor_view_set_language (self->view, language);
    }
}

static gint
compare_languages (gconstpointer a,
                   gconstpointer b,
                   gpointer      data)
{
  GtkSourceLanguage *al = (GtkSourceLanguage *)a;
  GtkSourceLanguage *bl = (GtkSourceLanguage *)b;

  return g_utf8_collate (gtk_source_language_get_name (al),
                         gtk_source_language_get_name (bl));
}

static gboolean
language_equal (GtkSourceLanguage *a,
                GtkSourceLanguage *b)
{
  return a == b ||
         g_strcmp0 (gtk_source_language_get_name (a),
                    gtk_source_language_get_name (b)) == 0;
}

static void
ide_editor_properties_notify_language (IdeEditorProperties *self,
                                       GParamSpec          *pspec,
                                       IdeBuffer           *buffer)
{
  GtkSourceLanguage *language;
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;

  g_assert (IDE_IS_EDITOR_PROPERTIES (self));
  g_assert (IDE_IS_BUFFER (buffer));

  selection = gtk_tree_view_get_selection (self->tree_view);
  language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer));

  if (language == NULL)
    {
      gtk_tree_selection_unselect_all (selection);
      return;
    }

  /* Model might be a filter */
  model = gtk_tree_view_get_model (self->tree_view);

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
        {
          g_autoptr(GtkSourceLanguage) ele = NULL;

          gtk_tree_model_get (model, &iter, 0, &ele, -1);

          if (language_equal (language, ele))
            {
              GtkTreePath *path = NULL;

              /* Be safe against re-entrancy */
              if (!gtk_tree_selection_iter_is_selected (selection, &iter))
                {
                  path = gtk_tree_model_get_path (model, &iter);
                  gtk_tree_selection_select_iter (selection, &iter);
                  gtk_tree_view_scroll_to_cell (self->tree_view, path, NULL, FALSE, 0, 0);
                  gtk_tree_path_free (path);
                }

              break;
            }
        }
      while (gtk_tree_model_iter_next (model, &iter));
    }
}

static void
ide_editor_properties_bind_buffer (IdeEditorProperties *self,
                                   IdeBuffer           *buffer,
                                   DzlSignalGroup      *signals)
{
  g_assert (IDE_IS_EDITOR_PROPERTIES (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (DZL_IS_SIGNAL_GROUP (signals));

  ide_editor_properties_notify_language (self, NULL, buffer);
}

static void
ide_editor_properties_reload_languages (IdeEditorProperties *self)
{
  GtkSourceLanguageManager *manager;
  const gchar * const *ids;

  g_assert (IDE_IS_EDITOR_PROPERTIES (self));

  gtk_list_store_clear (self->languages);

  manager = gtk_source_language_manager_get_default ();
  ids = gtk_source_language_manager_get_language_ids (manager);

  for (guint i = 0; ids[i] != NULL; i++)
    {
      GtkSourceLanguage *language = gtk_source_language_manager_get_language (manager, ids[i]);
      const gchar *id = gtk_source_language_get_id (language);
      GtkTreeIter iter;

      /* ignore the "default values" language */
      if (g_strcmp0 (id, "def") == 0)
        continue;

      dzl_gtk_list_store_insert_sorted (self->languages, &iter, language, 0,
                                        compare_languages, NULL);
      gtk_list_store_set (self->languages, &iter, 0, language, -1);
    }
}

static gboolean
ide_editor_properties_visibility_func (GtkTreeModel *model,
                                       GtkTreeIter  *iter,
                                       gpointer      user_data)
{
  g_autoptr(GtkSourceLanguage) language = NULL;
  DzlPatternSpec *spec = user_data;
  const gchar *id;
  const gchar *name;

  g_assert (GTK_IS_TREE_MODEL (model));
  g_assert (iter != NULL);
  g_assert (spec != NULL);

  gtk_tree_model_get (model, iter, 0, &language, -1);
  g_assert (language != NULL);

  id = gtk_source_language_get_id (language);
  name = gtk_source_language_get_name (language);

  return dzl_pattern_spec_match (spec, id) ||
         dzl_pattern_spec_match (spec, name);
}

static void
ide_editor_properties_entry_changed (IdeEditorProperties *self,
                                     GtkSearchEntry      *entry)
{
  g_autoptr(GtkTreeModel) filter = NULL;
  const gchar *text;

  g_assert (IDE_IS_EDITOR_PROPERTIES (self));
  g_assert (GTK_IS_SEARCH_ENTRY (entry));

  text = gtk_entry_get_text (GTK_ENTRY (entry));

  /* Clear any previous filter */
  if (ide_str_empty0 (text))
    {
      gtk_tree_view_set_model (self->tree_view, GTK_TREE_MODEL (self->languages));
      return;
    }

  /* We can't reuse existing filters, so create a new one */
  filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (self->languages), NULL);
  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (filter),
                                          ide_editor_properties_visibility_func,
                                          dzl_pattern_spec_new (text),
                                          (GDestroyNotify) dzl_pattern_spec_unref);
  gtk_tree_view_set_model (self->tree_view, filter);
}

static void
ide_editor_properties_constructed (GObject *object)
{
  IdeEditorProperties *self = (IdeEditorProperties *)object;

  G_OBJECT_CLASS (ide_editor_properties_parent_class)->constructed (object);

  ide_editor_properties_reload_languages (self);
}

static void
ide_editor_properties_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeEditorProperties *self = IDE_EDITOR_PROPERTIES (object);

  switch (prop_id)
    {
    case PROP_VIEW:
      ide_editor_properties_set_view (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_properties_class_init (IdeEditorPropertiesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = ide_editor_properties_constructed;
  object_class->set_property = ide_editor_properties_set_property;

  properties [PROP_VIEW] =
    g_param_spec_object ("view",
                         "View",
                         "The editor view to modify",
                         IDE_TYPE_EDITOR_VIEW,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/builder/ui/ide-editor-properties.ui");

  gtk_widget_class_bind_template_child (widget_class, IdeEditorProperties, auto_indent);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorProperties, highlight_current_line);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorProperties, insert_trailing_newline);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorProperties, language_cell);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorProperties, language_column);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorProperties, languages);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorProperties, overwrite_braces);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorProperties, show_line_numbers);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorProperties, show_right_margin);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorProperties, smart_backspace);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorProperties, tree_view);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorProperties, entry);

  gtk_widget_class_set_css_name (widget_class, "ideeditorproperties");
}

static void
ide_editor_properties_init (IdeEditorProperties *self)
{
  GtkTextDirection dir;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->buffer_signals = dzl_signal_group_new (IDE_TYPE_BUFFER);

  g_signal_connect_swapped (self->buffer_signals,
                            "bind",
                            G_CALLBACK (ide_editor_properties_bind_buffer),
                            self);

  dzl_signal_group_connect_swapped (self->buffer_signals,
                                    "notify::language",
                                    G_CALLBACK (ide_editor_properties_notify_language),
                                    self);

  /* Swap direction so check is at opposite end of checkbutton */
  dir = gtk_widget_get_direction (GTK_WIDGET (self));
  dir = (dir != GTK_TEXT_DIR_RTL) ? GTK_TEXT_DIR_RTL : GTK_TEXT_DIR_LTR;
  gtk_widget_set_direction (GTK_WIDGET (self->show_line_numbers), dir);
  gtk_widget_set_direction (GTK_WIDGET (self->show_right_margin), dir);
  gtk_widget_set_direction (GTK_WIDGET (self->highlight_current_line), dir);
  gtk_widget_set_direction (GTK_WIDGET (self->insert_trailing_newline), dir);
  gtk_widget_set_direction (GTK_WIDGET (self->overwrite_braces), dir);
  gtk_widget_set_direction (GTK_WIDGET (self->auto_indent), dir);
  gtk_widget_set_direction (GTK_WIDGET (self->smart_backspace), dir);

  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (self->language_column),
                                      GTK_CELL_RENDERER (self->language_cell),
                                      ide_editor_properties_cell_data_func,
                                      self, NULL);

  g_signal_connect_swapped (self->tree_view,
                            "row-activated",
                            G_CALLBACK (ide_editor_properties_language_activated),
                            self);

  g_signal_connect_swapped (self->entry,
                            "changed",
                            G_CALLBACK (ide_editor_properties_entry_changed),
                            self);
}

/**
 * ide_editor_properties_new:
 *
 * Creates a new #IdeEditorProperties.
 *
 * Returns: (transfer full): an #IdeEditorProperties
 *
 * Since: 3.26
 */
GtkWidget *
ide_editor_properties_new (void)
{
  return g_object_new (IDE_TYPE_EDITOR_PROPERTIES, NULL);
}

/**
 * ide_editor_properties_set_view:
 * @self: an #IdeEditorProperties
 * @view: (nullable): an #IdeEditorView
 *
 * Sets the view to be edited by the property editor.
 *
 * Since: 3.26
 */
void
ide_editor_properties_set_view (IdeEditorProperties *self,
                                IdeEditorView       *view)
{
  IdeBuffer *buffer = NULL;

  g_return_if_fail (IDE_IS_EDITOR_PROPERTIES (self));
  g_return_if_fail (!view || IDE_IS_EDITOR_VIEW (view));

  gtk_widget_set_sensitive (GTK_WIDGET (self), view != NULL);

  /* No reference, we clear it when focus changes */
  self->view = view;

  /* Track the buffer for language changes */
  if (view != NULL)
    buffer = ide_editor_view_get_buffer (view);
  dzl_signal_group_set_target (self->buffer_signals, buffer);

  dzl_gtk_widget_mux_action_groups (GTK_WIDGET (self),
                                    view ? GTK_WIDGET (view) : NULL,
                                    "IDE_EDITOR_PROPERTY_ACTIONS");
}
