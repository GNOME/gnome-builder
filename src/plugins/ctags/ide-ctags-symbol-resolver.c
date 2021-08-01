/* ide-ctags-symbol-resolver.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-ctags-symbol-resolver"

#include <errno.h>
#include <glib/gi18n.h>
#include <libide-code.h>

#include "ide-ctags-service.h"
#include "ide-ctags-symbol-node.h"
#include "ide-ctags-symbol-resolver.h"
#include "ide-ctags-symbol-tree.h"
#include "ide-ctags-util.h"

struct _IdeCtagsSymbolResolver
{
  IdeObject parent_instance;
};

typedef struct
{
  IdeCtagsIndexEntry *entry;
  gchar              *buffer_text;
  GMappedFile        *mapped;
} LookupSymbol;

enum {
  PROP_0,
  LAST_PROP
};

static void symbol_resolver_iface_init (IdeSymbolResolverInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeCtagsSymbolResolver,
                         ide_ctags_symbol_resolver,
                         IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_SYMBOL_RESOLVER,
                                                symbol_resolver_iface_init))

static void
lookup_symbol_free (gpointer data)
{
  LookupSymbol *lookup = data;

  ide_ctags_index_entry_free (lookup->entry);
  g_free (lookup->buffer_text);
  if (lookup->mapped)
    g_mapped_file_unref (lookup->mapped);
  g_slice_free (LookupSymbol, lookup);
}

static IdeSymbol *
create_symbol (IdeCtagsSymbolResolver   *self,
               const IdeCtagsIndexEntry *entry,
               gint                      line,
               gint                      line_offset,
               gint                      offset)
{
  g_autoptr(IdeLocation) loc = NULL;
  g_autoptr(GFile) gfile = NULL;

  gfile = g_file_new_for_path (entry->path);
  loc = ide_location_new (gfile, line, line_offset);

  return ide_symbol_new (entry->name,
                         ide_ctags_index_entry_kind_to_symbol_kind (entry->kind),
                         0, loc, loc);
}

static gboolean
is_regex (const gchar *pattern)
{
  return (pattern != NULL) && (*pattern == '/');
}

static gchar *
extract_regex (const gchar *pattern)
{
  const gchar *input = pattern;
  GString *str;
  const gchar *endptr;

  if (!pattern || *pattern != '/')
    goto failure;

  endptr = strrchr (pattern, ';');
  if (!endptr || endptr <= input)
    goto failure;

  endptr--;

  if (*endptr != '/')
    goto failure;

  pattern++;

  if (endptr < pattern)
    goto failure;

  str = g_string_new (NULL);

  for (const gchar *iter = pattern; iter < endptr; iter = g_utf8_next_char (iter))
    {
      gunichar ch = g_utf8_get_char (iter);

      switch (ch)
        {
        case '(':
        case ')':
        case '*':
          g_string_append_printf (str, "\\%c", ch);
          break;

        default:
          g_string_append_unichar (str, ch);
          break;
        }
    }

  return g_string_free (str, FALSE);

failure:
  return g_regex_escape_string (input, -1);
}

static void
calculate_offset (const gchar *data,
                  gsize        length,
                  gsize        offset,
                  gint        *line,
                  gint        *line_offset)
{
  IdeLineReader reader;
  gsize last_pos = 0;
  gint line_count = 0;

  *line = 0;
  *line_offset = 0;

  ide_line_reader_init (&reader, (gchar *)data, length);

  while (reader.pos < (gssize)offset)
    {
      gsize line_len;

      if (!ide_line_reader_next (&reader, &line_len))
        break;

      last_pos = reader.pos;
      line_count++;
    }

  *line = line_count;
  /*
   * FIXME: Technically we need to get the line offset in characters.
   *        So this isn't going to get the right answer if we have
   *        multi-byte characters.
   */
  *line_offset = offset - last_pos;
}

static void
regex_worker (IdeTask      *task,
              gpointer      source_object,
              gpointer      task_data,
              GCancellable *cancellable)
{
  IdeCtagsSymbolResolver *self = source_object;
  LookupSymbol *lookup = task_data;
  g_autoptr(GRegex) regex = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *pattern = NULL;
  g_autoptr(GMatchInfo) match_info = NULL;
  const gchar *data;
  gsize length;

  g_assert (IDE_IS_TASK (task));
  g_assert (lookup != NULL);

  if (lookup->buffer_text == NULL)
    {
      lookup->mapped = g_mapped_file_new (lookup->entry->path, FALSE, &error);

      if (lookup->mapped == NULL)
        {
          ide_task_return_error (task, g_steal_pointer (&error));
          return;
        }

      data = g_mapped_file_get_contents (lookup->mapped);
      length = g_mapped_file_get_length (lookup->mapped);
    }
  else
    {
      data = lookup->buffer_text;
      length = strlen (data);
    }

  pattern = extract_regex (lookup->entry->pattern);

  IDE_TRACE_MSG ("Looking for regex pattern: %s", pattern);

  if (!(regex = g_regex_new (pattern, G_REGEX_MULTILINE, 0, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_regex_match_full (regex, data, length, 0, 0, &match_info, &error);

  while (g_match_info_matches (match_info))
    {
      gint begin = 0;
      gint end = 0;

      if (g_match_info_fetch_pos (match_info, 0, &begin, &end))
        {
          g_autoptr(IdeSymbol) symbol = NULL;
          gint line = 0;
          gint line_offset = 0;

          calculate_offset (data, length, begin, &line, &line_offset);

          symbol = create_symbol (self, lookup->entry, line, line_offset, begin);
          ide_task_return_pointer (task,
                                   g_steal_pointer (&symbol),
                                   (GDestroyNotify)g_object_unref);

          return;
        }
    }

  if (error != NULL)
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_FOUND,
                               "Failed to locate symbol \"%s\"",
                               lookup->entry->name);
}

static gboolean
is_linenum (const gchar *pattern)
{
  if (!pattern)
    return FALSE;

  return g_ascii_isdigit (*pattern);
}

static void
ide_ctags_symbol_resolver_lookup_symbol_async (IdeSymbolResolver   *resolver,
                                               IdeLocation   *location,
                                               GCancellable        *cancellable,
                                               GAsyncReadyCallback  callback,
                                               gpointer             user_data)
{
  IdeCtagsSymbolResolver *self = (IdeCtagsSymbolResolver *)resolver;
  g_autoptr(IdeCtagsService) service = NULL;
  IdeContext *context;
  IdeBufferManager *bufmgr;
  g_autofree gchar *keyword = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GPtrArray) indexes = NULL;
  const gchar * const *allowed;
  const gchar *lang_id = NULL;
  GtkSourceLanguage *language;
  GFile *file;
  IdeBuffer *buffer;
  GtkTextIter iter;
  gsize i;
  guint line;
  guint line_offset;

  g_assert (IDE_IS_CTAGS_SYMBOL_RESOLVER (self));
  g_assert (location != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);

  file = ide_location_get_file (location);
  line = ide_location_get_line (location);
  line_offset = ide_location_get_line_offset (location);

  if (!(context = ide_object_get_context (IDE_OBJECT (self))) ||
      !(service = ide_object_get_child_typed (IDE_OBJECT (context), IDE_TYPE_CTAGS_SERVICE)))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Service is not loaded. Likely no project was loaded");
      return;
    }

  indexes = ide_ctags_service_get_indexes (service);

  bufmgr = ide_buffer_manager_from_context (context);
  buffer = ide_buffer_manager_find_buffer (bufmgr, file);

  if (!buffer)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_FOUND,
                                 "The document buffer was not available.");
      return;
    }

  if ((language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer))))
    lang_id = gtk_source_language_get_id (language);
  allowed = ide_ctags_get_allowed_suffixes (lang_id);

  gtk_text_buffer_get_iter_at_line_offset (GTK_TEXT_BUFFER (buffer), &iter, line, line_offset);

  keyword = ide_buffer_get_word_at_iter (buffer, &iter);

  for (i = 0; i < indexes->len; i++)
    {
      IdeCtagsIndex *index = g_ptr_array_index (indexes, i);
      const IdeCtagsIndexEntry *entries;
      gsize count;
      gsize j;

      entries = ide_ctags_index_lookup (index, keyword, &count);

      for (j = 0; j < count; j++)
        {
          const IdeCtagsIndexEntry *entry = &entries [j];
          IdeCtagsIndexEntry *copy;
          LookupSymbol *lookup;
          g_autoptr(GFile) other_file = NULL;
          IdeBuffer *other_buffer;
          gchar *path;

          if (!ide_ctags_is_allowed (entry, allowed))
            continue;

          /*
           * Adjust the filename in our copy to be the full path.
           * Sort of grabbing at internals here, but hey, we are
           * our own plugin.
           */
          copy = ide_ctags_index_entry_copy (entry);
          path = ide_ctags_index_resolve_path (index, copy->path);
          g_free ((gchar *)copy->path);
          copy->path = path;

          lookup = g_slice_new0 (LookupSymbol);
          lookup->entry = copy;

          other_file = g_file_new_for_path (copy->path);

          if ((other_buffer = ide_buffer_manager_find_buffer (bufmgr, other_file)))
            {
              GtkTextIter begin, end;

              gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (other_buffer), &begin, &end);
              lookup->buffer_text = gtk_text_iter_get_slice (&begin, &end);
            }

          /*
           * Since all we have is a regex pattern, we need to open
           * the target file and run a GRegex. Best to do that
           * on a worker thread.
           */
          ide_task_set_task_data (task, lookup, lookup_symbol_free);

          if (is_regex (entry->pattern))
            {
              ide_task_run_in_thread (task, regex_worker);
              return;
            }
          else if (is_linenum (entry->pattern))
            {
              g_autoptr(IdeSymbol) symbol = NULL;
              gint64 parsed;

              parsed = g_ascii_strtoll (entry->pattern, NULL, 10);

              if (((parsed == 0) && (errno == ERANGE)) || (parsed > G_MAXINT) || (parsed < 0))
                goto failure;

              symbol = create_symbol (self, entry, parsed, 0, 0);
              ide_task_return_pointer (task,
                                       g_steal_pointer (&symbol),
                                       (GDestroyNotify)g_object_unref);
              return;
            }
        }
    }

failure:
  ide_task_return_new_error (task,
                             G_IO_ERROR,
                             G_IO_ERROR_NOT_FOUND,
                             "Failed to locate symbol \"%s\"",
                             keyword);
}

static IdeSymbol *
ide_ctags_symbol_resolver_lookup_symbol_finish (IdeSymbolResolver  *resolver,
                                                GAsyncResult       *result,
                                                GError            **error)
{
  IdeTask *task = (IdeTask *)result;

  g_assert (IDE_IS_CTAGS_SYMBOL_RESOLVER (resolver));
  g_assert (IDE_IS_TASK (task));

  return ide_task_propagate_pointer (task, error);
}

typedef struct
{
  GPtrArray *indexes;
  GFile *file;
} TreeResolverState;

static void
tree_resolver_state_free (gpointer data)
{
  TreeResolverState *state = data;

  if (state != NULL)
    {
      g_clear_pointer (&state->indexes, g_ptr_array_unref);
      g_clear_object (&state->file);
      g_slice_free (TreeResolverState, state);
    }
}

static gboolean
maybe_attach_to_parent (IdeCtagsSymbolNode       *node,
                        const IdeCtagsIndexEntry *entry,
                        GHashTable               *parents)
{
  g_assert (IDE_IS_CTAGS_SYMBOL_NODE (node));
  g_assert (parents != NULL);

  if (entry->keyval != NULL)
    {
      g_auto(GStrv) parts = g_strsplit (entry->keyval, "\t", 0);

      for (guint i = 0; parts[i] != NULL; i++)
        {
          IdeCtagsSymbolNode *parent;

          if (NULL != (parent = g_hash_table_lookup (parents, parts[i])))
            {
              ide_ctags_symbol_node_take_child (parent, node);
              return TRUE;
            }
        }
    }

  return FALSE;
}

static gchar *
make_parent_key (const IdeCtagsIndexEntry *entry)
{
  switch (entry->kind)
    {
    case IDE_CTAGS_INDEX_ENTRY_CLASS_NAME:
      return g_strdup_printf ("class:%s", entry->name);

    case IDE_CTAGS_INDEX_ENTRY_UNION:
      return g_strdup_printf ("union:%s", entry->name);

    case IDE_CTAGS_INDEX_ENTRY_STRUCTURE:
      return g_strdup_printf ("struct:%s", entry->name);

    case IDE_CTAGS_INDEX_ENTRY_IMPORT:
      return g_strdup_printf ("package:%s", entry->name);

    case IDE_CTAGS_INDEX_ENTRY_FUNCTION:
    case IDE_CTAGS_INDEX_ENTRY_MEMBER:
      {
        const gchar *colon;

        /*
         * If there is a keyval (like class:foo, then we strip the
         * key, and make a key like type:parent.name.
         */
        if (entry->keyval && NULL != (colon = strchr (entry->keyval, ':')))
          return g_strdup_printf ("function:%s.%s", colon + 1, entry->name);
        return g_strdup_printf ("function:%s", entry->name);
      }

    case IDE_CTAGS_INDEX_ENTRY_ENUMERATION_NAME:
      return g_strdup_printf ("enum:%s", entry->name);

    case IDE_CTAGS_INDEX_ENTRY_VARIABLE:
    case IDE_CTAGS_INDEX_ENTRY_PROTOTYPE:
    case IDE_CTAGS_INDEX_ENTRY_DEFINE:
    case IDE_CTAGS_INDEX_ENTRY_TYPEDEF:
    case IDE_CTAGS_INDEX_ENTRY_FILE_NAME:
    case IDE_CTAGS_INDEX_ENTRY_ANCHOR:
    case IDE_CTAGS_INDEX_ENTRY_ENUMERATOR:
    default:
      break;
    }

  return NULL;
}

static void
ide_ctags_symbol_resolver_get_symbol_tree_worker (IdeTask      *task,
                                                  gpointer      source_object,
                                                  gpointer      task_data,
                                                  GCancellable *cancellable)
{
  IdeCtagsSymbolResolver *self = source_object;
  TreeResolverState *state = task_data;
  g_autoptr(GPtrArray) ar = NULL;
  g_autoptr(GFile) parent = NULL;
  g_autofree gchar *parent_path = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CTAGS_SYMBOL_RESOLVER (self));
  g_assert (IDE_IS_TASK (task));
  g_assert (state != NULL);
  g_assert (G_IS_FILE (state->file));
  g_assert (state->indexes != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  parent = g_file_get_parent (state->file);
  parent_path = g_file_get_path (parent);

  /*
   * Ctags does not give us enough information to create a tree, so our
   * symbols will not have a hierarchy.
   */
  ar = g_ptr_array_new_with_free_func (g_object_unref);

  for (guint i = 0; i < state->indexes->len; i++)
    {
      IdeCtagsIndex *index = g_ptr_array_index (state->indexes, i);
      const gchar *base_path = ide_ctags_index_get_path_root (index);
      g_autoptr(GFile) base_dir = NULL;
      g_autoptr(GPtrArray) entries = NULL;
      g_autofree gchar *relative_path = NULL;
      g_autoptr(GHashTable) keymap = NULL;
      g_autoptr(GPtrArray) tmp = NULL;

      if (!g_str_has_prefix (parent_path, base_path))
        continue;

      base_dir = g_file_new_for_path (base_path);
      relative_path = g_file_get_relative_path (base_dir, state->file);

      /* Shouldn't happen, but be safe */
      if G_UNLIKELY (relative_path == NULL)
        continue;

      /* We use keymap to find the parent for things like class:Foo */
      keymap = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
      entries = ide_ctags_index_find_with_path (index, relative_path);
      tmp = g_ptr_array_new ();

      /*
       * We have to build the items in two steps incase the parent for
       * an item comes after the child. Once we have the parent names
       * inflated into the hashtable, we can resolve them and build the
       * final tree.
       */

      for (guint j = 0; j < entries->len; j++)
        {
          const IdeCtagsIndexEntry *entry = g_ptr_array_index (entries, j);
          g_autoptr(IdeCtagsSymbolNode) node = NULL;

          switch (entry->kind)
            {
            case IDE_CTAGS_INDEX_ENTRY_CLASS_NAME:
            case IDE_CTAGS_INDEX_ENTRY_UNION:
            case IDE_CTAGS_INDEX_ENTRY_STRUCTURE:
            case IDE_CTAGS_INDEX_ENTRY_TYPEDEF:
            case IDE_CTAGS_INDEX_ENTRY_MEMBER:
            case IDE_CTAGS_INDEX_ENTRY_FUNCTION:
            case IDE_CTAGS_INDEX_ENTRY_VARIABLE:
            case IDE_CTAGS_INDEX_ENTRY_PROTOTYPE:
            case IDE_CTAGS_INDEX_ENTRY_DEFINE:
            case IDE_CTAGS_INDEX_ENTRY_IMPORT:
            case IDE_CTAGS_INDEX_ENTRY_ENUMERATION_NAME:
              node = ide_ctags_symbol_node_new (self, index, entry);
              break;

            case IDE_CTAGS_INDEX_ENTRY_FILE_NAME:
            case IDE_CTAGS_INDEX_ENTRY_ANCHOR:
            case IDE_CTAGS_INDEX_ENTRY_ENUMERATOR:
            default:
              break;
            }

          if (node != NULL)
            {
              gchar *key = make_parent_key (entry);
              if (key != NULL)
                g_hash_table_insert (keymap, key, node);
              g_ptr_array_add (tmp, g_steal_pointer (&node));
            }
        }

      /*
       * Now go resolve parents and build the tree.
       */

      for (guint j = 0; j < tmp->len; j++)
        {
          IdeCtagsSymbolNode *node = g_ptr_array_index (tmp, j);
          const IdeCtagsIndexEntry *entry = ide_ctags_symbol_node_get_entry (node);

          if (!maybe_attach_to_parent (node, entry, keymap))
            g_ptr_array_add (ar, node);
        }
    }

  ide_task_return_pointer (task, ide_ctags_symbol_tree_new (g_steal_pointer (&ar)), g_object_unref);

  IDE_EXIT;
}

static void
ide_ctags_symbol_resolver_get_symbol_tree_async (IdeSymbolResolver   *resolver,
                                                 GFile               *file,
                                                 GBytes              *contents,
                                                 GCancellable        *cancellable,
                                                 GAsyncReadyCallback  callback,
                                                 gpointer             user_data)
{
  IdeCtagsSymbolResolver *self = (IdeCtagsSymbolResolver *)resolver;
  TreeResolverState *state;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GPtrArray) indexes = NULL;
  g_autoptr(IdeCtagsService) service = NULL;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_CTAGS_SYMBOL_RESOLVER (self));
  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_ctags_symbol_resolver_get_symbol_tree_async);

  if (!(context = ide_object_get_context (IDE_OBJECT (self))) ||
      !(service = ide_object_get_child_typed (IDE_OBJECT (context), IDE_TYPE_CTAGS_SERVICE)))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "No ctags service is loaded, likely no project was loaded");
      return;
    }

  indexes = ide_ctags_service_get_indexes (service);

  if (indexes == NULL || indexes->len == 0)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "No ctags indexes are loaded");
      IDE_EXIT;
    }

  state = g_slice_new0 (TreeResolverState);
  state->file = g_object_ref (file);
  state->indexes = g_ptr_array_new_with_free_func (g_object_unref);

  /*
   * We make a copy of the indexes so that we can access them in a thread.
   * The container is not mutable, but the indexes are thread safe in that
   * they don't do mutation after creation.
   */
  for (guint i = 0; i < indexes->len; i++)
    {
      IdeCtagsIndex *index = g_ptr_array_index (indexes, i);

      g_ptr_array_add (state->indexes, g_object_ref (index));
    }

  ide_task_set_task_data (task, state, tree_resolver_state_free);
  ide_task_run_in_thread (task, ide_ctags_symbol_resolver_get_symbol_tree_worker);

  IDE_EXIT;
}

static IdeSymbolTree *
ide_ctags_symbol_resolver_get_symbol_tree_finish (IdeSymbolResolver  *resolver,
                                                  GAsyncResult       *result,
                                                  GError            **error)
{
  IdeSymbolTree *ret;

  IDE_ENTRY;

  g_assert (IDE_IS_CTAGS_SYMBOL_RESOLVER (resolver));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_ctags_symbol_resolver_class_init (IdeCtagsSymbolResolverClass *klass)
{
}

static void
ide_ctags_symbol_resolver_init (IdeCtagsSymbolResolver *resolver)
{
}

static void
symbol_resolver_iface_init (IdeSymbolResolverInterface *iface)
{
  iface->lookup_symbol_async = ide_ctags_symbol_resolver_lookup_symbol_async;
  iface->lookup_symbol_finish = ide_ctags_symbol_resolver_lookup_symbol_finish;
  iface->get_symbol_tree_async = ide_ctags_symbol_resolver_get_symbol_tree_async;
  iface->get_symbol_tree_finish = ide_ctags_symbol_resolver_get_symbol_tree_finish;
}

void
ide_ctags_symbol_resolver_get_location_async (IdeCtagsSymbolResolver   *self,
                                              IdeCtagsIndex            *index,
                                              const IdeCtagsIndexEntry *entry,
                                              GCancellable             *cancellable,
                                              GAsyncReadyCallback       callback,
                                              gpointer                  user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GFile) other_file = NULL;
  IdeBuffer *other_buffer = NULL;
  IdeCtagsIndexEntry *copy;
  IdeBufferManager *bufmgr;
  LookupSymbol *lookup;
  IdeContext *context;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_CTAGS_SYMBOL_RESOLVER (self));
  g_return_if_fail (entry != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (self));
  bufmgr = ide_buffer_manager_from_context (context);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_ctags_symbol_resolver_get_location_async);

  if (is_linenum (entry->pattern))
    {
      g_autoptr(IdeSymbol) symbol = NULL;
      gint64 parsed;

      parsed = g_ascii_strtoll (entry->pattern, NULL, 10);

      if (((parsed == 0) && (errno == ERANGE)) || (parsed > G_MAXINT) || (parsed < 0))
        goto not_a_number;

      symbol = create_symbol (self, entry, parsed, 0, 0);
      ide_task_return_pointer (task,
                               g_steal_pointer (&symbol),
                               (GDestroyNotify)g_object_unref);

      IDE_EXIT;
    }

not_a_number:

  if (!is_regex (entry->pattern))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Failed to decode jump in ctag entry");
      IDE_EXIT;
    }

  /*
   * Adjust the filename in our copy to be the full path.
   * Sort of grabbing at internals here, but hey, we are
   * our own plugin.
   */
  copy = ide_ctags_index_entry_copy (entry);
  g_free ((gchar *)copy->path);
  copy->path = ide_ctags_index_resolve_path (index, entry->path);

  lookup = g_slice_new0 (LookupSymbol);
  lookup->entry = copy;

  other_file = g_file_new_for_path (copy->path);

  if (NULL != (other_buffer = ide_buffer_manager_find_buffer (bufmgr, other_file)))
    {
      GtkTextIter begin, end;

      gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (other_buffer), &begin, &end);
      lookup->buffer_text = gtk_text_iter_get_slice (&begin, &end);
    }

  ide_task_set_task_data (task, lookup, lookup_symbol_free);
  ide_task_run_in_thread (task, regex_worker);

  IDE_EXIT;
}

IdeLocation *
ide_ctags_symbol_resolver_get_location_finish (IdeCtagsSymbolResolver  *self,
                                               GAsyncResult            *result,
                                               GError                 **error)
{
  g_autoptr(IdeSymbol) symbol = NULL;
  IdeLocation *ret = NULL;

  g_return_val_if_fail (IDE_IS_CTAGS_SYMBOL_RESOLVER (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  symbol = ide_task_propagate_pointer (IDE_TASK (result), error);

  if (symbol != NULL)
    {
      if ((ret = ide_symbol_get_location (symbol)))
        g_object_ref (ret);
      else
        g_set_error (error,
                     G_IO_ERROR,
                     G_IO_ERROR_NOT_FOUND,
                     "Failed to locate symbol location");
    }

  return ret;
}
