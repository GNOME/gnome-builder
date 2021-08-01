/* gbp-editor-session-addin.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-editor-session-addin"

#include "config.h"

#include <libide-code.h>
#include <libide-gui.h>
#include <libide-editor.h>
#include <libide-threading.h>

#include "ide-editor-private.h"
#include "ide-gui-private.h"

#include "gbp-editor-session-addin.h"

struct _GbpEditorSessionAddin
{
  IdeObject parent_instance;
};

typedef struct
{
  gchar *uri;
  struct {
    gchar    *keyword;
    gboolean  case_sensitive;
    gboolean  regex_enabled;
    gboolean  at_word_boundaries;
  } search;
} Item;

static void
free_item (Item *item)
{
  g_clear_pointer (&item->uri, g_free);
  g_clear_pointer (&item->search.keyword, g_free);
  g_slice_free (Item, item);
}

static void
gbp_editor_session_addin_save_page_async (IdeSessionAddin     *addin,
                                          IdePage             *page,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  IdeBuffer *buffer = ide_editor_page_get_buffer (IDE_EDITOR_PAGE (page));
  GFile *file = ide_buffer_get_file (buffer);

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SESSION_ADDIN (addin));
  g_assert (IDE_IS_PAGE (page));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (addin, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_editor_session_addin_save_page_async);

  if (!ide_buffer_get_is_temporary (buffer))
    {
      GVariantDict state_dict;
      IdeEditorSearch *search = ide_editor_page_get_search (IDE_EDITOR_PAGE (page));
      GVariantDict search_dict;
      const char *search_keyword = ide_editor_search_get_search_text (search);
      g_autofree char *uri = g_file_get_uri (file);
      GVariant *v;

      g_variant_dict_init (&search_dict, NULL);
      g_variant_dict_insert (&search_dict, "search.keyword", "s", search_keyword ? search_keyword : "");
      g_variant_dict_insert (&search_dict, "search.at-word-boundaries", "b", ide_editor_search_get_at_word_boundaries (search));
      g_variant_dict_insert (&search_dict, "search.regex-enabled", "b", ide_editor_search_get_regex_enabled (search));
      g_variant_dict_insert (&search_dict, "search.case-sensitive", "b", ide_editor_search_get_case_sensitive (search));

      g_variant_dict_init (&state_dict, NULL);
      g_variant_dict_insert (&state_dict, "uri", "s", uri);
      g_variant_dict_insert_value (&state_dict, "search", g_variant_dict_end (&search_dict));

      v = g_variant_dict_end (&state_dict);
      g_debug ("Saved editor page: %s", g_variant_print (v, TRUE));

      ide_task_return_pointer (task, g_variant_take_ref (v), g_variant_unref);
    }
  else
    {
      ide_task_return_new_error (task,
                                 G_FILE_ERROR,
                                 G_FILE_ERROR_FAILED,
                                 "Can't save page as it's a temporary buffer");
    }

  IDE_EXIT;
}

static GVariant *
gbp_editor_session_addin_save_page_finish (IdeSessionAddin  *self,
                                           GAsyncResult     *result,
                                           GError          **error)
{
  g_assert (GBP_IS_EDITOR_SESSION_ADDIN (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
gbp_editor_session_addin_load_file_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  IdeBufferManager *bufmgr = (IdeBufferManager *)object;
  g_autoptr(IdeBuffer) buffer = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GbpEditorSessionAddin *self;
  IdeEditorPage *page;
  IdeEditorSearch *search;
  Item *item;

  g_assert (IDE_IS_BUFFER_MANAGER (bufmgr));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(buffer = ide_buffer_manager_load_file_finish (bufmgr, result, &error)))
    {
      g_warning ("Failed to load buffer: %s", error->message);

      ide_task_return_new_error (task,
                                 error->domain,
                                 error->code,
                                 "Couldn't load buffer: %s", error->message);

      return;
    }

  item = ide_task_get_task_data (task);
  self = ide_task_get_source_object (task);

  g_assert (item != NULL);
  g_assert (GBP_IS_EDITOR_SESSION_ADDIN (self));

  page = g_object_new (IDE_TYPE_EDITOR_PAGE,
                       "buffer", buffer,
                       "visible", TRUE,
                       NULL);

  search = ide_editor_page_get_search (page);

  ide_editor_search_set_search_text (search, item->search.keyword);
  ide_editor_search_set_at_word_boundaries (search, item->search.at_word_boundaries);
  ide_editor_search_set_case_sensitive (search, item->search.case_sensitive);
  ide_editor_search_set_regex_enabled (search, item->search.regex_enabled);

  ide_task_return_pointer (task, page, g_object_unref);
}

static void
gbp_editor_session_addin_restore_page_async (IdeSessionAddin     *addin,
                                             GVariant            *state,
                                             GCancellable        *cancellable,
                                             GAsyncReadyCallback  callback,
                                             gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  GVariantDict state_dict;
  g_autoptr(GVariant) extra = NULL;
  g_autoptr(GFile) file = NULL;
  IdeContext *context;
  IdeBufferManager *bufmgr;
  Item *item = NULL;

  g_assert (GBP_IS_EDITOR_SESSION_ADDIN (addin));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (g_variant_is_of_type (state, G_VARIANT_TYPE_VARDICT));

  task = ide_task_new (addin, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_editor_session_addin_restore_page_async);

  item = g_slice_new0 (Item);
  ide_task_set_task_data (task, item, free_item);

  g_variant_dict_init (&state_dict, state);
  g_variant_dict_lookup (&state_dict, "uri", "s", &item->uri);
  extra = g_variant_dict_lookup_value (&state_dict, "search", G_VARIANT_TYPE_VARDICT);
  g_assert (extra != NULL);
  g_assert (item->uri != NULL);

  IDE_TRACE_MSG ("Restoring editor page URI \"%s\"", item->uri);

  g_variant_lookup (extra, "search.keyword", "s", &item->search.keyword);
  g_variant_lookup (extra, "search.at-word-boundaries", "b", &item->search.at_word_boundaries);
  g_variant_lookup (extra, "search.case-sensitive", "b", &item->search.case_sensitive);
  g_variant_lookup (extra, "search.regex-enabled", "b", &item->search.regex_enabled);

  file = g_file_new_for_uri (item->uri);
  context = ide_object_get_context (IDE_OBJECT (addin));
  bufmgr = ide_buffer_manager_from_context (context);

  ide_buffer_manager_load_file_async (bufmgr,
                                      file,
                                      IDE_BUFFER_OPEN_FLAGS_NO_VIEW,
                                      NULL,
                                      ide_task_get_cancellable (task),
                                      gbp_editor_session_addin_load_file_cb,
                                      g_object_ref (task));
}

static IdePage *
gbp_editor_session_addin_restore_page_finish (IdeSessionAddin  *self,
                                              GAsyncResult     *result,
                                              GError          **error)
{
  g_assert (GBP_IS_EDITOR_SESSION_ADDIN (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static gboolean
gbp_editor_session_addin_can_save_page (IdeSessionAddin *addin,
                                        IdePage         *page)
{
  g_assert (GBP_IS_EDITOR_SESSION_ADDIN (addin));

  return IDE_IS_EDITOR_PAGE (page);
}

static char **
gbp_editor_session_addin_get_autosave_properties (IdeSessionAddin *addin)
{
  GStrvBuilder *builder = NULL;

  g_assert (GBP_IS_EDITOR_SESSION_ADDIN (addin));

  builder = g_strv_builder_new ();
  /* This is not an ideal property to pick, but the editor page file URI is burried
   * in the buffer property. In GTK 4 we'll likely be able to use GtkExpression to
   * really access the URI in the buffer.
   */
  g_strv_builder_add (builder, "buffer-file");

  return g_strv_builder_end (builder);
}

static void
session_addin_iface_init (IdeSessionAddinInterface *iface)
{
  iface->save_page_async = gbp_editor_session_addin_save_page_async;
  iface->save_page_finish = gbp_editor_session_addin_save_page_finish;
  iface->restore_page_async = gbp_editor_session_addin_restore_page_async;
  iface->restore_page_finish = gbp_editor_session_addin_restore_page_finish;
  iface->can_save_page = gbp_editor_session_addin_can_save_page;
  iface->get_autosave_properties = gbp_editor_session_addin_get_autosave_properties;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpEditorSessionAddin, gbp_editor_session_addin, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_SESSION_ADDIN, session_addin_iface_init))

static void
gbp_editor_session_addin_class_init (GbpEditorSessionAddinClass *klass)
{
}

static void
gbp_editor_session_addin_init (GbpEditorSessionAddin *self)
{
}
