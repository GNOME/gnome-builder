/*
 * code-query.c
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at
 * your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "config.h"

#include "code-query-private.h"
#include "code-query-spec-private.h"
#include "code-result-private.h"
#include "code-sparse-set.h"

struct _CodeQuery
{
  GObject        parent_instance;
  CodeQuerySpec *spec;
};

G_DEFINE_FINAL_TYPE (CodeQuery, code_query, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_SPEC,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
code_query_finalize (GObject *object)
{
  CodeQuery *self = (CodeQuery *)object;

  g_clear_object (&self->spec);

  G_OBJECT_CLASS (code_query_parent_class)->finalize (object);
}

static void
code_query_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  CodeQuery *self = CODE_QUERY (object);

  switch (prop_id)
    {
    case PROP_SPEC:
      g_value_set_object (value, code_query_get_spec (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
code_query_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  CodeQuery *self = CODE_QUERY (object);

  switch (prop_id)
    {
    case PROP_SPEC:
      self->spec = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
code_query_class_init (CodeQueryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = code_query_finalize;
  object_class->get_property = code_query_get_property;
  object_class->set_property = code_query_set_property;

  properties [PROP_SPEC] =
    g_param_spec_object ("spec", NULL, NULL,
                         CODE_TYPE_QUERY_SPEC,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
code_query_init (CodeQuery *self)
{
}

CodeQuery *
code_query_new (CodeQuerySpec *spec)
{
  g_return_val_if_fail (CODE_IS_QUERY_SPEC (spec), NULL);

  return g_object_new (CODE_TYPE_QUERY,
                       "spec", spec,
                       NULL);
}

/**
 * code_query_get_spec:
 * @self: a #CodeQuery
 *
 * Gets the #CodeQuery:spec property.
 *
 * Returns: (transfer none) (nullable): a #CodeQuerySpec or %NULL if
 *   the specification has not been set.
 */
CodeQuerySpec *
code_query_get_spec (CodeQuery *self)
{
  g_return_val_if_fail (CODE_IS_QUERY (self), NULL);

  return self->spec;
}

void
_code_query_get_trigrams (CodeQuery  *query,
                          guint     **trigrams,
                          guint      *n_trigrams)
{
  g_auto(CodeSparseSet) set = CODE_SPARSE_SET_INIT (1 << 24);

  _code_query_spec_collect_trigrams (query->spec, &set);

  *n_trigrams = set.len;
  *trigrams = g_new (guint, set.len);

  for (guint i = 0; i < set.len; i++)
    (*trigrams)[i] = set.dense[i].value;
}

typedef struct _CodeQueryFiber
{
  CodeQuerySpec *spec;
  CodeIndex     *index;
  char          *path;
  DexChannel    *channel;
} CodeQueryFiber;

static inline CodeQueryFiber *
code_query_fiber_new (CodeQuerySpec *spec,
                      CodeIndex     *index,
                      const char    *path,
                      DexChannel    *channel)
{
  CodeQueryFiber *fiber = g_new0 (CodeQueryFiber, 1);

  fiber->spec = g_object_ref (spec);
  fiber->index = code_index_ref (index);
  fiber->path = g_strdup (path);
  fiber->channel = dex_ref (channel);

  return fiber;
}

static inline void
code_query_fiber_free (CodeQueryFiber *fiber)
{
  g_clear_object (&fiber->spec);
  g_clear_pointer (&fiber->index, code_index_unref);
  g_clear_pointer (&fiber->path, g_free);
  dex_clear (&fiber->channel);
  g_free (fiber);
}

static DexFuture *
code_query_fiber (gpointer user_data)
{
  CodeQueryFiber *fiber = user_data;
  g_autoptr(GBytes) bytes = NULL;

  /* TODO: It might be nice to allow the loader to provide annotations which
   *       we can pass along to the CodeResult such as icon, title, etc.
   */

  if (!(bytes = dex_await_boxed (code_index_load_document_path (fiber->index, fiber->path), NULL)))
    return dex_future_new_for_boolean (FALSE);

  if (_code_query_spec_matches (fiber->spec, fiber->path, bytes))
    return dex_channel_send (fiber->channel,
                             dex_future_new_take_object (_code_result_new (g_steal_pointer (&fiber->index),
                                                                           g_steal_pointer (&fiber->path))));

  return dex_future_new_for_boolean (TRUE);
}

DexFuture *
_code_query_match (CodeQuery    *query,
                   CodeIndex    *index,
                   const char   *path,
                   DexChannel   *channel,
                   DexScheduler *scheduler)
{
  return dex_scheduler_spawn (scheduler, 0,
                              code_query_fiber,
                              code_query_fiber_new (query->spec, index, path, channel),
                              (GDestroyNotify) code_query_fiber_free);
}
