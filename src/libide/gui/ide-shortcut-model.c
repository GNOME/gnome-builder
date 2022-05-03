/* ide-shortcut-model.c
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

#define G_LOG_DOMAIN "ide-shortcut-model"

#include "config.h"

#include <gtk/gtk.h>

#include "ide-shortcut-model-private.h"

struct _IdeShortcutModel
{
  GObject     parent_instance;
  IdeContext *context;
};

enum {
  PROP_0,
  PROP_CONTEXT,
  N_PROPS
};

static guint
ide_shortcut_model_get_n_items (GListModel *model)
{
  return 0;
}

static gpointer
ide_shortcut_model_get_item (GListModel *model,
                             guint       position)
{
  return NULL;
}

static GType
ide_shortcut_model_get_item_type (GListModel *model)
{
  return GTK_TYPE_SHORTCUT;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_n_items = ide_shortcut_model_get_n_items;
  iface->get_item = ide_shortcut_model_get_item;
  iface->get_item_type = ide_shortcut_model_get_item_type;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeShortcutModel, ide_shortcut_model, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static GParamSpec *properties [N_PROPS];

static void
ide_shortcut_model_finalize (GObject *object)
{
  IdeShortcutModel *self = (IdeShortcutModel *)object;

  g_clear_object (&self->context);

  G_OBJECT_CLASS (ide_shortcut_model_parent_class)->finalize (object);
}

static void
ide_shortcut_model_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  IdeShortcutModel *self = IDE_SHORTCUT_MODEL (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, ide_shortcut_model_get_context (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_shortcut_model_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  IdeShortcutModel *self = IDE_SHORTCUT_MODEL (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      ide_shortcut_model_set_context (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_shortcut_model_class_init (IdeShortcutModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_shortcut_model_finalize;
  object_class->get_property = ide_shortcut_model_get_property;
  object_class->set_property = ide_shortcut_model_set_property;

  properties [PROP_CONTEXT] =
    g_param_spec_object ("context",
                         "Context",
                         "The IdeContext if any",
                         IDE_TYPE_CONTEXT,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_shortcut_model_init (IdeShortcutModel *self)
{
}

/**
 * ide_shortcut_model_get_context:
 * @self: a #IdeShortcutModel
 *
 * Gets the #IdeContext associated with a model, if any.
 *
 * Returns: (transfer none): an #IdeContext or %NULL
 */
IdeContext *
ide_shortcut_model_get_context (IdeShortcutModel *self)
{
  g_return_val_if_fail (IDE_IS_SHORTCUT_MODEL (self), NULL);

  return self->context;
}

/**
 * ide_shortcut_model_set_context:
 * @self: a #IdeShortcutModel
 * @context: (nullable): an #IdeContext or %NULL
 *
 * Sets the context for a shortcut model, if any.
 *
 * Setting the context for the model will cause custom shortcuts to be loaded
 * that have been loaded for the project.
 */
void
ide_shortcut_model_set_context (IdeShortcutModel *self,
                                IdeContext       *context)
{
  g_return_if_fail (IDE_IS_SHORTCUT_MODEL (self));
  g_return_if_fail (!context || IDE_IS_CONTEXT (context));

  /* TODO: Load context-specific shortcuts */
  if (g_set_object (&self->context, context))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CONTEXT]);
}

IdeShortcutModel *
ide_shortcut_model_new (void)
{
  return g_object_new (IDE_TYPE_SHORTCUT_MODEL, NULL);
}
