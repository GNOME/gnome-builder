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

static void
ide_run_context_layer_clear (IdeRunContextLayer *layer)
{
  g_assert (layer != NULL);
  g_assert (layer->qlink.data == layer);
  g_assert (layer->qlink.prev == NULL);
  g_assert (layer->qlink.next == NULL);

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

static IdeRunContextLayer *
ide_run_context_current_layer (IdeRunContext *self)
{
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
  self->root.qlink.data = self;
  g_queue_push_head_link (&self->layers, &self->root.qlink);
}

static void
strptr_free (gpointer data)
{
  char **strptr = data;
  g_clear_pointer (strptr, g_free);
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
  layer->qlink.data = layer;
  layer->argv = g_array_new (TRUE, FALSE, sizeof (char *));
  layer->env = g_array_new (TRUE, FALSE, sizeof (char *));
  layer->unix_fd_map = ide_unix_fd_map_new ();
  layer->handler = handler;
  layer->handler_data = handler_data;
  layer->handler_data_destroy = handler_data_destroy;

  g_array_set_clear_func (layer->argv, strptr_free);
  g_array_set_clear_func (layer->env, strptr_free);

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

  if (environ != NULL)
    {
      char **copy = g_strdupv ((char **)environ);
      g_array_append_vals (layer->env, copy, g_strv_length (copy));
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
  g_return_if_fail (args != NULL);

  layer = ide_run_context_current_layer (self);

  copy = g_strdupv ((char **)args);
  g_array_append_vals (layer->argv, copy, g_strv_length (copy));
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

  if (!g_shell_parse_argv (args, &argc, &argv, error))
    return FALSE;

  layer = ide_run_context_current_layer (self);
  g_array_append_vals (layer->argv, argv, argc);

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
  g_return_val_if_fail (IDE_IS_RUN_CONTEXT (self), NULL);
  g_return_val_if_fail (self->ended == FALSE, NULL);

  self->ended = TRUE;

  /* TODO: Process layers */

  return NULL;
}
