/* gb-editor-workspace.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "editor-workspace"

#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>

#include "gb-devhelp-document.h"
#include "gb-devhelp-view.h"
#include "gb-document-grid.h"
#include "gb-editor-document.h"
#include "gb-editor-workspace.h"
#include "gb-log.h"
#include "gb-project-tree-builder.h"
#include "gb-tree.h"
#include "gb-widget.h"
#include "gb-workbench.h"
#include "gb-dnd.h"

enum
{
  TARGET_URI_LIST = 100
};

static const GtkTargetEntry drop_types [] = {
  { "text/uri-list", 0, TARGET_URI_LIST}
};

struct _GbEditorWorkspacePrivate
{
  GHashTable         *command_map;
  GtkPaned           *paned;
  gchar              *current_folder_uri;

  /* References not owned by this instance */
  GbDocumentGrid       *document_grid;
  GbProjectTreeBuilder *project_tree_builder;
  GbTree               *tree;
  GtkSizeGroup         *title_size_group;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbEditorWorkspace, gb_editor_workspace,
                            GB_TYPE_WORKSPACE)

void
gb_editor_workspace_open (GbEditorWorkspace *workspace,
                          GFile             *file)
{
  GbEditorWorkspacePrivate *priv;
  GbDocumentManager *manager;
  GbWorkbench *workbench;
  GbDocument *document;

  g_return_if_fail (GB_IS_EDITOR_WORKSPACE (workspace));

  priv = workspace->priv;

  workbench = gb_widget_get_workbench (GTK_WIDGET (workspace));
  manager = gb_workbench_get_document_manager (workbench);
  document = gb_document_manager_find_with_file (manager, file);

  if (!document)
    {
      gboolean close_untitled = FALSE;
      GList *list;

      /*
       * If we have a single document open, and it is an untitled document,
       * we want to close it so that it appears that this new document opens
       * in its place.
       */
      list = gb_document_manager_get_documents (manager);
      if ((g_list_length (list) == 1) &&
          gb_document_is_untitled (list->data) &&
          !gb_document_get_modified (list->data))
        close_untitled = TRUE;
      g_list_free (list);

      /*
       * Now open the new document.
       */
      document = GB_DOCUMENT (gb_editor_document_new ());
      gb_editor_document_load_async (GB_EDITOR_DOCUMENT (document),
                                     file, NULL, NULL, NULL);
      gb_document_manager_add (manager, document);
      gb_document_grid_focus_document (priv->document_grid, document);
      g_object_unref (document);

      /*
       * Now close the existing views if necessary.
       */
      if (close_untitled)
        gb_document_grid_close_untitled (priv->document_grid);
    }
  else
    gb_document_grid_focus_document (priv->document_grid, document);
}

static void
gb_editor_workspace_open_uri_list (GbEditorWorkspace  *workspace,
                                   const gchar       **uri_list)
{
  GFile *file;
  guint i;

  g_return_if_fail (GB_IS_EDITOR_WORKSPACE (workspace));
  g_return_if_fail (uri_list);

  for (i = 0; uri_list [i]; i++)
    {
      file = g_file_new_for_commandline_arg (uri_list [i]);

      if (file)
        {
          gb_editor_workspace_open (workspace, file);
          g_clear_object (&file);
        }
      else
        g_warning ("Received invalid URI target");
    }
}

static void
gb_editor_workspace_drag_data_received (GtkWidget        *widget,
                                        GdkDragContext   *context,
                                        gint              x,
                                        gint              y,
                                        GtkSelectionData *selection_data,
                                        guint             info,
                                        guint             timestamp,
                                        gpointer          data)
{
  GbEditorWorkspace *workspace = (GbEditorWorkspace *)widget;
  gchar **uri_list;
  gboolean handled = FALSE;

  g_return_if_fail (GB_IS_EDITOR_WORKSPACE (workspace));

  switch (info)
    {
    case TARGET_URI_LIST:
      uri_list = gb_dnd_get_uri_list (selection_data);

      if (uri_list)
        {
          gb_editor_workspace_open_uri_list (workspace,
                                             (const gchar **)uri_list);
  				g_strfreev (uri_list);
        }

      handled = TRUE;
      break;

    default:
      break;
    }

  gtk_drag_finish (context, handled, FALSE, timestamp);
}

static void
gb_editor_workspace_action_jump_to_doc (GSimpleAction *action,
                                        GVariant      *parameter,
                                        gpointer       user_data)
{
  GbEditorWorkspacePrivate *priv;
  GbEditorWorkspace *workspace = user_data;
  GbDocumentManager *manager;
  GbWorkbench *workbench;
  const gchar *search_text;
  GbDocument *document;
  GbDocument *reffed = NULL;

  g_return_if_fail (GB_IS_EDITOR_WORKSPACE (workspace));

  priv = workspace->priv;

  search_text = g_variant_get_string (parameter, NULL);
  if (!search_text || !*search_text)
    return;

  workbench = gb_widget_get_workbench (GTK_WIDGET (workspace));
  manager = gb_workbench_get_document_manager (workbench);
  document = gb_document_manager_find_with_type (manager,
                                                 GB_TYPE_DEVHELP_DOCUMENT);

  if (!document)
    {
      document = GB_DOCUMENT (gb_devhelp_document_new ());
      gb_document_manager_add (manager, document);
      reffed = document;
    }

  gb_devhelp_document_set_search (GB_DEVHELP_DOCUMENT (document), search_text);
  gb_document_grid_focus_document (priv->document_grid, document);

  g_clear_object (&reffed);
}

static void
gb_editor_workspace_action_open_uri_list (GSimpleAction *action,
                                          GVariant      *parameter,
                                          gpointer       user_data)
{
  GbEditorWorkspace *workspace = user_data;
  const gchar **uri_list;

  g_return_if_fail (GB_IS_EDITOR_WORKSPACE (workspace));

  uri_list = g_variant_get_strv (parameter, NULL);
  if(uri_list != NULL)
    {
      gb_editor_workspace_open_uri_list (workspace, uri_list);
      g_free (uri_list);
    }
}

void
gb_editor_workspace_new_document (GbEditorWorkspace *workspace)
{
  GbDocumentManager *manager;
  GbWorkbench *workbench;
  GbDocument *document;

  g_return_if_fail (GB_IS_EDITOR_WORKSPACE (workspace));

  workbench = gb_widget_get_workbench (GTK_WIDGET (workspace));
  manager = gb_workbench_get_document_manager (workbench);
  document = GB_DOCUMENT (gb_editor_document_new ());

  gb_document_manager_add (manager, document);
  gb_document_grid_focus_document (workspace->priv->document_grid, document);

  g_clear_object (&document);
}

static void
gb_editor_workspace_action_new_document (GSimpleAction *action,
                                         GVariant      *parameter,
                                         gpointer       user_data)
{
  GbEditorWorkspace *workspace = user_data;
  g_return_if_fail (GB_IS_EDITOR_WORKSPACE (workspace));
  gb_editor_workspace_new_document (workspace);
}

static void
gb_editor_workspace_action_open (GSimpleAction *action,
                                 GVariant      *parameter,
                                 gpointer       user_data)
{
  GbEditorWorkspace *workspace = user_data;
  GbEditorWorkspacePrivate *priv = workspace->priv;
  GtkFileChooserDialog *dialog;
  GtkWidget *toplevel;
  GtkWidget *suggested;
  GtkResponseType response;

  g_return_if_fail (GB_IS_EDITOR_WORKSPACE (workspace));

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (workspace));

  dialog = g_object_new (GTK_TYPE_FILE_CHOOSER_DIALOG,
                         "action", GTK_FILE_CHOOSER_ACTION_OPEN,
                         "local-only", FALSE,
                         "select-multiple", TRUE,
                         "show-hidden", FALSE,
                         "transient-for", toplevel,
                         "title", _("Open Document"),
                         NULL);

  if (priv->current_folder_uri)
    gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER (dialog),
                                             priv->current_folder_uri);

  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          _("Cancel"), GTK_RESPONSE_CANCEL,
                          _("Open"), GTK_RESPONSE_OK,
                          NULL);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  suggested = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog),
                                                  GTK_RESPONSE_OK);
  gtk_style_context_add_class (gtk_widget_get_style_context (suggested),
                               GTK_STYLE_CLASS_SUGGESTED_ACTION);

  response = gtk_dialog_run (GTK_DIALOG (dialog));

  if (response == GTK_RESPONSE_OK)
    {
      GSList *files;
      GSList *iter;
      gchar *file_uri;
      gchar *uri;

      file_uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));
      uri = g_path_get_dirname (file_uri);
      if (g_strcmp0 (priv->current_folder_uri, uri) != 0)
        {
          g_free (priv->current_folder_uri);
          priv->current_folder_uri = uri;
          uri = NULL;
        }
      g_free (uri);
      g_free (file_uri);

      files = gtk_file_chooser_get_files (GTK_FILE_CHOOSER (dialog));

      for (iter = files; iter; iter = iter->next)
        {
          gb_editor_workspace_open (workspace, G_FILE (iter->data));
          g_clear_object (&iter->data);
        }

      g_slist_free (files);
    }

  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
gb_editor_workspace_grab_focus (GtkWidget *widget)
{
  GbEditorWorkspace *workspace = (GbEditorWorkspace *)widget;

  ENTRY;

  g_return_if_fail (GB_IS_EDITOR_WORKSPACE (workspace));

  gtk_widget_grab_focus (GTK_WIDGET (workspace->priv->document_grid));

  EXIT;
}

static void
gb_editor_workspace_context_set (GbEditorWorkspace *workspace,
                                 GParamSpec        *pspec,
                                 GbWorkbench       *workbench)
{
  GbEditorWorkspacePrivate *priv;
  GbTreeNode *root;
  IdeContext *context;

  g_return_if_fail (GB_IS_EDITOR_WORKSPACE (workspace));
  g_return_if_fail (GB_IS_WORKBENCH (workbench));

  priv = workspace->priv;

  context = gb_workbench_get_context (workbench);

  root = gb_tree_get_root (priv->tree);
  gb_tree_node_set_item (root, G_OBJECT (context));

  gb_project_tree_builder_set_context (priv->project_tree_builder, context);
}

static void
gb_editor_workspace_map (GtkWidget *widget)
{
  GbEditorWorkspacePrivate *priv;
  GbEditorWorkspace *workspace = (GbEditorWorkspace *)widget;
  GbDocumentManager *document_manager;
  GbWorkbench *workbench;

  g_return_if_fail (GB_IS_EDITOR_WORKSPACE (workspace));

  priv = workspace->priv;

  GTK_WIDGET_CLASS (gb_editor_workspace_parent_class)->map (widget);

  workbench = gb_widget_get_workbench (GTK_WIDGET (workspace));
  document_manager = gb_workbench_get_document_manager (workbench);
  gb_document_grid_set_document_manager (priv->document_grid, document_manager);

  g_signal_connect_object (workbench,
                           "notify::context",
                           G_CALLBACK (gb_editor_workspace_context_set),
                           widget,
                           G_CONNECT_SWAPPED);

  gb_editor_workspace_context_set (workspace, NULL, workbench);
}

static void
gb_editor_workspace_constructed (GObject *object)
{
  GbEditorWorkspacePrivate *priv = GB_EDITOR_WORKSPACE (object)->priv;

  G_OBJECT_CLASS (gb_editor_workspace_parent_class)->constructed (object);

  priv->project_tree_builder = gb_project_tree_builder_new (NULL);
  gb_tree_add_builder (priv->tree, GB_TREE_BUILDER (priv->project_tree_builder));
  gb_tree_set_root (priv->tree, gb_tree_node_new ());
}

static void
gb_editor_workspace_finalize (GObject *object)
{
  GbEditorWorkspacePrivate *priv = GB_EDITOR_WORKSPACE (object)->priv;

  g_clear_pointer (&priv->command_map, g_hash_table_unref);
  g_clear_pointer (&priv->current_folder_uri, g_free);

  G_OBJECT_CLASS (gb_editor_workspace_parent_class)->finalize (object);
}

static void
gb_editor_workspace_class_init (GbEditorWorkspaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gb_editor_workspace_constructed;
  object_class->finalize = gb_editor_workspace_finalize;

  widget_class->grab_focus = gb_editor_workspace_grab_focus;
  widget_class->map = gb_editor_workspace_map;

  GB_WIDGET_CLASS_TEMPLATE (klass, "gb-editor-workspace.ui");
  GB_WIDGET_CLASS_BIND (klass, GbEditorWorkspace, document_grid);
  GB_WIDGET_CLASS_BIND (klass, GbEditorWorkspace, paned);
  GB_WIDGET_CLASS_BIND (klass, GbEditorWorkspace, title_size_group);
  GB_WIDGET_CLASS_BIND (klass, GbEditorWorkspace, tree);

  g_type_ensure (GB_TYPE_DOCUMENT_GRID);
  g_type_ensure (GB_TYPE_TREE);
}

static void
gb_editor_workspace_init (GbEditorWorkspace *workspace)
{
  const GActionEntry entries[] = {
    { "open",          gb_editor_workspace_action_open },
    { "new-document",  gb_editor_workspace_action_new_document },
    { "jump-to-doc",   gb_editor_workspace_action_jump_to_doc,   "s" },
    { "open-uri-list", gb_editor_workspace_action_open_uri_list, "as" }
  };
  GSimpleActionGroup *actions;

  workspace->priv = gb_editor_workspace_get_instance_private (workspace);

  workspace->priv->command_map = g_hash_table_new (g_str_hash, g_str_equal);
  workspace->priv->current_folder_uri = NULL;

  gtk_widget_init_template (GTK_WIDGET (workspace));

  actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (actions),
                                   entries, G_N_ELEMENTS (entries),
                                   workspace);
  gtk_widget_insert_action_group (GTK_WIDGET (workspace), "workspace",
                                  G_ACTION_GROUP (actions));
  g_clear_object (&actions);

  /* Drag and drop support*/
  gtk_drag_dest_set (GTK_WIDGET (workspace),
                     GTK_DEST_DEFAULT_MOTION |
                     GTK_DEST_DEFAULT_HIGHLIGHT |
                     GTK_DEST_DEFAULT_DROP,
                     drop_types,
                     G_N_ELEMENTS (drop_types),
                     GDK_ACTION_COPY);

  g_signal_connect (workspace,
                    "drag-data-received",
                    G_CALLBACK(gb_editor_workspace_drag_data_received),
                    NULL);

}
