/* code-result.c
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "config.h"

#include "code-index.h"
#include "code-result-private.h"

struct _CodeResult
{
  GObject    parent_instance;
  CodeIndex *index;
  char      *path;
};

G_DEFINE_FINAL_TYPE (CodeResult, code_result, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_INDEX,
  PROP_PATH,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

CodeResult *
_code_result_new (CodeIndex *index,
                  char      *path)
{
  CodeResult *self;

  self = g_object_new (CODE_TYPE_RESULT, NULL);
  self->index = index;
  self->path = path;

  return self;
}

static void
code_result_finalize (GObject *object)
{
  CodeResult *self = (CodeResult *)object;

  g_clear_pointer (&self->index, code_index_unref);
  g_clear_pointer (&self->path, g_free);

  G_OBJECT_CLASS (code_result_parent_class)->finalize (object);
}

static void
code_result_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  CodeResult *self = CODE_RESULT (object);

  switch (prop_id)
    {
    case PROP_INDEX:
      g_value_set_boxed (value, self->index);
      break;

    case PROP_PATH:
      g_value_set_string (value, self->path);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
code_result_class_init (CodeResultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = code_result_finalize;
  object_class->get_property = code_result_get_property;

  properties [PROP_INDEX] =
    g_param_spec_boxed ("index", NULL, NULL,
                        CODE_TYPE_INDEX,
                        (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_PATH] =
    g_param_spec_string ("path", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
code_result_init (CodeResult *self)
{
}

const char *
code_result_get_path (CodeResult *self)
{
  g_return_val_if_fail (CODE_IS_RESULT (self), NULL);

  return self->path;
}

/**
 * code_result_get_index:
 *
 * Gets the index for the result.
 *
 * Returns: (transfer none): a #CodeIndex
 */
CodeIndex *
code_result_get_index (CodeResult *self)
{
  g_return_val_if_fail (CODE_IS_RESULT (self), NULL);

  return self->index;
}
