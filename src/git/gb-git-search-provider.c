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
#include <string.h>

#include "fuzzy.h"
#include "gb-git-search-provider.h"
#include "gb-glib.h"
#include "gb-editor-workspace.h"
#include "gb-search-context.h"
#include "gb-search-reducer.h"
#include "gb-search-result.h"
#include "gb-string.h"
#include "gb-workbench.h"

#define GB_GIT_SEARCH_PROVIDER_MAX_MATCHES 1000

struct _GbGitSearchProviderPrivate
{
  GgitRepository *repository;
  Fuzzy          *file_index;
  GFile          *repository_dir;
  gchar          *repository_shorthand;
  GbWorkbench    *workbench;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbGitSearchProvider,
                            gb_git_search_provider,
                            GB_TYPE_SEARCH_PROVIDER)

enum {
  PROP_0,
  PROP_REPOSITORY,
  PROP_WORKBENCH,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];
static GQuark      gQuarkPath;

GbWorkbench *
gb_git_search_provider_get_workbench (GbGitSearchProvider *provider)
{
  g_return_val_if_fail (GB_IS_GIT_SEARCH_PROVIDER (provider), NULL);

  return provider->priv->workbench;
}

static void
gb_git_search_provider_set_workbench (GbGitSearchProvider *provider,
                                      GbWorkbench         *workbench)
{
  g_return_if_fail (GB_IS_GIT_SEARCH_PROVIDER (provider));
  g_return_if_fail (GB_IS_WORKBENCH (workbench));

  gb_set_weak_pointer (workbench, &provider->priv->workbench);
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
      g_clear_pointer (&provider->priv->repository_shorthand, g_free);
      provider->priv->repository_shorthand =
        g_strdup (g_object_get_data (G_OBJECT (task), "shorthand"));

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
  GgitRef *ref;
  GError *error = NULL;
  GFile *repository_dir = task_data;
  Fuzzy *fuzzy;
  guint count;
  guint i;

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
      goto cleanup;
    }

  ref = ggit_repository_get_head (repository, NULL);
  if (ref)
    {
      const gchar *shorthand;

      /* pass back the repo shorthand name via gobject data.
       * not clean, but works for now */
      shorthand = ggit_ref_get_shorthand (ref);
      if (shorthand)
        g_object_set_data_full (G_OBJECT (task), "shorthand",
                                g_strdup (shorthand), g_free);
      g_clear_object (&ref);
    }

  index = ggit_repository_get_index (repository, &error);
  if (!index)
    {
      g_task_return_error (task, error);
      goto cleanup;
    }

  entries = ggit_index_get_entries (index);

  fuzzy = fuzzy_new_with_free_func (FALSE, g_free);
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
        {
          const gchar *shortname = strrchr (path, '/');

          if (shortname)
            fuzzy_insert (fuzzy, shortname, g_strdup (path));
          else
            fuzzy_insert (fuzzy, path, g_strdup (path));
        }

      ggit_index_entry_unref (entry);
    }

  fuzzy_end_bulk_insert (fuzzy);
  g_task_return_pointer (task, fuzzy, (GDestroyNotify)fuzzy_unref);

cleanup:
  g_clear_pointer (&entries, ggit_index_entries_unref);
  g_clear_object (&index);
  g_clear_object (&repository);
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

static void
activate_cb (GbSearchResult *result,
             gpointer        user_data)
{
  GbGitSearchProvider *provider = user_data;
  GbWorkspace *workspace;
  GFile *file;
  GFile *base_location;
  gchar *base_path;
  gchar *path;

  base_location = ggit_repository_get_workdir (provider->priv->repository);
  base_path = g_file_get_path (base_location);
  path = g_build_filename (base_path,
                           g_object_get_qdata (G_OBJECT (result), gQuarkPath),
                           NULL);
  file = g_file_new_for_path (path);

  workspace = gb_workbench_get_workspace (provider->priv->workbench,
                                          GB_TYPE_EDITOR_WORKSPACE);
  gb_editor_workspace_open (GB_EDITOR_WORKSPACE (workspace), file);

  g_clear_object (&base_location);
  g_clear_object (&file);
  g_free (path);
  g_free (base_path);
}

static void
gb_git_search_provider_populate (GbSearchProvider *provider,
                                 GbSearchContext  *context,
                                 const gchar      *search_terms,
                                 gsize             max_results,
                                 GCancellable     *cancellable)
{
  GbGitSearchProvider *self = (GbGitSearchProvider *)provider;

  g_return_if_fail (GB_IS_GIT_SEARCH_PROVIDER (self));
  g_return_if_fail (GB_IS_SEARCH_CONTEXT (context));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (self->priv->file_index)
    {
      GString *str = g_string_new (NULL);
      GbSearchReducer reducer = { 0 };
      gchar *delimited;
      GArray *matches;
      guint i;
      guint truncate_len;

      delimited = remove_spaces (search_terms);
      matches = fuzzy_match (self->priv->file_index, delimited,
                             GB_GIT_SEARCH_PROVIDER_MAX_MATCHES);

      if (self->priv->repository)
        {
          GFile *repo_dir = NULL;

          repo_dir = ggit_repository_get_location (self->priv->repository);
          if (repo_dir)
            {
              gchar *repo_name;

              repo_name = g_file_get_basename (repo_dir);

              if (g_strcmp0 (repo_name, ".git") == 0)
                {
                  GFile *tmp;

                  tmp = repo_dir;
                  repo_dir = g_file_get_parent (repo_dir);
                  g_clear_object (&tmp);

                  repo_name = g_file_get_basename (repo_dir);
                }

              g_string_append (str, repo_name);

              g_clear_object (&repo_dir);
              g_free (repo_name);
            }

          if (self->priv->repository_shorthand)
            g_string_append_printf (str, "[%s]",
                                    self->priv->repository_shorthand);
        }

      truncate_len = str->len;

      gb_search_reducer_init (&reducer, context, provider);

      for (i = 0; i < matches->len; i++)
        {
          FuzzyMatch *match;
          gchar *shortname = NULL;
          gchar **parts;
          guint j;

          match = &g_array_index (matches, FuzzyMatch, i);

          if (gb_search_reducer_accepts (&reducer, match->score))
            {
              GbSearchResult *result;
              gchar *markup;

              parts = split_path (match->value, &shortname);
              for (j = 0; parts [j]; j++)
                g_string_append_printf (str, " / %s", parts [j]);

              markup = gb_str_highlight (shortname, search_terms);

              result = gb_search_result_new (markup, str->str, match->score);
              g_object_set_qdata_full (G_OBJECT (result), gQuarkPath,
                                       g_strdup (match->value), g_free);
              g_signal_connect (result,
                                "activate",
                                G_CALLBACK (activate_cb),
                                provider);
              gb_search_reducer_push (&reducer, result);
              g_object_unref (result);

              g_free (markup);
              g_free (shortname);
              g_strfreev (parts);
              g_string_truncate (str, truncate_len);
            }
        }

      gb_search_context_set_provider_count (context, provider, matches->len);

      gb_search_reducer_destroy (&reducer);
      g_array_unref (matches);
      g_free (delimited);
      g_string_free (str, TRUE);
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

          g_clear_object (&priv->repository_dir);
          priv->repository_dir = ggit_repository_get_location (repository);

          priv->repository = g_object_ref (repository);
          task = g_task_new (provider, NULL, load_cb, provider);
          g_task_set_task_data (task,
                                g_object_ref (priv->repository_dir),
                                g_object_unref);
          g_task_run_in_thread (task, gb_git_search_provider_build_file_index);
          g_clear_object (&task);
        }
    }
}

const gchar *
gb_git_search_provider_get_verb (GbSearchProvider *provider)
{
  g_return_val_if_fail (GB_IS_GIT_SEARCH_PROVIDER (provider), NULL);

  return _("Switch To");
}

static void
gb_git_search_provider_finalize (GObject *object)
{
  GbGitSearchProviderPrivate *priv = GB_GIT_SEARCH_PROVIDER (object)->priv;

  g_clear_pointer (&priv->repository_shorthand, g_free);
  g_clear_object (&priv->repository_dir);
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

    case PROP_WORKBENCH:
      g_value_set_object (value, gb_git_search_provider_get_workbench (self));
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

    case PROP_WORKBENCH:
      gb_git_search_provider_set_workbench (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_git_search_provider_class_init (GbGitSearchProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GbSearchProviderClass *provider_class = GB_SEARCH_PROVIDER_CLASS (klass);

  object_class->finalize = gb_git_search_provider_finalize;
  object_class->get_property = gb_git_search_provider_get_property;
  object_class->set_property = gb_git_search_provider_set_property;

  provider_class->populate = gb_git_search_provider_populate;
  provider_class->get_verb = gb_git_search_provider_get_verb;

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

  gParamSpecs [PROP_WORKBENCH] =
    g_param_spec_object ("workbench",
                         _("Workbench"),
                         _("The workbench window."),
                         GB_TYPE_WORKBENCH,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_WORKBENCH,
                                   gParamSpecs [PROP_WORKBENCH]);

  gQuarkPath = g_quark_from_static_string ("PATH");
}

static void
gb_git_search_provider_init (GbGitSearchProvider *self)
{
  self->priv = gb_git_search_provider_get_instance_private (self);
}
