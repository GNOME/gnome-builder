/* ide-snippet-storage.c
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

#define G_LOG_DOMAIN "ide-snippet-storage"

#include "config.h"

#include <libide-io.h>
#include <stdlib.h>
#include <string.h>

#include "ide-snippet-storage.h"

#define SNIPPETS_DIRECTORY "/org/gnome/builder/snippets/"

/**
 * SECTION:ide-snippet-storage
 * @title: IdeSnippetStorage
 * @short_description: storage and loading of snippets
 *
 * The #IdeSnippetStorage object manages parsing snippet files from disk.
 * To avoid creating lots of small allocations, it delays parsing of
 * snippets fully until necessary.
 *
 * To do this, mapped files are used and just enough information is
 * extracted to describe the snippets. Then snippets are inflated and
 * fully parsed when requested.
 *
 * In doing so, we can use #GStringChunk for the meta-data, and then only
 * create all the small strings when we inflate the snippet and its chunks.
 *
 * Since: 3.32
 */

struct _IdeSnippetStorage
{
  IdeObject     parent_instance;
  GStringChunk *strings;
  GArray       *infos;
  GPtrArray    *bytes;

  guint         loaded : 1;
};

typedef struct
{
  gchar *name;
  gchar *desc;
  gchar *scopes;
  const gchar *beginptr;
  const gchar *endptr;
} LoadState;

static void async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeSnippetStorage, ide_snippet_storage, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_iface_init))

static void
ide_snippet_storage_finalize (GObject *object)
{
  IdeSnippetStorage *self = (IdeSnippetStorage *)object;

  g_clear_pointer (&self->bytes, g_ptr_array_unref);
  g_clear_pointer (&self->strings, g_string_chunk_free);
  g_clear_pointer (&self->infos, g_array_unref);

  G_OBJECT_CLASS (ide_snippet_storage_parent_class)->finalize (object);
}

static void
ide_snippet_storage_class_init (IdeSnippetStorageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_snippet_storage_finalize;
}

static void
ide_snippet_storage_init (IdeSnippetStorage *self)
{
  self->strings = g_string_chunk_new (4096);
  self->infos = g_array_new (FALSE, FALSE, sizeof (IdeSnippetInfo));
  self->bytes = g_ptr_array_new_with_free_func ((GDestroyNotify)g_bytes_unref);
}

IdeSnippetStorage *
ide_snippet_storage_new (void)
{
  return g_object_new (IDE_TYPE_SNIPPET_STORAGE, NULL);
}

static gint
snippet_info_compare (gconstpointer a,
                      gconstpointer b)
{
  const IdeSnippetInfo *ai = a;
  const IdeSnippetInfo *bi = b;
  gint r;

  if (!(r = g_strcmp0 (ai->lang, bi->lang)))
    r = g_strcmp0 (ai->name, bi->name);

  return r;
}

static gboolean
str_starts_with (const gchar *str,
                 gsize        len,
                 const gchar *needle)
{
  gsize needle_len = strlen (needle);
  if (len < needle_len)
    return FALSE;
  return strncmp (str, needle, needle_len) == 0;
}

static void
flush_load_state (IdeSnippetStorage *self,
                  const gchar       *default_scope,
                  LoadState         *state)
{
  g_auto(GStrv) scopes = NULL;
  IdeSnippetInfo info = {0};
  gboolean needs_default = TRUE;

  if (state->name == NULL)
    goto cleanup;

  g_assert (state->beginptr);
  g_assert (state->endptr);
  g_assert (state->endptr > state->beginptr);

  if (state->scopes != NULL)
    scopes = g_strsplit (state->scopes, ",", 0);

  info.name = g_string_chunk_insert_const (self->strings, state->name);
  if (state->desc)
    info.desc = g_string_chunk_insert_const (self->strings, state->desc);

  info.begin = state->beginptr;
  info.len = state->endptr - state->beginptr;
  info.default_lang = g_string_chunk_insert_const (self->strings, default_scope);

  if (scopes != NULL)
    {
      for (guint i = 0; scopes[i] != NULL; i++)
        {
          g_strstrip (scopes[i]);
          if (g_strcmp0 (scopes[i], default_scope) == 0)
            needs_default = FALSE;
          info.lang = g_string_chunk_insert_const (self->strings, scopes[i]);
          g_array_append_val (self->infos, info);
        }
    }

  if (needs_default && default_scope)
    {
      info.lang = g_string_chunk_insert_const (self->strings, default_scope);
      g_array_append_val (self->infos, info);
    }

cleanup:
  /* Leave name in-tact */
  g_clear_pointer (&state->desc, g_free);
  g_clear_pointer (&state->scopes, g_free);
}

void
ide_snippet_storage_add (IdeSnippetStorage *self,
                         const gchar       *default_scope,
                         GBytes            *bytes)
{
  IdeLineReader reader;
  LoadState state = {0};
  const gchar *data;
  const gchar *line;
  gsize line_len;
  gsize len;
  gboolean found_data = FALSE;

  g_return_if_fail (IDE_IS_SNIPPET_STORAGE (self));
  g_return_if_fail (bytes != NULL);

  g_ptr_array_add (self->bytes, g_bytes_ref (bytes));

  data = g_bytes_get_data (bytes, &len);
  state.beginptr = data;

  ide_line_reader_init (&reader, (gchar *)data, len);

#define COPY_AFTER(dst, str) \
  G_STMT_START { \
    g_free (state.dst); \
    state.dst = g_strstrip(g_strndup(line + strlen(str), line_len - strlen(str))); \
  } G_STMT_END

  while ((line = ide_line_reader_next (&reader, &line_len)))
    {
      if (str_starts_with (line, line_len, "snippet "))
        {
          if (state.name && found_data)
            flush_load_state (self, default_scope, &state);
          state.beginptr = line;
          COPY_AFTER (name, "snippet ");
          found_data = FALSE;
        }
      else if (str_starts_with (line, line_len, "- desc "))
        {
          COPY_AFTER (desc, "- desc");
        }
      else if (str_starts_with (line, line_len, "- scope "))
        {
          /* We could have repeated scopes, so if we get a folloup -scope, we need
           * to flush the previous and then update beginptr/endptr.
           */
          if (state.name && found_data)
            flush_load_state (self, default_scope, &state);
          COPY_AFTER (scopes, "- scope ");
          found_data = FALSE;
        }
      else
        {
          found_data = TRUE;
        }

      state.endptr = line + line_len;
    }

#undef COPY_AFTER

  flush_load_state (self, default_scope, &state);

  g_array_sort (self->infos, snippet_info_compare);

  g_clear_pointer (&state.name, g_free);
  g_clear_pointer (&state.desc, g_free);
  g_clear_pointer (&state.scopes, g_free);
}

/**
 * ide_snippet_storage_foreach:
 * @self: a #IdeSnippetStorage
 * @foreach: (scope call): the closure to call for each info
 * @user_data: closure data for @foreach
 *
 * This will call @foreach for every item that has been loaded.
 *
 * Since: 3.32
 */
void
ide_snippet_storage_foreach (IdeSnippetStorage        *self,
                             IdeSnippetStorageForeach  foreach,
                             gpointer                  user_data)
{
  g_return_if_fail (IDE_IS_SNIPPET_STORAGE (self));
  g_return_if_fail (foreach != NULL);

  for (guint i = 0; i < self->infos->len; i++)
    {
      const IdeSnippetInfo *info = &g_array_index (self->infos, IdeSnippetInfo, i);

      foreach (self, info, user_data);
    }
}

static gint
query_compare (gconstpointer a,
               gconstpointer b)
{
  const IdeSnippetInfo *ai = a;
  const IdeSnippetInfo *bi = b;
  gboolean r;

  if (!(r = g_strcmp0 (ai->lang, bi->lang)))
    {
      if (g_str_has_prefix (bi->name, ai->name))
        return 0;
      r = g_strcmp0 (ai->name, bi->name);
    }

  return r;
}

/**
 * ide_snippet_storage_query:
 * @self: a #IdeSnippetStorage
 * @lang: language to query
 * @prefix: (nullable): prefix for query
 * @foreach: (scope call): the closure to call for each match
 * @user_data: closure data for @foreach
 *
 * This will call @foreach for every info that matches the query. This is
 * useful when building autocompletion lists based on word prefixes.
 *
 * Since: 3.32
 */
void
ide_snippet_storage_query (IdeSnippetStorage        *self,
                           const gchar              *lang,
                           const gchar              *prefix,
                           IdeSnippetStorageForeach  foreach,
                           gpointer                  user_data)
{
  IdeSnippetInfo key = { 0 };
  const IdeSnippetInfo *endptr;
  const IdeSnippetInfo *base;

  g_return_if_fail (IDE_IS_SNIPPET_STORAGE (self));
  g_return_if_fail (lang != NULL);
  g_return_if_fail (foreach != NULL);

  if (self->infos->len == 0)
    return;

  if (prefix == NULL)
    prefix = "";

  key.lang = lang;
  key.name = prefix;

  base = bsearch (&key,
                  self->infos->data,
                  self->infos->len,
                  sizeof (IdeSnippetInfo),
                  query_compare);

  if (base == NULL)
    return;

  while ((gpointer)base > (gpointer)self->infos->data)
    {
      const IdeSnippetInfo *prev = base - 1;

      if (base->lang == prev->lang && g_str_has_prefix (prev->name, prefix))
        base = prev;
      else
        break;
    }

  endptr = &g_array_index (self->infos, IdeSnippetInfo, self->infos->len);

  for (; base < endptr; base++)
    {
      if (g_strcmp0 (base->lang, lang) != 0)
        break;

      if (!g_str_has_prefix (base->name, prefix))
        break;

      foreach (self, base, user_data);
    }
}

static void
ide_snippet_storage_init_async (GAsyncInitable      *initable,
                                gint                 io_priority,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  IdeSnippetStorage *self = (IdeSnippetStorage *)initable;
  g_autofree gchar *local = NULL;
  g_autoptr(GTask) task = NULL;
  g_autoptr(GDir) dir = NULL;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) names = NULL;

  g_return_if_fail (IDE_IS_SNIPPET_STORAGE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_snippet_storage_init_async);

  if (self->loaded)
    {
      g_task_return_boolean (task, TRUE);
      return;
    }

  self->loaded = TRUE;

  if (!(names = g_resources_enumerate_children (SNIPPETS_DIRECTORY,
                                                G_RESOURCE_LOOKUP_FLAGS_NONE,
                                                &error)))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  for (guint i = 0; names[i] != NULL; i++)
    {
      g_autofree gchar *path = g_build_filename (SNIPPETS_DIRECTORY, names[i], NULL);
      g_autoptr(GBytes) bytes = g_resources_lookup_data (path, 0, NULL);
      g_autofree gchar *base = NULL;
      const gchar *dot;

      if (bytes == NULL)
        continue;

      if ((dot = strrchr (names[i], '.')))
        base = g_strndup (names[i], dot - names[i]);

      ide_snippet_storage_add (self, base, bytes);
    }

  /* TODO: Do this async */

  local = g_build_filename (g_get_user_config_dir (),
                            "gnome-builder",
                            "snippets",
                            NULL);

  if ((dir = g_dir_open (local, 0, NULL)))
    {
      const gchar *name;

      while ((name = g_dir_read_name (dir)))
        {
          g_autofree gchar *path = g_build_filename (local, name, NULL);
          g_autoptr(GMappedFile) mf = NULL;
          g_autoptr(GBytes) bytes = NULL;
          g_autofree gchar *base = NULL;
          const gchar *dot;

          if (!(mf = g_mapped_file_new (path, FALSE, &error)))
            {
              g_message ("%s", error->message);
              g_clear_error (&error);
              continue;
            }

          bytes = g_mapped_file_get_bytes (mf);

          if ((dot = strrchr (name, '.')))
            base = g_strndup (name, dot - name);

          ide_snippet_storage_add (self, base, bytes);
        }
    }

  g_task_return_boolean (task, TRUE);
}

static gboolean
ide_snippet_storage_init_finish (GAsyncInitable  *initable,
                                 GAsyncResult    *result,
                                 GError         **error)
{
  g_return_val_if_fail (IDE_IS_SNIPPET_STORAGE (initable), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = ide_snippet_storage_init_async;
  iface->init_finish = ide_snippet_storage_init_finish;
}

/**
 * ide_snippet_storage_from_context:
 * @context: an #IdeContext
 *
 * Gets the snippet storage for the context.
 *
 * Returns: (transfer none): an #IdeSnippetStorage
 *
 * Since: 3.32
 */
IdeSnippetStorage *
ide_snippet_storage_from_context (IdeContext *context)
{
  IdeSnippetStorage *ret;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  /* Give back a borrowed reference instead of full */
  ret = ide_object_ensure_child_typed (IDE_OBJECT (context), IDE_TYPE_SNIPPET_STORAGE);
  g_object_unref (ret);

  return ret;
}
