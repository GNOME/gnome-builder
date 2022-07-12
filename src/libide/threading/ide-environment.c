/* ide-environment.c
 *
 * Copyright 2016-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-environment"

#include "config.h"

#include <libide-core.h>

#include "ide-environment.h"
#include "ide-environment-variable.h"

struct _IdeEnvironment
{
  GObject    parent_instance;
  GPtrArray *variables;
};

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeEnvironment, ide_environment, G_TYPE_OBJECT, G_TYPE_FLAG_FINAL,
                        G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

enum {
  CHANGED,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL];

static void
ide_environment_finalize (GObject *object)
{
  IdeEnvironment *self = (IdeEnvironment *)object;

  g_clear_pointer (&self->variables, g_ptr_array_unref);

  G_OBJECT_CLASS (ide_environment_parent_class)->finalize (object);
}

static void
ide_environment_class_init (IdeEnvironmentClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_environment_finalize;

  signals [CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  g_signal_set_va_marshaller (signals [CHANGED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__VOIDv);
}

static void
ide_environment_items_changed (IdeEnvironment *self)
{
  g_assert (IDE_IS_ENVIRONMENT (self));

  g_signal_emit (self, signals [CHANGED], 0);
}

static void
ide_environment_init (IdeEnvironment *self)
{
  self->variables = g_ptr_array_new_with_free_func (g_object_unref);

  g_signal_connect (self,
                    "items-changed",
                    G_CALLBACK (ide_environment_items_changed),
                    NULL);
}

static GType
ide_environment_get_item_type (GListModel *model)
{
  return IDE_TYPE_ENVIRONMENT_VARIABLE;
}

static gpointer
ide_environment_get_item (GListModel *model,
                          guint       position)
{
  IdeEnvironment *self = (IdeEnvironment *)model;

  g_return_val_if_fail (IDE_IS_ENVIRONMENT (self), NULL);
  g_return_val_if_fail (position < self->variables->len, NULL);

  return g_object_ref (g_ptr_array_index (self->variables, position));
}

static guint
ide_environment_get_n_items (GListModel *model)
{
  IdeEnvironment *self = (IdeEnvironment *)model;

  g_return_val_if_fail (IDE_IS_ENVIRONMENT (self), 0);

  return self->variables->len;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_n_items = ide_environment_get_n_items;
  iface->get_item = ide_environment_get_item;
  iface->get_item_type = ide_environment_get_item_type;
}

static void
ide_environment_variable_notify (IdeEnvironment         *self,
                                 GParamSpec             *pspec,
                                 IdeEnvironmentVariable *variable)
{
  g_assert (IDE_IS_ENVIRONMENT (self));

  g_signal_emit (self, signals [CHANGED], 0);
}

void
ide_environment_setenv (IdeEnvironment *self,
                        const gchar    *key,
                        const gchar    *value)
{
  guint i;

  g_return_if_fail (IDE_IS_ENVIRONMENT (self));
  g_return_if_fail (key != NULL);

  for (i = 0; i < self->variables->len; i++)
    {
      IdeEnvironmentVariable *var = g_ptr_array_index (self->variables, i);
      const gchar *var_key = ide_environment_variable_get_key (var);

      if (g_strcmp0 (key, var_key) == 0)
        {
          if (value == NULL)
            {
              g_ptr_array_remove_index (self->variables, i);
              g_list_model_items_changed (G_LIST_MODEL (self), i, 1, 0);
              return;
            }

          ide_environment_variable_set_value (var, value);
          return;
        }
    }

  if (value != NULL)
    {
      IdeEnvironmentVariable *var;
      guint position = self->variables->len;

      var = g_object_new (IDE_TYPE_ENVIRONMENT_VARIABLE,
                          "key", key,
                          "value", value,
                          NULL);
      g_signal_connect_object (var,
                               "notify",
                               G_CALLBACK (ide_environment_variable_notify),
                               self,
                               G_CONNECT_SWAPPED);
      g_ptr_array_add (self->variables, var);
      g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);
    }
}

const gchar *
ide_environment_getenv (IdeEnvironment *self,
                        const gchar    *key)
{
  guint i;

  g_return_val_if_fail (IDE_IS_ENVIRONMENT (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  for (i = 0; i < self->variables->len; i++)
    {
      IdeEnvironmentVariable *var = g_ptr_array_index (self->variables, i);
      const gchar *var_key = ide_environment_variable_get_key (var);

      if (g_strcmp0 (key, var_key) == 0)
        return ide_environment_variable_get_value (var);
    }

  return NULL;
}

/**
 * ide_environment_get_environ:
 * @self: An #IdeEnvironment
 *
 * Gets the environment as a set of key=value pairs, suitable for use
 * in various GLib process functions.
 *
 * Returns: (transfer full): A newly allocated string array.
 */
gchar **
ide_environment_get_environ (IdeEnvironment *self)
{
  GPtrArray *ar;
  guint i;

  g_return_val_if_fail (IDE_IS_ENVIRONMENT (self), NULL);

  ar = g_ptr_array_new ();

  for (i = 0; i < self->variables->len; i++)
    {
      IdeEnvironmentVariable *var = g_ptr_array_index (self->variables, i);
      const gchar *key = ide_environment_variable_get_key (var);
      const gchar *value = ide_environment_variable_get_value (var);

      if (value == NULL)
        value = "";

      if (key != NULL)
        g_ptr_array_add (ar, g_strdup_printf ("%s=%s", key, value));
    }

  g_ptr_array_add (ar, NULL);

  return (gchar **)g_ptr_array_free (ar, FALSE);
}

void
ide_environment_set_environ (IdeEnvironment      *self,
                             const gchar * const *env)
{
  guint len;

  g_return_if_fail (IDE_IS_ENVIRONMENT (self));

  len = self->variables->len;

  if (len > 0)
    {
      g_ptr_array_remove_range (self->variables, 0, len);
      g_list_model_items_changed (G_LIST_MODEL (self), 0, len, 0);
    }

  if (env != NULL)
    {
      for (guint i = 0; env[i]; i++)
        {
          g_autofree gchar *key = NULL;
          g_autofree gchar *val = NULL;

          if (ide_environ_parse (env[i], &key, &val))
            ide_environment_setenv (self, key, val);
        }
    }
}

IdeEnvironment *
ide_environment_new (void)
{
  return g_object_new (IDE_TYPE_ENVIRONMENT, NULL);
}

void
ide_environment_remove (IdeEnvironment         *self,
                        IdeEnvironmentVariable *variable)
{
  guint i;

  g_return_if_fail (IDE_IS_ENVIRONMENT (self));
  g_return_if_fail (IDE_IS_ENVIRONMENT_VARIABLE (variable));

  for (i = 0; i < self->variables->len; i++)
    {
      IdeEnvironmentVariable *item = g_ptr_array_index (self->variables, i);

      if (item == variable)
        {
          g_ptr_array_remove_index (self->variables, i);
          g_list_model_items_changed (G_LIST_MODEL (self), i, 1, 0);
          break;
        }
    }
}

void
ide_environment_append (IdeEnvironment         *self,
                        IdeEnvironmentVariable *variable)
{
  guint position;

  g_return_if_fail (IDE_IS_ENVIRONMENT (self));
  g_return_if_fail (IDE_IS_ENVIRONMENT_VARIABLE (variable));

  position = self->variables->len;

  g_signal_connect_object (variable,
                           "notify",
                           G_CALLBACK (ide_environment_variable_notify),
                           self,
                           G_CONNECT_SWAPPED);
  g_ptr_array_add (self->variables, g_object_ref (variable));
  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);
}

/**
 * ide_environment_copy:
 * @self: An #IdeEnvironment
 *
 * Copies the contents of #IdeEnvironment into a newly allocated #IdeEnvironment.
 *
 * Returns: (transfer full): An #IdeEnvironment.
 */
IdeEnvironment *
ide_environment_copy (IdeEnvironment *self)
{
  g_autoptr(IdeEnvironment) copy = NULL;

  g_return_val_if_fail (IDE_IS_ENVIRONMENT (self), NULL);

  copy = ide_environment_new ();
  ide_environment_copy_into (self, copy, TRUE);

  return g_steal_pointer (&copy);
}

void
ide_environment_copy_into (IdeEnvironment *self,
                           IdeEnvironment *dest,
                           gboolean        replace)
{
  g_return_if_fail (IDE_IS_ENVIRONMENT (self));
  g_return_if_fail (IDE_IS_ENVIRONMENT (dest));

  for (guint i = 0; i < self->variables->len; i++)
    {
      IdeEnvironmentVariable *var = g_ptr_array_index (self->variables, i);
      const gchar *key = ide_environment_variable_get_key (var);
      const gchar *value = ide_environment_variable_get_value (var);

      if (replace || ide_environment_getenv (dest, key) == NULL)
        ide_environment_setenv (dest, key, value);
    }
}

/**
 * ide_environ_parse:
 * @pair: the KEY=VALUE pair
 * @key: (out) (optional): a location for a @key
 * @value: (out) (optional): a location for a @value
 *
 * Parses a KEY=VALUE style key-pair into @key and @value.
 *
 * Returns: %TRUE if @pair was successfully parsed
 */
gboolean
ide_environ_parse (const gchar  *pair,
                   gchar       **key,
                   gchar       **value)
{
  const gchar *eq;

  g_return_val_if_fail (pair != NULL, FALSE);

  if (key != NULL)
    *key = NULL;

  if (value != NULL)
    *value = NULL;

  if ((eq = strchr (pair, '=')))
    {
      if (key != NULL)
        *key = g_strndup (pair, eq - pair);

      if (value != NULL)
        *value = g_strdup (eq + 1);

      return TRUE;
    }

  return FALSE;
}
