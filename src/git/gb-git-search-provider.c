/* gb-git-search-provider.c
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

#define G_LOG_DOMAIN "git-search"

#include <glib/gi18n.h>

#include "fuzzy.h"
#include "gb-git-search-provider.h"
#include "gb-git-search-result.h"
#include "gb-log.h"
#include "gb-search-context.h"
#include "gb-search-result.h"

#define GB_GIT_SEARCH_PROVIDER_MAX_MATCHES 1000

struct _GbGitSearchProviderPrivate
{
  GgitRepository *repository;
  Fuzzy          *file_index;
};

static void search_provider_init (GbSearchProviderInterface *iface);

G_DEFINE_TYPE_EXTENDED (GbGitSearchProvider,
                        gb_git_search_provider,
                        G_TYPE_OBJECT,
                        0,
                        G_ADD_PRIVATE (GbGitSearchProvider)
                        G_IMPLEMENT_INTERFACE (GB_TYPE_SEARCH_PROVIDER,
                                               search_provider_init))

enum {
  PROP_0,
  PROP_REPOSITORY,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GbSearchProvider *
gb_git_search_provider_new (GgitRepository *repository)
{
  return g_object_new (GB_TYPE_GIT_SEARCH_PROVIDER,
                       "repository", repository,
                       NULL);
}

static void
load_cb (GObject      *object,
         GAsyncResult *result,
         gpointer      user_data)
{
  GbGitSearchProvider *provider = (GbGitSearchProvider *)object;
  GTask *task = (GTask *)result;
  Fuzzy *file_index;
  GError *error = NULL;

  g_return_if_fail (GB_IS_GIT_SEARCH_PROVIDER (provider));
  g_return_if_fail (G_IS_TASK (task));

  file_index = g_task_propagate_pointer (task, &error);

  if (!file_index)
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
    }
  else
    {
      g_clear_pointer (&provider->priv->file_index, fuzzy_unref);
      provider->priv->file_index = fuzzy_ref (file_index);
      g_message ("Git file index loaded.");
    }
}

static void
gb_git_search_provider_build_file_index (GTask        *task,
                                         gpointer      source_object,
                                         gpointer      task_data,
                                         GCancellable *cancellable)
{
  GgitRepository *repository = NULL;
  GgitIndexEntries *entries = NULL;
  GgitIndex *index = NULL;
  GError *error = NULL;
  GFile *repository_dir = task_data;
  Fuzzy *fuzzy;
  guint count;
  guint i;

  ENTRY;

  g_return_if_fail (G_IS_FILE (repository_dir));

  /*
   * The process below works as follows:
   *
   * 1) Load a new GgitRepository to avoid thread-safey issues.
   * 2) Walk the file index for HEAD and add them to the fuzzy index.
   * 3) Complete the bulk insert of the fuzzy index (we do this so we can
   *    coallesce the index build, as it's *much* faster since you don't have
   *    to do as much index reordering.
   * 4) Return the fuzzy index back to the task.
   */

  repository = ggit_repository_open (repository_dir, &error);
  if (!repository)
    {
      g_task_return_error (task, error);
      GOTO (cleanup);
    }

  index = ggit_repository_get_index (repository, &error);
  if (!index)
    {
      g_task_return_error (task, error);
      GOTO (cleanup);
    }

  entries = ggit_index_get_entries (index);

  fuzzy = fuzzy_new (FALSE);
  fuzzy_begin_bulk_insert (fuzzy);

  count = ggit_index_entries_size (entries);

  for (i = 0; i < count; i++)
    {
      GgitIndexEntry *entry;
      const gchar *path;

      entry = ggit_index_entries_get_by_index (entries, i);
      path = ggit_index_entry_get_path (entry);

      /* FIXME:
       *
       *   fuzzy does not yet support UTF-8, which is the native format
       *   for the filesystem. It wont be as fast, but we can just take
       *   the cost of gunichar most likely.
       */
      if (g_str_is_ascii (path))
        fuzzy_insert (fuzzy, path, NULL);

      ggit_index_entry_unref (entry);
    }

  fuzzy_end_bulk_insert (fuzzy);
  g_task_return_pointer (task, fuzzy, (GDestroyNotify)fuzzy_unref);

cleanup:
  g_clear_pointer (&entries, ggit_index_entries_unref);
  g_clear_object (&index);
  g_clear_object (&repository);

  EXIT;
}

static gchar *
remove_spaces (const gchar *text)
{
  GString *str = g_string_new (NULL);

  for (; *text; text = g_utf8_next_char (text))
    {
      gunichar ch = g_utf8_get_char (text);

      if (ch != ' ')
        g_string_append_unichar (str, ch);
    }

  return g_string_free (str, FALSE);
}

static void
gb_git_search_provider_populate (GbSearchProvider *provider,
                                 GbSearchContext  *context,
                                 GCancellable     *cancellable)
{
  GbGitSearchProvider *self = (GbGitSearchProvider *)provider;
  GList *list = NULL;

  g_return_if_fail (GB_IS_GIT_SEARCH_PROVIDER (self));
  g_return_if_fail (GB_IS_SEARCH_CONTEXT (context));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (self->priv->file_index)
    {
      const gchar *search_text;
      gchar *delimited;
      GArray *matches;
      guint i;

      search_text = gb_search_context_get_search_text (context);
      delimited = remove_spaces (search_text);
      matches = fuzzy_match (self->priv->file_index, delimited,
                             GB_GIT_SEARCH_PROVIDER_MAX_MATCHES);

      for (i = 0; i < matches->len; i++)
        {
          FuzzyMatch *match;
          GtkWidget *widget;

          match = &g_array_index (matches, FuzzyMatch, i);

          /* TODO: Make a git file search result */
          widget = g_object_new (GB_TYPE_GIT_SEARCH_RESULT,
                                 "visible", TRUE,
                                 "score", match->score,
                                 "path", match->key,
                                 NULL);
          list = g_list_prepend (list, widget);
        }

      list = g_list_reverse (list);
      gb_search_context_add_results (context, provider, list, TRUE);

      g_array_unref (matches);
      g_free (delimited);
    }
}

GgitRepository *
gb_git_search_provider_get_repository (GbGitSearchProvider *provider)
{
  g_return_val_if_fail (GB_IS_GIT_SEARCH_PROVIDER (provider), NULL);

  return provider->priv->repository;
}

void
gb_git_search_provider_set_repository (GbGitSearchProvider *provider,
                                       GgitRepository      *repository)
{
  GbGitSearchProviderPrivate *priv;

  g_return_if_fail (GB_IS_GIT_SEARCH_PROVIDER (provider));

  priv = provider->priv;

  if (priv->repository != repository)
    {
      if (priv->repository)
        g_clear_object (&provider->priv->repository);

      if (repository)
        {
          GTask *task;

          priv->repository = g_object_ref (repository);
          task = g_task_new (provider, NULL, load_cb, provider);
          g_task_set_task_data (task,
                                ggit_repository_get_location (repository),
                                g_object_unref);
          g_task_run_in_thread (task, gb_git_search_provider_build_file_index);
          g_clear_object (&task);
        }
    }
}

static void
gb_git_search_provider_finalize (GObject *object)
{
  GbGitSearchProviderPrivate *priv = GB_GIT_SEARCH_PROVIDER (object)->priv;

  g_clear_object (&priv->repository);
  g_clear_pointer (&priv->file_index, fuzzy_unref);

  G_OBJECT_CLASS (gb_git_search_provider_parent_class)->finalize (object);
}

static void
gb_git_search_provider_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GbGitSearchProvider *self = GB_GIT_SEARCH_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_REPOSITORY:
      g_value_set_object (value, gb_git_search_provider_get_repository (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_git_search_provider_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GbGitSearchProvider *self = GB_GIT_SEARCH_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_REPOSITORY:
      gb_git_search_provider_set_repository (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_git_search_provider_class_init (GbGitSearchProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_git_search_provider_finalize;
  object_class->get_property = gb_git_search_provider_get_property;
  object_class->set_property = gb_git_search_provider_set_property;

  /**
   * GbGitSearchProvider:repository:
   *
   * The repository that will be used to extract filenames and other
   * information.
   */
  gParamSpecs [PROP_REPOSITORY] =
    g_param_spec_object ("repository",
                         _("Repository"),
                         _("The repository to use for search data."),
                         GGIT_TYPE_REPOSITORY,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_REPOSITORY,
                                   gParamSpecs [PROP_REPOSITORY]);
}

static void
gb_git_search_provider_init (GbGitSearchProvider *self)
{
  self->priv = gb_git_search_provider_get_instance_private (self);
}

static void
search_provider_init (GbSearchProviderInterface *iface)
{
  iface->populate = gb_git_search_provider_populate;
}
