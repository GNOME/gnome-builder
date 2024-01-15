/* gbp-editorui-workspace-addin.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-editorui-workspace-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-gui.h>
#include <libide-editor.h>

#include "ide-workspace-private.h"

#include "gbp-editorui-position-label.h"
#include "gbp-editorui-workspace-addin.h"

struct _GbpEditoruiWorkspaceAddin
{
  GObject                   parent_instance;

  IdeWorkspace             *workspace;
  PanelStatusbar           *statusbar;

  IdePropertyActionGroup   *buffer_actions;
  IdePropertyActionGroup   *view_actions;

  GBindingGroup            *buffer_bindings;
  GSignalGroup             *buffer_signals;
  GSignalGroup             *view_signals;

  GtkMenuButton            *indentation;
  GtkLabel                 *indentation_label;

  GtkMenuButton            *line_ends;
  GtkLabel                 *line_ends_label;

  GtkMenuButton            *position;
  GbpEditoruiPositionLabel *position_label;

  GtkMenuButton            *encoding;
  GtkLabel                 *encoding_label;

  GtkLabel                 *syntax_label;
  GtkMenuButton            *syntax;

  GtkLabel                 *mode_label;

  GSettings                *editor_settings;

  guint                     queued_cursor_moved;

  IdeEditorPage            *page;
};

typedef struct
{
  IdeWorkspace *workspace;
  PanelPosition *position;
  char *uri;
  char *language_id;
  guint sel_insert_line;
  guint sel_insert_line_offset;
  guint sel_bounds_line;
  guint sel_bounds_line_offset;
  gboolean has_focus;
} RestorePage;

static IdeActionMixin action_mixin;

#define clear_from_statusbar(s,w) clear_from_statusbar(s, (GtkWidget **)w)

static void
(clear_from_statusbar) (PanelStatusbar  *statusbar,
                        GtkWidget      **widget)
{
  if (*widget)
    {
      panel_statusbar_remove (statusbar, *widget);
      *widget = NULL;
    }
}

static void
restore_page_free (RestorePage *rp)
{
  g_clear_object (&rp->workspace);
  g_clear_object (&rp->position);
  g_clear_pointer (&rp->uri, g_free);
  g_clear_pointer (&rp->language_id, g_free);
  g_slice_free (RestorePage, rp);
}

static gboolean
language_to_label (GBinding     *binding,
                   const GValue *from_value,
                   GValue       *to_value,
                   gpointer      user_data)
{
  GtkSourceLanguage *language = g_value_get_object (from_value);

  if (language != NULL)
    g_value_set_string (to_value, gtk_source_language_get_name (language));
  else
    /* translators: "Plain Text" means text without formatting */
    g_value_set_static_string (to_value, _("Plain Text"));

  return TRUE;
}

static gboolean
newline_type_to_label (GBinding     *binding,
                       const GValue *from_value,
                       GValue       *to_value,
                       gpointer      user_data)
{
  GtkSourceNewlineType newline_type = g_value_get_enum (from_value);

  switch (newline_type)
    {
    default:
    case GTK_SOURCE_NEWLINE_TYPE_LF:
      g_value_set_static_string (to_value, "LF");
      return TRUE;

    case GTK_SOURCE_NEWLINE_TYPE_CR:
      g_value_set_static_string (to_value, "CR");
      return TRUE;

    case GTK_SOURCE_NEWLINE_TYPE_CR_LF:
      g_value_set_static_string (to_value, "CR/LF");
      return TRUE;
    }
}

static void
notify_overwrite_cb (GbpEditoruiWorkspaceAddin *self)
{
  IdeSourceView *view;

  g_assert (GBP_IS_EDITORUI_WORKSPACE_ADDIN (self));

  if ((view = _g_signal_group_get_target (self->view_signals)))
    {
      gboolean overwrite = gtk_text_view_get_overwrite (GTK_TEXT_VIEW (view));

      if (overwrite)
        gtk_label_set_label (self->mode_label, "OVR");
      else
        gtk_label_set_label (self->mode_label, "INS");
    }
}

static void
notify_indentation_cb (GbpEditoruiWorkspaceAddin *self)
{
  IdeSourceView *view;

  g_assert (GBP_IS_EDITORUI_WORKSPACE_ADDIN (self));

  if ((view = _g_signal_group_get_target (self->view_signals)))
    {
      g_autofree char *label = NULL;
      gboolean insert_spaces_instead_of_tabs;
      guint tab_width;
      int indent_width;

      g_object_get (view,
                    "tab-width", &tab_width,
                    "indent-width", &indent_width,
                    "insert-spaces-instead-of-tabs", &insert_spaces_instead_of_tabs,
                    NULL);

      if (indent_width <= 0)
        indent_width = tab_width;

      if (indent_width < 0 || indent_width == (int)tab_width)
        label = g_strdup_printf ("%s: %u",
                                 insert_spaces_instead_of_tabs ? _("Spaces") : _("Tabs"),
                                 tab_width);
      else
        label = g_strdup_printf ("%s: %u:%u",
                                 insert_spaces_instead_of_tabs ?  _("Spaces") : _("Tabs"),
                                 tab_width, indent_width);

      gtk_label_set_label (self->indentation_label, label);
    }
}

static void
update_position (GbpEditoruiWorkspaceAddin *self)
{
  IdeSourceView *view;

  g_assert (GBP_IS_EDITORUI_WORKSPACE_ADDIN (self));

  if ((view = _g_signal_group_get_target (self->view_signals)))
    {
      guint line, column, range;

      ide_source_view_get_visual_position_range (view, &line, &column, &range);
      gbp_editorui_position_label_update (self->position_label, line, column, range);
    }
}

static gboolean
update_position_idle (gpointer data)
{
  GbpEditoruiWorkspaceAddin *self = data;

  g_assert (GBP_IS_EDITORUI_WORKSPACE_ADDIN (self));

  self->queued_cursor_moved = 0;
  update_position (self);
  return G_SOURCE_REMOVE;
}

static void
cursor_moved_cb (GbpEditoruiWorkspaceAddin *self)
{
  g_assert (GBP_IS_EDITORUI_WORKSPACE_ADDIN (self));

  if (self->queued_cursor_moved)
    return;

  self->queued_cursor_moved = g_idle_add (update_position_idle, self);
}

static void
new_file_cb (GObject      *object,
             GAsyncResult *result,
             gpointer      user_data)
{
  IdeBufferManager *bufmgr = (IdeBufferManager *)object;
  g_autoptr(IdeWorkspace) workspace = user_data;
  g_autoptr(PanelPosition) position = NULL;
  g_autoptr(IdeBuffer) buffer = NULL;
  g_autoptr(GError) error = NULL;
  GtkWidget *page;

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER_MANAGER (bufmgr));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!(buffer = ide_buffer_manager_load_file_finish (bufmgr, result, &error)))
    {
      g_warning ("Failed to create new buffer: %s", error->message);
      IDE_EXIT;
    }

  page = ide_editor_page_new (buffer);
  position = panel_position_new ();
  ide_workspace_add_page (workspace, IDE_PAGE (page), position);
  panel_widget_raise (PANEL_WIDGET (page));
  gtk_widget_grab_focus (GTK_WIDGET (page));

  IDE_EXIT;
}

static void
new_file (gpointer    instance,
          const char *action_name,
          GVariant   *param)
{
  GbpEditoruiWorkspaceAddin *self = instance;
  IdeBufferManager *bufmgr;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (GBP_IS_EDITORUI_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (self->workspace));

  context = ide_workspace_get_context (self->workspace);
  bufmgr = ide_buffer_manager_from_context (context);

  ide_buffer_manager_load_file_async (bufmgr,
                                      NULL,
                                      IDE_BUFFER_OPEN_FLAGS_NONE,
                                      NULL,
                                      NULL,
                                      new_file_cb,
                                      g_object_ref (self->workspace));

  IDE_EXIT;
}

static void
go_to_line_activate_cb (GbpEditoruiWorkspaceAddin *self,
                        const char                *str,
                        IdeEntryPopover           *entry)
{
  int line = -1;
  int column = -1;

  IDE_ENTRY;

  g_assert (GBP_IS_EDITORUI_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_ENTRY_POPOVER (entry));

  if (ide_str_empty0 (str) || sscanf (str, "%d:%d", &line, &column) < 1)
    IDE_EXIT;

  line--;
  column--;

  ide_editor_page_scroll_to_visual_position (self->page, MAX (0, line), MAX (0, column));
  gtk_widget_grab_focus (GTK_WIDGET (self->page));

  IDE_EXIT;
}

static gboolean
go_to_line_insert_text_cb (GbpEditoruiWorkspaceAddin *self,
                           guint                      pos,
                           const char                *str,
                           guint                      n_chars,
                           IdeEntryPopover           *entry)
{
  IDE_ENTRY;

  g_assert (GBP_IS_EDITORUI_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_ENTRY_POPOVER (entry));

  for (const char *iter = str; *iter; iter = g_utf8_next_char (iter))
    {
      if (*iter != ':' && !g_ascii_isdigit (*iter))
        IDE_RETURN (GDK_EVENT_STOP);
    }

  IDE_RETURN (GDK_EVENT_PROPAGATE);
}

static void
go_to_line_changed_cb (GbpEditoruiWorkspaceAddin *self,
                       IdeEntryPopover           *entry)
{
  const char *text;
  int line = -1;
  int column = -1;

  IDE_ENTRY;

  g_assert (GBP_IS_EDITORUI_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_ENTRY_POPOVER (entry));

  text = ide_entry_popover_get_text (entry);

  if (ide_str_empty0 (text) ||
      sscanf (text, "%d:%d", &line, &column) < 1)
    {
      ide_entry_popover_set_ready (entry, FALSE);
      IDE_EXIT;
    }

  ide_entry_popover_set_ready (entry, TRUE);

  IDE_EXIT;
}

static void
show_go_to_line_cb (GbpEditoruiWorkspaceAddin *self,
                    IdeEntryPopover           *popover)
{
  g_autofree char *text = NULL;
  IdeSourceView *view;
  guint line;
  guint column;

  g_assert (GBP_IS_EDITORUI_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_ENTRY_POPOVER (popover));

  view = ide_editor_page_get_view (self->page);
  ide_source_view_get_visual_position (view, &line, &column);
  text = g_strdup_printf ("%u:%u", line+1, column+1);
  ide_entry_popover_set_text (popover, text);
  ide_entry_popover_select_all (popover);
}

static void
show_go_to_line (gpointer    instance,
                 const char *action_name,
                 GVariant   *param)
{
  GbpEditoruiWorkspaceAddin *self = instance;

  g_assert (GBP_IS_EDITORUI_WORKSPACE_ADDIN (self));

  if (self->page != NULL)
    gtk_menu_button_popup (self->position);
}

static void
gbp_editorui_workspace_addin_load (IdeWorkspaceAddin *addin,
                                   IdeWorkspace      *workspace)
{
  GbpEditoruiWorkspaceAddin *self = (GbpEditoruiWorkspaceAddin *)addin;
  g_autoptr(GMenuModel) encoding_menu = NULL;
  g_autoptr(GMenuModel) syntax_menu = NULL;
  IdeActionMuxer *muxer;
  GtkPopover *popover;
  GMenu *menu;

  IDE_ENTRY;

  g_assert (GBP_IS_EDITORUI_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  self->workspace = workspace;
  self->statusbar = ide_workspace_get_statusbar (workspace);

  self->buffer_actions = ide_property_action_group_new (IDE_TYPE_BUFFER);
  ide_property_action_group_add_string (self->buffer_actions, "encoding", "charset", TRUE);
  ide_property_action_group_add (self->buffer_actions, "newline-type", "newline-type");
  ide_property_action_group_add (self->buffer_actions, "language", "language-id");

  self->view_actions = ide_property_action_group_new (IDE_TYPE_SOURCE_VIEW);
  ide_property_action_group_add (self->view_actions, "indent-width", "indent-width");
  ide_property_action_group_add (self->view_actions, "tab-width", "tab-width");
  ide_property_action_group_add (self->view_actions, "use-spaces", "insert-spaces-instead-of-tabs");

  muxer = ide_action_mixin_get_action_muxer (self);
  ide_action_muxer_insert_action_group (muxer,
                                        "buffer",
                                        G_ACTION_GROUP (self->buffer_actions));
  ide_action_muxer_insert_action_group (muxer,
                                        "view",
                                        G_ACTION_GROUP (self->view_actions));

  self->encoding_label = g_object_new (GTK_TYPE_LABEL, NULL);
  self->line_ends_label = g_object_new (GTK_TYPE_LABEL, NULL);
  self->syntax_label = g_object_new (GTK_TYPE_LABEL, NULL);

  self->buffer_signals = g_signal_group_new (IDE_TYPE_BUFFER);
  g_signal_group_connect_object (self->buffer_signals,
                                   "cursor-moved",
                                   G_CALLBACK (cursor_moved_cb),
                                   self,
                                   G_CONNECT_SWAPPED);

  self->buffer_bindings = g_binding_group_new ();
  g_binding_group_bind (self->buffer_bindings, "charset",
                          self->encoding_label, "label",
                          G_BINDING_SYNC_CREATE);
  g_binding_group_bind_full (self->buffer_bindings, "newline-type",
                               self->line_ends_label, "label",
                               G_BINDING_SYNC_CREATE,
                               newline_type_to_label,
                               NULL, NULL, NULL);
  g_binding_group_bind_full (self->buffer_bindings, "language",
                               self->syntax_label, "label",
                               G_BINDING_SYNC_CREATE,
                               language_to_label,
                               NULL, NULL, NULL);

  self->view_signals = g_signal_group_new (IDE_TYPE_SOURCE_VIEW);
  g_signal_group_connect_object (self->view_signals,
                                   "notify::indent-width",
                                   G_CALLBACK (notify_indentation_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
  g_signal_group_connect_object (self->view_signals,
                                   "notify::tab-width",
                                   G_CALLBACK (notify_indentation_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
  g_signal_group_connect_object (self->view_signals,
                                   "notify::insert-spaces-instead-of-tabs",
                                   G_CALLBACK (notify_indentation_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
  g_signal_group_connect_object (self->view_signals,
                                   "notify::overwrite",
                                   G_CALLBACK (notify_overwrite_cb),
                                   self,
                                   G_CONNECT_SWAPPED);

  /* Language Syntax */
  syntax_menu = ide_editor_syntax_menu_new ("workspace.editorui.buffer.language");
  self->syntax = g_object_new (GTK_TYPE_MENU_BUTTON,
                               "menu-model", syntax_menu,
                               "direction", GTK_ARROW_UP,
                               "visible", FALSE,
                               "child", self->syntax_label,
                               NULL);
  panel_statusbar_add_suffix (self->statusbar, 1001, GTK_WIDGET (self->syntax));

  /* Line ending */
  menu = ide_application_get_menu_by_id (IDE_APPLICATION_DEFAULT, "editorui-line-ends-menu");
  self->line_ends = g_object_new (GTK_TYPE_MENU_BUTTON,
                                  "menu-model", menu,
                                  "direction", GTK_ARROW_UP,
                                  "visible", FALSE,
                                  "child", self->line_ends_label,
                                  NULL);
  panel_statusbar_add_suffix (self->statusbar, 1002, GTK_WIDGET (self->line_ends));

  /* Encoding */
  encoding_menu = ide_editor_encoding_menu_new ("workspace.editorui.buffer.encoding");
  self->encoding = g_object_new (GTK_TYPE_MENU_BUTTON,
                                 "menu-model", encoding_menu,
                                 "direction", GTK_ARROW_UP,
                                 "visible", FALSE,
                                 "child", self->encoding_label,
                                 NULL);
  panel_statusbar_add_suffix (self->statusbar, 1003, GTK_WIDGET (self->encoding));

  /* Indentation status, tabs/spaces/etc */
  menu = ide_application_get_menu_by_id (IDE_APPLICATION_DEFAULT, "editorui-indent-menu");
  self->indentation_label = g_object_new (GTK_TYPE_LABEL, NULL);
  self->indentation = g_object_new (GTK_TYPE_MENU_BUTTON,
                                    "menu-model", menu,
                                    "direction", GTK_ARROW_UP,
                                    "visible", FALSE,
                                    "child", self->indentation_label,
                                    NULL);
  panel_statusbar_add_suffix (self->statusbar, 1004, GTK_WIDGET (self->indentation));

  /* Label for cursor position and jump to line/column */
  popover = g_object_new (IDE_TYPE_ENTRY_POPOVER,
                          "button-text", _("Go"),
                          "title", _("Go to Line"),
                          NULL);
  g_signal_connect_object (popover,
                           "show",
                           G_CALLBACK (show_go_to_line_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (popover,
                           "changed",
                           G_CALLBACK (go_to_line_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (popover,
                           "insert-text",
                           G_CALLBACK (go_to_line_insert_text_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (popover,
                           "activate",
                           G_CALLBACK (go_to_line_activate_cb),
                           self,
                           G_CONNECT_SWAPPED);
  self->position_label = g_object_new (GBP_TYPE_EDITORUI_POSITION_LABEL, NULL);
  self->position = g_object_new (GTK_TYPE_MENU_BUTTON,
                                 "direction", GTK_ARROW_UP,
                                 "visible", FALSE,
                                 "child", self->position_label,
                                 "popover", popover,
                                 NULL);
  panel_statusbar_add_suffix (self->statusbar, 1005, GTK_WIDGET (self->position));

  self->mode_label = g_object_new (GTK_TYPE_LABEL,
                                   "label", "INS",
                                   "width-chars", 4,
                                   "visible", FALSE,
                                   NULL);
  panel_statusbar_add_suffix (self->statusbar, 1006, GTK_WIDGET (self->mode_label));

  self->editor_settings = g_settings_new ("org.gnome.builder.editor");

  IDE_EXIT;
}

static void
gbp_editorui_workspace_addin_unload (IdeWorkspaceAddin *addin,
                                     IdeWorkspace      *workspace)
{
  GbpEditoruiWorkspaceAddin *self = (GbpEditoruiWorkspaceAddin *)addin;
  IdeActionMuxer *muxer;

  IDE_ENTRY;

  g_assert (GBP_IS_EDITORUI_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  muxer = ide_action_mixin_get_action_muxer (addin);
  ide_action_muxer_remove_all (muxer);

  ide_property_action_group_set_item (self->buffer_actions, NULL);
  ide_property_action_group_set_item (self->view_actions, NULL);

  g_clear_object (&self->buffer_actions);
  g_clear_object (&self->view_actions);

  g_clear_object (&self->buffer_bindings);
  g_clear_object (&self->buffer_signals);
  g_clear_object (&self->view_signals);
  g_clear_object (&self->editor_settings);

  g_clear_handle_id (&self->queued_cursor_moved, g_source_remove);

  clear_from_statusbar (self->statusbar, &self->indentation);
  clear_from_statusbar (self->statusbar, &self->position);

  self->indentation_label = NULL;
  self->position_label = NULL;

  self->workspace = NULL;
  self->statusbar = NULL;

  IDE_EXIT;
}

static void
gbp_editorui_workspace_addin_page_changed (IdeWorkspaceAddin *addin,
                                           IdePage           *page)
{
  GbpEditoruiWorkspaceAddin *self = (GbpEditoruiWorkspaceAddin *)addin;
  g_autofree char *keybindings = NULL;
  IdeSourceView *view = NULL;
  IdeBuffer *buffer = NULL;

  g_assert (GBP_IS_EDITORUI_WORKSPACE_ADDIN (self));
  g_assert (!page || IDE_IS_PAGE (page));

  g_clear_handle_id (&self->queued_cursor_moved, g_source_remove);

  if (!IDE_IS_EDITOR_PAGE (page))
    page = NULL;

  self->page = IDE_EDITOR_PAGE (page);

  if (self->page != NULL)
    {
      buffer = ide_editor_page_get_buffer (self->page);
      view = ide_editor_page_get_view (self->page);
    }

  ide_property_action_group_set_item (self->buffer_actions, buffer);
  ide_property_action_group_set_item (self->view_actions, view);

  g_binding_group_set_source (self->buffer_bindings, buffer);
  g_signal_group_set_target (self->buffer_signals, buffer);
  g_signal_group_set_target (self->view_signals, view);

  notify_overwrite_cb (self);
  notify_indentation_cb (self);
  update_position (self);

  keybindings = g_settings_get_string (self->editor_settings, "keybindings");

  gtk_widget_set_visible (GTK_WIDGET (self->indentation), page != NULL);
  gtk_widget_set_visible (GTK_WIDGET (self->line_ends), page != NULL);
  gtk_widget_set_visible (GTK_WIDGET (self->position), page != NULL);
  gtk_widget_set_visible (GTK_WIDGET (self->encoding), page != NULL);
  gtk_widget_set_visible (GTK_WIDGET (self->mode_label), page != NULL && !ide_str_equal0 (keybindings, "vim"));
  gtk_widget_set_visible (GTK_WIDGET (self->syntax), page != NULL);
}

static GActionGroup *
gbp_editorui_workspace_addin_ref_action_group (IdeWorkspaceAddin *addin)
{
  return g_object_ref (G_ACTION_GROUP (ide_action_mixin_get_action_muxer (addin)));
}

static void
gbp_editorui_workspace_addin_save_session_page_cb (IdePage  *page,
                                                   gpointer  user_data)
{
  IdeSession *session = user_data;

  g_assert (IDE_IS_PAGE (page));
  g_assert (IDE_IS_SESSION (session));

  if (IDE_IS_EDITOR_PAGE (page))
    {
      g_autoptr(PanelPosition) position = ide_page_get_position (page);
      g_autoptr(IdeSessionItem) item = ide_session_item_new ();
      IdeBuffer *buffer = ide_editor_page_get_buffer (IDE_EDITOR_PAGE (page));
      GFile *file = ide_buffer_get_file (buffer);
      g_autofree char *uri = g_file_get_uri (file);
      IdeWorkspace *workspace = ide_widget_get_workspace (GTK_WIDGET (page));
      const char *id = ide_workspace_get_id (workspace);
      const char *language_id = ide_buffer_get_language_id (buffer);
      GtkTextIter insert, selection;

      g_debug ("Saving session information for %s", uri);

      gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer),
                                        &insert,
                                        gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (buffer)));
      gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer),
                                        &selection,
                                        gtk_text_buffer_get_selection_bound (GTK_TEXT_BUFFER (buffer)));

      ide_session_item_set_module_name (item, "editorui");
      ide_session_item_set_type_hint (item, "IdeEditorPage");
      ide_session_item_set_workspace (item, id);
      ide_session_item_set_position (item, position);
      ide_session_item_set_metadata (item, "uri", "s", uri);
      ide_session_item_set_metadata (item, "selection", "((uu)(uu))",
                                     gtk_text_iter_get_line (&insert),
                                     gtk_text_iter_get_line_offset (&insert),
                                     gtk_text_iter_get_line (&selection),
                                     gtk_text_iter_get_line_offset (&selection));

      if (language_id != NULL && !ide_str_equal0 (language_id, "plain"))
        ide_session_item_set_metadata (item, "language-id", "s", language_id);

      if (page == ide_workspace_get_most_recent_page (workspace))
        ide_session_item_set_metadata (item, "has-focus", "b", TRUE);

      ide_session_append (session, item);
    }
}

static void
gbp_editorui_workspace_addin_save_session (IdeWorkspaceAddin *addin,
                                           IdeSession        *session)
{
  GbpEditoruiWorkspaceAddin *self = (GbpEditoruiWorkspaceAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_EDITORUI_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (self->workspace));
  g_assert (IDE_IS_SESSION (session));

  ide_workspace_foreach_page (self->workspace,
                              gbp_editorui_workspace_addin_save_session_page_cb,
                              session);
}

static void
restore_page_cb (GObject      *object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  IdeBufferManager *buffer_manager = (IdeBufferManager *)object;
  g_autoptr(IdeBuffer) buffer = NULL;
  g_autoptr(GError) error = NULL;
  RestorePage *rp = user_data;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (rp != NULL);

  if ((buffer = ide_buffer_manager_load_file_finish (buffer_manager, result, &error)))
    {
      GtkWidget *page = ide_editor_page_new (buffer);

      if (!ide_str_empty0 (rp->language_id))
        ide_buffer_set_language_id (buffer, rp->language_id);

      if (rp->sel_insert_line || rp->sel_insert_line_offset ||
          rp->sel_bounds_line || rp->sel_bounds_line_offset)
        {
          GtkTextIter insert;
          GtkTextIter bounds;

          gtk_text_buffer_get_iter_at_line_offset (GTK_TEXT_BUFFER (buffer),
                                                   &insert,
                                                   rp->sel_insert_line,
                                                   rp->sel_insert_line_offset);
          gtk_text_buffer_get_iter_at_line_offset (GTK_TEXT_BUFFER (buffer),
                                                   &bounds,
                                                   rp->sel_bounds_line,
                                                   rp->sel_bounds_line_offset);
          gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer), &insert, &bounds);
        }

      ide_workspace_add_page (rp->workspace, IDE_PAGE (page), rp->position);

      if (rp->has_focus)
        {
          panel_widget_raise (PANEL_WIDGET (page));
          gtk_widget_grab_focus (GTK_WIDGET (page));
        }
    }

  restore_page_free (rp);

  IDE_EXIT;
}

static void
gbp_editorui_workspace_addin_restore_page (GbpEditoruiWorkspaceAddin *self,
                                           IdeSessionItem            *item)
{
  IdeBufferManager *buffer_manager;
  g_autoptr(IdeNotification) notif = NULL;
  g_autoptr(GFile) file = NULL;
  RestorePage *rp;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_EDITORUI_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_SESSION_ITEM (item));

  context = ide_workspace_get_context (self->workspace);
  buffer_manager = ide_buffer_manager_from_context (context);

  rp = g_slice_new0 (RestorePage);
  g_set_object (&rp->workspace, self->workspace);
  g_set_object (&rp->position, ide_session_item_get_position (item));

  if (ide_session_item_has_metadata_with_type (item, "uri", G_VARIANT_TYPE ("s")))
    ide_session_item_get_metadata (item, "uri", "s", &rp->uri);

  if (ide_session_item_has_metadata_with_type (item, "language-id", G_VARIANT_TYPE ("s")))
    ide_session_item_get_metadata (item, "language-id", "s", &rp->language_id);

  if (ide_session_item_has_metadata_with_type (item, "has-focus", G_VARIANT_TYPE ("b")))
    ide_session_item_get_metadata (item, "has-focus", "b", &rp->has_focus);

  if (ide_session_item_has_metadata_with_type (item, "selection", G_VARIANT_TYPE ("((uu)(uu))")))
    ide_session_item_get_metadata (item, "selection", "((uu)(uu))",
                                   &rp->sel_insert_line,
                                   &rp->sel_insert_line_offset,
                                   &rp->sel_bounds_line,
                                   &rp->sel_bounds_line_offset);

  if (!(file = g_file_new_for_uri (rp->uri)))
    IDE_GOTO (failure);

  notif = ide_notification_new ();

  ide_buffer_manager_load_file_async (buffer_manager,
                                      file,
                                      IDE_BUFFER_OPEN_FLAGS_NONE,
                                      notif,
                                      NULL,
                                      restore_page_cb,
                                      g_steal_pointer (&rp));

  IDE_EXIT;

failure:
  restore_page_free (rp);

  IDE_EXIT;
}

static void
gbp_editorui_workspace_addin_restore_session_item (IdeWorkspaceAddin *addin,
                                                   IdeSession        *session,
                                                   IdeSessionItem    *item)
{
  GbpEditoruiWorkspaceAddin *self = (GbpEditoruiWorkspaceAddin *)addin;
  const char *type_hint;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_EDITORUI_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (self->workspace));
  g_assert (IDE_IS_SESSION (session));

  type_hint = ide_session_item_get_type_hint (item);

  if (ide_str_equal0 (type_hint, "IdeEditorPage"))
    gbp_editorui_workspace_addin_restore_page (self, item);
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_editorui_workspace_addin_load;
  iface->unload = gbp_editorui_workspace_addin_unload;
  iface->page_changed = gbp_editorui_workspace_addin_page_changed;
  iface->ref_action_group = gbp_editorui_workspace_addin_ref_action_group;
  iface->save_session = gbp_editorui_workspace_addin_save_session;
  iface->restore_session_item = gbp_editorui_workspace_addin_restore_session_item;
}

G_DEFINE_TYPE_WITH_CODE (GbpEditoruiWorkspaceAddin, gbp_editorui_workspace_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init))

static void
gbp_editorui_workspace_addin_constructed (GObject *object)
{
  G_OBJECT_CLASS (gbp_editorui_workspace_addin_parent_class)->constructed (object);
  ide_action_mixin_constructed (&action_mixin, object);
}

static void
gbp_editorui_workspace_addin_class_init (GbpEditoruiWorkspaceAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gbp_editorui_workspace_addin_constructed;

  ide_action_mixin_init (&action_mixin, object_class);
  ide_action_mixin_install_action (&action_mixin, "page.go-to-line", NULL, show_go_to_line);
  ide_action_mixin_install_action (&action_mixin, "page.new", NULL, new_file);
}

static void
gbp_editorui_workspace_addin_init (GbpEditoruiWorkspaceAddin *self)
{
}
