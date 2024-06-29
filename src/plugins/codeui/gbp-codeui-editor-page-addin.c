/* gbp-codeui-editor-page-addin.c
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

#define G_LOG_DOMAIN "gbp-codeui-editor-page-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-editor.h>

#include "gbp-codeui-code-action-dialog.h"
#include "gbp-codeui-editor-page-addin.h"
#include "gbp-codeui-range-dialog.h"
#include "gbp-codeui-rename-dialog.h"

struct _GbpCodeuiEditorPageAddin
{
  GObject        parent_instance;

  IdeEditorPage *page;

  IdeBuffer     *buffer;
  gulong         notify_rename_provider;
  gulong         notify_has_selection;
};

static void code_actions_action     (GbpCodeuiEditorPageAddin *self,
                                     GVariant                 *params);
static void find_references_action  (GbpCodeuiEditorPageAddin *self,
                                     GVariant                 *params);
static void goto_declaration_action (GbpCodeuiEditorPageAddin *self,
                                     GVariant                 *params);
static void goto_definition_action  (GbpCodeuiEditorPageAddin *self,
                                     GVariant                 *params);
static void rename_symbol_action    (GbpCodeuiEditorPageAddin *self,
                                     GVariant                 *params);

IDE_DEFINE_ACTION_GROUP (GbpCodeuiEditorPageAddin, gbp_codeui_editor_page_addin, {
  { "rename-symbol", rename_symbol_action },
  { "code-actions", code_actions_action },
  { "goto-declaration", goto_declaration_action },
  { "goto-definition", goto_definition_action },
  { "find-references", find_references_action },
})

static void
gbp_codeui_editor_page_addin_update_state (GbpCodeuiEditorPageAddin *self)
{
  IdeCodeActionProvider *code_action_provider;
  IdeRenameProvider *rename_provider;
  gboolean has_selection;
  gboolean has_resolvers;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODEUI_EDITOR_PAGE_ADDIN (self));
  g_assert (IDE_IS_BUFFER (self->buffer));

  has_resolvers = ide_buffer_has_symbol_resolvers (self->buffer);
  code_action_provider = ide_buffer_get_code_action_provider (self->buffer);
  rename_provider = ide_buffer_get_rename_provider (self->buffer);
  has_selection = gtk_text_buffer_get_has_selection (GTK_TEXT_BUFFER (self->buffer));

  gbp_codeui_editor_page_addin_set_action_enabled (self, "rename-symbol",
                                                   has_selection && rename_provider != NULL);
  gbp_codeui_editor_page_addin_set_action_enabled (self, "code-actions",
                                                   code_action_provider != NULL);
  gbp_codeui_editor_page_addin_set_action_enabled (self, "goto-declaration", has_resolvers);
  gbp_codeui_editor_page_addin_set_action_enabled (self, "goto-definition", has_resolvers);
  gbp_codeui_editor_page_addin_set_action_enabled (self, "find-references", has_resolvers);

  IDE_EXIT;
}

static void
gbp_codeui_editor_page_addin_load (IdeEditorPageAddin *addin,
                                   IdeEditorPage      *page)
{
  GbpCodeuiEditorPageAddin *self = (GbpCodeuiEditorPageAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_CODEUI_EDITOR_PAGE_ADDIN (self));
  g_assert (IDE_IS_EDITOR_PAGE (page));

  self->page = page;
  self->buffer = g_object_ref (ide_editor_page_get_buffer (page));

  self->notify_rename_provider =
    g_signal_connect_object (self->buffer,
                             "notify::rename-provider",
                             G_CALLBACK (gbp_codeui_editor_page_addin_update_state),
                             self,
                             G_CONNECT_SWAPPED);

  self->notify_has_selection =
    g_signal_connect_object (self->buffer,
                             "notify::has-selection",
                             G_CALLBACK (gbp_codeui_editor_page_addin_update_state),
                             self,
                             G_CONNECT_SWAPPED);

  gbp_codeui_editor_page_addin_update_state (self);

  IDE_EXIT;
}

static void
gbp_codeui_editor_page_addin_unload (IdeEditorPageAddin *addin,
                                     IdeEditorPage      *page)
{
  GbpCodeuiEditorPageAddin *self = (GbpCodeuiEditorPageAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_CODEUI_EDITOR_PAGE_ADDIN (self));
  g_assert (IDE_IS_EDITOR_PAGE (page));

  g_clear_signal_handler (&self->notify_has_selection, self->buffer);
  g_clear_signal_handler (&self->notify_rename_provider, self->buffer);

  g_clear_object (&self->buffer);

  self->page = NULL;

  IDE_EXIT;
}

static void
editor_page_addin_iface_init (IdeEditorPageAddinInterface *iface)
{
  iface->load = gbp_codeui_editor_page_addin_load;
  iface->unload = gbp_codeui_editor_page_addin_unload;
}

static void
code_actions_action (GbpCodeuiEditorPageAddin *self,
                     GVariant                 *param)
{
  AdwDialog *dialog;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODEUI_EDITOR_PAGE_ADDIN (self));
  g_assert (IDE_IS_BUFFER (self->buffer));

  dialog = gbp_codeui_code_action_dialog_new (self->buffer);
  adw_dialog_present (dialog, GTK_WIDGET (self->page));

  IDE_EXIT;
}

static void
rename_symbol_action (GbpCodeuiEditorPageAddin *self,
                      GVariant                 *param)
{
  g_autoptr(IdeLocation) location = NULL;
  IdeRenameProvider *provider;
  GtkTextBuffer *buffer;
  GtkTextIter begin, end;
  AdwDialog *dialog;
  g_autofree char *word = NULL;
  gboolean failed = FALSE;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODEUI_EDITOR_PAGE_ADDIN (self));
  g_assert (IDE_IS_BUFFER (self->buffer));

  if (!(provider = ide_buffer_get_rename_provider (self->buffer)))
    IDE_EXIT;

  buffer = GTK_TEXT_BUFFER (self->buffer);

  if (!gtk_text_buffer_get_selection_bounds (buffer, &begin, &end))
    IDE_EXIT;

  gtk_text_iter_order (&begin, &end);

  for (GtkTextIter iter = begin;
       gtk_text_iter_compare (&iter, &end) < 0;
       gtk_text_iter_forward_char (&iter))
    {
      gunichar ch = gtk_text_iter_get_char (&iter);

      if (g_unichar_isspace (ch))
        {
          failed = TRUE;
          break;
        }
    }

  if (failed)
    {
      dialog = adw_alert_dialog_new (_("Symbol Not Selected"),
                                     _("A symbol to rename must be selected"));
      adw_alert_dialog_add_response (ADW_ALERT_DIALOG (dialog), "close", _("Close"));
      adw_dialog_present (dialog, GTK_WIDGET (self->page));

      IDE_EXIT;
    }

  word = gtk_text_iter_get_slice (&begin, &end);
  location = ide_buffer_get_iter_location (self->buffer, &begin);

  dialog = gbp_codeui_rename_dialog_new (provider, location, word);
  adw_dialog_present (dialog, GTK_WIDGET (self->page));

  IDE_EXIT;
}

static void
gbp_codeui_editor_page_addin_error (GbpCodeuiEditorPageAddin *self,
                                    const GError             *error)
{
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODEUI_EDITOR_PAGE_ADDIN (self));
  g_assert (error != NULL);

  if (self->page == NULL)
    return;

  context = ide_widget_get_context (GTK_WIDGET (self->page));
  ide_context_warning (context, "%s", error->message);
}

static void
gbp_codeui_editor_page_addin_navigate_to (GbpCodeuiEditorPageAddin *self,
                                          IdeLocation              *location)
{
  g_autoptr(PanelPosition) position = NULL;
  IdeWorkspace *workspace;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODEUI_EDITOR_PAGE_ADDIN (self));

  if (self->page == NULL || location == NULL)
    return;

  workspace = ide_widget_get_workspace (GTK_WIDGET (self->page));
  position = panel_widget_get_position (PANEL_WIDGET (self->page));

  ide_editor_focus_location (workspace, position, location);
}

static void
goto_declaration_cb (GObject      *object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  IdeBuffer *buffer = (IdeBuffer *)object;
  g_autoptr(GbpCodeuiEditorPageAddin) self = user_data;
  g_autoptr(IdeSymbol) symbol = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_CODEUI_EDITOR_PAGE_ADDIN (self));

  if (!(symbol = ide_buffer_get_symbol_at_location_finish (buffer, result, &error)))
    gbp_codeui_editor_page_addin_error (self, error);
  else
    gbp_codeui_editor_page_addin_navigate_to (self,
                                              ide_symbol_get_header_location (symbol) ?
                                              ide_symbol_get_header_location (symbol) :
                                              ide_symbol_get_location (symbol));

  IDE_EXIT;
}

static void
goto_declaration_action (GbpCodeuiEditorPageAddin *self,
                         GVariant                 *params)
{
  GtkTextIter insert;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODEUI_EDITOR_PAGE_ADDIN (self));

  ide_buffer_get_selection_bounds (self->buffer, &insert, NULL);

  ide_buffer_get_symbol_at_location_async (self->buffer,
                                           &insert,
                                           NULL,
                                           goto_declaration_cb,
                                           g_object_ref (self));

  IDE_EXIT;
}

static void
goto_definition_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  IdeBuffer *buffer = (IdeBuffer *)object;
  g_autoptr(GbpCodeuiEditorPageAddin) self = user_data;
  g_autoptr(IdeSymbol) symbol = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_CODEUI_EDITOR_PAGE_ADDIN (self));

  if (!(symbol = ide_buffer_get_symbol_at_location_finish (buffer, result, &error)))
    gbp_codeui_editor_page_addin_error (self, error);
  else
    gbp_codeui_editor_page_addin_navigate_to (self, ide_symbol_get_location (symbol));

  IDE_EXIT;
}

static void
goto_definition_action (GbpCodeuiEditorPageAddin *self,
                        GVariant                 *params)
{
  GtkTextIter insert;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODEUI_EDITOR_PAGE_ADDIN (self));

  ide_buffer_get_selection_bounds (self->buffer, &insert, NULL);

  ide_buffer_get_symbol_at_location_async (self->buffer,
                                           &insert,
                                           NULL,
                                           goto_definition_cb,
                                           g_object_ref (self));

  IDE_EXIT;
}

typedef struct
{
  IdeLocation          *location;
  GPtrArray            *active;
  GListStore           *references;
  GbpCodeuiRangeDialog *dialog;
} FindReferences;

static void
find_references_free (FindReferences *state)
{
  g_clear_object (&state->location);
  g_clear_object (&state->references);
  g_clear_object (&state->dialog);
  g_clear_pointer (&state->active, g_ptr_array_unref);
  g_slice_free (FindReferences, state);
}

static void
find_references_append (FindReferences *state,
                        GPtrArray      *ranges)
{
  g_assert (state != NULL);
  g_assert (G_IS_LIST_STORE (state->references));
  g_assert (ranges != NULL);

  for (guint i = 0; i < ranges->len; i++)
    g_list_store_append (state->references, g_ptr_array_index (ranges, i));
}

static void
find_references_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  IdeSymbolResolver *resolver = (IdeSymbolResolver *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GPtrArray) references = NULL;
  g_autoptr(GError) error = NULL;
  GbpCodeuiEditorPageAddin *self;
  FindReferences *state;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SYMBOL_RESOLVER (resolver));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  state = ide_task_get_task_data (task);

  if (!(references = ide_symbol_resolver_find_references_finish (resolver, result, &error)))
    gbp_codeui_editor_page_addin_error (self, error);
  else
    find_references_append (state, references);

  g_ptr_array_remove (state->active, resolver);

  if (state->active->len == 0)
    {
      adw_alert_dialog_set_response_label (ADW_ALERT_DIALOG (state->dialog),
                                           "close", _("Close"));
      gbp_codeui_range_dialog_done (state->dialog);
      ide_task_return_boolean (task, TRUE);
    }
}

static void
find_references_action (GbpCodeuiEditorPageAddin *self,
                        GVariant                 *params)
{
  g_autoptr(GPtrArray) resolvers = NULL;
  g_autoptr(IdeTask) task = NULL;
  FindReferences *state;
  const char *language_id;
  AdwDialog *dialog;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODEUI_EDITOR_PAGE_ADDIN (self));

  resolvers = ide_buffer_get_symbol_resolvers (self->buffer);
  IDE_PTR_ARRAY_SET_FREE_FUNC (resolvers, g_object_unref);

  if (resolvers == NULL || resolvers->len == 0)
    return;

  task = ide_task_new (self, NULL, NULL, NULL);
  ide_task_set_source_tag (task, find_references_action);

  state = g_slice_new0 (FindReferences);
  state->location = ide_buffer_get_insert_location (self->buffer);
  state->active = g_ptr_array_ref (resolvers);
  state->references = g_list_store_new (IDE_TYPE_RANGE);
  ide_task_set_task_data (task, state, find_references_free);

  language_id = ide_buffer_get_language_id (self->buffer);

  for (guint i = 0; i < resolvers->len; i++)
    {
      IdeSymbolResolver *resolver = g_ptr_array_index (resolvers, i);

      ide_symbol_resolver_find_references_async (resolver,
                                                 state->location,
                                                 language_id,
                                                 NULL,
                                                 find_references_cb,
                                                 g_object_ref (task));
    }

  dialog = g_object_new (GBP_TYPE_CODEUI_RANGE_DIALOG,
                         "model", state->references,
                         "heading", _("Find References"),
                         NULL);

  state->dialog = g_object_ref_sink (GBP_CODEUI_RANGE_DIALOG (dialog));

  adw_dialog_present (dialog, GTK_WIDGET (self->page));

  IDE_EXIT;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpCodeuiEditorPageAddin, gbp_codeui_editor_page_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, gbp_codeui_editor_page_addin_init_action_group)
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_PAGE_ADDIN, editor_page_addin_iface_init))

static void
gbp_codeui_editor_page_addin_class_init (GbpCodeuiEditorPageAddinClass *klass)
{
}

static void
gbp_codeui_editor_page_addin_init (GbpCodeuiEditorPageAddin *self)
{
}
