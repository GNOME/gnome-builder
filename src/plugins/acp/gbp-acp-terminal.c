/* gbp-acp-terminal.c
 *
 * Copyright 2026 GNOME Builder contributors
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

#define G_LOG_DOMAIN "gbp-acp-terminal"

#include "config.h"

#include <signal.h>
#include <string.h>

#include <glib/gi18n.h>

#include <libide-threading.h>

#include "gbp-acp-terminal.h"

struct _GbpAcpTerminal
{
  GObject             parent_instance;
  gchar              *id;
  gchar              *command;
  gchar             **args;
  gchar             **env_names;
  gchar             **env_values;
  gchar              *cwd;
  guint64             output_byte_limit;
  IdeSubprocess      *subprocess;
  GCancellable       *cancellable;
  GString            *output;
  gboolean            truncated;
  gboolean            exited;
  gboolean            has_exit_code;
  gint                exit_code;
  gchar              *signal;
  GList               *waiters;
};

G_DEFINE_FINAL_TYPE (GbpAcpTerminal, gbp_acp_terminal, G_TYPE_OBJECT)

static guint terminal_counter;

static void
gbp_acp_terminal_finalize (GObject *object)
{
  GbpAcpTerminal *self = (GbpAcpTerminal *)object;

  if (self->cancellable != NULL)
    g_cancellable_cancel (self->cancellable);

  g_clear_object (&self->subprocess);
  g_clear_object (&self->cancellable);

  if (self->output != NULL)
    {
      g_string_free (self->output, TRUE);
      self->output = NULL;
    }

  g_clear_pointer (&self->id, g_free);
  g_clear_pointer (&self->command, g_free);
  g_clear_pointer (&self->args, g_strfreev);
  g_clear_pointer (&self->env_names, g_strfreev);
  g_clear_pointer (&self->env_values, g_strfreev);
  g_clear_pointer (&self->cwd, g_free);
  g_clear_pointer (&self->signal, g_free);
  g_clear_list (&self->waiters, g_object_unref);

  G_OBJECT_CLASS (gbp_acp_terminal_parent_class)->finalize (object);
}

static void
gbp_acp_terminal_class_init (GbpAcpTerminalClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_acp_terminal_finalize;
}

static void
gbp_acp_terminal_init (GbpAcpTerminal *self)
{
  self->output = g_string_new (NULL);
  self->output_byte_limit = 1024 * 1024;
}

GbpAcpTerminal *
gbp_acp_terminal_new (const gchar        *command,
                      const gchar *const *args,
                      guint               n_args,
                      const gchar *const *env_names,
                      const gchar *const *env_values,
                      guint               n_env,
                      const gchar        *cwd,
                      guint64             output_byte_limit)
{
  GbpAcpTerminal *self;

  g_return_val_if_fail (command != NULL, NULL);

  self = g_object_new (GBP_TYPE_ACP_TERMINAL, NULL);
  self->id = g_strdup_printf ("term_%u", g_atomic_int_add (&terminal_counter, 1));
  self->command = g_strdup (command);
  self->cwd = g_strdup (cwd);

  if (args != NULL && n_args > 0)
    {
      self->args = g_new0 (gchar *, n_args + 1);
      for (guint i = 0; i < n_args; i++)
        self->args[i] = g_strdup (args[i]);
    }

  if (env_names != NULL && env_values != NULL && n_env > 0)
    {
      self->env_names = g_new0 (gchar *, n_env + 1);
      self->env_values = g_new0 (gchar *, n_env + 1);
      for (guint i = 0; i < n_env; i++)
        {
          self->env_names[i] = g_strdup (env_names[i]);
          self->env_values[i] = g_strdup (env_values[i]);
        }
    }

  if (output_byte_limit > 0)
    self->output_byte_limit = output_byte_limit;

  return self;
}

const gchar *
gbp_acp_terminal_get_id (GbpAcpTerminal *self)
{
  g_return_val_if_fail (GBP_IS_ACP_TERMINAL (self), NULL);
  return self->id;
}

gboolean
gbp_acp_terminal_get_exited (GbpAcpTerminal *self)
{
  g_return_val_if_fail (GBP_IS_ACP_TERMINAL (self), FALSE);
  return self->exited;
}

gint
gbp_acp_terminal_get_exit_code (GbpAcpTerminal *self)
{
  g_return_val_if_fail (GBP_IS_ACP_TERMINAL (self), 0);
  return self->exit_code;
}

const gchar *
gbp_acp_terminal_get_signal (GbpAcpTerminal *self)
{
  g_return_val_if_fail (GBP_IS_ACP_TERMINAL (self), NULL);
  return self->signal;
}

const gchar *
gbp_acp_terminal_get_output (GbpAcpTerminal *self)
{
  g_return_val_if_fail (GBP_IS_ACP_TERMINAL (self), NULL);
  return self->output->str;
}

gboolean
gbp_acp_terminal_get_truncated (GbpAcpTerminal *self)
{
  g_return_val_if_fail (GBP_IS_ACP_TERMINAL (self), FALSE);
  return self->truncated;
}

static void
gbp_acp_terminal_enforce_limit (GbpAcpTerminal *self)
{
  gsize len = self->output->len;

  if (len <= self->output_byte_limit)
    return;

  self->truncated = TRUE;

  g_string_erase (self->output, 0, len - self->output_byte_limit);

  /* Do not start the retained buffer in the middle of a UTF-8 sequence. */
  while (self->output->len > 0 &&
         ((guchar)self->output->str[0] & 0xC0) == 0x80)
    g_string_erase (self->output, 0, 1);
}

static void
gbp_acp_terminal_read_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  g_autoptr(GbpAcpTerminal) self = user_data;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GError) error = NULL;
  const guint8 *data;
  gsize len;

  g_assert (GBP_IS_ACP_TERMINAL (self));

  bytes = g_input_stream_read_bytes_finish (G_INPUT_STREAM (object), result, &error);

  if (bytes == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_debug ("terminal %s read failed: %s", self->id, error->message);
      return;
    }

  data = g_bytes_get_data (bytes, &len);
  if (len == 0)
    return;

  g_string_append_len (self->output, (const gchar *)data, len);
  gbp_acp_terminal_enforce_limit (self);

  g_input_stream_read_bytes_async (G_INPUT_STREAM (object),
                                   8192,
                                   G_PRIORITY_DEFAULT,
                                   self->cancellable,
                                   gbp_acp_terminal_read_cb,
                                   g_object_ref (self));
}

static void
gbp_acp_terminal_complete_waiters (GbpAcpTerminal *self)
{
  for (GList *iter = self->waiters; iter != NULL; iter = iter->next)
    g_task_return_boolean (G_TASK (iter->data), TRUE);
  g_clear_list (&self->waiters, g_object_unref);
}

static void
gbp_acp_terminal_wait_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  g_autoptr(GbpAcpTerminal) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_ACP_TERMINAL (self));
  g_assert (IDE_IS_SUBPROCESS (object));

  if (!ide_subprocess_wait_finish (IDE_SUBPROCESS (object), result, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_debug ("terminal %s wait failed: %s", self->id, error->message);
      return;
    }

  self->exited = TRUE;

  if (ide_subprocess_get_if_exited (self->subprocess))
    {
      self->has_exit_code = TRUE;
      self->exit_code = ide_subprocess_get_exit_status (self->subprocess);
    }
  else if (ide_subprocess_get_if_signaled (self->subprocess))
    {
      gint sig = ide_subprocess_get_term_sig (self->subprocess);
      g_clear_pointer (&self->signal, g_free);
      self->signal = g_strdup (g_strsignal (sig));
    }

  gbp_acp_terminal_complete_waiters (self);
}

void
gbp_acp_terminal_start (GbpAcpTerminal *self)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(GError) error = NULL;
  GInputStream *stdout_pipe;

  g_return_if_fail (GBP_IS_ACP_TERMINAL (self));
  g_return_if_fail (self->subprocess == NULL);

  self->cancellable = g_cancellable_new ();

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                          G_SUBPROCESS_FLAGS_STDERR_MERGE);
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);

  if (self->cwd != NULL)
    ide_subprocess_launcher_set_cwd (launcher, self->cwd);

  ide_subprocess_launcher_push_argv (launcher, self->command);

  if (self->args != NULL)
    for (guint i = 0; self->args[i] != NULL; i++)
      ide_subprocess_launcher_push_argv (launcher, self->args[i]);

  if (self->env_names != NULL && self->env_values != NULL)
    for (guint i = 0; self->env_names[i] != NULL; i++)
      ide_subprocess_launcher_setenv (launcher,
                                      self->env_names[i],
                                      self->env_values[i],
                                      TRUE);

  self->subprocess = ide_subprocess_launcher_spawn (launcher, self->cancellable, &error);

  if (self->subprocess == NULL)
    {
      g_warning ("Failed to spawn terminal command “%s”: %s",
                 self->command,
                 error->message);
      self->exited = TRUE;
      gbp_acp_terminal_complete_waiters (self);
      return;
    }

  stdout_pipe = ide_subprocess_get_stdout_pipe (self->subprocess);

  if (stdout_pipe != NULL)
    g_input_stream_read_bytes_async (stdout_pipe,
                                     8192,
                                     G_PRIORITY_DEFAULT,
                                     self->cancellable,
                                     gbp_acp_terminal_read_cb,
                                     g_object_ref (self));

  ide_subprocess_wait_async (self->subprocess,
                             self->cancellable,
                             gbp_acp_terminal_wait_cb,
                             g_object_ref (self));
}

void
gbp_acp_terminal_wait_async (GbpAcpTerminal     *self,
                             GCancellable       *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer            user_data)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (GBP_IS_ACP_TERMINAL (self));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_acp_terminal_wait_async);

  if (self->exited)
    g_task_return_boolean (task, TRUE);
  else
    self->waiters = g_list_prepend (self->waiters, g_steal_pointer (&task));
}

gboolean
gbp_acp_terminal_wait_finish (GbpAcpTerminal  *self,
                              GAsyncResult    *result,
                              GError         **error)
{
  g_return_val_if_fail (GBP_IS_ACP_TERMINAL (self), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

void
gbp_acp_terminal_kill (GbpAcpTerminal *self)
{
  g_return_if_fail (GBP_IS_ACP_TERMINAL (self));

  if (self->subprocess != NULL && !self->exited)
    ide_subprocess_send_signal (self->subprocess, SIGTERM);
}
