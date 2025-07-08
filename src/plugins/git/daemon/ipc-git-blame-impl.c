/*
 * ipc-git-blame-impl.c
 *
 * Copyright 2025 Nokse <nokse@posteo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define G_LOG_DOMAIN "ipc-git-blame-impl"

#include <glib/gi18n.h>
#include "ipc-git-blame-impl.h"

struct _IpcGitBlameImpl
{
  IpcGitBlameSkeleton  parent;
  char                *path;
  GgitRepository      *repository;
  GBytes              *contents;
  GgitBlame           *blame;
  GgitBlame           *base_blame;
  gboolean             needs_refresh;
};

static gboolean
ipc_git_blame_impl_update_blame (IpcGitBlameImpl  *self,
                                 GError          **error)
{
  g_autoptr (GFile) file = NULL;
  const guint8 *buffer_data = NULL;
  gsize buffer_size = 0;

  if (self->blame != NULL && !self->needs_refresh)
    return TRUE;

  if (self->repository == NULL || self->path == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                   "Repository or path is NULL");
      return FALSE;
    }

  if (self->contents == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                   "No file contents available");
      return FALSE;
    }

  file = g_file_new_for_path (self->path);

  if (!g_file_query_exists (file, NULL))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                   "Non existent file at path");
      return FALSE;
    }

  if (self->base_blame == NULL)
    {
      g_autoptr(GError) blame_error = NULL;

      self->base_blame = ggit_repository_blame_file (self->repository,
                                                     file,
                                                     NULL,
                                                     &blame_error);

      if (self->base_blame == NULL)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                       "Could not create blame for file: %s",
                       blame_error ? blame_error->message : "Unknown error");
          return FALSE;
        }
    }

  buffer_data = g_bytes_get_data (self->contents, &buffer_size);
  if (buffer_data == NULL || buffer_size == 0) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                 "Invalid or empty file contents");
    return FALSE;
  }

  g_clear_object (&self->blame);
  self->blame = ggit_blame_from_buffer (self->base_blame,
                                        buffer_data,
                                        buffer_size,
                                        error);
  if (self->blame == NULL)
    return FALSE;

  self->needs_refresh = FALSE;
  return TRUE;
}

static gboolean
ipc_git_blame_impl_handle_query_line (IpcGitBlame           *blame,
                                      GDBusMethodInvocation *invocation,
                                      guint                  line_number)
{
  IpcGitBlameImpl *self = (IpcGitBlameImpl*)blame;
  g_autoptr (GError) error = NULL;
  GgitBlameHunk *hunk = NULL;
  guint start_final = 0;
  g_autoptr (GgitOId) commit_id = NULL;
  g_autoptr (GgitCommit) commit = NULL;
  g_autoptr (GgitSignature) signature = NULL;
  g_autofree char *author_name = NULL;
  g_autofree char *author_email = NULL;
  g_autofree char *commit_date = NULL;
  g_autofree char *commit_message = NULL;
  const char *msg = NULL;
  guint orig_start = 0;
  guint line_offset = 0;
  guint line_in_commit = 0;

  if (!ipc_git_blame_impl_update_blame (self, &error))
    goto gerror;

  if (!(hunk = ggit_blame_get_hunk_by_line (self->blame, line_number + 1)))
    {
      g_set_error (&error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                   "Line number %u not found in blame data", line_number);
      goto gerror;
    }

  if (!(commit_id = ggit_blame_hunk_get_final_commit_id (hunk)) ||
      !(commit_id) ||
      !(commit = ggit_repository_lookup_commit (self->repository, commit_id, NULL)))
    {
      g_set_error (&error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                   "Couldn't lookup commit");
      goto gerror;
    }

  if ((signature = ggit_commit_get_author (commit)))
    {
      const char *nm = NULL;
      const char *em = NULL;
      GDateTime *dt = NULL;

      nm = ggit_signature_get_name (signature);
      em = ggit_signature_get_email (signature);
      author_name  = g_strdup (nm ? nm : "Unknown");
      author_email = g_strdup (em ? em : "");

      dt = ggit_signature_get_time (signature);
      if (dt)
        commit_date = g_date_time_format_iso8601 (dt);
      else
        commit_date = g_strdup ("");
    }
  else
    {
      author_name  = g_strdup ("Unknown");
      author_email = g_strdup ("");
      commit_date  = g_strdup ("");
    }

  msg = ggit_commit_get_message (commit);
  commit_message = g_strdup (msg ? msg : "");

  start_final = ggit_blame_hunk_get_final_start_line_number (hunk);
  orig_start = ggit_blame_hunk_get_orig_start_line_number (hunk);
  line_offset = (line_number + 1) - start_final;
  line_in_commit = orig_start + line_offset;

  ipc_git_blame_complete_query_line (blame,
                                     invocation,
                                     ggit_oid_to_string (commit_id),
                                     author_name,
                                     author_email,
                                     commit_message,
                                     commit_date,
                                     line_in_commit);
  return TRUE;

gerror:
  if (error != NULL && error->domain != G_IO_ERROR)
    {
      g_autoptr (GError) wrapped = g_steal_pointer (&error);

      g_set_error (&error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   _("Original error: %s"),
                   wrapped->message);
    }

  if (error != NULL)
    g_dbus_method_invocation_return_gerror (invocation, error);
  return TRUE;
}

static gboolean
ipc_git_blame_impl_handle_query_line_range (IpcGitBlame           *blame,
                                            GDBusMethodInvocation *invocation,
                                            guint                  first,
                                            guint                  range)
{
  IpcGitBlameImpl *self = (IpcGitBlameImpl*)blame;
  g_autoptr (GError) error = NULL;
  GVariantBuilder builder;
  guint line_number = 0;

  if (range == 0)
    {
      g_set_error (&error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                   "Range must be >= 1");
      goto gerror;
    }

  if (!ipc_git_blame_impl_update_blame (self, &error))
    goto gerror;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{sv}"));

  for (line_number = first; line_number <= first + range - 1; line_number++)
    {
      gboolean line_found = FALSE;
      GVariantBuilder line_builder;
      GgitBlameHunk *hunk;

      g_variant_builder_init (&line_builder, G_VARIANT_TYPE ("a{sv}"));

      if ((hunk = ggit_blame_get_hunk_by_line (self->blame, line_number + 1)))
        {
          guint start_final = 0;
          g_autoptr (GgitOId) commit_id = NULL;
          g_autoptr (GgitCommit) commit = NULL;
          g_autoptr (GgitSignature) signature = NULL;
          g_autofree char *author_name = NULL;
          g_autofree char *author_email = NULL;
          g_autofree char *commit_date = NULL;
          g_autofree char *commit_message = NULL;
          const char *msg = NULL;
          guint orig_start = 0;
          guint line_offset = 0;
          guint line_in_commit = 0;

          if (!(commit_id = ggit_blame_hunk_get_final_commit_id (hunk)) ||
              !(commit_id) ||
              !(commit = ggit_repository_lookup_commit (self->repository, commit_id, NULL)))
            {
              g_variant_builder_add (&line_builder,
                                     "{sv}",
                                     "line_number",
                                     g_variant_new_uint32 (line_number));
              g_variant_builder_add (&line_builder,
                                     "{sv}",
                                     "error",
                                     g_variant_new_string ("Couldn't lookup commit"));
              line_found = TRUE;
            }
          else
            {
              if ((signature = ggit_commit_get_author (commit)))
                {
                  const char *nm = NULL;
                  const char *em = NULL;
                  GDateTime *dt = NULL;

                  nm = ggit_signature_get_name (signature);
                  em = ggit_signature_get_email (signature);
                  author_name  = g_strdup (nm ? nm : "Unknown");
                  author_email = g_strdup (em ? em : "");

                  dt = ggit_signature_get_time (signature);
                  if (dt)
                    commit_date = g_date_time_format_iso8601 (dt);
                  else
                    commit_date = g_strdup ("");
                }
              else
                {
                  author_name  = g_strdup ("Unknown");
                  author_email = g_strdup ("");
                  commit_date  = g_strdup ("");
                }

              msg = ggit_commit_get_message (commit);
              commit_message = g_strdup (msg ? msg : "");

              start_final = ggit_blame_hunk_get_final_start_line_number (hunk);
              orig_start = ggit_blame_hunk_get_orig_start_line_number (hunk);
              line_offset = (line_number + 1) - start_final;
              line_in_commit = orig_start + line_offset;

              g_variant_builder_add (&line_builder, "{sv}",
                                     "line_number",
                                     g_variant_new_uint32 (line_number));
              g_variant_builder_add (&line_builder, "{sv}",
                                     "commit_id",
                                     g_variant_new_string (ggit_oid_to_string (commit_id)));
              g_variant_builder_add (&line_builder, "{sv}",
                                     "author_name",
                                     g_variant_new_string (author_name));
              g_variant_builder_add (&line_builder, "{sv}",
                                     "author_email",
                                     g_variant_new_string (author_email));
              g_variant_builder_add (&line_builder, "{sv}",
                                     "commit_message",
                                     g_variant_new_string (commit_message));
              g_variant_builder_add (&line_builder, "{sv}",
                                     "commit_date",
                                     g_variant_new_string (commit_date));
              g_variant_builder_add (&line_builder, "{sv}",
                                     "line_in_commit",
                                     g_variant_new_uint32 (line_in_commit));

              line_found = TRUE;
            }
        }

      if (!line_found)
        {
          g_variant_builder_add (&line_builder, "{sv}",
                                 "line_number", g_variant_new_uint32 (line_number));
          g_variant_builder_add (&line_builder, "{sv}",
                                 "error", g_variant_new_string ("Line not found in blame data"));
        }

      g_variant_builder_add (&builder, "a{sv}", &line_builder);
    }

  ipc_git_blame_complete_query_line_range (blame, invocation,
                                           g_variant_builder_end (&builder));
  return TRUE;

gerror:
  if (error != NULL && error->domain != G_IO_ERROR)
    {
      g_autoptr (GError) wrapped = g_steal_pointer (&error);

      g_set_error (&error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   _("Original error: %s"),
                   wrapped->message);
    }

  if (error != NULL)
    g_dbus_method_invocation_return_gerror (invocation, error);
  return TRUE;
}

static gboolean
ipc_git_blame_impl_handle_update_content (IpcGitBlame           *blame,
                                          GDBusMethodInvocation *invocation,
                                          const char            *contents)
{
  IpcGitBlameImpl *self = (IpcGitBlameImpl*)blame;

  g_assert (IPC_IS_GIT_BLAME_IMPL (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (contents != NULL);

  self->needs_refresh = TRUE;
  g_clear_pointer (&self->contents, g_bytes_unref);
  self->contents = g_bytes_new_take (g_strdup (contents), strlen (contents));

  ipc_git_blame_complete_update_content (blame, invocation);

  return TRUE;
}

static void
git_blame_iface_init (IpcGitBlameIface *iface)
{
  iface->handle_update_content = ipc_git_blame_impl_handle_update_content;
  iface->handle_query_line = ipc_git_blame_impl_handle_query_line;
  iface->handle_query_line_range = ipc_git_blame_impl_handle_query_line_range;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IpcGitBlameImpl, ipc_git_blame_impl, IPC_TYPE_GIT_BLAME_SKELETON,
                               G_IMPLEMENT_INTERFACE (IPC_TYPE_GIT_BLAME, git_blame_iface_init))

static void
ipc_git_blame_impl_finalize (GObject *object)
{
  IpcGitBlameImpl *self = NULL;

  self = (IpcGitBlameImpl*)object;

  g_clear_object (&self->blame);
  g_clear_object (&self->repository);
  g_clear_pointer (&self->contents, g_bytes_unref);
  g_clear_pointer (&self->path, g_free);
  G_OBJECT_CLASS (ipc_git_blame_impl_parent_class)->finalize (object);
}

static void
ipc_git_blame_impl_class_init (IpcGitBlameImplClass *klass)
{
  GObjectClass *object_class = NULL;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = ipc_git_blame_impl_finalize;
}

static void
ipc_git_blame_impl_init (IpcGitBlameImpl *self)
{
  self->needs_refresh = TRUE;
}

IpcGitBlame *
ipc_git_blame_impl_new (GgitRepository *repository,
                        const char     *path)
{
  IpcGitBlameImpl *inst = NULL;

  inst = g_object_new (IPC_TYPE_GIT_BLAME_IMPL, NULL);
  inst->path = g_strdup (path);
  inst->repository = g_object_ref (repository);

  return IPC_GIT_BLAME (g_steal_pointer (&inst));
}

void
ipc_git_blame_impl_reset (IpcGitBlameImpl *self)
{
  g_return_if_fail (IPC_IS_GIT_BLAME_IMPL (self));
  g_clear_object (&self->blame);
  self->needs_refresh = TRUE;
}
