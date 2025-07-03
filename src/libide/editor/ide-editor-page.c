/* ide-editor-page.c
 *
 * Copyright 2017-2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-editor-page"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-code.h>
#include <libide-sourceview.h>
#include <libide-threading.h>

#include "ide-source-view-private.h"

#include "ide-editor-info-bar-private.h"
#include "ide-editor-page-addin.h"
#include "ide-editor-page-private.h"
#include "ide-editor-print-operation.h"
#include "ide-editor-save-delegate.h"
#include "ide-scrollbar.h"
#include "ide-source-map.h"

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GActionGroup, g_object_unref)

enum {
  PROP_0,
  PROP_BUFFER,
  PROP_GUTTER,
  PROP_VIEW,
  N_PROPS
};

G_DEFINE_TYPE (IdeEditorPage, ide_editor_page, IDE_TYPE_PAGE)

static GParamSpec *properties [N_PROPS];

static void
ide_editor_page_query_file_info_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(IdeEditorPage) self = user_data;
  g_autoptr(GFileInfo) info = NULL;
  g_autoptr(GIcon) icon = NULL;
  const char *content_type;
  const char *name;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_EDITOR_PAGE (self));

  if (!(info = g_file_query_info_finish (file, result, NULL)))
    return;

  content_type = g_file_info_get_content_type (info);
  name = g_file_info_get_name (info);
  icon = ide_g_content_type_get_symbolic_icon (content_type, name);

  panel_widget_set_icon (PANEL_WIDGET (self), icon);
}

static void
ide_editor_page_notify_file_cb (IdeEditorPage *self,
                                GParamSpec    *pspec,
                                IdeBuffer     *buffer)
{
  GFile *file;

  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (IDE_IS_BUFFER (buffer));

  file = ide_buffer_get_file (buffer);

  g_file_query_info_async (file,
                           G_FILE_ATTRIBUTE_STANDARD_NAME","
                           G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                           G_FILE_QUERY_INFO_NONE,
                           G_PRIORITY_DEFAULT,
                           NULL,
                           ide_editor_page_query_file_info_cb,
                           g_object_ref (self));
}

static void
ide_editor_page_update_actions (IdeEditorPage *self)
{
  IdeFormatter *formatter;
  gboolean has_selection;

  g_assert (IDE_IS_EDITOR_PAGE (self));

  formatter = ide_buffer_get_formatter (self->buffer);
  has_selection = gtk_text_buffer_get_has_selection (GTK_TEXT_BUFFER (self->buffer));

  panel_widget_action_set_enabled (PANEL_WIDGET (self), "editor.format-document", formatter && !has_selection);
  panel_widget_action_set_enabled (PANEL_WIDGET (self), "editor.format-selection", formatter && has_selection);
}

static void
ide_editor_page_notify_formatter_cb (IdeEditorPage *self,
                                     GParamSpec    *pspec,
                                     IdeBuffer     *buffer)
{
  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (IDE_IS_BUFFER (buffer));

  ide_editor_page_update_actions (self);
}

static void
ide_editor_page_notify_has_selection_cb (IdeEditorPage *self,
                                         GParamSpec    *pspec,
                                         IdeBuffer     *buffer)
{
  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (IDE_IS_BUFFER (buffer));

  ide_editor_page_update_actions (self);
}

static void
ide_editor_page_modified_changed_cb (IdeEditorPage *self,
                                     IdeBuffer     *buffer)
{
  IDE_ENTRY;

  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (IDE_IS_BUFFER (buffer));

  panel_widget_set_modified (PANEL_WIDGET (self),
                             gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (buffer)));

  IDE_EXIT;
}

static void
ide_editor_page_style_scheme_changed_cb (IdeEditorPage *self,
                                         GParamSpec    *pspec,
                                         IdeBuffer     *buffer)
{
  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (IDE_IS_BUFFER (buffer));

  if (self->gutter != NULL)
    ide_gutter_style_changed (self->gutter);
}

static gboolean
file_to_basename (GBinding     *binding,
                  const GValue *from_value,
                  GValue       *to_value,
                  gpointer      user_data)
{
  GFile *file = g_value_get_object (from_value);
  g_value_take_string (to_value, g_file_get_basename (file));
  return TRUE;
}

static void
ide_editor_page_set_buffer (IdeEditorPage *self,
                            IdeBuffer     *buffer)
{
  IDE_ENTRY;

  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (IDE_IS_BUFFER (buffer));

  if (g_set_object (&self->buffer, buffer))
    {
      ide_buffer_hold (buffer);

      gtk_text_view_set_buffer (GTK_TEXT_VIEW (self->view), GTK_TEXT_BUFFER (buffer));

      g_signal_connect_object (buffer,
                               "modified-changed",
                               G_CALLBACK (ide_editor_page_modified_changed_cb),
                               self,
                               G_CONNECT_SWAPPED);

      g_signal_connect_object (buffer,
                               "notify::file",
                               G_CALLBACK (ide_editor_page_notify_file_cb),
                               self,
                               G_CONNECT_SWAPPED);

      g_signal_connect_object (buffer,
                               "notify::formatter",
                               G_CALLBACK (ide_editor_page_notify_formatter_cb),
                               self,
                               G_CONNECT_SWAPPED);

      g_signal_connect_object (buffer,
                               "notify::has-selection",
                               G_CALLBACK (ide_editor_page_notify_has_selection_cb),
                               self,
                               G_CONNECT_SWAPPED);

      g_signal_connect_object (buffer,
                               "notify::file-settings",
                               G_CALLBACK (_ide_editor_page_settings_reload),
                               self,
                               G_CONNECT_SWAPPED);

      g_signal_connect_object (buffer,
                               "notify::style-scheme",
                               G_CALLBACK (ide_editor_page_style_scheme_changed_cb),
                               self,
                               G_CONNECT_SWAPPED);

      g_object_bind_property_full (buffer, "file",
                                   self, "title",
                                   G_BINDING_SYNC_CREATE,
                                   file_to_basename, NULL,
                                   NULL, NULL);
      g_object_bind_property (buffer, "title",
                              self, "tooltip",
                              G_BINDING_SYNC_CREATE);

      ide_editor_page_notify_file_cb (self, NULL, buffer);
      ide_editor_page_notify_formatter_cb (self, NULL, buffer);
      ide_editor_page_modified_changed_cb (self, buffer);
      _ide_editor_page_settings_init (self);
    }

  IDE_EXIT;
}

static gboolean
ide_editor_page_grab_focus (GtkWidget *widget)
{
  return gtk_widget_grab_focus (GTK_WIDGET (IDE_EDITOR_PAGE (widget)->view));
}

static void
ide_editor_page_focus_enter_cb (IdeEditorPage           *self,
                                GtkEventControllerFocus *controller)
{
  g_autofree char *title = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_FOCUS (controller));

  title = ide_buffer_dup_title (self->buffer);
  g_debug ("Keyboard focus entered page \"%s\"", title);

  ide_page_mark_used (IDE_PAGE (self));

  IDE_EXIT;
}

static void
ide_editor_page_notify_frame_set (IdeExtensionSetAdapter *set,
                                  PeasPluginInfo         *plugin_info,
                                  GObject          *exten,
                                  gpointer                user_data)
{
  IdeFrame *frame = user_data;
  IdeEditorPageAddin *addin = (IdeEditorPageAddin *)exten;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_EDITOR_PAGE_ADDIN (addin));
  g_assert (IDE_IS_FRAME (frame));

  ide_editor_page_addin_frame_set (addin, frame);
}

static void
ide_editor_page_addin_added (IdeExtensionSetAdapter *set,
                             PeasPluginInfo         *plugin_info,
                             GObject          *exten,
                             gpointer                user_data)
{
  IdeEditorPage *self = user_data;
  IdeEditorPageAddin *addin = (IdeEditorPageAddin *)exten;
  g_autoptr(GActionGroup) action_group = NULL;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_EDITOR_PAGE_ADDIN (addin));
  g_assert (IDE_IS_EDITOR_PAGE (self));

  ide_editor_page_addin_load (addin, self);

  if ((action_group = ide_editor_page_addin_ref_action_group (addin)))
    panel_widget_insert_action_group (PANEL_WIDGET (self),
                                      peas_plugin_info_get_module_name (plugin_info),
                                      action_group);
}

static void
ide_editor_page_addin_removed (IdeExtensionSetAdapter *set,
                               PeasPluginInfo         *plugin_info,
                               GObject          *exten,
                               gpointer                user_data)
{
  IdeEditorPage *self = user_data;
  IdeEditorPageAddin *addin = (IdeEditorPageAddin *)exten;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_EDITOR_PAGE_ADDIN (addin));
  g_assert (IDE_IS_EDITOR_PAGE (self));

  panel_widget_insert_action_group (PANEL_WIDGET (self),
                                    peas_plugin_info_get_module_name (plugin_info),
                                    NULL);

  ide_editor_page_addin_unload (addin, self);
}

static void
ide_editor_page_root (GtkWidget *widget)
{
  IdeEditorPage *self = (IdeEditorPage *)widget;
  IdeContext *context;
  GtkWidget *frame;

  IDE_ENTRY;

  GTK_WIDGET_CLASS (ide_editor_page_parent_class)->root (widget);

  context = ide_widget_get_context (widget);
  frame = gtk_widget_get_ancestor (widget, IDE_TYPE_FRAME);

  if (self->addins == NULL && context != NULL)
    {
      self->addins = ide_extension_set_adapter_new (IDE_OBJECT (context),
                                                    peas_engine_get_default (),
                                                    IDE_TYPE_EDITOR_PAGE_ADDIN,
                                                    "Editor-Page-Languages",
                                                    ide_buffer_get_language_id (self->buffer));

      g_signal_connect (self->addins,
                        "extension-added",
                        G_CALLBACK (ide_editor_page_addin_added),
                        self);

      g_signal_connect (self->addins,
                        "extension-removed",
                        G_CALLBACK (ide_editor_page_addin_removed),
                        self);

      ide_extension_set_adapter_foreach (self->addins,
                                         ide_editor_page_addin_added,
                                         self);
    }

  if (self->addins != NULL && frame != NULL)
    ide_extension_set_adapter_foreach (self->addins,
                                       ide_editor_page_notify_frame_set,
                                       frame);

  IDE_EXIT;
}

static void
ide_editor_page_unroot (GtkWidget *widget)
{
  IdeEditorPage *self = (IdeEditorPage *)widget;

  g_assert (IDE_IS_EDITOR_PAGE (self));

  /* Unload addins before disconnecting from the widget tree so that
   * the addins can find the workspace/workbench/etc.
   */
  ide_clear_and_destroy_object (&self->addins);

  GTK_WIDGET_CLASS (ide_editor_page_parent_class)->unroot (widget);
}

static IdePage *
ide_editor_page_create_split (IdePage *page)
{
  IdeEditorPage *self = (IdeEditorPage *)page;
  GtkWidget *ret;

  IDE_ENTRY;

  g_assert (IDE_IS_EDITOR_PAGE (self));

  ret = ide_editor_page_new (self->buffer);

  IDE_RETURN (IDE_PAGE (ret));
}

static GFile *
ide_editor_page_get_file_or_directory (IdePage *page)
{
  GFile *ret;

  IDE_ENTRY;

  g_assert (IDE_IS_EDITOR_PAGE (page));

  ret = ide_buffer_get_file (IDE_EDITOR_PAGE (page)->buffer);

  if (ret != NULL)
    g_object_ref (ret);

  IDE_RETURN (ret);
}

static void
set_search_visible (IdeEditorPage          *self,
                    gboolean                search_visible,
                    IdeEditorSearchBarMode  mode)
{
  g_return_if_fail (IDE_IS_EDITOR_PAGE (self));

  if (search_visible)
    {
      _ide_editor_search_bar_set_mode (self->search_bar, mode);
      _ide_editor_search_bar_attach (self->search_bar, self->buffer);
    }
  else
    {
      _ide_editor_search_bar_detach (self->search_bar);
    }

  gtk_revealer_set_reveal_child (self->search_revealer, search_visible);

  if (search_visible)
    _ide_editor_search_bar_grab_focus (self->search_bar);

  _ide_source_view_set_search_context (self->view, self->search_bar->context);
}

static void
search_hide_action (GtkWidget  *widget,
                    const char *action_name,
                    GVariant   *param)
{
  IdeEditorPage *self = IDE_EDITOR_PAGE (widget);

  set_search_visible (self, FALSE, 0);
  gtk_widget_grab_focus (GTK_WIDGET (self->view));
}

static void
search_begin_find_action (GtkWidget  *widget,
                          const char *action_name,
                          GVariant   *param)
{
  set_search_visible (IDE_EDITOR_PAGE (widget), TRUE, IDE_EDITOR_SEARCH_BAR_MODE_SEARCH);
}

static void
search_begin_replace_action (GtkWidget  *widget,
                             const char *action_name,
                             GVariant   *param)
{
  set_search_visible (IDE_EDITOR_PAGE (widget), TRUE, IDE_EDITOR_SEARCH_BAR_MODE_REPLACE);
}

static void
search_move_next_action (GtkWidget  *widget,
                         const char *action_name,
                         GVariant   *param)
{
  IdeEditorPage *self = IDE_EDITOR_PAGE (widget);

  _ide_editor_search_bar_move_next (self->search_bar, FALSE);
}

static void
search_move_previous_action (GtkWidget  *widget,
                             const char *action_name,
                             GVariant   *param)
{
  IdeEditorPage *self = IDE_EDITOR_PAGE (widget);

  _ide_editor_search_bar_move_previous (self->search_bar, FALSE);
}

static void
handle_print_result (IdeEditorPage           *self,
                     GtkPrintOperation       *operation,
                     GtkPrintOperationResult  result)
{
  IDE_ENTRY;

  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (GTK_IS_PRINT_OPERATION (operation));

  if (result == GTK_PRINT_OPERATION_RESULT_ERROR)
    {
      g_autoptr(GError) error = NULL;

      gtk_print_operation_get_error (operation, &error);

      g_warning ("%s", error->message);
      ide_page_report_error (IDE_PAGE (self),
                             /* translators: %s is the error message */
                             _("Print failed: %s"), error->message);
    }

  IDE_EXIT;
}

static void
print_done (GtkPrintOperation       *operation,
            GtkPrintOperationResult  result,
            gpointer                 user_data)
{
  IdeEditorPage *self = user_data;

  IDE_ENTRY;

  g_assert (GTK_IS_PRINT_OPERATION (operation));
  g_assert (IDE_IS_EDITOR_PAGE (self));

  handle_print_result (self, operation, result);

  g_object_unref (operation);
  g_object_unref (self);

  IDE_EXIT;
}

static void
print_action (GtkWidget  *widget,
              const char *action_name,
              GVariant   *param)
{
  IdeEditorPage *self = (IdeEditorPage *)widget;
  g_autoptr(IdeEditorPrintOperation) operation = NULL;
  GtkPrintOperationResult result;
  IdeSourceView *view;
  GtkRoot *root;

  g_assert (IDE_IS_EDITOR_PAGE (self));

  root = gtk_widget_get_root (GTK_WIDGET (self));
  view = ide_editor_page_get_view (self);
  operation = ide_editor_print_operation_new (view);

  /* keep a ref until "done" is emitted */
  g_object_ref (operation);
  g_signal_connect_after (g_object_ref (operation),
                          "done",
                          G_CALLBACK (print_done),
                          g_object_ref (self));

  result = gtk_print_operation_run (GTK_PRINT_OPERATION (operation),
                                    GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
                                    GTK_WINDOW (root),
                                    NULL);

  handle_print_result (self, GTK_PRINT_OPERATION (operation), result);
}

static void
format_selection_cb (GObject      *object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  IdeBuffer *buffer = (IdeBuffer *)object;
  g_autoptr(IdeEditorPage) self = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_EDITOR_PAGE (self));

  if (!ide_buffer_format_selection_finish (buffer, result, &error))
    ide_page_report_error (IDE_PAGE (self),
                           /* translators: %s contains the error message */
                           _("Failed to format selection: %s"),
                           error->message);

  panel_widget_raise (PANEL_WIDGET (self));
  gtk_widget_grab_focus (GTK_WIDGET (self));

#if 0
  gtk_text_view_set_editable (GTK_TEXT_VIEW (self->view), TRUE);
#endif

  IDE_EXIT;
}

static void
format_action (GtkWidget  *widget,
               const char *action_name,
               GVariant   *param)
{
  IdeEditorPage *self = (IdeEditorPage *)widget;
  g_autoptr(IdeFormatterOptions) options = NULL;
  gboolean insert_spaces_instead_of_tabs;
  guint tab_width;

  IDE_ENTRY;

  g_assert (IDE_IS_EDITOR_PAGE (self));

  g_object_get (self->view,
                "tab-width", &tab_width,
                "insert-spaces-instead-of-tabs", &insert_spaces_instead_of_tabs,
                NULL);

  options = ide_formatter_options_new ();
  ide_formatter_options_set_tab_width (options, tab_width);
  ide_formatter_options_set_insert_spaces (options, insert_spaces_instead_of_tabs);

#if 0
  /* Disable editing while we format */
  /* BUG: we can't currently do this because it breaks input methods */
  gtk_text_view_set_editable (GTK_TEXT_VIEW (self->view), FALSE);
#endif

  ide_buffer_format_selection_async (self->buffer,
                                     options,
                                     NULL,
                                     format_selection_cb,
                                     g_object_ref (self));

  IDE_EXIT;
}

static void
reload_action (GtkWidget  *widget,
               const char *action_name,
               GVariant   *param)
{
  IdeEditorPage *self = (IdeEditorPage *)widget;
  IdeBufferManager *buffer_manager;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EDITOR_PAGE (self));

  context = ide_widget_get_context (widget);
  buffer_manager = ide_buffer_manager_from_context (context);

  ide_buffer_manager_load_file_async (buffer_manager,
                                      ide_buffer_get_file (self->buffer),
                                      IDE_BUFFER_OPEN_FLAGS_FORCE_RELOAD,
                                      NULL, /* TODO: Progress */
                                      NULL, NULL, NULL);

  IDE_EXIT;
}

static void
next_diagnostic_cb (guint                 line,
                    IdeDiagnosticSeverity severity,
                    gpointer              user_data)
{
  int *out_line = user_data;

  if (*out_line == -1)
    *out_line = line;
}

static void
diagnostics_next_action (GtkWidget  *widget,
                         const char *action_name,
                         GVariant   *param)
{
  IdeEditorPage *self = (IdeEditorPage *)widget;
  IdeDiagnostics *diagnostics;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EDITOR_PAGE (self));

  if ((diagnostics = ide_buffer_get_diagnostics (self->buffer)))
    {
      GtkTextIter insert;
      GtkTextIter begin, end;
      int line = -1;

      ide_buffer_get_selection_bounds (self->buffer, &insert, NULL);
      gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (self->buffer), &begin, &end);

      ide_diagnostics_foreach_line_in_range (diagnostics,
                                             ide_buffer_get_file (self->buffer),
                                             gtk_text_iter_get_line (&insert) + 1,
                                             gtk_text_iter_get_line (&end),
                                             next_diagnostic_cb,
                                             &line);

      if (line == -1)
        ide_diagnostics_foreach_line_in_range (diagnostics,
                                               ide_buffer_get_file (self->buffer),
                                               0,
                                               gtk_text_iter_get_line (&insert),
                                               next_diagnostic_cb,
                                               &line);

      if (line != -1)
        {
          gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (self->buffer), &insert, line);
          gtk_text_buffer_select_range (GTK_TEXT_BUFFER (self->buffer), &insert, &insert);
          ide_editor_page_scroll_to_insert (self, GTK_DIR_DOWN);
        }
    }

  IDE_EXIT;
}

static void
previous_diagnostic_cb (guint                 line,
                        IdeDiagnosticSeverity severity,
                        gpointer              user_data)
{
  int *out_line = user_data;
  *out_line = line;
}

static void
diagnostics_previous_action (GtkWidget  *widget,
                             const char *action_name,
                             GVariant   *param)
{
  IdeEditorPage *self = (IdeEditorPage *)widget;
  IdeDiagnostics *diagnostics;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EDITOR_PAGE (self));

  if ((diagnostics = ide_buffer_get_diagnostics (self->buffer)))
    {
      GtkTextIter insert;
      GtkTextIter begin, end;
      int line = -1;

      ide_buffer_get_selection_bounds (self->buffer, &insert, NULL);
      gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (self->buffer), &begin, &end);

      ide_diagnostics_foreach_line_in_range (diagnostics,
                                             ide_buffer_get_file (self->buffer),
                                             0,
                                             MAX (1, gtk_text_iter_get_line (&insert)) - 1,
                                             previous_diagnostic_cb,
                                             &line);

      if (line == -1)
        ide_diagnostics_foreach_line_in_range (diagnostics,
                                               ide_buffer_get_file (self->buffer),
                                               gtk_text_iter_get_line (&insert),
                                               gtk_text_iter_get_line (&end),
                                               previous_diagnostic_cb,
                                               &line);

      if (line != -1)
        {
          gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (self->buffer), &insert, line);
          gtk_text_buffer_select_range (GTK_TEXT_BUFFER (self->buffer), &insert, &insert);
          ide_editor_page_scroll_to_insert (self, GTK_DIR_UP);
        }
    }

  IDE_EXIT;
}

static void
ide_editor_page_constructed (GObject *object)
{
  IdeEditorPage *self = (IdeEditorPage *)object;
  g_autoptr(PanelSaveDelegate) save_delegate = NULL;

  G_OBJECT_CLASS (ide_editor_page_parent_class)->constructed (object);

  save_delegate = ide_editor_save_delegate_new (self);
  panel_widget_set_save_delegate (PANEL_WIDGET (self), save_delegate);
}

static void
ide_editor_page_dispose (GObject *object)
{
  IdeEditorPage *self = (IdeEditorPage *)object;

  ide_editor_page_set_gutter (self, NULL);

  g_clear_object (&self->buffer_file_settings);
  g_clear_object (&self->view_file_settings);

  if (self->buffer != NULL)
    {
      ide_buffer_release (self->buffer);
      g_clear_object (&self->buffer);
    }

  G_OBJECT_CLASS (ide_editor_page_parent_class)->dispose (object);
}

static void
ide_editor_page_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeEditorPage *self = IDE_EDITOR_PAGE (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      g_value_set_object (value, ide_editor_page_get_buffer (self));
      break;

    case PROP_GUTTER:
      g_value_set_object (value, ide_editor_page_get_gutter (self));
      break;

    case PROP_VIEW:
      g_value_set_object (value, ide_editor_page_get_view (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_page_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeEditorPage *self = IDE_EDITOR_PAGE (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      ide_editor_page_set_buffer (self, g_value_get_object (value));
      break;

    case PROP_GUTTER:
      ide_editor_page_set_gutter (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_page_class_init (IdeEditorPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  IdePageClass *page_class = IDE_PAGE_CLASS (klass);
  PanelWidgetClass *panel_widget_class = PANEL_WIDGET_CLASS (widget_class);

  object_class->constructed = ide_editor_page_constructed;
  object_class->dispose = ide_editor_page_dispose;
  object_class->get_property = ide_editor_page_get_property;
  object_class->set_property = ide_editor_page_set_property;

  widget_class->grab_focus = ide_editor_page_grab_focus;
  widget_class->root = ide_editor_page_root;
  widget_class->unroot = ide_editor_page_unroot;

  page_class->get_file_or_directory = ide_editor_page_get_file_or_directory;
  page_class->create_split = ide_editor_page_create_split;

  /**
   * IdeEditorPage:buffer:
   *
   * The #IdeBuffer that is displayed within the #IdeSourceView.
   */
  properties [PROP_BUFFER] =
    g_param_spec_object ("buffer",
                         "Buffer",
                         "The buffer to be displayed within the page",
                         IDE_TYPE_BUFFER,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeEditorPage:gutter:
   *
   * The "gutter" property contains an #IdeGutter or %NULL, which is a
   * specialized renderer for the sourceview which can bring together a number
   * of types of content which needs to be displayed, in a single renderer.
   */
  properties [PROP_GUTTER] =
    g_param_spec_object ("gutter",
                         "Gutter",
                         "The primary gutter renderer in the left gutter window",
                         IDE_TYPE_GUTTER,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeEditorPage:view:
   *
   * The #IdeSourceView contained within the page.
   */
  properties [PROP_VIEW] =
    g_param_spec_object ("view",
                         "View",
                         "The view displaying the buffer",
                         IDE_TYPE_SOURCE_VIEW,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-editor/ide-editor-page.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPage, map);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPage, scrollbar);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPage, scrubber_revealer);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPage, search_bar);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPage, search_revealer);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPage, scroller);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPage, view);
  gtk_widget_class_bind_template_callback (widget_class, ide_editor_page_focus_enter_cb);

  panel_widget_class_install_action (panel_widget_class, "search.hide", NULL, search_hide_action);
  panel_widget_class_install_action (panel_widget_class, "search.begin-find", NULL, search_begin_find_action);
  panel_widget_class_install_action (panel_widget_class, "search.begin-replace", NULL, search_begin_replace_action);
  panel_widget_class_install_action (panel_widget_class, "search.move-next", NULL, search_move_next_action);
  panel_widget_class_install_action (panel_widget_class, "search.move-previous", NULL, search_move_previous_action);
  panel_widget_class_install_action (panel_widget_class, "editor.print", NULL, print_action);
  panel_widget_class_install_action (panel_widget_class, "editor.format-document", NULL, format_action);
  panel_widget_class_install_action (panel_widget_class, "editor.format-selection", NULL, format_action);
  panel_widget_class_install_action (panel_widget_class, "editor.reload", NULL, reload_action);
  panel_widget_class_install_action (panel_widget_class, "editor.diagnostics.next", NULL, diagnostics_next_action);
  panel_widget_class_install_action (panel_widget_class, "editor.diagnostics.previous", NULL, diagnostics_previous_action);

  g_type_ensure (IDE_TYPE_EDITOR_INFO_BAR);
  g_type_ensure (IDE_TYPE_EDITOR_SEARCH_BAR);
  g_type_ensure (IDE_TYPE_SCROLLBAR);
  g_type_ensure (IDE_TYPE_SOURCE_MAP);
}

static void
ide_editor_page_init (IdeEditorPage *self)
{
  GtkSourceGutterRenderer *renderer;
  GtkSourceGutter *gutter;
  GMenu *menu;

  gtk_widget_init_template (GTK_WIDGET (self));

  ide_page_set_can_split (IDE_PAGE (self), TRUE);
  ide_page_set_menu_id (IDE_PAGE (self), "ide-editor-page-menu");

  /* Add menus to source view */
  menu = ide_application_get_menu_by_id (IDE_APPLICATION_DEFAULT, "ide-source-view-popup-menu");
  ide_source_view_append_menu (self->view, G_MENU_MODEL (menu));

  /* Add gutter changes to the overview map */
  gutter = ide_source_map_get_gutter (self->map, GTK_TEXT_WINDOW_LEFT);
  renderer = g_object_new (IDE_TYPE_LINE_CHANGE_GUTTER_RENDERER,
                           "width-request", 1,
                           NULL);
  gtk_source_gutter_insert (gutter, renderer, 100);
}

GtkWidget *
ide_editor_page_new (IdeBuffer *buffer)
{
  g_return_val_if_fail (IDE_IS_BUFFER (buffer), NULL);

  return g_object_new (IDE_TYPE_EDITOR_PAGE,
                       "buffer", buffer,
                       NULL);
}

/**
 * ide_editor_page_get_view:
 * @self: a #IdeEditorPage
 *
 * Gets the #IdeSourceView for the page.
 *
 * Returns: (transfer none): an #IdeSourceView
 */
IdeSourceView *
ide_editor_page_get_view (IdeEditorPage *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_PAGE (self), NULL);

  return self->view;
}

/**
 * ide_editor_page_get_buffer:
 * @self: a #IdeEditorPage
 *
 * Gets the #IdeBuffer for the page.
 *
 * Returns: (transfer none): an #IdeBuffer
 */
IdeBuffer *
ide_editor_page_get_buffer (IdeEditorPage *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_PAGE (self), NULL);

  return self->buffer;
}

/**
 * ide_editor_page_get_file:
 * @self: a #IdeEditorPage
 *
 * Gets the file for the document.
 *
 * This is a convenience function around ide_buffer_get_file().
 *
 * Returns: (transfer none): a #GFile
 */
GFile *
ide_editor_page_get_file (IdeEditorPage *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_PAGE (self), NULL);

  return ide_buffer_get_file (self->buffer);
}

static void
ide_editor_page_save_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  IdeBuffer *buffer = (IdeBuffer *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeEditorPage *self;

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);

  g_assert (IDE_IS_EDITOR_PAGE (self));

  ide_page_set_progress (IDE_PAGE (self), NULL);

  if (!ide_buffer_save_file_finish (buffer, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_editor_page_save_response (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  GtkFileDialog *dialog = (GtkFileDialog *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(IdeNotification) notif = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = NULL;
  IdeEditorPage *self;
  IdeBuffer *buffer;

  IDE_ENTRY;

  g_assert (GTK_IS_FILE_DIALOG (dialog));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  buffer = ide_task_get_task_data (task);

  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (IDE_IS_BUFFER (buffer));

  if (!(file = gtk_file_dialog_save_finish (dialog, result, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  ide_buffer_save_file_async (buffer,
                              file,
                              ide_task_get_cancellable (task),
                              &notif,
                              ide_editor_page_save_cb,
                              g_object_ref (task));

  ide_page_set_progress (IDE_PAGE (self), notif);

  IDE_EXIT;
}

void
ide_editor_page_save_async (IdeEditorPage       *self,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(IdeNotification) notif = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_EDITOR_PAGE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (IDE_IS_BUFFER (self->buffer));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_editor_page_save_async);
  ide_task_set_task_data (task, ide_buffer_hold (self->buffer), ide_buffer_release);

  if (ide_buffer_get_is_temporary (self->buffer))
    {
      g_autoptr(GtkFileDialog) dialog = NULL;
      g_autoptr(GFile) workdir = NULL;
      IdeWorkspace *workspace;
      IdeContext *context;

      workspace = ide_widget_get_workspace (GTK_WIDGET (self));
      context = ide_workspace_get_context (workspace);
      workdir = ide_context_ref_workdir (context);

      dialog = gtk_file_dialog_new ();
      gtk_file_dialog_set_accept_label (dialog, _("Save File"));
      gtk_file_dialog_set_modal (dialog, TRUE);
      gtk_file_dialog_set_initial_folder (dialog, workdir);

      gtk_file_dialog_save (dialog,
                            GTK_WINDOW (workspace),
                            NULL,
                            ide_editor_page_save_response,
                            g_object_ref (task));

      IDE_EXIT;
    }

  ide_buffer_save_file_async (self->buffer,
                              ide_buffer_get_file (self->buffer),
                              cancellable,
                              &notif,
                              ide_editor_page_save_cb,
                              g_steal_pointer (&task));

  ide_page_set_progress (IDE_PAGE (self), notif);

  IDE_EXIT;
}

gboolean
ide_editor_page_save_finish (IdeEditorPage  *self,
                             GAsyncResult   *result,
                             GError        **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_EDITOR_PAGE (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_editor_page_discard_changes_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  IdeBufferManager *bufmgr = (IdeBufferManager *)object;
  g_autoptr(IdeBuffer) buffer = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeEditorPage *self;

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER_MANAGER (bufmgr));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);

  g_assert (IDE_IS_EDITOR_PAGE (self));

  ide_page_set_progress (IDE_PAGE (self), NULL);

  if (!(buffer = ide_buffer_manager_load_file_finish (bufmgr, result, &error)))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  g_assert (!buffer || IDE_IS_BUFFER (buffer));

  IDE_EXIT;
}

void
ide_editor_page_discard_changes_async (IdeEditorPage       *self,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(IdeNotification) notif = NULL;
  IdeBufferManager *bufmgr;
  IdeContext *context;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_EDITOR_PAGE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (IDE_IS_BUFFER (self->buffer));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_editor_page_discard_changes_async);
  ide_task_set_task_data (task, ide_buffer_hold (self->buffer), ide_buffer_release);

  if (ide_buffer_get_is_temporary (self->buffer))
    {
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  context = ide_widget_get_context (GTK_WIDGET (self));
  bufmgr = ide_buffer_manager_from_context (context);
  notif = ide_notification_new ();
  ide_page_set_progress (IDE_PAGE (self), notif);

  ide_buffer_manager_load_file_async (bufmgr,
                                      ide_buffer_get_file (self->buffer),
                                      IDE_BUFFER_OPEN_FLAGS_FORCE_RELOAD,
                                      notif,
                                      cancellable,
                                      ide_editor_page_discard_changes_cb,
                                      g_steal_pointer (&task));

  IDE_EXIT;
}

gboolean
ide_editor_page_discard_changes_finish (IdeEditorPage  *self,
                                        GAsyncResult   *result,
                                        GError        **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_EDITOR_PAGE (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

/**
 * ide_editor_page_get_gutter:
 * @self: a #IdeEditorPage
 *
 * Gets the #IdeGutter displayed in the editor page.
 *
 * Returns: (transfer none) (nullable): an #IdeGutter or %NULL
 */
IdeGutter *
ide_editor_page_get_gutter (IdeEditorPage *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_PAGE (self), NULL);

  return self->gutter;
}

void
ide_editor_page_set_gutter (IdeEditorPage *self,
                            IdeGutter     *gutter)
{
  GtkSourceGutter *container;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_EDITOR_PAGE (self));
  g_return_if_fail (!gutter || IDE_IS_GUTTER (gutter));

  if (gutter == self->gutter)
    IDE_EXIT;

  container = gtk_source_view_get_gutter (GTK_SOURCE_VIEW (self->view),
                                          GTK_TEXT_WINDOW_LEFT);

  if (self->gutter)
    {
      gtk_source_gutter_remove (container, GTK_SOURCE_GUTTER_RENDERER (self->gutter));
      _ide_editor_page_settings_disconnect_gutter (self, self->gutter);
      g_clear_object (&self->gutter);
    }

  if (gutter)
    {
      g_set_object (&self->gutter, gutter);
      gtk_source_gutter_insert (container, GTK_SOURCE_GUTTER_RENDERER (self->gutter), 0);
      _ide_editor_page_settings_connect_gutter (self, self->gutter);
      ide_gutter_style_changed (self->gutter);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_GUTTER]);

  IDE_EXIT;
}

void
ide_editor_page_scroll_to_visual_position (IdeEditorPage *self,
                                           guint          line,
                                           guint          column)
{
  GtkTextIter iter;

  g_return_if_fail (IDE_IS_EDITOR_PAGE (self));

  ide_source_view_get_iter_at_visual_position (self->view, &iter, line, column);
  gtk_text_buffer_select_range (GTK_TEXT_BUFFER (self->buffer), &iter, &iter);
  gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (self->view),
                                      gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (self->buffer)));
}

void
ide_editor_page_scroll_to_insert (IdeEditorPage    *self,
                                  GtkDirectionType  dir)
{
  g_return_if_fail (IDE_IS_EDITOR_PAGE (self));

  ide_source_view_scroll_to_insert (self->view, dir);
}
