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
#include "gb-tree.h"
#include "gb-widget.h"
#include "gb-workbench.h"

struct _GbEditorWorkspacePrivate
{
  GHashTable         *command_map;
  GtkPaned           *paned;
  GbDocumentGrid     *document_grid;
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
      document = GB_DOCUMENT (gb_editor_document_new ());
      gb_editor_document_load_async (GB_EDITOR_DOCUMENT (document),
                                     file, NULL, NULL, NULL);
      gb_document_manager_add (manager, document);
      gb_document_grid_focus_document (priv->document_grid, document);
      g_object_unref (document);
    }
  else
    gb_document_grid_focus_document (priv->document_grid, document);
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
gb_editor_workspace_action_new_document (GSimpleAction *action,
                                         GVariant      *parameter,
                                         gpointer       user_data)
{
  GbEditorWorkspace *workspace = user_data;
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
gb_editor_workspace_action_open (GSimpleAction *action,
                                 GVariant      *parameter,
                                 gpointer       user_data)
{
  GbEditorWorkspace *workspace = user_data;
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
}

static void
gb_editor_workspace_finalize (GObject *object)
{
  GbEditorWorkspacePrivate *priv = GB_EDITOR_WORKSPACE (object)->priv;

  g_clear_pointer (&priv->command_map, g_hash_table_unref);

  G_OBJECT_CLASS (gb_editor_workspace_parent_class)->finalize (object);
}

static void
gb_editor_workspace_class_init (GbEditorWorkspaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gb_editor_workspace_finalize;

  widget_class->grab_focus = gb_editor_workspace_grab_focus;
  widget_class->map = gb_editor_workspace_map;

  GB_WIDGET_CLASS_TEMPLATE (klass, "gb-editor-workspace.ui");
  GB_WIDGET_CLASS_BIND (klass, GbEditorWorkspace, paned);
  GB_WIDGET_CLASS_BIND (klass, GbEditorWorkspace, document_grid);

  g_type_ensure (GB_TYPE_DOCUMENT_GRID);
  g_type_ensure (GB_TYPE_TREE);
}

static void
gb_editor_workspace_init (GbEditorWorkspace *workspace)
{
  const GActionEntry entries[] = {
    { "open",         gb_editor_workspace_action_open },
    { "new-document", gb_editor_workspace_action_new_document },
    { "jump-to-doc",  gb_editor_workspace_action_jump_to_doc, "s" },
  };
  GSimpleActionGroup *actions;

  workspace->priv = gb_editor_workspace_get_instance_private (workspace);

  workspace->priv->command_map = g_hash_table_new (g_str_hash, g_str_equal);

  gtk_widget_init_template (GTK_WIDGET (workspace));

  actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (actions),
                                   entries, G_N_ELEMENTS (entries),
                                   workspace);
  gtk_widget_insert_action_group (GTK_WIDGET (workspace), "workspace",
                                  G_ACTION_GROUP (actions));
  g_clear_object (&actions);
}
