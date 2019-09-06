/* ide-editor-settings-dialog.c
 *
 * Copyright 2018 Christian Hergert <unknown@domain.org>
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

#define G_LOG_DOMAIN "gbp-editor-settings"

#include "config.h"

#include "ide-editor-settings-dialog.h"

struct _IdeEditorSettingsDialog
{
  GtkDialog       parent_instance;

  IdeEditorPage  *page;

  GtkTreeView    *tree_view;
  GtkListStore   *store;
  GtkSearchEntry *entry;
};

G_DEFINE_TYPE (IdeEditorSettingsDialog, ide_editor_settings_dialog, GTK_TYPE_DIALOG)

enum {
  PROP_0,
  PROP_PAGE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_editor_settings_dialog_row_activated (IdeEditorSettingsDialog *self,
                                          GtkTreePath             *path,
                                          GtkTreeViewColumn       *column,
                                          GtkTreeView             *tree_view)
{
  GtkTreeModel *model;
  GtkTreeIter iter;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EDITOR_SETTINGS_DIALOG (self));
  g_assert (path != NULL);
  g_assert (GTK_IS_TREE_VIEW_COLUMN (column));
  g_assert (GTK_IS_TREE_VIEW (tree_view));

  model = gtk_tree_view_get_model (tree_view);

  if (gtk_tree_model_get_iter (model, &iter, path))
    {
      g_autofree gchar *id = NULL;
      IdeBuffer *buffer;

      gtk_tree_model_get (model, &iter, 0, &id, -1);

      if ((buffer = ide_editor_page_get_buffer (self->page)))
        ide_buffer_set_language_id (buffer, id);
    }
}

static void
ide_editor_settings_dialog_notify_file_settings (IdeEditorSettingsDialog *self,
                                                 GParamSpec              *pspec,
                                                 IdeBuffer               *buffer)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_EDITOR_SETTINGS_DIALOG (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));

  /* Update muxed action groups for new file-settings */
  dzl_gtk_widget_mux_action_groups (GTK_WIDGET (self),
                                    GTK_WIDGET (self->page),
                                    "IDE_EDITOR_PAGE_ACTIONS");
}

static void
ide_editor_settings_dialog_notify_language (IdeEditorSettingsDialog *self,
                                            GParamSpec              *pspec,
                                            IdeBuffer               *buffer)
{
  const gchar *lang_id;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_EDITOR_SETTINGS_DIALOG (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));

  if ((lang_id = ide_buffer_get_language_id (buffer)))
    {
      GtkTreeSelection *selection = gtk_tree_view_get_selection (self->tree_view);
      GtkTreeModel *model = gtk_tree_view_get_model (self->tree_view);
      GtkTreeIter iter;

      if (gtk_tree_model_get_iter_first (model, &iter))
        {
          do
            {
              GValue idval = {0};

              gtk_tree_model_get_value (model, &iter, 0, &idval);

              if (ide_str_equal0 (lang_id, g_value_get_string (&idval)))
                {
                  g_autoptr(GtkTreePath) path = gtk_tree_model_get_path (model, &iter);

                  gtk_tree_selection_select_iter (selection, &iter);
                  gtk_tree_view_scroll_to_cell (self->tree_view, path, NULL, FALSE, 0, 0);

                  return;
                }
            }
          while (gtk_tree_model_iter_next (model, &iter));

          gtk_tree_selection_unselect_all (selection);
        }
    }
}

static gboolean
filter_func (GtkTreeModel *model,
             GtkTreeIter  *iter,
             gpointer      data)
{
  DzlPatternSpec *spec = data;
  GValue idval = {0};
  GValue nameval = {0};
  gboolean ret;

  gtk_tree_model_get_value (model, iter, 0, &idval);
  gtk_tree_model_get_value (model, iter, 1, &nameval);

  ret = dzl_pattern_spec_match (spec, g_value_get_string (&idval)) ||
        dzl_pattern_spec_match (spec, g_value_get_string (&nameval));

  g_value_unset (&idval);
  g_value_unset (&nameval);

  return ret;
}

static void
ide_editor_settings_dialog_entry_changed (IdeEditorSettingsDialog *self,
                                          GtkSearchEntry          *entry)
{
  g_autoptr(GtkTreeModel) filter = NULL;
  const gchar *text;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EDITOR_SETTINGS_DIALOG (self));
  g_assert (GTK_IS_SEARCH_ENTRY (entry));

  text = gtk_entry_get_text (GTK_ENTRY (entry));
  filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (self->store), NULL);
  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (filter),
                                          filter_func,
                                          dzl_pattern_spec_new (text),
                                          (GDestroyNotify)dzl_pattern_spec_unref);

  gtk_tree_view_set_model (self->tree_view, GTK_TREE_MODEL (filter));
}

static void
ide_editor_settings_dialog_set_page (IdeEditorSettingsDialog *self,
                                     IdeEditorPage           *page)
{
  IdeBuffer *buffer;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_EDITOR_SETTINGS_DIALOG (self));

  g_set_object (&self->page, page);

  g_signal_connect_object (self->entry,
                           "changed",
                           G_CALLBACK (ide_editor_settings_dialog_entry_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect (self->entry,
                    "stop-search",
                    G_CALLBACK (gtk_entry_set_text),
                    (gpointer) "");

  dzl_gtk_widget_mux_action_groups (GTK_WIDGET (self),
                                    GTK_WIDGET (page),
                                    "IDE_EDITOR_PAGE_ACTIONS");

  buffer = ide_editor_page_get_buffer (page);

  g_signal_connect_object (buffer,
                           "notify::language",
                           G_CALLBACK (ide_editor_settings_dialog_notify_language),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (buffer,
                           "notify::file-settings",
                           G_CALLBACK (ide_editor_settings_dialog_notify_file_settings),
                           self,
                           G_CONNECT_SWAPPED);

  ide_editor_settings_dialog_notify_language (self, NULL, buffer);
}

IdeEditorSettingsDialog *
ide_editor_settings_dialog_new (IdeEditorPage *page)
{
  GtkWidget *toplevel;

  g_return_val_if_fail (IDE_IS_EDITOR_PAGE (page), NULL);

  if ((toplevel = gtk_widget_get_toplevel (GTK_WIDGET (page))) && !IDE_IS_WORKSPACE (toplevel))
    toplevel = NULL;

  return g_object_new (IDE_TYPE_EDITOR_SETTINGS_DIALOG,
                       "transient-for", toplevel,
                       "modal", FALSE,
                       "page", page,
                       NULL);
}

static void
ide_editor_settings_dialog_destroy (GtkWidget *widget)
{
  IdeEditorSettingsDialog *self = (IdeEditorSettingsDialog *)widget;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EDITOR_SETTINGS_DIALOG (self));

  g_clear_object (&self->page);

  GTK_WIDGET_CLASS (ide_editor_settings_dialog_parent_class)->destroy (widget);
}

static void
ide_editor_settings_dialog_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  IdeEditorSettingsDialog *self = IDE_EDITOR_SETTINGS_DIALOG (object);

  switch (prop_id)
    {
    case PROP_PAGE:
      ide_editor_settings_dialog_set_page (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_settings_dialog_class_init (IdeEditorSettingsDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = ide_editor_settings_dialog_set_property;

  widget_class->destroy = ide_editor_settings_dialog_destroy;

  properties [PROP_PAGE] =
    g_param_spec_object ("page",
                         "Page",
                         "The editor page to be observed",
                         IDE_TYPE_EDITOR_PAGE,
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-editor/ui/ide-editor-settings-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSettingsDialog, entry);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSettingsDialog, tree_view);
}

static void
ide_editor_settings_dialog_init (IdeEditorSettingsDialog *self)
{
  g_autoptr(GtkListStore) store = NULL;
  GtkSourceLanguageManager *manager;
  const gchar * const *lang_ids;
  GValue idval = {0};
  GValue nameval = {0};

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_window_set_resizable (GTK_WINDOW (self), FALSE);

  g_signal_connect_object (self->tree_view,
                           "row-activated",
                           G_CALLBACK (ide_editor_settings_dialog_row_activated),
                           self,
                           G_CONNECT_SWAPPED);

  manager = gtk_source_language_manager_get_default ();
  lang_ids = gtk_source_language_manager_get_language_ids (manager);
  self->store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

  g_value_init (&idval, G_TYPE_STRING);
  g_value_init (&nameval, G_TYPE_STRING);

  for (guint i = 0; lang_ids[i]; i++)
    {
      GtkSourceLanguage *lang = gtk_source_language_manager_get_language (manager, lang_ids[i]);
      GtkTreeIter iter;

      g_value_set_static_string (&idval, g_intern_string (gtk_source_language_get_id (lang)));
      g_value_set_static_string (&nameval, g_intern_string (gtk_source_language_get_name (lang)));

      gtk_list_store_append (self->store, &iter);
      gtk_list_store_set_value (self->store, &iter, 0, &idval);
      gtk_list_store_set_value (self->store, &iter, 1, &nameval);

      g_value_reset (&idval);
      g_value_reset (&nameval);
    }

  gtk_tree_view_set_model (self->tree_view, GTK_TREE_MODEL (self->store));
}
