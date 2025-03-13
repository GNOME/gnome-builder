/*
 * manuals-devhelp-importer.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include <glib/gi18n.h>

#include "manuals-book.h"
#include "manuals-devhelp-importer.h"
#include "manuals-gio.h"
#include "manuals-gom.h"
#include "manuals-heading.h"
#include "manuals-keyword.h"

#define JOB_FRACTION_QUERIED_INFO      .1
#define JOB_FRACTION_FOUND_BOOK        .2
#define JOB_FRACTION_REMOVED_BOOK      .3
#define JOB_FRACTION_LOADED_CONTENTS   .4
#define JOB_FRACTION_PARSED_INDEX      .5
#define JOB_FRACTION_INSERTED_BOOK     .6
#define JOB_FRACTION_INSERTED_HEADINGS .7
#define JOB_FRACTION_INSERTED_KEYWORDS .8
#define JOB_FRACTION_UPDATED_ETAG      .9

struct _ManualsDevhelpImporter
{
  ManualsImporter parent_instance;
  GArray *directories;
};

G_DEFINE_FINAL_TYPE (ManualsDevhelpImporter, manuals_devhelp_importer, MANUALS_TYPE_IMPORTER)

typedef struct _DevhelpHeading
{
  GPtrArray *children;
  ManualsHeading *resource;
  gint64 parent_id;
  char *title;
  char *link;
} DevhelpHeading;

typedef struct _DevhelpKeyword
{
  char *deprecated;
  char *kind;
  char *path;
  char *name;
  char *since;
  char *stability;
} DevhelpKeyword;

typedef struct _DevhelpBook
{
  GPtrArray *headings;
  GPtrArray *keywords;
  char *language;
  char *online_uri;
  char *title;
  char *link;
} DevhelpBook;

static void
devhelp_heading_free (DevhelpHeading *heading)
{
  g_clear_pointer (&heading->children, g_ptr_array_unref);
  g_clear_object (&heading->resource);
  g_clear_pointer (&heading->link, g_free);
  g_clear_pointer (&heading->title, g_free);
  g_free (heading);
}

static void
devhelp_keyword_free (DevhelpKeyword *keyword)
{
  g_clear_pointer (&keyword->deprecated, g_free);
  g_clear_pointer (&keyword->kind, g_free);
  g_clear_pointer (&keyword->path, g_free);
  g_clear_pointer (&keyword->name, g_free);
  g_clear_pointer (&keyword->since, g_free);
  g_clear_pointer (&keyword->stability, g_free);
  g_free (keyword);
}

static void
devhelp_book_free (DevhelpBook *book)
{
  g_clear_pointer (&book->headings, g_ptr_array_unref);
  g_clear_pointer (&book->keywords, g_ptr_array_unref);
  g_clear_pointer (&book->language, g_free);
  g_clear_pointer (&book->online_uri, g_free);
  g_clear_pointer (&book->title, g_free);
  g_clear_pointer (&book->link, g_free);
  g_free (book);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (DevhelpBook, devhelp_book_free)

typedef struct _Directory
{
  gint64 sdk_id;
  char *path;
} Directory;

static void
directory_clear (gpointer data)
{
  Directory *d = data;
  g_free (d->path);
}

static void chapters_parser_end_element   (GMarkupParseContext  *context,
                                           const char           *element_name,
                                           gpointer              user_data,
                                           GError              **error);
static void chapters_parser_start_element (GMarkupParseContext  *context,
                                           const char           *element_name,
                                           const char          **attribute_names,
                                           const char          **attribute_values,
                                           gpointer              user_data,
                                           GError              **error);

static const GMarkupParser chapters_parser = {
  .start_element = chapters_parser_start_element,
  .end_element = chapters_parser_end_element,
};

static void
chapters_parser_start_element (GMarkupParseContext  *context,
                               const char           *element_name,
                               const char          **attribute_names,
                               const char          **attribute_values,
                               gpointer              user_data,
                               GError              **error)
{
  DevhelpHeading *heading = user_data;

  if (strcmp (element_name, "sub") == 0)
    {
      DevhelpHeading *child;
      const char *name;
      const char *link;

      if (!g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
                                        G_MARKUP_COLLECT_STRING, "name", &name,
                                        G_MARKUP_COLLECT_STRING, "link", &link,
                                        G_MARKUP_COLLECT_INVALID))
        return;

      child = g_new0 (DevhelpHeading, 1);

      g_set_str (&child->title, name);
      g_set_str (&child->link, link);

      if (heading->children == NULL)
        heading->children = g_ptr_array_new_with_free_func ((GDestroyNotify)devhelp_heading_free);

      g_ptr_array_add (heading->children, child);

      g_markup_parse_context_push (context, &chapters_parser, child);
    }
}

static void
chapters_parser_end_element (GMarkupParseContext  *context,
                             const char           *element_name,
                             gpointer              user_data,
                             GError              **error)
{
  if (strcmp (element_name, "sub") == 0)
    g_markup_parse_context_pop (context);
}

static void
functions_parser_start_element (GMarkupParseContext  *context,
                                const char           *element_name,
                                const char          **attribute_names,
                                const char          **attribute_values,
                                gpointer              user_data,
                                GError              **error)
{
  DevhelpBook *book = user_data;

  if (strcmp (element_name, "keyword") == 0)
    {
      DevhelpKeyword *keyword;
      const char *link;
      const char *name;
      const char *type;
      const char *since = NULL;
      const char *deprecated = NULL;
      const char *stability = NULL;

      if (!g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
                                        G_MARKUP_COLLECT_STRING, "type", &type,
                                        G_MARKUP_COLLECT_STRING, "name", &name,
                                        G_MARKUP_COLLECT_STRING, "link", &link,
                                        G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "since", &since,
                                        G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "deprecated", &deprecated,
                                        G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "stability", &stability,
                                        G_MARKUP_COLLECT_INVALID))
        return;

      keyword = g_new0 (DevhelpKeyword, 1);
      keyword->path = g_strdup (link);
      keyword->name = g_strdup (name);
      keyword->kind = g_strdup (type);
      keyword->since = g_strdup (since);
      keyword->deprecated = g_strdup (deprecated);
      keyword->stability = g_strdup (stability);

      g_ptr_array_add (book->keywords, keyword);
    }
}

static const GMarkupParser functions_parser = {
  .start_element = functions_parser_start_element,
};

static void
book_parser_start_element (GMarkupParseContext  *context,
                           const char           *element_name,
                           const char          **attribute_names,
                           const char          **attribute_values,
                           gpointer              user_data,
                           GError              **error)
{
  DevhelpBook *book = user_data;

  if (strcmp (element_name, "chapters") == 0)
    {
      DevhelpHeading *heading;

      heading = g_new0 (DevhelpHeading, 1);
      g_set_str (&heading->title, book->title);
      g_set_str (&heading->link, book->link);

      g_ptr_array_add (book->headings, heading);

      g_markup_parse_context_push (context, &chapters_parser, heading);
    }
  else if (strcmp (element_name, "functions") == 0)
    {
      g_markup_parse_context_push (context, &functions_parser, book);
    }
}

static void
book_parser_end_element (GMarkupParseContext  *context,
                         const char           *element_name,
                         gpointer              user_data,
                         GError              **error)
{
  if (strcmp (element_name, "chapters") == 0)
    g_markup_parse_context_pop (context);
  else if (strcmp (element_name, "functions") == 0)
    g_markup_parse_context_pop (context);
}

static const GMarkupParser book_parser = {
  .start_element = book_parser_start_element,
  .end_element = book_parser_end_element,
};

static const char *strip_suffixes[] = {
  " reference manual",
  " api reference",
  " api references",
  " manual",
};

static void
doc_parser_start_element (GMarkupParseContext  *context,
                          const char           *element_name,
                          const char          **attribute_names,
                          const char          **attribute_values,
                          gpointer              user_data,
                          GError              **error)
{
  DevhelpBook *book = user_data;

  if (strcmp (element_name, "book") == 0)
    {
      g_autofree char *title = NULL;
      const char *language = NULL;
      const char *xmlns = NULL;
      const char *author = NULL;
      const char *version = NULL;
      const char *online = NULL;
      const char *name;
      const char *link;

      if (!g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
                                        G_MARKUP_COLLECT_STRDUP, "title", &title,
                                        G_MARKUP_COLLECT_STRING, "name", &name,
                                        G_MARKUP_COLLECT_STRING, "link", &link,
                                        G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "author", &author,
                                        G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "language", &language,
                                        G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "xmlns", &xmlns,
                                        G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "version", &version,
                                        G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "online", &online,
                                        G_MARKUP_COLLECT_INVALID))
        return;

      /* If a version is specified and it is not 2, then just
       * ignore this file altogether.
       */
      if (version != NULL && strcmp (version, "2") != 0)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_NOT_SUPPORTED,
                       "Cannot parse devhelp version %s",
                       version);
          return;
        }

      /* Drop the whole "Reference Manual" suffix because
       * that is obvious in our context.
       */
      for (guint i = 0; i < G_N_ELEMENTS (strip_suffixes); i++)
        {
          const char *suffix = strip_suffixes[i];
          gsize suffix_len = strlen (suffix);
          gsize len = strlen (title);

          if (suffix_len < len &&
              strcasecmp (&title[len - suffix_len], suffix) == 0)
            {
              title[len - suffix_len] = 0;
              break;
            }
        }

      g_set_str (&book->title, title);
      g_set_str (&book->online_uri, online);
      g_set_str (&book->language, language);
      g_set_str (&book->link, link);

      g_markup_parse_context_push (context, &book_parser, book);
    }
}

static void
doc_parser_end_element (GMarkupParseContext  *context,
                        const char           *element_name,
                        gpointer              user_data,
                        GError              **error)
{
  if (strcmp (element_name, "book") == 0)
    g_markup_parse_context_pop (context);
}

static const GMarkupParser doc_parser = {
  .start_element = doc_parser_start_element,
  .end_element = doc_parser_end_element,
};

static DexFuture *
manuals_devhelp_importer_find_book (ManualsRepository *repository,
                                    GFile             *file,
                                    const char        *etag)
{
  g_autoptr(GomFilter) and = NULL;
  g_autoptr(GomFilter) by_file = NULL;
  g_autoptr(GomFilter) by_etag = NULL;
  g_auto(GValue) uri_value = G_VALUE_INIT;
  g_auto(GValue) etag_value = G_VALUE_INIT;

  g_assert (MANUALS_IS_REPOSITORY (repository));
  g_assert (G_IS_FILE (file));

  g_value_init (&uri_value, G_TYPE_STRING);
  g_value_take_string (&uri_value, g_file_get_uri (file));

  g_value_init (&etag_value, G_TYPE_STRING);
  g_value_set_string (&etag_value, etag);

  by_file = gom_filter_new_eq (MANUALS_TYPE_BOOK, "uri", &uri_value);
  by_etag = gom_filter_new_eq (MANUALS_TYPE_BOOK, "etag", &etag_value);
  and = gom_filter_new_and (by_file, by_etag);

  if (etag == NULL)
    return manuals_repository_find_one (repository, MANUALS_TYPE_BOOK, by_file);
  else
    return manuals_repository_find_one (repository, MANUALS_TYPE_BOOK, and);
}

static DexFuture *
manuals_devhelp_importer_remove_book (ManualsRepository *repository,
                                      ManualsBook       *book)
{
  g_autoptr(GomResourceGroup) headings = NULL;
  g_autoptr(GomFilter) book_id_filter = NULL;
  g_autoptr(GomFilter) id_filter = NULL;
  GValue id_value = G_VALUE_INIT;
  GError *error = NULL;

  g_assert (MANUALS_IS_REPOSITORY (repository));
  g_assert (MANUALS_IS_BOOK (book));

  g_value_init (&id_value, G_TYPE_INT64);
  g_value_set_int64 (&id_value, manuals_book_get_id (book));

  /* Delete all of the headings in book */
  book_id_filter = gom_filter_new_eq (MANUALS_TYPE_HEADING, "book-id", &id_value);
  if (!dex_await (manuals_repository_delete (repository,
                                             MANUALS_TYPE_HEADING,
                                             book_id_filter),
                  &error))
    g_warning ("Failed to delete headings: %s", error->message);
  g_clear_error (&error);

  /* Delete all of the keywords in book */
  g_clear_object (&book_id_filter);
  book_id_filter = gom_filter_new_eq (MANUALS_TYPE_KEYWORD, "book-id", &id_value);
  if (!dex_await (manuals_repository_delete (repository,
                                             MANUALS_TYPE_KEYWORD,
                                             book_id_filter),
                  &error))
    g_warning ("Failed to delete keywords: %s", error->message);
  g_clear_error (&error);

  /* Delete the book itself */
  id_filter = gom_filter_new_eq (MANUALS_TYPE_BOOK, "id", &id_value);
  if (!dex_await (manuals_repository_delete (repository,
                                             MANUALS_TYPE_BOOK,
                                             id_filter),
                  &error))
    g_warning ("Failed to delete book: %s", error->message);
  g_clear_error (&error);

  return dex_future_new_for_boolean (TRUE);
}

typedef struct _ImportFile
{
  ManualsRepository *repository;
  ManualsProgress *progress;
  GFile *file;
  gint64 sdk_id;
} ImportFile;

static void
import_file_free (ImportFile *state)
{
  g_clear_object (&state->repository);
  g_clear_object (&state->progress);
  g_clear_object (&state->file);
  g_free (state);
}

static void
import_keywords (ManualsRepository *repository,
                 gint64             book_id,
                 const char        *base_path,
                 GPtrArray         *keywords)
{
  g_autoptr(GomResourceGroup) group = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (MANUALS_IS_REPOSITORY (repository));
  g_assert (book_id > 0);
  g_assert (keywords != NULL);

  if (keywords->len == 0)
    return;

  group = gom_resource_group_new (GOM_REPOSITORY (repository));

  for (guint i = 0; i < keywords->len; i++)
    {
      const DevhelpKeyword *info = g_ptr_array_index (keywords, i);
      g_autoptr(ManualsKeyword) keyword = NULL;
      g_autofree char *uri = g_strdup_printf ("file://%s/%s", base_path, info->path);

      keyword = g_object_new (MANUALS_TYPE_KEYWORD,
                              "book-id", book_id,
                              "deprecated", info->deprecated,
                              "kind", info->kind,
                              "name", info->name,
                              "uri", uri,
                              "repository", repository,
                              "since", info->since,
                              "stability", info->stability,
                              NULL);

      gom_resource_group_append (group, GOM_RESOURCE (keyword));
    }

  if (!dex_await (gom_resource_group_write (group), &error))
    g_warning ("Failed to insert keywords: %s", error->message);
}

static void
insert_headings_collect (ManualsRepository *repository,
                         gint64             book_id,
                         const char        *base_uri,
                         GPtrArray         *headings,
                         GomResourceGroup  *group)
{
  g_assert (MANUALS_IS_REPOSITORY (repository));
  g_assert (GOM_IS_RESOURCE_GROUP (group));
  g_assert (headings != NULL);
  g_assert (book_id > 0);

  for (guint i = 0; i < headings->len; i++)
    {
      DevhelpHeading *heading = g_ptr_array_index (headings, i);
      g_autofree char *uri = g_strdup_printf ("%s/%s", base_uri, heading->link);
      g_autoptr(ManualsHeading) resource = NULL;

      resource = g_object_new (MANUALS_TYPE_HEADING,
                               "repository", repository,
                               "book-id", book_id,
                               "parent-id", heading->parent_id,
                               "title", heading->title,
                               "uri", uri,
                               NULL);

      gom_resource_group_append (group, GOM_RESOURCE (resource));

      heading->resource = g_steal_pointer (&resource);
    }
}

static void
insert_headings_recursive (ManualsRepository *repository,
                           gint64             book_id,
                           const char        *base_uri,
                           GPtrArray         *headings)
{
  g_autoptr(GomResourceGroup) group = NULL;
  g_autoptr(GPtrArray) next_level = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (MANUALS_IS_REPOSITORY (repository));
  g_assert (headings != NULL);
  g_assert (book_id > 0);

  if (headings->len == 0)
    return;

  /* Write this level all as a single group */
  group = gom_resource_group_new (GOM_REPOSITORY (repository));
  insert_headings_collect (repository, book_id, base_uri, headings, group);
  if (!dex_await (gom_resource_group_write (group), &error))
    {
      g_warning ("Failed to insert resources for %s: %s",
                 base_uri, error->message);
      return;
    }

  /* Now do all the children of all the current children as one group
   * so that we reduce the number of transactions to the max height of
   * the virtual headings tree.
   */
  next_level = g_ptr_array_new ();
  for (guint i = 0; i < headings->len; i++)
    {
      DevhelpHeading *heading = g_ptr_array_index (headings, i);

      if (heading->children == NULL)
        continue;

      /* Update the parent_id to whatever we just inserted for that
       * particular resource.
       */
      for (guint j = 0; j < heading->children->len; j++)
        {
          DevhelpHeading *child = g_ptr_array_index (heading->children, j);
          child->parent_id = manuals_heading_get_id (heading->resource);
        }

      g_ptr_array_extend (next_level, heading->children, NULL, NULL);
    }

  if (next_level->len > 0)
    insert_headings_recursive (repository, book_id, base_uri, next_level);
}

static DexFuture *
manuals_devhelp_importer_import_file_fiber (gpointer user_data)
{
  ImportFile *import_file = user_data;
  g_autoptr(GMarkupParseContext) context = NULL;
  g_autoptr(ManualsJobMonitor) monitor = NULL;
  g_autoptr(DevhelpBook) devhelp_book = NULL;
  g_autoptr(ManualsBook) book = NULL;
  g_autoptr(GFileInfo) file_info = NULL;
  g_autoptr(DexFuture) removal = NULL;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) parent = NULL;
  g_autofree char *subtitle = NULL;
  g_autofree char *uri = NULL;
  g_autofree char *base_uri = NULL;
  g_autofree char *default_uri = NULL;
  const char *contents;
  const char *etag;
  const char *name;
  gsize contents_len;

  g_assert (import_file != NULL);
  g_assert (G_IS_FILE (import_file->file));
  g_assert (MANUALS_IS_PROGRESS (import_file->progress));
  g_assert (MANUALS_IS_REPOSITORY (import_file->repository));

  /* Load the etag for the devhelp2 file so we can compare to what
   * might already be stored in the repository.
   */
  if (!(file_info = dex_await_object (dex_file_query_info (import_file->file,
                                                           G_FILE_ATTRIBUTE_STANDARD_NAME","
                                                           G_FILE_ATTRIBUTE_ETAG_VALUE,
                                                           G_FILE_QUERY_INFO_NONE,
                                                           G_PRIORITY_DEFAULT),
                                      &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  /* Make sure we complete our job at all exit points */
  monitor = manuals_progress_begin_job (import_file->progress);

  manuals_job_set_fraction (monitor, JOB_FRACTION_QUERIED_INFO);

  /* Locate the book if it's already in our repository */
  uri = g_file_get_uri (import_file->file);
  etag = g_file_info_get_etag (file_info);
  name = g_file_info_get_name (file_info);
  book = dex_await_object (manuals_devhelp_importer_find_book (import_file->repository,
                                                               import_file->file,
                                                               NULL),
                           NULL);

  manuals_job_set_fraction (monitor, JOB_FRACTION_FOUND_BOOK);

  /* If the book exists and the etag matches, then there is
   * nothing to do here and we can skip any sort of import
   * parsing and/or record insertions.
   */
  if (book != NULL &&
      etag != NULL &&
      g_strcmp0 (etag, manuals_book_get_etag (book)) == 0)
    {
      g_debug ("%s is already up to date [etag %s]",
               g_file_peek_path (import_file->file),
               etag);
      return dex_future_new_for_boolean (TRUE);
    }

  /* Otherwise we need to delete the book if it exists and all
   * of the headings that go with it so we can re-import it.
   * However, let's not block parsing while we do it. We will
   * await on that after parsing is done before we start inserting
   * our new book items.
   */
  if (book != NULL)
    removal = manuals_devhelp_importer_remove_book (import_file->repository, book);

  /* Now we've parsed our book but the delete job may still be
   * running. Await on that so we won't have any sort of collisions
   * which could break invariants in the schema.
   */
  if (removal != NULL)
    dex_await (dex_ref (removal), NULL);

  manuals_job_set_fraction (monitor, JOB_FRACTION_REMOVED_BOOK);

  /* Now load the devhelp2 file so we can parse it */
  if (!(bytes = dex_await_boxed (dex_file_load_contents_bytes (import_file->file), &error)))
    {
      g_debug ("Failed to load %s: %s",
               g_file_peek_path (import_file->file),
               error->message);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  manuals_job_set_fraction (monitor, JOB_FRACTION_LOADED_CONTENTS);

  /* Note to the user we're importing this book */
  subtitle = g_strdup_printf (_("Importing %s…"), name);
  manuals_job_set_subtitle (monitor, subtitle);

  /* Create state for book importing which will recurse into
   * sub-structures as we parse into the document. The hierarchy
   * is retained in our strucutres.
   */
  devhelp_book = g_new0 (DevhelpBook, 1);
  devhelp_book->headings = g_ptr_array_new_with_free_func ((GDestroyNotify)devhelp_heading_free);
  devhelp_book->keywords = g_ptr_array_new_with_free_func ((GDestroyNotify)devhelp_keyword_free);
  context = g_markup_parse_context_new (&doc_parser,
                                        G_MARKUP_IGNORE_QUALIFIED,
                                        devhelp_book,
                                        NULL);

  /* Parse the document and bail if there are errors */
  contents = (const char *)g_bytes_get_data (bytes, &contents_len);
  if (!g_markup_parse_context_parse (context, contents, contents_len, &error))
    {
      g_debug ("Failed to parse %s: %s",
               g_file_peek_path (import_file->file),
               error->message);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  manuals_job_set_fraction (monitor, JOB_FRACTION_PARSED_INDEX);

  /* Get our base_uri for all "link" attributes */
  parent = g_file_get_parent (import_file->file);
  base_uri = g_file_get_uri (parent);

  if (devhelp_book->link)
    default_uri = g_strdup_printf ("%s/%s", base_uri, devhelp_book->link);

  /* Create our new book item but with an invalid etag. We won't
   * write that until we've completed all insertions so if we
   * crash we don't have half inserted book.
   */
  g_clear_object (&book);
  book = g_object_new (MANUALS_TYPE_BOOK,
                       "etag", "",
                       "language", devhelp_book->language,
                       "default-uri", default_uri,
                       "online-uri", devhelp_book->online_uri,
                       "repository", import_file->repository,
                       "sdk-id", import_file->sdk_id,
                       "title", devhelp_book->title,
                       "uri", uri,
                       NULL);
  if (!dex_await (gom_resource_save (GOM_RESOURCE (book)), &error))
    g_warning ("Failed to insert book for %s: %s",
               g_file_peek_path (import_file->file),
               error->message);

  manuals_job_set_fraction (monitor, JOB_FRACTION_INSERTED_BOOK);

  if (devhelp_book->headings->len > 0)
    {
      DevhelpHeading *first = g_ptr_array_index (devhelp_book->headings, 0);

      insert_headings_recursive (import_file->repository,
                                 manuals_book_get_id (book),
                                 base_uri,
                                 first->children);
    }

  manuals_job_set_fraction (monitor, JOB_FRACTION_INSERTED_HEADINGS);

  import_keywords (import_file->repository,
                   manuals_book_get_id (book),
                   g_file_peek_path (parent),
                   devhelp_book->keywords);

  manuals_job_set_fraction (monitor, JOB_FRACTION_INSERTED_KEYWORDS);

  /* Now update our etag so that we are finished inserting.
   * That way a crash doesn't leave this half imported.
   */
  manuals_book_set_etag (book, etag);
  if (!dex_await (gom_resource_save (GOM_RESOURCE (book)), &error))
    g_warning ("Failed to insert book for %s: %s",
               g_file_peek_path (import_file->file),
               error->message);

  manuals_job_set_fraction (monitor, JOB_FRACTION_UPDATED_ETAG);

  g_debug ("Imported %s (%s)",
           g_file_peek_path (import_file->file),
           devhelp_book->title);

  return dex_future_new_for_boolean (TRUE);
}

static DexFuture *
manuals_devhelp_importer_import_file (ManualsRepository *repository,
                                      GFile             *file,
                                      ManualsProgress   *progress,
                                      gint64             sdk_id)
{
  ImportFile *import_file;

  g_assert (MANUALS_IS_REPOSITORY (repository));
  g_assert (G_IS_FILE (file));
  g_assert (MANUALS_IS_PROGRESS (progress));
  g_assert (sdk_id > 0);

  import_file = g_new0 (ImportFile, 1);
  import_file->repository = g_object_ref (repository);
  import_file->file = g_object_ref (file);
  import_file->progress = g_object_ref (progress);
  import_file->sdk_id = sdk_id;

  return dex_scheduler_spawn (dex_thread_pool_scheduler_get_default (), 0,
                              manuals_devhelp_importer_import_file_fiber,
                              import_file,
                              (GDestroyNotify)import_file_free);
}

typedef struct
{
  ManualsDevhelpImporter *self;
  ManualsRepository      *repository;
  ManualsProgress        *progress;
  GArray                 *directories;
} Import;

static void
import_free (Import *state)
{
  g_clear_pointer (&state->directories, g_array_unref);
  g_clear_object (&state->self);
  g_clear_object (&state->repository);
  g_clear_object (&state->progress);
  g_free (state);
}

static DexFuture *
manuals_devhelp_importer_import_fiber (gpointer user_data)
{
  g_autoptr(GPtrArray) futures = NULL;
  Import *state = user_data;

  g_assert (state != NULL);
  g_assert (MANUALS_IS_DEVHELP_IMPORTER (state->self));
  g_assert (MANUALS_IS_REPOSITORY (state->repository));
  g_assert (MANUALS_IS_PROGRESS (state->progress));
  g_assert (state->directories != NULL);

  futures = g_ptr_array_new_with_free_func (dex_unref);

  for (guint i = 0; i < state->directories->len; i++)
    {
      const Directory *d = &g_array_index (state->directories, Directory, i);
      g_autoptr(GFile) file = g_file_new_for_path (d->path);
      g_autoptr(GPtrArray) directories = NULL;
      g_autoptr(GError) error = NULL;
      gint64 sdk_id = d->sdk_id;

      if (!(directories = dex_await_boxed (manuals_list_children_typed (file,
                                                                        G_FILE_TYPE_DIRECTORY,
                                                                        G_FILE_ATTRIBUTE_ETAG_VALUE),
                                           &error)))
        continue;

      for (guint j = 0; j < directories->len; j++)
        {
          GFileInfo *file_info = g_ptr_array_index (directories, j);
          const char *name = g_file_info_get_name (file_info);
          g_autofree char *name_devhelp2 = g_strdup_printf ("%s.devhelp2", name);
          g_autoptr(GFile) devhelp2_file = g_file_new_build_filename (d->path, name, name_devhelp2, NULL);

          g_ptr_array_add (futures,
                           manuals_devhelp_importer_import_file (state->repository,
                                                                 devhelp2_file,
                                                                 state->progress,
                                                                 sdk_id));
        }
    }

  /* Wait for all import files to complete */
  if (futures->len > 0)
    dex_await (dex_future_allv ((DexFuture **)futures->pdata, futures->len), NULL);

  return dex_future_new_for_boolean (TRUE);
}

static DexFuture *
manuals_devhelp_importer_import (ManualsImporter   *importer,
                                 ManualsRepository *repository,
                                 ManualsProgress   *progress)
{
  ManualsDevhelpImporter *self = (ManualsDevhelpImporter *)importer;
  Import *state;

  g_assert (MANUALS_IS_DEVHELP_IMPORTER (self));
  g_assert (MANUALS_IS_REPOSITORY (repository));
  g_assert (MANUALS_IS_PROGRESS (progress));

  if (self->directories->len == 0)
    return dex_future_new_for_boolean (TRUE);

  state = g_new0 (Import, 1);
  g_set_object (&state->self, self);
  g_set_object (&state->repository, repository);
  g_set_object (&state->progress, progress);
  state->directories = g_array_new (FALSE, FALSE, sizeof (Directory));
  g_array_set_clear_func (state->directories, directory_clear);

  for (guint i = 0; i < self->directories->len; i++)
    {
      Directory d = g_array_index (self->directories, Directory, i);
      d.path = g_strdup (d.path);
      g_array_append_val (state->directories, d);
    }

  return dex_scheduler_spawn (dex_thread_pool_scheduler_get_default (),
                              0,
                              manuals_devhelp_importer_import_fiber,
                              state,
                              (GDestroyNotify)import_free);
}

static void
manuals_devhelp_importer_finalize (GObject *object)
{
  ManualsDevhelpImporter *self = (ManualsDevhelpImporter *)object;

  g_clear_pointer (&self->directories, g_array_unref);

  G_OBJECT_CLASS (manuals_devhelp_importer_parent_class)->finalize (object);
}

static void
manuals_devhelp_importer_class_init (ManualsDevhelpImporterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ManualsImporterClass *importer_class = MANUALS_IMPORTER_CLASS (klass);

  object_class->finalize = manuals_devhelp_importer_finalize;

  importer_class->import = manuals_devhelp_importer_import;
}

static void
manuals_devhelp_importer_init (ManualsDevhelpImporter *self)
{
  self->directories = g_array_new (FALSE, FALSE, sizeof (Directory));
  g_array_set_clear_func (self->directories, directory_clear);
}

ManualsDevhelpImporter *
manuals_devhelp_importer_new (void)
{
  return g_object_new (MANUALS_TYPE_DEVHELP_IMPORTER, NULL);
}

void
manuals_devhelp_importer_add_directory (ManualsDevhelpImporter *self,
                                        const char             *directory,
                                        gint64                  sdk_id)
{
  Directory d;

  g_return_if_fail (MANUALS_IS_DEVHELP_IMPORTER (self));
  g_return_if_fail (directory != NULL);

  d.sdk_id = sdk_id;
  d.path = g_strdup (directory);

  g_array_append_val (self->directories, d);
}

guint
manuals_devhelp_importer_get_size (ManualsDevhelpImporter *self)
{
  g_return_val_if_fail (MANUALS_IS_DEVHELP_IMPORTER (self), 0);

  return self->directories->len;
}

void
manuals_devhelp_importer_set_sdk_id (ManualsDevhelpImporter *self,
                                     gint64                  sdk_id)
{
  g_return_if_fail (MANUALS_IS_DEVHELP_IMPORTER (self));

  for (guint i = 0; i < self->directories->len; i++)
    {
      Directory *d = &g_array_index (self->directories, Directory, i);

      d->sdk_id = sdk_id;
    }
}
