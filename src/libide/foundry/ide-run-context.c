/* ide-run-context.c
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

#define G_LOG_DOMAIN "ide-run-context"

#include "config.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <libide-io.h>

#include "ide-private.h"

#include "ide-run-context.h"

typedef struct
{
  GList                 qlink;
  char                 *cwd;
  GArray               *argv;
  GArray               *env;
  IdeUnixFDMap         *unix_fd_map;
  IdeRunContextHandler  handler;
  gpointer              handler_data;
  GDestroyNotify        handler_data_destroy;
} IdeRunContextLayer;

struct _IdeRunContext
{
  GObject            parent_instance;
  GQueue             layers;
  IdeRunContextLayer root;
  guint              ended : 1;
  guint              setup_tty : 1;
};

G_DEFINE_FINAL_TYPE (IdeRunContext, ide_run_context, G_TYPE_OBJECT)

IdeRunContext *
ide_run_context_new (void)
{
  return g_object_new (IDE_TYPE_RUN_CONTEXT, NULL);
}

static void
copy_envvar_with_fallback (IdeRunContext      *run_context,
                           const char * const *environ,
                           const char         *key,
                           const char         *fallback)
{
  const char *val;

  if ((val = g_environ_getenv ((char **)environ, key)))
    ide_run_context_setenv (run_context, key, val);
  else if (fallback != NULL)
    ide_run_context_setenv (run_context, key, fallback);
}

/**
 * ide_run_context_add_minimal_environment:
 * @self: a #IdeRunContext
 *
 * Adds a minimal set of environment variables.
 *
 * This is useful to get access to things like the display or other
 * expected variables.
 */
void
ide_run_context_add_minimal_environment (IdeRunContext *self)
{
  const gchar * const *host_environ = _ide_host_environ ();
  static const char *copy_env[] = {
    "AT_SPI_BUS_ADDRESS",
    "DBUS_SESSION_BUS_ADDRESS",
    "DBUS_SYSTEM_BUS_ADDRESS",
    "DESKTOP_SESSION",
    "DISPLAY",
    "LANG",
    "HOME",
    "SHELL",
    "SSH_AUTH_SOCK",
    "USER",
    "WAYLAND_DISPLAY",
    "XAUTHORITY",
    "XDG_CURRENT_DESKTOP",
    "XDG_MENU_PREFIX",
    "XDG_SEAT",
    "XDG_SESSION_DESKTOP",
    "XDG_SESSION_ID",
    "XDG_SESSION_TYPE",
    "XDG_VTNR",
  };
  const char *val;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_RUN_CONTEXT (self));

  for (guint i = 0; i < G_N_ELEMENTS (copy_env); i++)
    {
      const char *key = copy_env[i];

      if ((val = g_environ_getenv ((char **)host_environ, key)))
        ide_run_context_setenv (self, key, val);
    }

  copy_envvar_with_fallback (self, host_environ, "TERM", "xterm-256color");
  copy_envvar_with_fallback (self, host_environ, "COLORTERM", "truecolor");

  IDE_EXIT;
}

static void
ide_run_context_layer_clear (IdeRunContextLayer *layer)
{
  g_assert (layer != NULL);
  g_assert (layer->qlink.data == layer);
  g_assert (layer->qlink.prev == NULL);
  g_assert (layer->qlink.next == NULL);

  if (layer->handler_data_destroy)
    g_clear_pointer (&layer->handler_data, layer->handler_data_destroy);

  g_clear_pointer (&layer->cwd, g_free);
  g_clear_pointer (&layer->argv, g_array_unref);
  g_clear_pointer (&layer->env, g_array_unref);
  g_clear_object (&layer->unix_fd_map);
}

static void
ide_run_context_layer_free (IdeRunContextLayer *layer)
{
  ide_run_context_layer_clear (layer);

  g_slice_free (IdeRunContextLayer, layer);
}

static void
strptr_free (gpointer data)
{
  char **strptr = data;
  g_clear_pointer (strptr, g_free);
}

static void
ide_run_context_layer_init (IdeRunContextLayer *layer)
{
  g_assert (layer != NULL);

  layer->qlink.data = layer;
  layer->argv = g_array_new (TRUE, TRUE, sizeof (char *));
  layer->env = g_array_new (TRUE, TRUE, sizeof (char *));
  layer->unix_fd_map = ide_unix_fd_map_new ();

  g_array_set_clear_func (layer->argv, strptr_free);
  g_array_set_clear_func (layer->env, strptr_free);
}

static IdeRunContextLayer *
ide_run_context_current_layer (IdeRunContext *self)
{
  g_assert (IDE_IS_RUN_CONTEXT (self));
  g_assert (self->layers.length > 0);

  return self->layers.head->data;
}

static void
ide_run_context_dispose (GObject *object)
{
  IdeRunContext *self = (IdeRunContext *)object;
  IdeRunContextLayer *layer;

  while ((layer = g_queue_peek_head (&self->layers)))
    {
      g_queue_unlink (&self->layers, &layer->qlink);
      if (layer != &self->root)
        ide_run_context_layer_free (layer);
    }

  ide_run_context_layer_clear (&self->root);

  G_OBJECT_CLASS (ide_run_context_parent_class)->dispose (object);
}

static void
ide_run_context_class_init (IdeRunContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_run_context_dispose;
}

static void
ide_run_context_init (IdeRunContext *self)
{
  ide_run_context_layer_init (&self->root);

  g_queue_push_head_link (&self->layers, &self->root.qlink);

  self->setup_tty = TRUE;
}

void
ide_run_context_push (IdeRunContext        *self,
                      IdeRunContextHandler  handler,
                      gpointer              handler_data,
                      GDestroyNotify        handler_data_destroy)
{
  IdeRunContextLayer *layer;

  g_return_if_fail (IDE_IS_RUN_CONTEXT (self));

  layer = g_slice_new0 (IdeRunContextLayer);

  ide_run_context_layer_init (layer);

  layer->handler = handler;
  layer->handler_data = handler_data;
  layer->handler_data_destroy = handler_data_destroy;

  g_queue_push_head_link (&self->layers, &layer->qlink);
}

void
ide_run_context_push_at_base (IdeRunContext        *self,
                              IdeRunContextHandler  handler,
                              gpointer              handler_data,
                              GDestroyNotify        handler_data_destroy)
{
  IdeRunContextLayer *layer;

  g_return_if_fail (IDE_IS_RUN_CONTEXT (self));

  layer = g_slice_new0 (IdeRunContextLayer);

  ide_run_context_layer_init (layer);

  layer->handler = handler;
  layer->handler_data = handler_data;
  layer->handler_data_destroy = handler_data_destroy;

  g_queue_insert_before_link (&self->layers, &self->root.qlink, &layer->qlink);
}

static gboolean
ide_run_context_host_handler (IdeRunContext       *self,
                              const char * const  *argv,
                              const char * const  *env,
                              const char          *cwd,
                              IdeUnixFDMap        *unix_fd_map,
                              gpointer             user_data,
                              GError             **error)
{
  guint length;

  g_assert (IDE_IS_RUN_CONTEXT (self));
  g_assert (argv != NULL);
  g_assert (env != NULL);
  g_assert (IDE_IS_UNIX_FD_MAP (unix_fd_map));
  g_assert (ide_is_flatpak ());

  ide_run_context_append_argv (self, "flatpak-spawn");
  ide_run_context_append_argv (self, "--host");
  ide_run_context_append_argv (self, "--watch-bus");

  if (env != NULL)
    {
      for (guint i = 0; env[i]; i++)
        ide_run_context_append_formatted (self, "--env=%s", env[i]);
    }

  if (cwd != NULL)
    ide_run_context_append_formatted (self, "--directory=%s", cwd);

  if ((length = ide_unix_fd_map_get_length (unix_fd_map)))
    {
      for (guint i = 0; i < length; i++)
        {
          int source_fd;
          int dest_fd;

          source_fd = ide_unix_fd_map_peek (unix_fd_map, i, &dest_fd);

          if (dest_fd < STDERR_FILENO)
            continue;

          g_debug ("Mapping Builder FD %d to target FD %d via flatpak-spawn",
                   source_fd, dest_fd);

          if (source_fd != -1 && dest_fd != -1)
            ide_run_context_append_formatted (self, "--forward-fd=%d", dest_fd);
        }

      if (!ide_run_context_merge_unix_fd_map (self, unix_fd_map, error))
        return FALSE;
    }

  /* Now append the arguments */
  ide_run_context_append_args (self, argv);

  return TRUE;
}

static gboolean
is_empty (IdeRunContext *self)
{
  IdeRunContextLayer *root;

  g_assert (IDE_IS_RUN_CONTEXT (self));

  if (self->layers.length > 1)
    return FALSE;

  root = g_queue_peek_head (&self->layers);

  return root->argv->len == 0;
}

/**
 * ide_run_context_push_host:
 * @self: a #IdeRunContext
 *
 * Pushes handler to transform command to run on host.
 *
 * If necessary, a layer is pushed to ensure the command is run on the
 * host instead of the application container.
 *
 * If Builder is running on the host already, this function does nothing.
 */
void
ide_run_context_push_host (IdeRunContext *self)
{
  g_return_if_fail (IDE_IS_RUN_CONTEXT (self));

  /* We use flatpak-spawn to jump to the host even if we're
   * inside a container like toolbox first.
   */
  if (ide_is_flatpak () || !is_empty (self))
    {
      self->setup_tty = FALSE;
      ide_run_context_push (self,
                            ide_run_context_host_handler,
                            NULL,
                            NULL);
    }
}

typedef struct
{
  char *shell;
  IdeRunContextShell kind : 2;
} Shell;

static void
shell_free (gpointer data)
{
  Shell *shell = data;
  g_clear_pointer (&shell->shell, g_free);
  g_slice_free (Shell, shell);
}

static gboolean
ide_run_context_shell_handler (IdeRunContext       *self,
                               const char * const  *argv,
                               const char * const  *env,
                               const char          *cwd,
                               IdeUnixFDMap        *unix_fd_map,
                               gpointer             user_data,
                               GError             **error)
{
  Shell *shell = user_data;
  g_autoptr(GString) str = NULL;

  g_assert (IDE_IS_RUN_CONTEXT (self));
  g_assert (argv != NULL);
  g_assert (env != NULL);
  g_assert (IDE_IS_UNIX_FD_MAP (unix_fd_map));
  g_assert (shell != NULL);
  g_assert (shell->shell != NULL);

  if (!ide_run_context_merge_unix_fd_map (self, unix_fd_map, error))
    return FALSE;

  if (cwd != NULL)
    ide_run_context_set_cwd (self, cwd);

  ide_run_context_append_argv (self, shell->shell);
  if (shell->kind == IDE_RUN_CONTEXT_SHELL_LOGIN)
    ide_run_context_append_argv (self, "-l");
  else if (shell->kind == IDE_RUN_CONTEXT_SHELL_INTERACTIVE)
    ide_run_context_append_argv (self, "-i");
  ide_run_context_append_argv (self, "-c");

  str = g_string_new (NULL);

  if (env[0] != NULL)
    {
      g_string_append (str, "env");

      for (guint i = 0; env[i]; i++)
        {
          g_autofree char *quoted = g_shell_quote (env[i]);

          g_string_append_c (str, ' ');
          g_string_append (str, quoted);
        }

      g_string_append_c (str, ' ');
    }

  for (guint i = 0; argv[i]; i++)
    {
      g_autofree char *quoted = g_shell_quote (argv[i]);

      if (i > 0)
        g_string_append_c (str, ' ');
      g_string_append (str, quoted);
    }

  ide_run_context_append_argv (self, str->str);

  return TRUE;
}

/**
 * ide_run_context_push_shell:
 * @self: a #IdeRunContext
 * @shell: the kind of shell to be used
 *
 * Pushes a shell which can run the upper layer command with -c.
 */
void
ide_run_context_push_shell (IdeRunContext      *self,
                            IdeRunContextShell  shell)
{
  Shell *state;

  g_return_if_fail (IDE_IS_RUN_CONTEXT (self));

  state = g_slice_new0 (Shell);
  state->shell = g_strdup ("/bin/sh");
  state->kind = shell;

  ide_run_context_push (self, ide_run_context_shell_handler, state, shell_free);
}

void
ide_run_context_push_user_shell (IdeRunContext      *self,
                                 IdeRunContextShell  shell)
{
  const char *user_shell;
  Shell *state;

  g_return_if_fail (IDE_IS_RUN_CONTEXT (self));

  user_shell = ide_get_user_shell ();

  if (!ide_shell_supports_dash_c (user_shell))
    user_shell = "/bin/sh";

  switch (shell)
    {
    default:
    case IDE_RUN_CONTEXT_SHELL_DEFAULT:
      break;

    case IDE_RUN_CONTEXT_SHELL_LOGIN:
      if (!ide_shell_supports_dash_login (user_shell))
        user_shell = "/bin/sh";
      break;

    case IDE_RUN_CONTEXT_SHELL_INTERACTIVE:
      break;
    }

  state = g_slice_new0 (Shell);
  state->shell = g_strdup (user_shell);
  state->kind = shell;

  ide_run_context_push (self, ide_run_context_shell_handler, state, shell_free);
}

static gboolean
ide_run_context_error_handler (IdeRunContext       *self,
                               const char * const  *argv,
                               const char * const  *env,
                               const char          *cwd,
                               IdeUnixFDMap        *unix_fd_map,
                               gpointer             user_data,
                               GError             **error)
{
  const GError *local_error = user_data;

  g_assert (IDE_IS_RUN_CONTEXT (self));
  g_assert (IDE_IS_UNIX_FD_MAP (unix_fd_map));
  g_assert (local_error != NULL);

  if (error != NULL)
    *error = g_error_copy (local_error);

  return FALSE;
}

/**
 * ide_run_context_push_error:
 * @self: a #IdeRunContext
 * @error: (transfer full) (in): a #GError
 *
 * Pushes a new layer that will always fail with @error.
 *
 * This is useful if you have an error when attempting to build
 * a run command, but need it to deliver the error when attempting
 * to create a subprocess launcher.
 */
void
ide_run_context_push_error (IdeRunContext *self,
                            GError        *error)
{
  g_return_if_fail (IDE_IS_RUN_CONTEXT (self));
  g_return_if_fail (error != NULL);

  ide_run_context_push (self,
                        ide_run_context_error_handler,
                        error,
                        (GDestroyNotify)g_error_free);
}

static gboolean
next_variable (const char *str,
               guint      *cursor,
               guint      *begin)
{
  for (guint i = *cursor; str[i]; i++)
    {
      /* Skip past escaped $ */
      if (str[i] == '\\' && str[i+1] == '$')
        {
          i++;
          continue;
        }

      if (str[i] == '$')
        {
          *begin = i;
          *cursor = i;

          for (guint j = i+1; str[j]; j++)
            {
              if (!g_ascii_isalnum (str[j]) && str[j] != '_')
                {
                  *cursor = j;
                  break;
                }
            }

          if (*cursor > ((*begin) + 1))
            return TRUE;
        }
    }

  return FALSE;
}

static char *
wordexp_with_environ (const char         *input,
                      const char * const *environ)
{
  g_autoptr(GString) str = NULL;
  guint cursor = 0;
  guint begin;

  g_assert (input != NULL);
  g_assert (environ != NULL);

  str = g_string_new (input);

  while (next_variable (str->str, &cursor, &begin))
    {
      g_autofree char *key = NULL;
      guint key_len = cursor - begin;
      const char *value;
      guint value_len;

      g_assert (str->str[begin] == '$');

      key = g_strndup (str->str + begin, key_len);
      value = g_environ_getenv ((char **)environ, key+1);
      value_len = value ? strlen (value) : 0;

      if (value != NULL)
        {
          g_string_erase (str, begin, key_len);
          g_string_insert_len (str, begin, value, value_len);

          if (value_len > key_len)
            cursor += (value_len - key_len);
          else if (value_len < key_len)
            cursor -= (key_len - value_len);
        }
    }

  return g_string_free (g_steal_pointer (&str), FALSE);
}

static gboolean
ide_run_context_expansion_handler (IdeRunContext       *self,
                                   const char * const  *argv,
                                   const char * const  *env,
                                   const char          *cwd,
                                   IdeUnixFDMap        *unix_fd_map,
                                   gpointer             user_data,
                                   GError             **error)
{
  const char * const *environ = user_data;

  IDE_ENTRY;

  g_assert (IDE_IS_RUN_CONTEXT (self));
  g_assert (argv != NULL);
  g_assert (environ != NULL);
  g_assert (IDE_IS_UNIX_FD_MAP (unix_fd_map));

  if (!ide_run_context_merge_unix_fd_map (self, unix_fd_map, error))
    IDE_RETURN (FALSE);

  if (cwd != NULL)
    {
      g_autofree char *newcwd = wordexp_with_environ (cwd, environ);
      g_autofree char *expanded = ide_path_expand (newcwd);

      ide_run_context_set_cwd (self, expanded);
    }

  if (env != NULL)
    {
      g_autoptr(GPtrArray) newenv = g_ptr_array_new_null_terminated (0, g_free, TRUE);

      for (guint i = 0; env[i]; i++)
        {
          char *expanded = wordexp_with_environ (env[i], environ);
          g_ptr_array_add (newenv, expanded);
        }

      ide_run_context_add_environ (self, (const char * const *)(gpointer)newenv->pdata);
    }

  if (argv != NULL)
    {
      g_autoptr(GPtrArray) newargv = g_ptr_array_new_null_terminated (0, g_free, TRUE);

      for (guint i = 0; argv[i]; i++)
        {
          char *expanded = wordexp_with_environ (argv[i], environ);
          g_ptr_array_add (newargv, expanded);
        }

      ide_run_context_append_args (self, (const char * const *)(gpointer)newargv->pdata);
    }

  IDE_RETURN (TRUE);
}

/**
 * ide_run_context_push_expansion:
 * @self: a #IdeRunContext
 *
 * Pushes a layer to expand known environment variables.
 *
 * The command argv and cwd will have `$FOO` style environment
 * variables expanded that are known. This can be useful to allow
 * things like `$BUILDDIR` be expanded at this layer.
 */
void
ide_run_context_push_expansion (IdeRunContext      *self,
                                const char * const *environ)
{
  g_return_if_fail (IDE_IS_RUN_CONTEXT (self));

  if (environ != NULL)
    ide_run_context_push (self,
                          ide_run_context_expansion_handler,
                          g_strdupv ((char **)environ),
                          (GDestroyNotify)g_strfreev);
}

const char * const *
ide_run_context_get_argv (IdeRunContext *self)
{
  IdeRunContextLayer *layer;

  g_return_val_if_fail (IDE_IS_RUN_CONTEXT (self), NULL);

  layer = ide_run_context_current_layer (self);

  return (const char * const *)(gpointer)layer->argv->data;
}

void
ide_run_context_set_argv (IdeRunContext      *self,
                          const char * const *argv)
{
  IdeRunContextLayer *layer;

  g_return_if_fail (IDE_IS_RUN_CONTEXT (self));

  layer = ide_run_context_current_layer (self);

  g_array_set_size (layer->argv, 0);

  if (argv != NULL)
    {
      char **copy = g_strdupv ((char **)argv);
      g_array_append_vals (layer->argv, copy, g_strv_length (copy));
      g_free (copy);
    }
}

const char * const *
ide_run_context_get_environ (IdeRunContext *self)
{
  IdeRunContextLayer *layer;

  g_return_val_if_fail (IDE_IS_RUN_CONTEXT (self), NULL);

  layer = ide_run_context_current_layer (self);

  return (const char * const *)(gpointer)layer->env->data;
}

void
ide_run_context_set_environ (IdeRunContext      *self,
                             const char * const *environ)
{
  IdeRunContextLayer *layer;

  g_return_if_fail (IDE_IS_RUN_CONTEXT (self));

  layer = ide_run_context_current_layer (self);

  g_array_set_size (layer->env, 0);

  if (environ != NULL && environ[0] != NULL)
    {
      char **copy = g_strdupv ((char **)environ);
      g_array_append_vals (layer->env, copy, g_strv_length (copy));
      g_free (copy);
    }
}

void
ide_run_context_add_environ (IdeRunContext      *self,
                             const char * const *environ)
{
  IdeRunContextLayer *layer;

  g_return_if_fail (IDE_IS_RUN_CONTEXT (self));

  if (environ == NULL || environ[0] == NULL)
    return;

  layer = ide_run_context_current_layer (self);

  for (guint i = 0; environ[i]; i++)
    {
      const char *pair = environ[i];
      const char *eq = strchr (pair, '=');
      char **dest = NULL;
      gsize keylen;

      if (eq == NULL)
        continue;

      keylen = eq - pair;

      for (guint j = 0; j < layer->env->len; j++)
        {
          const char *ele = g_array_index (layer->env, const char *, j);

          if (strncmp (pair, ele, keylen) == 0 && ele[keylen] == '=')
            {
              dest = &g_array_index (layer->env, char *, j);
              break;
            }
        }

      if (dest == NULL)
        {
          g_array_set_size (layer->env, layer->env->len + 1);
          dest = &g_array_index (layer->env, char *, layer->env->len - 1);
        }

      g_clear_pointer (dest, g_free);
      *dest = g_strdup (pair);
    }
}

const char *
ide_run_context_get_cwd (IdeRunContext *self)
{
  IdeRunContextLayer *layer;

  g_return_val_if_fail (IDE_IS_RUN_CONTEXT (self), NULL);

  layer = ide_run_context_current_layer (self);

  return layer->cwd;
}

void
ide_run_context_set_cwd (IdeRunContext *self,
                         const char    *cwd)
{
  IdeRunContextLayer *layer;

  g_return_if_fail (IDE_IS_RUN_CONTEXT (self));

  layer = ide_run_context_current_layer (self);

  g_set_str (&layer->cwd, cwd);
}

void
ide_run_context_prepend_argv (IdeRunContext *self,
                              const char    *arg)
{
  IdeRunContextLayer *layer;
  char *copy;

  g_return_if_fail (IDE_IS_RUN_CONTEXT (self));
  g_return_if_fail (arg != NULL);

  layer = ide_run_context_current_layer (self);

  copy = g_strdup (arg);
  g_array_insert_val (layer->argv, 0, copy);
}

void
ide_run_context_prepend_args (IdeRunContext      *self,
                              const char * const *args)
{
  IdeRunContextLayer *layer;
  char **copy;

  g_return_if_fail (IDE_IS_RUN_CONTEXT (self));

  if (args == NULL || args[0] == NULL)
    return;

  layer = ide_run_context_current_layer (self);

  copy = g_strdupv ((char **)args);
  g_array_insert_vals (layer->argv, 0, copy, g_strv_length (copy));
  g_free (copy);
}

void
ide_run_context_append_argv (IdeRunContext *self,
                             const char    *arg)
{
  IdeRunContextLayer *layer;
  char *copy;

  g_return_if_fail (IDE_IS_RUN_CONTEXT (self));
  g_return_if_fail (arg != NULL);

  layer = ide_run_context_current_layer (self);

  copy = g_strdup (arg);
  g_array_append_val (layer->argv, copy);
}

void
ide_run_context_append_formatted (IdeRunContext *self,
                                  const char    *format,
                                  ...)
{
  g_autofree char *arg = NULL;
  va_list args;

  g_return_if_fail (IDE_IS_RUN_CONTEXT (self));
  g_return_if_fail (format != NULL);

  va_start (args, format);
  arg = g_strdup_vprintf (format, args);
  va_end (args);

  ide_run_context_append_argv (self, arg);
}

void
ide_run_context_append_args (IdeRunContext      *self,
                             const char * const *args)
{
  IdeRunContextLayer *layer;
  char **copy;

  g_return_if_fail (IDE_IS_RUN_CONTEXT (self));

  if (args == NULL || args[0] == NULL)
    return;

  layer = ide_run_context_current_layer (self);

  copy = g_strdupv ((char **)args);
  g_array_append_vals (layer->argv, copy, g_strv_length (copy));
  g_free (copy);
}

gboolean
ide_run_context_append_args_parsed (IdeRunContext  *self,
                                    const char     *args,
                                    GError        **error)
{
  IdeRunContextLayer *layer;
  char **argv = NULL;
  int argc;

  g_return_val_if_fail (IDE_IS_RUN_CONTEXT (self), FALSE);
  g_return_val_if_fail (args != NULL, FALSE);

  layer = ide_run_context_current_layer (self);

  if (!g_shell_parse_argv (args, &argc, &argv, error))
    return FALSE;

  g_array_append_vals (layer->argv, argv, argc);
  g_free (argv);

  return TRUE;
}

void
ide_run_context_take_fd (IdeRunContext *self,
                         int            source_fd,
                         int            dest_fd)
{
  IdeRunContextLayer *layer;

  g_return_if_fail (IDE_IS_RUN_CONTEXT (self));
  g_return_if_fail (source_fd >= -1);
  g_return_if_fail (dest_fd > -1);

  layer = ide_run_context_current_layer (self);

  ide_unix_fd_map_take (layer->unix_fd_map, source_fd, dest_fd);
}

const char *
ide_run_context_getenv (IdeRunContext *self,
                        const char    *key)
{
  IdeRunContextLayer *layer;
  gsize keylen;

  g_return_val_if_fail (IDE_IS_RUN_CONTEXT (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  layer = ide_run_context_current_layer (self);

  keylen = strlen (key);

  for (guint i = 0; i < layer->env->len; i++)
    {
      const char *envvar = g_array_index (layer->env, const char *, i);

      if (strncmp (key, envvar, keylen) == 0 && envvar[keylen] == '=')
        return &envvar[keylen+1];
    }

  return NULL;
}

void
ide_run_context_setenv (IdeRunContext *self,
                        const char    *key,
                        const char    *value)
{
  IdeRunContextLayer *layer;
  char *element;
  gsize keylen;

  g_return_if_fail (IDE_IS_RUN_CONTEXT (self));
  g_return_if_fail (key != NULL);

  if (value == NULL)
    {
      ide_run_context_unsetenv (self, key);
      return;
    }

  layer = ide_run_context_current_layer (self);

  keylen = strlen (key);
  element = g_strconcat (key, "=", value, NULL);

  g_array_append_val (layer->env, element);

  for (guint i = 0; i < layer->env->len-1; i++)
    {
      const char *envvar = g_array_index (layer->env, const char *, i);

      if (strncmp (key, envvar, keylen) == 0 && envvar[keylen] == '=')
        {
          g_array_remove_index_fast (layer->env, i);
          break;
        }
    }
}

void
ide_run_context_unsetenv (IdeRunContext *self,
                          const char    *key)
{
  IdeRunContextLayer *layer;
  gsize len;

  g_return_if_fail (IDE_IS_RUN_CONTEXT (self));
  g_return_if_fail (key != NULL);

  layer = ide_run_context_current_layer (self);

  len = strlen (key);

  for (guint i = 0; i < layer->env->len; i++)
    {
      const char *envvar = g_array_index (layer->env, const char *, i);

      if (strncmp (key, envvar, len) == 0 && envvar[len] == '=')
        {
          g_array_remove_index_fast (layer->env, i);
          return;
        }
    }
}

void
ide_run_context_environ_to_argv (IdeRunContext *self)
{
  IdeRunContextLayer *layer;
  const char **copy;

  g_assert (IDE_IS_RUN_CONTEXT (self));

  layer = ide_run_context_current_layer (self);

  if (layer->env->len == 0)
    return;

  copy = (const char **)g_new0 (char *, layer->env->len + 2);
  copy[0] = "env";
  for (guint i = 0; i < layer->env->len; i++)
    copy[1+i] = g_array_index (layer->env, const char *, i);
  ide_run_context_prepend_args (self, (const char * const *)copy);
  g_free (copy);

  g_array_set_size (layer->env, 0);
}

static gboolean
ide_run_context_default_handler (IdeRunContext       *self,
                                 const char * const  *argv,
                                 const char * const  *env,
                                 const char          *cwd,
                                 IdeUnixFDMap        *unix_fd_map,
                                 gpointer             user_data,
                                 GError             **error)
{
  IdeRunContextLayer *layer;

  g_assert (IDE_IS_RUN_CONTEXT (self));
  g_assert (argv != NULL);
  g_assert (env != NULL);
  g_assert (IDE_IS_UNIX_FD_MAP (unix_fd_map));

  layer = ide_run_context_current_layer (self);

  if (cwd != NULL)
    {
      /* If the working directories do not match, we can't satisfy this and
       * need to error out.
       */
      if (layer->cwd != NULL && !ide_str_equal (cwd, layer->cwd))
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "Cannot resolve differently requested cwd: %s and %s",
                       cwd, layer->cwd);
          return FALSE;
        }

      ide_run_context_set_cwd (self, cwd);
    }

  /* Merge all the FDs unless there are collisions */
  if (!ide_unix_fd_map_steal_from (layer->unix_fd_map, unix_fd_map, error))
    return FALSE;

  if (env[0] != NULL)
    {
      if (argv[0] == NULL)
        {
          ide_run_context_add_environ (self, env);
        }
      else
        {
          ide_run_context_append_argv (self, "env");
          ide_run_context_append_args (self, env);
        }
    }

  if (argv[0] != NULL)
    ide_run_context_append_args (self, argv);

  return TRUE;
}

static int
sort_strptr (gconstpointer a,
             gconstpointer b)
{
  const char * const *astr = a;
  const char * const *bstr = b;

  return g_strcmp0 (*astr, *bstr);
}

static gboolean
ide_run_context_callback_layer (IdeRunContext       *self,
                                IdeRunContextLayer  *layer,
                                GError             **error)
{
  IdeRunContextHandler handler;
  gpointer handler_data;
  gboolean ret;

  g_assert (IDE_IS_RUN_CONTEXT (self));
  g_assert (layer != NULL);
  g_assert (layer != &self->root);

  handler = layer->handler ? layer->handler : ide_run_context_default_handler;
  handler_data = layer->handler ? layer->handler_data : NULL;

  /* Sort environment variables first so that we have an easier time
   * finding them by eye in tooling which translates them.
   */
  g_array_sort (layer->env, sort_strptr);

  ret = handler (self,
                 (const char * const *)(gpointer)layer->argv->data,
                 (const char * const *)(gpointer)layer->env->data,
                 layer->cwd,
                 layer->unix_fd_map,
                 handler_data,
                 error);

  ide_run_context_layer_free (layer);

  return ret;
}

/**
 * ide_run_context_end:
 * @self: a #IdeRunContext
 *
 * Returns: (transfer full): an #IdeSubprocessLauncher if successful; otherwise
 *   %NULL and @error is set.
 */
IdeSubprocessLauncher *
ide_run_context_end (IdeRunContext  *self,
                     GError        **error)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  GSubprocessFlags flags = 0;
  guint length;

  g_return_val_if_fail (IDE_IS_RUN_CONTEXT (self), NULL);
  g_return_val_if_fail (self->ended == FALSE, NULL);

#ifdef IDE_ENABLE_TRACE
  {
    guint j = 0;
    for (const GList *iter = self->layers.head;
         iter != NULL;
         iter = iter->next)
      {
        IdeRunContextLayer *layer = iter->data;

        IDE_TRACE_MSG ("[%d]:    CWD: %s", j++, layer->cwd);
        IDE_TRACE_MSG ("        N FDS: %u", ide_unix_fd_map_get_length (layer->unix_fd_map));
        IDE_TRACE_MSG ("  Environment:");
        for (guint i = 0; i < layer->env->len; i++)
          IDE_TRACE_MSG ("  [%02u]: %s", i, g_array_index (layer->env, char *, i));
        IDE_TRACE_MSG ("  Arguments:");
        for (guint i = 0; i < layer->argv->len; i++)
          IDE_TRACE_MSG ("  [%02u]: %s ", i, g_array_index (layer->argv, char *, i));
      }
  }
#endif

  self->ended = TRUE;

  while (self->layers.length > 1)
    {
      IdeRunContextLayer *layer = ide_run_context_current_layer (self);

      g_queue_unlink (&self->layers, &layer->qlink);

      if (!ide_run_context_callback_layer (self, layer, error))
        return FALSE;
    }

  launcher = ide_subprocess_launcher_new (0);

  ide_subprocess_launcher_set_argv (launcher, ide_run_context_get_argv (self));
  ide_subprocess_launcher_set_environ (launcher, ide_run_context_get_environ (self));
  ide_subprocess_launcher_set_cwd (launcher, ide_run_context_get_cwd (self));
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);

  length = ide_unix_fd_map_get_length (self->root.unix_fd_map);

  for (guint i = 0; i < length; i++)
    {
      int source_fd;
      int dest_fd;

      source_fd = ide_unix_fd_map_steal (self->root.unix_fd_map, i, &dest_fd);

      if (dest_fd == STDOUT_FILENO && source_fd == -1)
        flags |= G_SUBPROCESS_FLAGS_STDOUT_SILENCE;

      if (dest_fd == STDERR_FILENO && source_fd == -1)
        flags |= G_SUBPROCESS_FLAGS_STDERR_SILENCE;

      if (source_fd != -1 && dest_fd != -1)
        {
          if (dest_fd == STDIN_FILENO)
            ide_subprocess_launcher_take_stdin_fd (launcher, source_fd);
          else if (dest_fd == STDOUT_FILENO)
            ide_subprocess_launcher_take_stdout_fd (launcher, source_fd);
          else if (dest_fd == STDERR_FILENO)
            ide_subprocess_launcher_take_stderr_fd (launcher, source_fd);
          else
            ide_subprocess_launcher_take_fd (launcher, source_fd, dest_fd);
        }
    }

  ide_subprocess_launcher_set_flags (launcher, flags);
  ide_subprocess_launcher_set_setup_tty (launcher, self->setup_tty);

  return g_steal_pointer (&launcher);
}

/**
 * ide_run_context_spawn:
 * @self: a #IdeRunContext
 *
 * Spwans the run command.
 *
 * If there is a failure to build the command into a subprocess launcher,
 * then %NULL is returned and @error is set.
 *
 * If the subprocess fails to launch, then %NULL is returned and @error is set.
 *
 * Returns: (transfer full): an #IdeSubprocess if successful; otherwise %NULL
 *   and @error is set.
 */
IdeSubprocess *
ide_run_context_spawn (IdeRunContext  *self,
                       GError        **error)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) ret = NULL;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_RUN_CONTEXT (self), NULL);

  if (!(launcher = ide_run_context_end (self, error)))
    IDE_RETURN (NULL);

  if (!(ret = ide_subprocess_launcher_spawn (launcher, NULL, error)))
    IDE_RETURN (NULL);

  g_return_val_if_fail (IDE_IS_SUBPROCESS (ret), NULL);

  IDE_RETURN (g_steal_pointer (&ret));
}

/**
 * ide_run_context_merge_unix_fd_map:
 * @self: a #IdeRunContext
 * @unix_fd_map: a #IdeUnixFDMap
 * @error: a #GError, or %NULL
 *
 * Merges the #IdeUnixFDMap into the current layer.
 *
 * If there are collisions in destination FDs, then that may cause an
 * error and %FALSE is returned.
 *
 * @unix_fd_map will have the FDs stolen using ide_unix_fd_map_steal_from()
 * which means that if successful, @unix_fd_map will not have any open
 * file-descriptors after calling this function.
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
ide_run_context_merge_unix_fd_map (IdeRunContext  *self,
                                   IdeUnixFDMap   *unix_fd_map,
                                   GError        **error)
{
  IdeRunContextLayer *layer;

  g_return_val_if_fail (IDE_IS_RUN_CONTEXT (self), FALSE);
  g_return_val_if_fail (IDE_IS_UNIX_FD_MAP (unix_fd_map), FALSE);

  layer = ide_run_context_current_layer (self);

  return ide_unix_fd_map_steal_from (layer->unix_fd_map, unix_fd_map, error);
}

/**
 * ide_run_context_set_pty_fd:
 * @self: an #IdeRunContext
 * @consumer_fd: the FD of the PTY consumer
 *
 * Sets up a PTY for the run context that will communicate with the
 * consumer. The consumer is the generally the widget that is rendering
 * the PTY contents and the producer is the FD that is connected to the
 * subprocess.
 */
void
ide_run_context_set_pty_fd (IdeRunContext *self,
                            int            consumer_fd)
{
  int stdin_fd = -1;
  int stdout_fd = -1;
  int stderr_fd = -1;

  g_return_if_fail (IDE_IS_RUN_CONTEXT (self));

  if (consumer_fd < 0)
    return;

  if (-1 == (stdin_fd = ide_pty_intercept_create_producer (consumer_fd, TRUE)))
    {
      int errsv = errno;
      g_critical ("Failed to create PTY device: %s", g_strerror (errsv));
      return;
    }

  if (-1 == (stdout_fd = dup (stdin_fd)))
    {
      int errsv = errno;
      g_critical ("Failed to dup stdout FD: %s", g_strerror (errsv));
    }

  if (-1 == (stderr_fd = dup (stdin_fd)))
    {
      int errsv = errno;
      g_critical ("Failed to dup stderr FD: %s", g_strerror (errsv));
    }

  g_assert (stdin_fd > -1);
  g_assert (stdout_fd > -1);
  g_assert (stderr_fd > -1);

  ide_run_context_take_fd (self, stdin_fd, STDIN_FILENO);
  ide_run_context_take_fd (self, stdout_fd, STDOUT_FILENO);
  ide_run_context_take_fd (self, stderr_fd, STDERR_FILENO);
}

/**
 * ide_run_context_set_pty:
 * @self: a #IdeRunContext
 *
 * Sets the PTY for a run context.
 */
void
ide_run_context_set_pty (IdeRunContext *self,
                         VtePty        *pty)
{
  int consumer_fd;

  g_return_if_fail (IDE_IS_RUN_CONTEXT (self));
  g_return_if_fail (VTE_IS_PTY (pty));

  consumer_fd = vte_pty_get_fd (pty);

  if (consumer_fd != -1)
    ide_run_context_set_pty_fd (self, consumer_fd);
}

/**
 * ide_run_context_create_stdio_stream:
 * @self: a #IdeRunContext
 * @error: a location for a #GError
 *
 * Creates a stream to communicate with the subprocess using stdin/stdout.
 *
 * The stream is created using UNIX pipes which are attached to the
 * stdin/stdout of the child process.
 *
 * Returns: (transfer full): a #GIOStream if successful; otherwise
 *   %NULL and @error is set.
 */
GIOStream *
ide_run_context_create_stdio_stream (IdeRunContext  *self,
                                     GError        **error)
{
  IdeRunContextLayer *layer;

  g_return_val_if_fail (IDE_IS_RUN_CONTEXT (self), NULL);

  layer = ide_run_context_current_layer (self);

  return ide_unix_fd_map_create_stream (layer->unix_fd_map,
                                        STDIN_FILENO,
                                        STDOUT_FILENO,
                                        error);
}

