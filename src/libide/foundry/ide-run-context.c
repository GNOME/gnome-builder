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

#include <string.h>

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

const char * const *
ide_run_context_get_argv (IdeRunContext *self)
{
  IdeRunContextLayer *layer;

  g_return_val_if_fail (IDE_IS_RUN_CONTEXT (self), NULL);

  layer = ide_run_context_current_layer (self);

  return (const char * const *)&g_array_index (layer->argv, char *, 0);
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

  return (const char * const *)&g_array_index (layer->env, char *, 0);
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

  for (guint i = 0; i < layer->env->len-1; i++)
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
  static const char *envstr = "env";
  IdeRunContextLayer *layer;
  char **args;
  gsize len;

  g_assert (IDE_IS_RUN_CONTEXT (self));

  layer = ide_run_context_current_layer (self);

  if (layer->env->len == 0)
    return;

  args = g_array_steal (layer->env, &len);
  g_array_insert_vals (layer->argv, 0, args, len);
  g_array_insert_val (layer->argv, 0, envstr);
  g_free (args);
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

  /* Replace environment for this layer to use "env FOO=Bar" style subcommand
   * so that it's evironment doesn't attach to the parent program.
   */
  ide_run_context_environ_to_argv (self);

  /* Then make sure the higher layer's environment has higher priority */
  ide_run_context_set_environ (self, env);

  /* Now prepend the arguments and set new working dir */
  ide_run_context_prepend_args (self, argv);

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

  g_return_val_if_fail (IDE_IS_RUN_CONTEXT (self), FALSE);
  g_return_val_if_fail (layer != NULL, FALSE);
  g_return_val_if_fail (layer != &self->root, FALSE);

  handler = layer->handler ? layer->handler : ide_run_context_default_handler;
  handler_data = layer->handler ? layer->handler_data : NULL;

  ret = handler (self,
                 (const char * const *)&g_array_index (layer->argv, const char *, 0),
                 (const char * const *)&g_array_index (layer->env, const char *, 0),
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

  return g_steal_pointer (&launcher);
}
