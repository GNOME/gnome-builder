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
#include "gb-log.h"

struct _GbGitSearchProviderPrivate
{
  GgitRepository *repository;
  Fuzzy          *file_index;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbGitSearchProvider, gb_git_search_provider,
                            G_TYPE_OBJECT)

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

G_GNUC_UNUSED static void
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
  g_clear_object (&entries);
  g_clear_object (&index);
  g_clear_object (&repository);

  EXIT;
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
  g_return_if_fail (GB_IS_GIT_SEARCH_PROVIDER (provider));

  if (provider->priv->repository != repository)
    {
      if (provider->priv->repository)
        {
          g_clear_object (&provider->priv->repository);
        }

      if (repository)
        {
          provider->priv->repository = g_object_ref (repository);
        }
    }
}

static void
gb_git_search_provider_finalize (GObject *object)
{
  GbGitSearchProviderPrivate *priv = GB_GIT_SEARCH_PROVIDER (object)->priv;

  g_clear_object (&priv->repository);

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
