/* ide-vcs-uri.c
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

#define G_LOG_DOMAIN "ide-vcs-uri"

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "ide-vcs-uri.h"

G_DEFINE_BOXED_TYPE (IdeVcsUri, ide_vcs_uri, ide_vcs_uri_ref, ide_vcs_uri_unref)

struct _IdeVcsUri
{
  volatile gint ref_count;

  /*
   * If the URI string was created and has not been changed, we try extra
   * hard to provide the same URI back from ide_vcs_uri_to_string(). This
   * field is cleared any time any of the other fields are changed.
   */
  gchar *non_destructive_uri;

  gchar *scheme;
  gchar *user;
  gchar *host;
  gchar *path;
  guint  port;
};

static inline void
ide_vcs_uri_set_dirty (IdeVcsUri *self)
{
  g_clear_pointer (&self->non_destructive_uri, g_free);
}

static gboolean
ide_vcs_uri_validate (const IdeVcsUri *self)
{
  g_assert (self != NULL);

  if (g_strcmp0 (self->scheme, "file") == 0)
    return ((self->path != NULL) &&
            (self->port == 0) &&
            (self->host == NULL) &&
            (self->user == NULL));

  if ((g_strcmp0 (self->scheme, "http") == 0) ||
      (g_strcmp0 (self->scheme, "ssh") == 0) ||
      (g_strcmp0 (self->scheme, "git") == 0) ||
      (g_strcmp0 (self->scheme, "https") == 0) ||
      (g_strcmp0 (self->scheme, "rsync") == 0))
    return ((self->path != NULL) && (self->host != NULL));

  return TRUE;
}

static gboolean
ide_vcs_uri_parse (IdeVcsUri   *self,
                   const gchar *str)
{
  static GRegex *regex1;
  static GRegex *regex2;
  static GRegex *regex3;
  static gsize initialized;
  GMatchInfo *match_info = NULL;
  gboolean ret = FALSE;

  if (g_once_init_enter (&initialized))
    {
      /* http://stackoverflow.com/questions/2514859/regular-expression-for-git-repository */

      regex1 = g_regex_new ("file://(.*)", 0, 0, NULL);
      g_assert (regex1);

      regex2 = g_regex_new ("(\\w+://)(.+@)*([\\w\\d\\.]+)(:[\\d]+){0,1}/*(.*)", 0, 0, NULL);
      g_assert (regex2);

      regex3 = g_regex_new ("(.+@)*([\\w\\d\\.]+):(.*)", 0, 0, NULL);
      g_assert (regex3);

      g_once_init_leave (&initialized, TRUE);
    }

  if (str == NULL)
    return FALSE;

  /* check for local file:// style uris */
  g_regex_match (regex1, str, 0, &match_info);
  if (g_match_info_matches (match_info))
    {
      g_autofree gchar *path = NULL;

      path = g_match_info_fetch (match_info, 1);

      ide_vcs_uri_set_scheme (self, "file://");
      ide_vcs_uri_set_user (self, NULL);
      ide_vcs_uri_set_host (self, NULL);
      ide_vcs_uri_set_port (self, 0);
      ide_vcs_uri_set_path (self, path);

      ret = TRUE;
    }
  g_clear_pointer (&match_info, g_match_info_free);

  if (ret)
    return ret;

  /* check for ssh:// style network uris */
  g_regex_match (regex2, str, 0, &match_info);
  if (g_match_info_matches (match_info))
    {
      g_autofree gchar *scheme = NULL;
      g_autofree gchar *user = NULL;
      g_autofree gchar *host = NULL;
      g_autofree gchar *path = NULL;
      g_autofree gchar *portstr = NULL;
      gint start_pos;
      gint end_pos;
      guint port = 0;

      scheme = g_match_info_fetch (match_info, 1);
      user = g_match_info_fetch (match_info, 2);
      host = g_match_info_fetch (match_info, 3);
      portstr = g_match_info_fetch (match_info, 4);
      path = g_match_info_fetch (match_info, 5);

      g_match_info_fetch_pos (match_info, 5, &start_pos, &end_pos);

      if (*path != '~' && (start_pos > 0) && str [start_pos-1] == '/')
        {
          gchar *tmp;

          tmp = path;
          path = g_strdup_printf ("/%s", path);
          g_free (tmp);
        }

      if (!ide_str_empty0 (portstr) && g_ascii_isdigit (portstr [1]))
        port = CLAMP (atoi (&portstr [1]), 1, G_MAXINT16);

      ide_vcs_uri_set_scheme (self, scheme);
      ide_vcs_uri_set_user (self, user);
      ide_vcs_uri_set_host (self, host);
      ide_vcs_uri_set_port (self, port);
      ide_vcs_uri_set_path (self, path);

      ret = TRUE;
    }
  g_clear_pointer (&match_info, g_match_info_free);

  if (ret)
    return ret;

  /* check for user@host style uris */
  g_regex_match (regex3, str, 0, &match_info);
  if (g_match_info_matches (match_info))
    {
      g_autofree gchar *user = NULL;
      g_autofree gchar *host = NULL;
      g_autofree gchar *path = NULL;

      user = g_match_info_fetch (match_info, 1);
      host = g_match_info_fetch (match_info, 2);
      path = g_match_info_fetch (match_info, 3);

      if (path && path[0] != '~' && path[0] != '/')
        {
          g_autofree gchar *tmp = path;
          path = g_strdup_printf ("~/%s", tmp);
        }

      ide_vcs_uri_set_user (self, user);
      ide_vcs_uri_set_host (self, host);
      ide_vcs_uri_set_path (self, path);
      ide_vcs_uri_set_scheme (self, "ssh://");

      ret = TRUE;
    }
  g_clear_pointer (&match_info, g_match_info_free);

  if (ret)
    return ret;

  /* try to avoid some in-progress schemes */
  if (strstr (str, "://"))
    return FALSE;

  ide_vcs_uri_set_scheme (self, "file://");
  ide_vcs_uri_set_user (self, NULL);
  ide_vcs_uri_set_host (self, NULL);
  ide_vcs_uri_set_port (self, 0);
  ide_vcs_uri_set_path (self, str);

  return TRUE;
}

IdeVcsUri *
ide_vcs_uri_new (const gchar *uri)
{
  IdeVcsUri *self;

  self = g_slice_new0 (IdeVcsUri);
  self->ref_count = 1;

  if (ide_vcs_uri_parse (self, uri) && ide_vcs_uri_validate (self))
    {
      self->non_destructive_uri = g_strdup (uri);
      return self;
    }

  ide_vcs_uri_unref (self);

  return NULL;
}

static void
ide_vcs_uri_finalize (IdeVcsUri *self)
{
  g_free (self->non_destructive_uri);
  g_free (self->scheme);
  g_free (self->user);
  g_free (self->host);
  g_free (self->path);
  g_slice_free (IdeVcsUri, self);
}

IdeVcsUri *
ide_vcs_uri_ref (IdeVcsUri *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count > 0, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
ide_vcs_uri_unref (IdeVcsUri *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count > 0);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    ide_vcs_uri_finalize (self);
}

const gchar *
ide_vcs_uri_get_scheme (const IdeVcsUri *self)
{
  g_return_val_if_fail (self, NULL);

  return self->scheme;
}

const gchar *
ide_vcs_uri_get_user (const IdeVcsUri *self)
{
  g_return_val_if_fail (self, NULL);

  return self->user;
}

const gchar *
ide_vcs_uri_get_host (const IdeVcsUri *self)
{
  g_return_val_if_fail (self, NULL);

  return self->host;
}

guint
ide_vcs_uri_get_port (const IdeVcsUri *self)
{
  g_return_val_if_fail (self, 0);

  return self->port;
}

const gchar *
ide_vcs_uri_get_path (const IdeVcsUri *self)
{
  g_return_val_if_fail (self, NULL);

  return self->path;
}

void
ide_vcs_uri_set_scheme (IdeVcsUri   *self,
                        const gchar *scheme)
{
  g_return_if_fail (self);

  if (ide_str_empty0 (scheme))
    scheme = NULL;

  if (scheme != self->scheme)
    {
      const gchar *tmp;

      g_clear_pointer (&self->scheme, g_free);

      if (scheme != NULL && (tmp = strchr (scheme, ':')))
        self->scheme = g_strndup (scheme, tmp - scheme);
      else
        self->scheme = g_strdup (scheme);
    }

  ide_vcs_uri_set_dirty (self);
}

void
ide_vcs_uri_set_user (IdeVcsUri   *self,
                      const gchar *user)
{
  g_return_if_fail (self);

  if (ide_str_empty0 (user))
    user = NULL;

  if (user != self->user)
    {
      const gchar *tmp;

      g_clear_pointer (&self->user, g_free);

      if (user != NULL && (tmp = strchr (user, '@')))
        self->user = g_strndup (user, tmp - user);
      else
        self->user = g_strdup (user);
    }

  ide_vcs_uri_set_dirty (self);
}

void
ide_vcs_uri_set_host (IdeVcsUri   *self,
                      const gchar *host)
{
  g_return_if_fail (self);

  if (ide_str_empty0 (host))
    host = NULL;

  if (host != self->host)
    {
      g_free (self->host);
      self->host = g_strdup (host);
    }

  ide_vcs_uri_set_dirty (self);
}

void
ide_vcs_uri_set_port (IdeVcsUri *self,
                      guint      port)
{
  g_return_if_fail (self);
  g_return_if_fail (port <= G_MAXINT16);

  self->port = port;

  ide_vcs_uri_set_dirty (self);
}

void
ide_vcs_uri_set_path (IdeVcsUri   *self,
                      const gchar *path)
{
  g_return_if_fail (self);

  if (ide_str_empty0 (path))
    path = NULL;

  if (path != self->path)
    {
      if (path != NULL && (*path == ':'))
        path++;
      g_free (self->path);
      self->path = g_strdup (path);
    }

  ide_vcs_uri_set_dirty (self);
}

gchar *
ide_vcs_uri_to_string (const IdeVcsUri *self)
{
  GString *str;

  g_return_val_if_fail (self, NULL);

  if (self->non_destructive_uri != NULL)
    return g_strdup (self->non_destructive_uri);

  str = g_string_new (NULL);

  g_string_append_printf (str, "%s://", self->scheme);

  if (0 == g_strcmp0 (self->scheme, "file"))
    {
      g_string_append (str, self->path);
      return g_string_free (str, FALSE);
    }

  if (self->user != NULL)
    g_string_append_printf (str, "%s@", self->user);

  g_string_append (str, self->host);

  if (self->port != 0)
    g_string_append_printf (str, ":%u", self->port);

  if (self->path == NULL)
    g_string_append (str, "/");
  else if (self->path [0] == '~')
    g_string_append_printf (str, "/%s", self->path);
  else if (self->path [0] != '/')
    g_string_append_printf (str, "/%s", self->path);
  else
    g_string_append (str, self->path);

  return g_string_free (str, FALSE);
}

gboolean
ide_vcs_uri_is_valid (const gchar *uri_string)
{
  gboolean ret = FALSE;

  if (uri_string != NULL)
    {
      IdeVcsUri *uri;

      uri = ide_vcs_uri_new (uri_string);
      ret = !!uri;
      g_clear_pointer (&uri, ide_vcs_uri_unref);
    }

  return ret;
}

/**
 * ide_vcs_uri_get_clone_name:
 * @self: an #ideVcsUri
 *
 * Determines a suggested name for the checkout directory. Some special
 * handling of suffixes such as ".git" are performed to improve the the
 * quality of results.
 *
 * Returns: (transfer full) (nullable): a string containing the suggested
 *   clone directory name, or %NULL.
 */
gchar *
ide_vcs_uri_get_clone_name (const IdeVcsUri *self)
{
  g_autofree gchar *name = NULL;
  const gchar *path;

  g_return_val_if_fail (self != NULL, NULL);

  if (!(path = ide_vcs_uri_get_path (self)))
    return NULL;

  if (ide_str_empty0 (path))
    return NULL;

  if (!(name = g_path_get_basename (path)))
    return NULL;

  /* Trim trailing ".git" */
  if (g_str_has_suffix (name, ".git"))
    *(strrchr (name, '.')) = '\0';

  if (!g_str_equal (name, "/") && !g_str_equal (name, "~"))
    return g_steal_pointer (&name);

  return NULL;
}
