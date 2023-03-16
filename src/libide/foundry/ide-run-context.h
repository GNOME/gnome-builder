/* ide-run-context.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#pragma once

#if !defined (IDE_FOUNDRY_INSIDE) && !defined (IDE_FOUNDRY_COMPILATION)
# error "Only <libide-foundry.h> can be included directly."
#endif

#include <vte/vte.h>

#include <libide-threading.h>

G_BEGIN_DECLS

#define IDE_TYPE_RUN_CONTEXT (ide_run_context_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeRunContext, ide_run_context, IDE, RUN_CONTEXT, GObject)

/**
 * IdeRunContextShell:
 * @IDE_RUN_CONTEXT_SHELL_DEFAULT: A basic shell with no user scripts
 * @IDE_RUN_CONTEXT_SHELL_LOGIN: A user login shell similar to `bash -l`
 * @IDE_RUN_CONTEXT_SHELL_INTERACTIVE: A user interactive shell similar to `bash -i`
 *
 * Describes the type of shell to be used within the context.
 */
typedef enum _IdeRunContextShell
{
  IDE_RUN_CONTEXT_SHELL_DEFAULT     = 0,
  IDE_RUN_CONTEXT_SHELL_LOGIN       = 1,
  IDE_RUN_CONTEXT_SHELL_INTERACTIVE = 2,
} IdeRunContextShell;

/**
 * IdeRunContextHandler:
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error must be set.
 */
typedef gboolean (*IdeRunContextHandler) (IdeRunContext       *run_context,
                                          const char * const  *argv,
                                          const char * const  *env,
                                          const char          *cwd,
                                          IdeUnixFDMap        *unix_fd_map,
                                          gpointer             user_data,
                                          GError             **error);

IDE_AVAILABLE_IN_ALL
IdeRunContext         *ide_run_context_new                     (void);
IDE_AVAILABLE_IN_ALL
void                   ide_run_context_push                    (IdeRunContext         *self,
                                                                IdeRunContextHandler   handler,
                                                                gpointer               handler_data,
                                                                GDestroyNotify         handler_data_destroy);
IDE_AVAILABLE_IN_ALL
void                   ide_run_context_push_at_base            (IdeRunContext         *self,
                                                                IdeRunContextHandler   handler,
                                                                gpointer               handler_data,
                                                                GDestroyNotify         handler_data_destroy);
IDE_AVAILABLE_IN_ALL
void                   ide_run_context_push_error              (IdeRunContext         *self,
                                                                GError                *error);
IDE_AVAILABLE_IN_ALL
void                   ide_run_context_push_host               (IdeRunContext         *self);
IDE_AVAILABLE_IN_ALL
void                   ide_run_context_push_shell              (IdeRunContext         *self,
                                                                IdeRunContextShell     shell);
IDE_AVAILABLE_IN_44
void                   ide_run_context_push_user_shell         (IdeRunContext         *self,
                                                                IdeRunContextShell     shell);
IDE_AVAILABLE_IN_ALL
void                   ide_run_context_push_expansion          (IdeRunContext         *self,
                                                                const char * const    *environ);
IDE_AVAILABLE_IN_ALL
const char * const    *ide_run_context_get_argv                (IdeRunContext         *self);
IDE_AVAILABLE_IN_ALL
void                   ide_run_context_set_argv                (IdeRunContext         *self,
                                                                const char * const    *argv);
IDE_AVAILABLE_IN_ALL
const char * const    *ide_run_context_get_environ             (IdeRunContext         *self);
IDE_AVAILABLE_IN_ALL
void                   ide_run_context_set_environ             (IdeRunContext         *self,
                                                                const char * const    *environ);
IDE_AVAILABLE_IN_ALL
void                   ide_run_context_add_environ             (IdeRunContext         *self,
                                                                const char * const    *environ);
IDE_AVAILABLE_IN_ALL
void                   ide_run_context_add_minimal_environment (IdeRunContext         *self);
IDE_AVAILABLE_IN_ALL
void                   ide_run_context_environ_to_argv         (IdeRunContext         *self);
IDE_AVAILABLE_IN_ALL
const char            *ide_run_context_get_cwd                 (IdeRunContext         *self);
IDE_AVAILABLE_IN_ALL
void                   ide_run_context_set_cwd                 (IdeRunContext         *self,
                                                                const char            *cwd);
IDE_AVAILABLE_IN_ALL
void                   ide_run_context_set_pty_fd              (IdeRunContext         *self,
                                                                int                    consumer_fd);
IDE_AVAILABLE_IN_ALL
void                   ide_run_context_set_pty                 (IdeRunContext         *self,
                                                                VtePty                *pty);
IDE_AVAILABLE_IN_ALL
void                   ide_run_context_take_fd                 (IdeRunContext         *self,
                                                                int                    source_fd,
                                                                int                    dest_fd);
IDE_AVAILABLE_IN_ALL
gboolean               ide_run_context_merge_unix_fd_map       (IdeRunContext         *self,
                                                                IdeUnixFDMap          *unix_fd_map,
                                                                GError               **error);
IDE_AVAILABLE_IN_ALL
void                   ide_run_context_prepend_argv            (IdeRunContext         *self,
                                                                const char            *arg);
IDE_AVAILABLE_IN_ALL
void                   ide_run_context_prepend_args            (IdeRunContext         *self,
                                                                const char * const    *args);
IDE_AVAILABLE_IN_ALL
void                   ide_run_context_append_argv             (IdeRunContext         *self,
                                                                const char            *arg);
IDE_AVAILABLE_IN_ALL
void                   ide_run_context_append_args             (IdeRunContext         *self,
                                                                const char * const    *args);
IDE_AVAILABLE_IN_ALL
gboolean               ide_run_context_append_args_parsed      (IdeRunContext         *self,
                                                                const char            *args,
                                                                GError               **error);
IDE_AVAILABLE_IN_ALL
void                   ide_run_context_append_formatted        (IdeRunContext         *self,
                                                                const char            *format,
                                                                ...) G_GNUC_PRINTF (2, 3);
IDE_AVAILABLE_IN_ALL
const char            *ide_run_context_getenv                  (IdeRunContext         *self,
                                                                const char            *key);
IDE_AVAILABLE_IN_ALL
void                   ide_run_context_setenv                  (IdeRunContext         *self,
                                                                const char            *key,
                                                                const char            *value);
IDE_AVAILABLE_IN_ALL
void                   ide_run_context_unsetenv                (IdeRunContext         *self,
                                                                const char            *key);
IDE_AVAILABLE_IN_ALL
GIOStream             *ide_run_context_create_stdio_stream     (IdeRunContext         *self,
                                                                GError               **error);
IDE_AVAILABLE_IN_ALL
IdeSubprocessLauncher *ide_run_context_end                     (IdeRunContext         *self,
                                                                GError               **error);
IDE_AVAILABLE_IN_ALL
IdeSubprocess         *ide_run_context_spawn                   (IdeRunContext         *self,
                                                                GError               **error);

G_END_DECLS
