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

#include "gb-devhelp-tab.h"
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
#if 0
  GbEditorTab *tab;

  g_return_if_fail (GB_IS_EDITOR_WORKSPACE (workspace));
  g_return_if_fail (G_IS_FILE (file));

  tab = g_object_new (GB_TYPE_EDITOR_TAB,
                      "visible", TRUE,
                      NULL);
  gtk_container_add (GTK_CONTAINER (workspace->priv->tab_grid),
                     GTK_WIDGET (tab));
  gb_tab_grid_focus_tab (workspace->priv->tab_grid, GB_TAB (tab));

  gb_editor_tab_open_file (tab, file);
#endif
}

static void
save_tab (GSimpleAction *action,
          GVariant      *parameter,
          gpointer       user_data)
{
#if 0
  GbEditorWorkspace *workspace = user_data;
  GbTab *tab;

  tab = gb_tab_grid_get_active (workspace->priv->tab_grid);
  if (GB_IS_EDITOR_TAB (tab))
    gb_editor_tab_save (GB_EDITOR_TAB (tab));
#endif
}

static void
save_as_tab (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
#if 0
  GbEditorWorkspace *workspace = user_data;
  GbTab *tab;

  tab = gb_tab_grid_get_active (workspace->priv->tab_grid);
  if (GB_IS_EDITOR_TAB (tab))
    gb_editor_tab_save_as (GB_EDITOR_TAB (tab));
#endif
}

static void
find_tab (GSimpleAction *action,
          GVariant      *parameter,
          gpointer       user_data)
{
#if 0
  GbEditorWorkspace *workspace = user_data;
  GbTab *tab;

  tab = gb_tab_grid_get_active (workspace->priv->tab_grid);
  if (GB_IS_EDITOR_TAB (tab))
    gb_editor_tab_find (GB_EDITOR_TAB (tab));
#endif
}

static void
reformat_tab (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
#if 0
  GbEditorWorkspace *workspace = user_data;
  GbTab *tab;

  tab = gb_tab_grid_get_active (workspace->priv->tab_grid);
  if (GB_IS_EDITOR_TAB (tab))
    gb_editor_tab_reformat (GB_EDITOR_TAB (tab));
#endif
}

static void
preview_tab (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
#if 0
  GbEditorWorkspacePrivate *priv;
  GbEditorWorkspace *workspace = user_data;
  GbTab *preview = NULL;
  GbTab *tab;

  g_return_if_fail (GB_IS_EDITOR_WORKSPACE (workspace));

  priv = workspace->priv;

  tab = gb_tab_grid_get_active (workspace->priv->tab_grid);

  if (GB_IS_EDITOR_TAB (tab))
    {
      preview = gb_editor_tab_preview (GB_EDITOR_TAB (tab));

      if (preview)
        {
          /*
           * This widget might be already consumed in a stack somewhere.
           * If so, we want to jump to it, otherwise we want to add it
           * to a stack next to the current tab.
           */
          if (!gtk_widget_get_parent (GTK_WIDGET (preview)))
            {
              gtk_container_add (GTK_CONTAINER (priv->tab_grid),
                                 GTK_WIDGET (preview));
              gb_tab_grid_move_tab_right (priv->tab_grid, preview);
              gb_tab_grid_focus_tab (priv->tab_grid, tab);
            }
          else
            {
              g_warning ("TODO: implement refocus tab.");
            }
        }
    }
#endif
}

static void
new_tab (GSimpleAction *action,
         GVariant      *parameter,
         gpointer       user_data)
{
#if 0
  GbEditorWorkspace *workspace = user_data;
  GbEditorTab *tab;

  tab = gb_editor_tab_new ();
  gtk_container_add (GTK_CONTAINER (workspace->priv->tab_grid),
                     GTK_WIDGET (tab));
  gtk_widget_show (GTK_WIDGET (tab));
  gtk_widget_grab_focus (GTK_WIDGET (tab));
#endif
}

static void
jump_to_doc_tab (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
#if 0
  GbEditorWorkspace *workspace = user_data;
  const gchar *search_text;
  GbTab *tab;

  search_text = g_variant_get_string (parameter, NULL);
  if (!search_text || !*search_text)
    return;

  tab = gb_tab_grid_find_tab_typed (workspace->priv->tab_grid,
                                    GB_TYPE_DEVHELP_TAB);

  if (!tab)
    {
      tab = g_object_new (GB_TYPE_DEVHELP_TAB,
                          "visible", TRUE,
                          NULL);
      gtk_container_add (GTK_CONTAINER (workspace->priv->tab_grid),
                         GTK_WIDGET (tab));
      gb_tab_grid_move_tab_right (workspace->priv->tab_grid, tab);
    }

  gb_devhelp_tab_jump_to_keyword (GB_DEVHELP_TAB (tab), search_text);
  gb_tab_grid_focus_tab (workspace->priv->tab_grid, tab);
#endif
}

static void
gb_editor_workspace_load_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  GbEditorDocument *document = (GbEditorDocument *)object;
  GbEditorWorkspace *workspace = user_data;
  GError *error = NULL;

  g_return_if_fail (GB_IS_EDITOR_WORKSPACE (workspace));
  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (document));

  if (!gb_editor_document_load_finish (document, result, &error))
    {
      /* TODO: propagate error */
      g_warning ("%s", error->message);
      g_clear_error (&error);
    }
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
          GbDocumentManager *manager;
          GbDocument *document;
          GFile *file = iter->data;

          manager = gb_document_manager_get_default ();
          document = gb_document_manager_find_with_file (manager, file);

          if (!document)
            {
              /*
               * TODO: I'm not convinced this goes here.
               *       It's also ugly.
               */

              document = GB_DOCUMENT (gb_editor_document_new ());
              gb_document_manager_add (manager, document);
              gb_document_grid_focus_document (workspace->priv->document_grid,
                                               document);
              gb_editor_document_load_async (GB_EDITOR_DOCUMENT (document),
                                             file,
                                             NULL, /* cancellable */
                                             gb_editor_workspace_load_cb,
                                             workspace);
              g_object_unref (document);
            }
          else
            gb_document_grid_focus_document (workspace->priv->document_grid,
                                             document);

          g_clear_object (&file);
        }

      g_slist_free (files);
    }

  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
switch_pane_tab (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
#if 0
  GbEditorWorkspace *workspace = user_data;
  GbTab *tab;

  tab = gb_tab_grid_get_active (workspace->priv->tab_grid);
  if (GB_IS_EDITOR_TAB (tab))
    gb_editor_tab_switch_pane (GB_EDITOR_TAB (tab));
#endif
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
      { "new-tab", new_tab },
      { "open", open_tab },
      { "save", save_tab },
      { "save-as", save_as_tab },
      { "find", find_tab },
      { "reformat", reformat_tab },
      { "preview", preview_tab },
      { "jump-to-doc", jump_to_doc_tab, "s" },
      { "switch-pane", switch_pane_tab },
    };

  workspace->priv = gb_editor_workspace_get_instance_private (workspace);

  workspace->priv->actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (workspace->priv->actions),
                                   entries, G_N_ELEMENTS (entries),
                                   workspace);

  workspace->priv->command_map = g_hash_table_new (g_str_hash, g_str_equal);

  gtk_widget_init_template (GTK_WIDGET (workspace));
}
