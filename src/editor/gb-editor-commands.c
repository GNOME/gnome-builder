/* gb-editor-commands.c
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

#define G_LOG_DOMAIN "editor-commands"

#include <glib/gi18n.h>

#include "gb-editor-commands.h"
#include "gb-editor-tab.h"
#include "gb-editor-tab-private.h"
#include "gb-editor-workspace.h"
#include "gb-editor-workspace-private.h"
#include "gb-log.h"
#include "gb-source-formatter.h"

typedef void (*GbEditorCommand) (GbEditorWorkspace *workspace,
                                 GbEditorTab       *tab);

typedef struct
{
  const gchar     *name;
  GbEditorCommand  command;
  gboolean         requires_tab;
} GbEditorCommandsEntry;

/**
 * gb_editor_commands_reformat:
 * @tab: A #GbEditorTab.
 *
 * Begin a source reformatting operation.
 *
 * TODO:
 *    - Use source reformatting rules based on the document language.
 *    - Perform operation asynchronously, while locking the editor.
 *    - Track editor state (loading/saving/operation/etc)
 *    - Maybe add GbSourceOperation? These could do lots of
 *      transforms, useful for FixIt's too?
 */
void
gb_editor_commands_reformat (GbEditorWorkspace *workspace,
                             GbEditorTab       *tab)
{
  GbEditorTabPrivate *priv;
  GbSourceFormatter *formatter;
  GtkSourceLanguage *language;
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;
  GtkTextIter iter;
  GtkTextMark *insert;
  gboolean fragment = TRUE;
  GError *error = NULL;
  gchar *input = NULL;
  gchar *output = NULL;
  guint line_number;
  guint char_offset;

  ENTRY;

  /*
   * TODO: Do this asynchronously, add tab state, propagate errors.
   */

  g_return_if_fail (GB_IS_EDITOR_TAB (tab));

  priv = tab->priv;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->source_view));

  gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);

  if (gtk_text_iter_compare (&begin, &end) == 0)
    {
      gtk_text_buffer_get_bounds (buffer, &begin, &end);
      fragment = FALSE;
    }

  input = gtk_text_buffer_get_text (buffer, &begin, &end, TRUE);

  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);
  char_offset = gtk_text_iter_get_line_offset (&iter);
  line_number = gtk_text_iter_get_line (&iter);

  language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer));
  formatter = gb_source_formatter_new_from_language (language);

  if (!gb_source_formatter_format (formatter, input, fragment, NULL, &output,
                                   &error))
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
      GOTO (cleanup);
    }

  gtk_text_buffer_begin_user_action (buffer);

  gb_source_view_clear_snippets (priv->source_view);

  /* TODO: Keep the cursor on same CXCursor from Clang instead of the
   *       same character offset within the buffer. We probably want
   *       to defer this to the formatter API since it will be language
   *       specific.
   */

  gtk_text_buffer_delete (buffer, &begin, &end);
  gtk_text_buffer_insert (buffer, &begin, output, -1);

  if (line_number >= gtk_text_buffer_get_line_count (buffer))
    {
      gtk_text_buffer_get_bounds (buffer, &begin, &iter);
      goto select_range;
    }

  gtk_text_buffer_get_iter_at_line (buffer, &iter, line_number);
  gtk_text_iter_forward_to_line_end (&iter);

  if (gtk_text_iter_get_line (&iter) != line_number)
    gtk_text_iter_backward_char (&iter);
  else if (gtk_text_iter_get_line_offset (&iter) > char_offset)
    gtk_text_buffer_get_iter_at_line_offset (buffer, &iter, line_number, char_offset);

select_range:
  gtk_text_buffer_select_range (buffer, &iter, &iter);
  gtk_text_buffer_end_user_action (buffer);

  gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (priv->source_view), &iter,
                                0.25, TRUE, 0.5, 0.5);

cleanup:
  g_free (input);
  g_free (output);
  g_clear_object (&formatter);

  EXIT;
}

/**
 * gb_editor_commands_go_to_start:
 * @tab: A #GbEditorTab.
 *
 * Move the insertion cursor to the beginning of the document.
 * Scroll the view appropriately so that the cursor is visible.
 */
void
gb_editor_commands_go_to_start (GbEditorWorkspace *workspace,
                                GbEditorTab       *tab)
{
  GbEditorTabPrivate *priv;
  GtkTextIter begin;
  GtkTextIter end;

  ENTRY;

  g_return_if_fail (GB_IS_EDITOR_TAB (tab));

  priv = tab->priv;

  gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (priv->document), &begin, &end);
  gtk_text_buffer_select_range (GTK_TEXT_BUFFER (priv->document),
                                &begin, &begin);
  gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (priv->source_view), &begin,
                                0.25, TRUE, 0.5, 0.5);

  EXIT;
}

/**
 * gb_editor_commands_go_to_end:
 * @tab: A #GbEditorTab.
 *
 * Move the insertion cursor to the end of the document.
 * Scroll the view appropriately so that the cursor is visible.
 */
void
gb_editor_commands_go_to_end (GbEditorWorkspace *workspace,
                              GbEditorTab       *tab)
{
  GbEditorTabPrivate *priv;
  GtkTextIter begin;
  GtkTextIter end;

  ENTRY;

  g_return_if_fail (GB_IS_EDITOR_TAB (tab));

  priv = tab->priv;

  gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (priv->document), &begin, &end);
  gtk_text_buffer_select_range (GTK_TEXT_BUFFER (priv->document), &end, &end);
  gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (priv->source_view), &end,
                                0.25, TRUE, 0.5, 0.5);

  EXIT;
}

void
gb_editor_commands_find (GbEditorWorkspace *workspace,
                         GbEditorTab       *tab)
{
  GbEditorTabPrivate *priv;

  ENTRY;

  g_return_if_fail (GB_IS_EDITOR_TAB (tab));

  priv = tab->priv;

  gtk_revealer_set_reveal_child (priv->revealer, TRUE);
  gtk_source_search_context_set_highlight (priv->search_context, TRUE);
  gtk_widget_grab_focus (GTK_WIDGET (priv->search_entry));

  EXIT;
}

void
gb_editor_commands_close_tab (GbEditorWorkspace *workspace,
                              GbEditorTab       *tab)
{
  g_return_if_fail (GB_IS_EDITOR_TAB (tab));

  gb_tab_close (GB_TAB (tab));
}

static void
gb_editor_commands_toggle_preview (GbEditorWorkspace *workspace,
                                   GbEditorTab       *tab)
{
  GbEditorTabPrivate *priv;
  GtkSourceLanguage *lang;
  GtkWidget *child;
  GList *children;

  g_return_if_fail (GB_IS_EDITOR_WORKSPACE (workspace));
  g_return_if_fail (GB_IS_EDITOR_TAB (tab));

  priv = tab->priv;

  children = gtk_container_get_children (GTK_CONTAINER (priv->preview_container));

  if (children)
    {
      child = children->data;
      g_list_free (children);

      gtk_container_remove (GTK_CONTAINER (priv->preview_container), child);
      gtk_widget_hide (GTK_WIDGET (priv->preview_container));

      return;
    }

  lang = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (priv->document));

  if (lang)
    {
      const gchar *lang_id;

      lang_id = gtk_source_language_get_id (lang);

      if (g_strcmp0 (lang_id, "markdown") == 0)
        {
          child = g_object_new (GB_TYPE_MARKDOWN_PREVIEW,
                                "buffer", priv->document,
                                "width-request", 100,
                                "hexpand", TRUE,
                                "visible", TRUE,
                                NULL);
          gtk_container_add (GTK_CONTAINER (priv->preview_container), child);
          gtk_widget_show (GTK_WIDGET (priv->preview_container));
        }
    }
}

static void
file_progress_cb (goffset      current_num_bytes,
                  goffset      total_num_bytes,
                  GbEditorTab *tab)
{
  GbEditorTabPrivate *priv;
  gdouble fraction;

  g_return_if_fail (GB_IS_EDITOR_TAB (tab));

  priv = tab->priv;

  if (priv->save_animation)
    {
      g_object_remove_weak_pointer (G_OBJECT (priv->save_animation),
                                    (gpointer *)&priv->save_animation);
      gb_animation_stop (priv->save_animation);
      priv->save_animation = NULL;
    }

  fraction = total_num_bytes
           ? ((gdouble)current_num_bytes / (gdouble)total_num_bytes)
           : 1.0;

  priv->save_animation = gb_object_animate (priv->progress_bar,
                                            GB_ANIMATION_LINEAR,
                                            250,
                                            NULL,
                                            "fraction", fraction,
                                            NULL);
  g_object_add_weak_pointer (G_OBJECT (priv->save_animation),
                             (gpointer *)&priv->save_animation);
}

static gboolean
hide_progress_bar_cb (gpointer data)
{
  GbEditorTab *tab = data;

  g_assert (GB_IS_EDITOR_TAB (tab));

  gb_object_animate_full (tab->priv->progress_bar,
                          GB_ANIMATION_EASE_OUT_CUBIC,
                          250,
                          NULL,
                          (GDestroyNotify)gtk_widget_hide,
                          tab->priv->progress_bar,
                          "opacity", 0.0,
                          NULL);

  g_object_unref (tab);

  return G_SOURCE_REMOVE;
}

static void
on_load_cb (GtkSourceFileLoader *loader,
            GAsyncResult        *result,
            GbEditorTab         *tab)
{
  GError *error = NULL;

  g_return_if_fail (GTK_SOURCE_IS_FILE_LOADER (loader));
  g_return_if_fail (G_IS_ASYNC_RESULT (result));
  g_return_if_fail (GB_IS_EDITOR_TAB (tab));

  /*
   * Hide the progress bar after a timeout period.
   */
  g_timeout_add (350, hide_progress_bar_cb, g_object_ref (tab));

  if (!gtk_source_file_loader_load_finish (loader, result, &error))
    {
      /*
       * TODO: Propagate error to tab.
       */
      g_warning ("%s", error->message);
      g_clear_error (&error);
    }

  g_object_unref (tab);
}

void
gb_editor_tab_open_file (GbEditorTab *tab,
                         GFile       *file)
{
  GbEditorTabPrivate *priv;
  GtkSourceFileLoader *loader;

  g_return_if_fail (GB_IS_EDITOR_TAB (tab));
  g_return_if_fail (G_IS_FILE (file));

  priv = tab->priv;

  gtk_source_file_set_location (priv->file, file);

  loader = gtk_source_file_loader_new (GTK_SOURCE_BUFFER (priv->document),
                                       priv->file);

  gtk_source_file_loader_load_async (loader,
                                     G_PRIORITY_DEFAULT,
                                     NULL, /* TODO: Cancellable */
                                     (GFileProgressCallback)file_progress_cb,
                                     tab,
                                     NULL,
                                     (GAsyncReadyCallback)on_load_cb,
                                     g_object_ref (tab));

  g_object_unref (loader);
}

static void
on_save_cb (GtkSourceFileSaver *saver,
            GAsyncResult       *result,
            GbEditorTab        *tab)
{
  GError *error = NULL;

  g_return_if_fail (GTK_SOURCE_IS_FILE_SAVER (saver));
  g_return_if_fail (G_IS_ASYNC_RESULT (result));
  g_return_if_fail (GB_IS_EDITOR_TAB (tab));

  /*
   * Hide the progress bar after a timeout period.
   */
  g_timeout_add (350, hide_progress_bar_cb, g_object_ref (tab));

  if (!gtk_source_file_saver_save_finish (saver, result, &error))
    {
      /*
       * TODO: Propagate error to tab.
       */
      g_warning ("%s", error->message);
      g_clear_error (&error);
    }
  else
    gtk_text_buffer_set_modified (GTK_TEXT_BUFFER (tab->priv->document),
                                  FALSE);

  g_object_unref (tab);
}

static void
gb_editor_tab_do_save (GbEditorTab *tab)
{
  GbEditorTabPrivate *priv;
  GtkSourceFileSaver *saver;

  g_return_if_fail (GB_IS_EDITOR_TAB (tab));
  g_return_if_fail (tab->priv->file);
  g_return_if_fail (gtk_source_file_get_location (tab->priv->file));

  priv = tab->priv;

  /*
   * TODO: Tab needs a state machine for what are valid operations.
   */

  gtk_progress_bar_set_fraction (priv->progress_bar, 0.0);
  gtk_widget_set_opacity (GTK_WIDGET (priv->progress_bar), 1.0);
  gtk_widget_show (GTK_WIDGET (priv->progress_bar));

  saver = gtk_source_file_saver_new (GTK_SOURCE_BUFFER (priv->document),
                                     priv->file);

  gtk_source_file_saver_save_async (saver,
                                    G_PRIORITY_DEFAULT,
                                    NULL, /* TODO: Cancellable */
                                    (GFileProgressCallback)file_progress_cb,
                                    tab,
                                    NULL,
                                    (GAsyncReadyCallback)on_save_cb,
                                    g_object_ref (tab));

  g_object_unref (saver);
}

void
gb_editor_tab_save_as (GbEditorTab *tab)
{
  GtkFileChooserDialog *dialog;
  GtkWidget *toplevel;
  GtkWidget *suggested;
  GtkResponseType response;

  g_return_if_fail (GB_IS_EDITOR_TAB (tab));

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (tab));

  dialog = g_object_new (GTK_TYPE_FILE_CHOOSER_DIALOG,
                         "action", GTK_FILE_CHOOSER_ACTION_SAVE,
                         "do-overwrite-confirmation", TRUE,
                         "local-only", FALSE,
                         "select-multiple", FALSE,
                         "show-hidden", FALSE,
                         "transient-for", toplevel,
                         "title", _("Save"),
                         NULL);

  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          _("Cancel"), GTK_RESPONSE_CANCEL,
                          _("Save"), GTK_RESPONSE_OK,
                          NULL);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  suggested = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog),
                                                  GTK_RESPONSE_OK);
  gtk_style_context_add_class (gtk_widget_get_style_context (suggested),
                               GTK_STYLE_CLASS_SUGGESTED_ACTION);

  response = gtk_dialog_run (GTK_DIALOG (dialog));

  if (response == GTK_RESPONSE_OK)
    {
      GFile *file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));
      gtk_source_file_set_location (tab->priv->file, file);
      gb_editor_tab_do_save (tab);
      g_clear_object (&file);
    }

  gtk_widget_destroy (GTK_WIDGET (dialog));
}

void
gb_editor_commands_save (GbEditorWorkspace *workspace,
                         GbEditorTab       *tab)
{
  g_return_if_fail (GB_IS_EDITOR_TAB (tab));

  if (!gtk_source_file_get_location (tab->priv->file))
    {
      gb_editor_tab_save_as (tab);
      return;
    }

  gb_editor_tab_do_save (tab);
}

void
gb_editor_commands_save_as (GbEditorWorkspace *workspace,
                            GbEditorTab       *tab)
{
  g_return_if_fail (GB_IS_EDITOR_TAB (tab));

  gb_editor_tab_save_as (tab);
}

static void
gb_editor_commands_open (GbEditorWorkspace *workspace,
                         GbEditorTab       *tab)
{
  GbEditorWorkspacePrivate *priv;
  GtkFileChooserDialog *dialog;
  GtkWidget *toplevel;
  GtkWidget *suggested;
  GtkResponseType response;
  GbNotebook *notebook;
  GbTab *active_tab;

  g_return_if_fail (GB_IS_EDITOR_WORKSPACE (workspace));
  g_return_if_fail (!tab || GB_IS_EDITOR_TAB (tab));

  priv = workspace->priv;

  active_tab = gb_multi_notebook_get_active_tab (priv->multi_notebook);
  notebook = gb_multi_notebook_get_active_notebook (priv->multi_notebook);

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (workspace));

  dialog = g_object_new (GTK_TYPE_FILE_CHOOSER_DIALOG,
                         "action", GTK_FILE_CHOOSER_ACTION_OPEN,
                         "local-only", FALSE,
                         "select-multiple", TRUE,
                         "show-hidden", FALSE,
                         "transient-for", toplevel,
                         "title", _("Open"),
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
          GFile *file = iter->data;

          if (!gb_editor_tab_get_is_default (GB_EDITOR_TAB (active_tab)))
            {
              tab = GB_EDITOR_TAB (gb_editor_tab_new ());
              gb_notebook_add_tab (notebook, GB_TAB (tab));
              gtk_widget_show (GTK_WIDGET (tab));
            }

          gb_editor_tab_open_file (tab, file);
          gb_notebook_raise_tab (notebook, GB_TAB (tab));

          g_clear_object (&file);
        }

      g_slist_free (files);
    }

  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
gb_editor_commands_new_tab (GbEditorWorkspace *workspace,
                            GbEditorTab       *tab)
{
  GbEditorWorkspacePrivate *priv;
  GbNotebook *notebook;
  gint page;

  g_return_if_fail (GB_IS_EDITOR_WORKSPACE (workspace));

  priv = workspace->priv;

  notebook = gb_multi_notebook_get_active_notebook (priv->multi_notebook);

  tab = g_object_new (GB_TYPE_EDITOR_TAB,
                      "visible", TRUE,
                      NULL);
  gb_notebook_add_tab (notebook, GB_TAB (tab));

  gtk_container_child_get (GTK_CONTAINER (notebook), GTK_WIDGET (tab),
                           "position", &page,
                           NULL);
  gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), page);
}

static void
gb_editor_commands_activate (GSimpleAction *action,
                             GVariant      *variant,
                             gpointer       user_data)
{
  GbEditorCommandsEntry *command;
  GbEditorWorkspace *workspace = user_data;
  const gchar *name;
  GHashTable *hash;
  GbTab *tab;

  g_return_if_fail (GB_IS_EDITOR_WORKSPACE (workspace));
  g_return_if_fail (G_IS_SIMPLE_ACTION (action));

  name = g_action_get_name (G_ACTION (action));
  if (!name)
    return;

  hash = workspace->priv->command_map;
  if (!hash)
    return;

  command = g_hash_table_lookup (hash, name);
  if (!command)
    return;

  tab = gb_multi_notebook_get_active_tab (workspace->priv->multi_notebook);
  if (!tab && command->requires_tab)
    return;

  if (tab)
    command->command (workspace, GB_EDITOR_TAB (tab));
  else
    command->command (workspace, NULL);
}

void
gb_editor_commands_init (GbEditorWorkspace *workspace)
{
  static const GbEditorCommandsEntry commands[] = {
    { "close-tab",      gb_editor_commands_close_tab,      TRUE },
    { "find",           gb_editor_commands_find,           TRUE },
    { "go-to-start",    gb_editor_commands_go_to_start,    TRUE },
    { "go-to-end",      gb_editor_commands_go_to_end,      TRUE },
    { "new-tab",        gb_editor_commands_new_tab,        FALSE },
    { "open",           gb_editor_commands_open,           FALSE },
    { "toggle-preview", gb_editor_commands_toggle_preview, TRUE },
    { "reformat",       gb_editor_commands_reformat,       TRUE },
    { "save",           gb_editor_commands_save,           TRUE },
    { "save-as",        gb_editor_commands_save_as,        TRUE },
    { NULL }
  };
  guint i;

  g_return_if_fail (GB_IS_EDITOR_WORKSPACE (workspace));
  g_return_if_fail (workspace->priv->command_map);
  g_return_if_fail (G_IS_SIMPLE_ACTION_GROUP (workspace->priv->actions));

  for (i = 0; commands [i].name; i++)
    {
      GActionEntry entry = { commands [i].name,
                             gb_editor_commands_activate };
      g_hash_table_insert (workspace->priv->command_map,
                           (gchar *)commands [i].name,
                           (gpointer)&commands [i]);
      g_action_map_add_action_entries (G_ACTION_MAP (workspace->priv->actions),
                                       &entry, 1, workspace);
    }
}
