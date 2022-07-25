/* gbp-todo-item.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-todo-item"

#include "gbp-todo-item.h"

G_DEFINE_FINAL_TYPE (GbpTodoItem, gbp_todo_item, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_SUBTITLE,
  PROP_TITLE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static char *
gbp_todo_item_dup_title (GbpTodoItem *self)
{
  const char *path;
  guint lineno;

  g_assert (GBP_IS_TODO_ITEM (self));

  path = gbp_todo_item_get_path (self);
  lineno = gbp_todo_item_get_lineno (self);

  return g_strdup_printf ("%s:%u", path, lineno);
}

static const char *
gbp_todo_item_get_subtitle (GbpTodoItem *self)
{
  const char *message;

  g_assert (GBP_IS_TODO_ITEM (self));

  message = gbp_todo_item_get_line (self, 0);
  /*
   * We don't trim the whitespace from lines so that we can keep
   * them in tact when showing tooltips. So we need to truncate
   * here for display in the pane.
   */
  while (g_ascii_isspace (*message))
    message++;

  return message;
}

static void
gbp_todo_item_finalize (GObject *object)
{
  GbpTodoItem *self = (GbpTodoItem *)object;

  g_clear_pointer (&self->bytes, g_bytes_unref);

  G_OBJECT_CLASS (gbp_todo_item_parent_class)->finalize (object);
}

static void
gbp_todo_item_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GbpTodoItem *self = GBP_TODO_ITEM (object);

  switch (prop_id)
    {
    case PROP_SUBTITLE:
      g_value_set_string (value, gbp_todo_item_get_subtitle (self));
      break;

    case PROP_TITLE:
      g_value_take_string (value, gbp_todo_item_dup_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_todo_item_class_init (GbpTodoItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_todo_item_finalize;
  object_class->get_property = gbp_todo_item_get_property;

  properties [PROP_SUBTITLE] =
    g_param_spec_string ("subtitle", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_todo_item_init (GbpTodoItem *self)
{
}

/**
 * gbp_todo_item_new:
 * @bytes: a buffer containing all the subsequent data
 *
 * This creates a new #GbpTodoItem.
 *
 * Getting the parameters right to this function is essential.
 *
 * @bytes should contain a buffer that can be used to access
 * the raw string pointers used in later API calls to this
 * object.
 *
 * This is done to avoid fragmentation due to lots of
 * small string allocations.
 *
 * Returns: (transfer full): A newly allocated #GbpTodoItem
 */
GbpTodoItem *
gbp_todo_item_new (GBytes *bytes)
{
  GbpTodoItem *self;

  g_assert (bytes != NULL);

  self = g_object_new (GBP_TYPE_TODO_ITEM, NULL);
  self->bytes = g_bytes_ref (bytes);

  return self;
}

void
gbp_todo_item_add_line (GbpTodoItem *self,
                        const gchar *line)
{
  g_assert (GBP_IS_TODO_ITEM (self));
  g_assert (line != NULL);

  for (guint i = 0; i < G_N_ELEMENTS (self->lines); i++)
    {
      if (self->lines[i] == NULL)
        {
          self->lines[i] = line;
          break;
        }
    }
}

const gchar *
gbp_todo_item_get_line (GbpTodoItem *self,
                        guint        nth)
{
  g_return_val_if_fail (GBP_IS_TODO_ITEM (self), NULL);

  if (nth < G_N_ELEMENTS (self->lines))
    return self->lines[nth];

  return NULL;
}

guint
gbp_todo_item_get_lineno (GbpTodoItem *self)
{
  g_return_val_if_fail (GBP_IS_TODO_ITEM (self), 0);

  return self->lineno;
}

void
gbp_todo_item_set_lineno (GbpTodoItem *self,
                          guint        lineno)
{
  g_return_if_fail (GBP_IS_TODO_ITEM (self));

  self->lineno = lineno;
}

const char *
gbp_todo_item_get_path (GbpTodoItem *self)
{
  g_return_val_if_fail (GBP_IS_TODO_ITEM (self), NULL);

  return self->path;
}

void
gbp_todo_item_set_path (GbpTodoItem *self,
                        const char  *path)
{
  g_return_if_fail (GBP_IS_TODO_ITEM (self));

  self->path = path;
}
