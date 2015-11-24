/* ide-ctags-symbol-resolver.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include <errno.h>
#include <glib/gi18n.h>

#include "ide-ctags-service.h"
#include "ide-ctags-symbol-resolver.h"
#include "ide-ctags-util.h"
#include "ide-internal.h"
#include "ide-line-reader.h"
#include "ide-symbol.h"

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

G_DEFINE_DYNAMIC_TYPE_EXTENDED (IdeCtagsSymbolResolver,
                                ide_ctags_symbol_resolver,
                                IDE_TYPE_OBJECT,
                                0,
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

static IdeSymbolKind
transform_kind (IdeCtagsIndexEntryKind kind)
{
  switch (kind)
    {
    case IDE_CTAGS_INDEX_ENTRY_TYPEDEF:
    case IDE_CTAGS_INDEX_ENTRY_PROTOTYPE:
      /* bit of an impedenece mismatch */
    case IDE_CTAGS_INDEX_ENTRY_CLASS_NAME:
      return IDE_SYMBOL_CLASS;

    case IDE_CTAGS_INDEX_ENTRY_ENUMERATOR:
      return IDE_SYMBOL_ENUM;

    case IDE_CTAGS_INDEX_ENTRY_ENUMERATION_NAME:
      return IDE_SYMBOL_ENUM_VALUE;

    case IDE_CTAGS_INDEX_ENTRY_FUNCTION:
      return IDE_SYMBOL_FUNCTION;

    case IDE_CTAGS_INDEX_ENTRY_MEMBER:
      return IDE_SYMBOL_FIELD;

    case IDE_CTAGS_INDEX_ENTRY_STRUCTURE:
      return IDE_SYMBOL_STRUCT;

    case IDE_CTAGS_INDEX_ENTRY_UNION:
      return IDE_SYMBOL_UNION;

    case IDE_CTAGS_INDEX_ENTRY_VARIABLE:
      return IDE_SYMBOL_VARIABLE;

    case IDE_CTAGS_INDEX_ENTRY_ANCHOR:
    case IDE_CTAGS_INDEX_ENTRY_DEFINE:
    case IDE_CTAGS_INDEX_ENTRY_FILE_NAME:
    default:
      return IDE_SYMBOL_NONE;
    }
}

static IdeSymbol *
create_symbol (IdeCtagsSymbolResolver   *self,
               const IdeCtagsIndexEntry *entry,
               gint                      line,
               gint                      line_offset,
               gint                      offset)
{
  g_autoptr(IdeSourceLocation) loc = NULL;
  g_autoptr(GFile) gfile = NULL;
  g_autoptr(IdeFile) file = NULL;
  IdeContext *context;

  context = ide_object_get_context (IDE_OBJECT (self));
  gfile = g_file_new_for_path (entry->path);
  file = g_object_new (IDE_TYPE_FILE,
                       "file", gfile,
                       "context", context,
                       NULL);
  loc = ide_source_location_new (file, line, line_offset, offset);

  return ide_symbol_new (entry->name, transform_kind (entry->kind), 0, loc, loc, loc);

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

  return g_strdelimit (g_strndup (pattern, endptr - pattern), "()", '.');

failure:
  return g_strdup (input);
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

  while (reader.pos < offset)
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
regex_worker (GTask        *task,
              gpointer      source_object,
              gpointer      task_data,
              GCancellable *cancellable)
{
  IdeCtagsSymbolResolver *self = source_object;
  LookupSymbol *lookup = task_data;
  g_autoptr(GMappedFile) mapped = NULL;
  g_autoptr(GRegex) regex = NULL;
  g_autofree gchar *pattern = NULL;
  GMatchInfo *match_info = NULL;
  GError *error = NULL;
  const gchar *data;
  gsize length;

  g_assert (G_IS_TASK (task));
  g_assert (lookup != NULL);

  if (lookup->buffer_text == NULL)
    {
      lookup->mapped = g_mapped_file_new (lookup->entry->path, FALSE, &error);

      if (lookup->mapped == NULL)
        {
          g_task_return_error (task, error);
          return;
        }

      data = g_mapped_file_get_contents (mapped);
      length = g_mapped_file_get_length (mapped);
    }
  else
    {
      data = lookup->buffer_text;
      length = strlen (data);
    }

  pattern = extract_regex (lookup->entry->pattern);

  if (!(regex = g_regex_new (pattern, G_REGEX_MULTILINE, 0, &error)))
    {
      g_task_return_error (task, error);
      return;
    }

  g_regex_match_full (regex, data, length, 0, 0, &match_info, &error);

  while (g_match_info_matches (match_info))
    {
      gint begin = 0;
      gint end = 0;

      if (g_match_info_fetch_pos (match_info, 0, &begin, &end))
        {
          IdeSymbol *symbol;
          gint line = 0;
          gint line_offset = 0;

          calculate_offset (data, length, begin, &line, &line_offset);

          symbol = create_symbol (self, lookup->entry, line, line_offset, begin);
          g_task_return_pointer (task, symbol, (GDestroyNotify)ide_symbol_unref);

          g_match_info_free (match_info);

          return;
        }
    }

  g_match_info_free (match_info);

  if (error != NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  g_task_return_new_error (task,
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
                                               IdeSourceLocation   *location,
                                               GCancellable        *cancellable,
                                               GAsyncReadyCallback  callback,
                                               gpointer             user_data)
{
  IdeCtagsSymbolResolver *self = (IdeCtagsSymbolResolver *)resolver;
  IdeContext *context;
  IdeBufferManager *bufmgr;
  IdeCtagsService *service;
  g_autofree gchar *keyword = NULL;
  g_autoptr(GTask) task = NULL;
  g_autoptr(GPtrArray) indexes = NULL;
  IdeFile *ifile;
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

  task = g_task_new (self, cancellable, callback, user_data);

  ifile = ide_source_location_get_file (location);
  file = ide_file_get_file (ifile);
  line = ide_source_location_get_line (location);
  line_offset = ide_source_location_get_line_offset (location);

  context = ide_object_get_context (IDE_OBJECT (self));
  service = ide_context_get_service_typed (context, IDE_TYPE_CTAGS_SERVICE);
  indexes = ide_ctags_service_get_indexes (service);

  bufmgr = ide_context_get_buffer_manager (context);
  buffer = ide_buffer_manager_find_buffer (bufmgr, file);

  if (!buffer)
    {
      g_task_return_new_error (task,
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
          g_task_set_task_data (task, lookup, lookup_symbol_free);

          if (is_regex (entry->pattern))
            {
              g_task_run_in_thread (task, regex_worker);
              return;
            }
          else if (is_linenum (entry->pattern))
            {
              IdeSymbol *symbol;
              gint64 parsed;

              parsed = g_ascii_strtoll (entry->pattern, NULL, 10);

              if (((parsed == 0) && (errno == ERANGE)) || (parsed > G_MAXINT) || (parsed < 0))
                goto failure;

              symbol = create_symbol (self, entry, parsed, 0, 0);
              g_task_return_pointer (task, symbol, (GDestroyNotify)ide_symbol_unref);
              return;
            }
        }
    }

failure:
  g_task_return_new_error (task,
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
  GTask *task = (GTask *)result;

  g_assert (IDE_IS_CTAGS_SYMBOL_RESOLVER (resolver));
  g_assert (G_IS_TASK (task));

  return g_task_propagate_pointer (task, error);
}

static void
ide_ctags_symbol_resolver_get_symbol_tree_async (IdeSymbolResolver   *resolver,
                                                 GFile               *file,
                                                 GCancellable        *cancellable,
                                                 GAsyncReadyCallback  callback,
                                                 gpointer             user_data)
{
  IdeCtagsSymbolResolver *self = (IdeCtagsSymbolResolver *)resolver;
  g_autoptr(GTask) task = NULL;

  g_assert (IDE_IS_CTAGS_SYMBOL_RESOLVER (self));
  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /*
   * FIXME: I think the symbol tree should be a separate interface.
   */

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_return_new_error (task,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "CTags symbol resolver does not support symbol tree.");
}

static IdeSymbolTree *
ide_ctags_symbol_resolver_get_symbol_tree_finish (IdeSymbolResolver  *resolver,
                                                  GAsyncResult       *result,
                                                  GError            **error)
{
  GTask *task = (GTask *)result;

  g_assert (IDE_IS_CTAGS_SYMBOL_RESOLVER (resolver));
  g_assert (G_IS_TASK (task));

  return g_task_propagate_pointer (task, error);
}

static void
ide_ctags_symbol_resolver_class_init (IdeCtagsSymbolResolverClass *klass)
{
}

static void
ide_ctags_symbol_resolver_class_finalize (IdeCtagsSymbolResolverClass *klass)
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
_ide_ctags_symbol_resolver_register_type (GTypeModule *module)
{
  ide_ctags_symbol_resolver_register_type (module);
}
