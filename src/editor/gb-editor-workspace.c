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
#include "gb-editor-document.h"
#include "gb-editor-workspace.h"
#include "gb-editor-workspace-private.h"
#include "gb-tree.h"

enum {
  PROP_0,
  LAST_PROP
};

G_DEFINE_TYPE_WITH_PRIVATE (GbEditorWorkspace,
                            gb_editor_workspace,
                            GB_TYPE_WORKSPACE)

void
gb_editor_workspace_open (GbEditorWorkspace *workspace,
                          GFile             *file)
{
  GbDocumentManager *manager;
  GbDocument *document;

  g_return_if_fail (GB_IS_EDITOR_WORKSPACE (workspace));

  manager = gb_document_manager_get_default ();
  document = gb_document_manager_find_with_file (manager, file);

  if (!document)
    {
      document = GB_DOCUMENT (gb_editor_document_new ());
      gb_document_manager_add (manager, document);
      gb_document_grid_focus_document (workspace->priv->document_grid,
                                       document);
      /* TODO: Should we add simplified gb_editor_document_open()? */
      gb_editor_document_load_async (GB_EDITOR_DOCUMENT (document),
                                     file,
                                     NULL, /* cancellable */
                                     NULL,
                                     workspace);
      g_object_unref (document);
    }
  else
    gb_document_grid_focus_document (workspace->priv->document_grid,
                                     document);
}

static void
jump_to_doc_tab (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  GbEditorWorkspace *workspace = user_data;
  GbDocumentManager *manager;
  const gchar *search_text;
  GbDocument *document;
  GbDocument *reffed = NULL;

  g_return_if_fail (GB_IS_EDITOR_WORKSPACE (workspace));

  search_text = g_variant_get_string (parameter, NULL);
  if (!search_text || !*search_text)
    return;

  manager = gb_document_manager_get_default ();
  document = gb_document_manager_find_with_type (manager,
                                                 GB_TYPE_DEVHELP_DOCUMENT);

  if (!document)
    {
      document = GB_DOCUMENT (gb_devhelp_document_new ());
      gb_document_manager_add (manager, document);
      reffed = document;
    }

  gb_devhelp_document_set_search (GB_DEVHELP_DOCUMENT (document),
                                  search_text);

  gb_document_grid_focus_document (workspace->priv->document_grid,
                                   document);

  g_clear_object (&reffed);
}

static void
open_tab (GSimpleAction *action,
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

static GActionGroup *
gb_editor_workspace_get_actions (GbWorkspace *workspace)
{
  return G_ACTION_GROUP (GB_EDITOR_WORKSPACE (workspace)->priv->actions);
}

static void
gb_editor_workspace_grab_focus (GtkWidget *widget)
{
  GbEditorWorkspace *workspace = GB_EDITOR_WORKSPACE (widget);

  g_return_if_fail (GB_IS_EDITOR_WORKSPACE (workspace));

  gtk_widget_grab_focus (GTK_WIDGET (workspace->priv->document_grid));
}

static void
gb_editor_workspace_constructed (GObject *object)
{
  GbEditorWorkspacePrivate *priv = GB_EDITOR_WORKSPACE (object)->priv;
  GbDocumentManager *document_manager;

  G_OBJECT_CLASS (gb_editor_workspace_parent_class)->constructed (object);

  document_manager = gb_document_manager_get_default ();
  gb_document_grid_set_document_manager (priv->document_grid,
                                         document_manager);
}

static void
gb_editor_workspace_finalize (GObject *object)
{
  GbEditorWorkspacePrivate *priv = GB_EDITOR_WORKSPACE (object)->priv;

  g_clear_object (&priv->actions);
  g_clear_pointer (&priv->command_map, g_hash_table_unref);

  G_OBJECT_CLASS (gb_editor_workspace_parent_class)->finalize (object);
}

static void
gb_editor_workspace_class_init (GbEditorWorkspaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GbWorkspaceClass *workspace_class = GB_WORKSPACE_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gb_editor_workspace_constructed;
  object_class->finalize = gb_editor_workspace_finalize;

  workspace_class->get_actions = gb_editor_workspace_get_actions;

  widget_class->grab_focus = gb_editor_workspace_grab_focus;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/builder/ui/gb-editor-workspace.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GbEditorWorkspace, paned);
  gtk_widget_class_bind_template_child_private (widget_class, GbEditorWorkspace, document_grid);

  g_type_ensure (GB_TYPE_EDITOR_TAB);
  g_type_ensure (GB_TYPE_DOCUMENT_GRID);
  g_type_ensure (GB_TYPE_TREE);
}

static void
gb_editor_workspace_init (GbEditorWorkspace *workspace)
{
    const GActionEntry entries[] = {
      { "open", open_tab },
      { "jump-to-doc", jump_to_doc_tab, "s" },
    };

  workspace->priv = gb_editor_workspace_get_instance_private (workspace);

  workspace->priv->actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (workspace->priv->actions),
                                   entries, G_N_ELEMENTS (entries),
                                   workspace);

  workspace->priv->command_map = g_hash_table_new (g_str_hash, g_str_equal);

  gtk_widget_init_template (GTK_WIDGET (workspace));
}
