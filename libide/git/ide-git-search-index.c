/* ide-git-search-index.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <ctype.h>
#include <fuzzy.h>
#include <glib/gi18n.h>
#include <libgit2-glib/ggit.h>

#include "ide-context.h"
#include "ide-git-search-index.h"
#include "ide-project.h"
#include "ide-search-context.h"
#include "ide-search-provider.h"
#include "ide-search-reducer.h"
#include "ide-search-result.h"

struct _IdeGitSearchIndex
{
  IdeObject parent_instance;

  GFile *location;
  gchar *shorthand;
  Fuzzy *fuzzy;
};

static GQuark gPathQuark;

static void async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_TYPE_EXTENDED (IdeGitSearchIndex,
                        ide_git_search_index,
                        IDE_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE,
                                               async_initable_iface_init))

enum {
  PROP_0,
  PROP_LOCATION,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static void
activate_cb (IdeSearchResult *result,
             gpointer         user_data)
{
  g_assert (IDE_IS_SEARCH_RESULT (result));

  /* TODO: Hook up document manager in LibIDE */
}

/**
 * ide_git_search_index_get_location:
 *
 * Returns the location of the .git directory.
 *
 * Returns: (transfer none): A #GFile.
 */
GFile *
ide_git_search_index_get_location (IdeGitSearchIndex *self)
{
  g_return_val_if_fail (IDE_IS_GIT_SEARCH_INDEX (self), NULL);

  return self->location;
}

static void
ide_git_search_index_set_location (IdeGitSearchIndex *self,
                                   GFile             *location)
{
  g_return_if_fail (IDE_IS_GIT_SEARCH_INDEX (self));
  g_return_if_fail (G_IS_FILE (location));

  if (g_set_object (&self->location, location))
    g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_LOCATION]);
}

static gchar *
str_highlight (const gchar *str,
               const gchar *match)
{
  GString *ret;
  gunichar str_ch;
  gunichar match_ch;

  g_return_val_if_fail (str, NULL);
  g_return_val_if_fail (match, NULL);

  ret = g_string_new (NULL);

  for (; *str; str = g_utf8_next_char (str))
    {
      str_ch = g_utf8_get_char (str);
      match_ch = g_utf8_get_char (match);

      if (str_ch == match_ch)
        {
          g_string_append (ret, "<u>");
          g_string_append_unichar (ret, str_ch);
          g_string_append (ret, "</u>");

          match = g_utf8_next_char (match);
        }
      else
        {
          g_string_append_unichar (ret, str_ch);
        }
    }

  return g_string_free (ret, FALSE);
}

static gchar *
filter_search_terms (const gchar *search_terms)
{
  GString *str;

  str = g_string_new (NULL);

  for (; *search_terms; search_terms = g_utf8_next_char (search_terms))
    {
      gunichar ch = g_utf8_get_char (search_terms);

      if ((isascii (ch) != 0) && !g_unichar_isspace (ch))
        g_string_append_unichar (str, ch);
    }

  return g_string_free (str, FALSE);
}

static gchar **
split_path (const gchar  *path,
            gchar       **shortname)
{
  gchar **parts;
  gsize len;

  g_return_val_if_fail (path, NULL);
  g_return_val_if_fail (shortname, NULL);

  *shortname = NULL;

  parts = g_strsplit (path, "/", 0);
  len = g_strv_length (parts);

  if (len)
    {
      *shortname = parts [len-1];
      parts [len-1] = 0;
    }

  return parts;
}

void
ide_git_search_index_populate (IdeGitSearchIndex *self,
                               IdeSearchProvider *provider,
                               IdeSearchContext  *search_context,
                               gsize              max_results,
                               const gchar       *search_terms)
{
  g_auto(IdeSearchReducer) reducer = { 0 };
  g_autoptr(gchar) delimited = NULL;
  IdeContext *context;
  IdeProject *project;
  const gchar *project_name;
  GArray *matches = NULL;
  GString *str = NULL;
  gsize truncate_len = 0;
  gsize i;

  g_return_if_fail (IDE_IS_GIT_SEARCH_INDEX (self));
  g_return_if_fail (IDE_IS_SEARCH_PROVIDER (provider));
  g_return_if_fail (IDE_IS_SEARCH_CONTEXT (search_context));
  g_return_if_fail (search_terms);

  context = ide_object_get_context (IDE_OBJECT (self));

  /* Filter space and non-ascii from the search terms */
  delimited = filter_search_terms (search_terms);

  /* Execute the search against the fuzzy index */
  matches = fuzzy_match (self->fuzzy, delimited, max_results);

  /* Generate the prefix for the secondary text */
  project = ide_context_get_project (context);
  project_name = ide_project_get_name (project);
  str = g_string_new (project_name);
  if (self->shorthand)
    g_string_append_printf (str, "[%s]", self->shorthand);
  truncate_len = str->len;

  /* initialize our reducer, which helps us prevent creating unnecessary
   * objects that will simply be discarded */
  ide_search_reducer_init (&reducer, search_context, provider, max_results);

  for (i = 0; i < matches->len; i++)
    {
      FuzzyMatch *match = &g_array_index (matches, FuzzyMatch, i);

      if (ide_search_reducer_accepts (&reducer, match->score))
        {
          g_autoptr(gchar) shortname = NULL;
          g_autoptr(gchar) markup = NULL;
          g_autoptr(IdeSearchResult) result = NULL;
          gchar **parts;
          gsize j;

          /* truncate the secondary text to the shared info */
          g_string_truncate (str, truncate_len);

          /* Generate pretty path to the directory */
          parts = split_path (match->value, &shortname);
          for (j = 0; parts [j]; j++)
            g_string_append_printf (str, " / %s", parts [j]);
          g_strfreev (parts);

          /* highlight the title string with underlines */
          markup = str_highlight (shortname, search_terms);

          /* create our search result and connect to signals */
          result = ide_search_result_new (context,
                                          markup,
                                          str->str,
                                          match->score);
          g_object_set_qdata_full (G_OBJECT (result), gPathQuark,
                                   g_strdup (match->value), g_free);
#if 0
          /* I think we might want to leave this signal on the provider */
          g_signal_connect (result, "activate", G_CALLBACK (activate_cb), NULL);
#endif

          /* push the result through the search reducer */
          ide_search_reducer_push (&reducer, result);
        }
    }

cleanup:
  g_clear_pointer (&matches, g_array_unref);
  g_string_free (str, TRUE);
}

static void
ide_git_search_index_finalize (GObject *object)
{
  IdeGitSearchIndex *self = (IdeGitSearchIndex *)object;

  g_clear_object (&self->location);
  g_clear_pointer (&self->shorthand, g_free);
  g_clear_pointer (&self->fuzzy, fuzzy_unref);

  G_OBJECT_CLASS (ide_git_search_index_parent_class)->finalize (object);
}

static void
ide_git_search_index_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  IdeGitSearchIndex *self = IDE_GIT_SEARCH_INDEX (object);

  switch (prop_id)
    {
    case PROP_LOCATION:
      g_value_set_object (value, ide_git_search_index_get_location (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_git_search_index_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  IdeGitSearchIndex *self = IDE_GIT_SEARCH_INDEX (object);

  switch (prop_id)
    {
    case PROP_LOCATION:
      ide_git_search_index_set_location (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_git_search_index_class_init (IdeGitSearchIndexClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_git_search_index_finalize;
  object_class->get_property = ide_git_search_index_get_property;
  object_class->set_property = ide_git_search_index_set_property;

  gPathQuark = g_quark_from_static_string ("IDE_GIT_SEARCH_INDEX_PATH");

  gParamSpecs [PROP_LOCATION] =
    g_param_spec_object ("location",
                         _("Location"),
                         _("The location of the .git index."),
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_LOCATION,
                                   gParamSpecs [PROP_LOCATION]);
}

static void
ide_git_search_index_init (IdeGitSearchIndex *self)
{
}

static void
ide_git_search_index_init_worker (GTask        *task,
                                  gpointer      source_object,
                                  gpointer      task_data,
                                  GCancellable *cancellable)
{
  IdeGitSearchIndex *self = source_object;
  GgitRepository *repository = NULL;
  GgitIndexEntries *entries = NULL;
  GgitIndex *index = NULL;
  GgitRef *ref;
  GError *error = NULL;
  GFile *repository_dir = task_data;
  guint count;
  guint i;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_GIT_SEARCH_INDEX (self));

  if (!self->location)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_FILENAME,
                               _("Location must be set to .git directory."));
      goto cleanup;
    }

  repository = ggit_repository_open (self->location, &error);

  if (!repository)
    {
      g_task_return_error (task, error);
      goto cleanup;
    }

  ref = ggit_repository_get_head (repository, NULL);

  if (ref)
    {
      self->shorthand = g_strdup (ggit_ref_get_shorthand (ref));
      g_clear_object (&ref);
    }

  index = ggit_repository_get_index (repository, &error);

  if (!index)
    {
      g_task_return_error (task, error);
      goto cleanup;
    }

  entries = ggit_index_get_entries (index);

  self->fuzzy = fuzzy_new_with_free_func (FALSE, g_free);
  count = ggit_index_entries_size (entries);

  fuzzy_begin_bulk_insert (self->fuzzy);

  for (i = 0; i < count; i++)
    {
      GgitIndexEntry *entry;
      const gchar *path;

      entry = ggit_index_entries_get_by_index (entries, i);
      path = ggit_index_entry_get_path (entry);

      /* FIXME:
       *
       * fuzzy does not yet support UTF-8, which is the native format
       * for the filesystem. It wont be as fast, but we can just take
       * the cost of gunichar most likely.
       */
      if (g_str_is_ascii (path))
        {
          const gchar *shortname = strrchr (path, '/');

          if (shortname)
            fuzzy_insert (self->fuzzy, shortname, g_strdup (path));
          else
            fuzzy_insert (self->fuzzy, path, g_strdup (path));
        }

      ggit_index_entry_unref (entry);
    }

  fuzzy_end_bulk_insert (self->fuzzy);

  g_task_return_boolean (task, TRUE);

cleanup:
  g_clear_pointer (&entries, ggit_index_entries_unref);
  g_clear_object (&index);
  g_clear_object (&repository);
}

static void
ide_git_search_index_init_async (GAsyncInitable      *initable,
                                 gint                 io_priority,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  IdeGitSearchIndex *self = (IdeGitSearchIndex *)initable;
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (IDE_IS_GIT_SEARCH_INDEX (self));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_run_in_thread (task, ide_git_search_index_init_worker);
}

static gboolean
ide_git_search_index_init_finish (GAsyncInitable  *initable,
                                  GAsyncResult    *result,
                                  GError         **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_GIT_SEARCH_INDEX (initable), FALSE);
  g_return_val_if_fail (G_IS_TASK (task), FALSE);

  return g_task_propagate_boolean (task, error);
}

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = ide_git_search_index_init_async;
  iface->init_finish = ide_git_search_index_init_finish;
}
