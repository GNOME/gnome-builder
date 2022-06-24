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
};

G_DEFINE_FINAL_TYPE (IdeRunContext, ide_run_context, G_TYPE_OBJECT)

IdeRunContext *
ide_run_context_new (void)
{
  return g_object_new (IDE_TYPE_RUN_CONTEXT, NULL);
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
  ide_run_context_append_argv (self, "--clear-env");
  ide_run_context_append_argv (self, "--share-pids");
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
      if (!ide_run_context_merge_unix_fd_map (self, unix_fd_map, error))
        return FALSE;

      for (guint i = 0; i < length; i++)
        {
          int source_fd;
          int dest_fd;

          source_fd = ide_unix_fd_map_peek (unix_fd_map, i, &dest_fd);

          if (source_fd != -1 && dest_fd != -1)
            ide_run_context_append_formatted (self, "--forward-fd=%d", dest_fd);
        }
    }

  /* Now append the arguments */
  ide_run_context_append_args (self, argv);

  return TRUE;
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

  if (ide_is_flatpak ())
    ide_run_context_push (self,
                          ide_run_context_host_handler,
                          NULL,
                          NULL);
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
      g_autoptr(GArray) newenv = g_array_new (TRUE, TRUE, sizeof (char *));

      for (guint i = 0; env[i]; i++)
        {
          char *expanded = wordexp_with_environ (env[i], environ);
          g_array_append_val (newenv, expanded);
        }

      ide_run_context_set_environ (self, (const char * const *)(gpointer)newenv->data);
    }

  if (argv != NULL)
    {
      g_autoptr(GArray) newargv = g_array_new (TRUE, TRUE, sizeof (char *));

      for (guint i = 0; argv[i]; i++)
        {
          char *expanded = wordexp_with_environ (argv[i], environ);
          g_array_append_val (newargv, expanded);
        }

      ide_run_context_set_argv (self, (const char * const *)(gpointer)newargv->data);
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

  if (g_strcmp0 (cwd, layer->cwd) != 0)
    {
      g_free (layer->cwd);
      layer->cwd = g_strdup (cwd);
    }
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
  g_return_if_fail (source_fd > -1);
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
  guint length;

  g_return_val_if_fail (IDE_IS_RUN_CONTEXT (self), NULL);
  g_return_val_if_fail (self->ended == FALSE, NULL);

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

  length = ide_unix_fd_map_get_length (self->root.unix_fd_map);

  for (guint i = 0; i < length; i++)
    {
      int source_fd;
      int dest_fd;

      source_fd = ide_unix_fd_map_steal (self->root.unix_fd_map, i, &dest_fd);

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
