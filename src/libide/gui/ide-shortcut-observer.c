/* ide-shortcut-observer.c
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-shortcut-observer"

#include "config.h"

#include <libide-core.h>

#include "ide-marshal.h"

#include "ide-shortcut-observer-private.h"

struct _IdeShortcutObserver
{
  GObject     parent_instance;
  GListModel *model;
  GHashTable *map;
  guint       reload_source;
};

enum {
  PROP_0,
  PROP_MODEL,
  N_PROPS
};

enum {
  ACCEL_CHANGED,
  N_SIGNALS
};

G_DEFINE_FINAL_TYPE (IdeShortcutObserver, ide_shortcut_observer, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static void
ide_shortcut_observer_reload (IdeShortcutObserver *self)
{
  g_autoptr(GHashTable) map = NULL;
  guint n_items;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SHORTCUT_OBSERVER (self));
  g_assert (self->model != NULL);
  g_assert (G_IS_LIST_MODEL (self->model));

  map = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  n_items = g_list_model_get_n_items (self->model);

  /* Build our map of new accels for actions */
  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(GtkShortcut) shortcut = g_list_model_get_item (self->model, i);
      GtkShortcutAction *action = gtk_shortcut_get_action (shortcut);
      GtkShortcutTrigger *trigger = gtk_shortcut_get_trigger (shortcut);
      g_autofree char *accel = NULL;
      const char *action_name;

      if (!GTK_IS_NAMED_ACTION (action))
        continue;

      if (!GTK_IS_SHORTCUT_TRIGGER (trigger))
        continue;

      action_name = gtk_named_action_get_action_name (GTK_NAMED_ACTION (action));
      accel = gtk_shortcut_trigger_to_string (trigger);

      if (!g_hash_table_contains (map, action_name))
        g_hash_table_insert (map,
                             g_strdup (action_name),
                             g_steal_pointer (&accel));
    }

  /* Now emit signals for removals of any accels for those actions */
  if (self->map != NULL)
    {
      GHashTableIter iter;
      const char *action_name;
      const char *old_accel;

      g_hash_table_iter_init (&iter, self->map);

      while (g_hash_table_iter_next (&iter, (gpointer *)&action_name, (gpointer *)&old_accel))
        {
          if (!g_hash_table_contains (map, action_name))
            g_signal_emit (self,
                           signals [ACCEL_CHANGED],
                           g_quark_from_string (action_name),
                           action_name,
                           NULL);
        }
    }

  if (TRUE)
    {
      GHashTableIter iter;
      const char *action_name;
      const char *new_accel;

      g_hash_table_iter_init (&iter, map);

      while (g_hash_table_iter_next (&iter, (gpointer *)&action_name, (gpointer *)&new_accel))
        {
          const char *old_accel;

          if (self->map == NULL ||
              !(old_accel = g_hash_table_lookup (self->map, action_name)) ||
              g_strcmp0 (old_accel, new_accel) != 0)
            g_signal_emit (self,
                           signals [ACCEL_CHANGED],
                           g_quark_from_string (action_name),
                           action_name,
                           new_accel);
        }
    }

  g_clear_pointer (&self->map, g_hash_table_unref);
  self->map = g_steal_pointer (&map);

  IDE_EXIT;
}

static gboolean
ide_shortcut_observer_reload_cb (gpointer user_data)
{
  IdeShortcutObserver *self = user_data;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SHORTCUT_OBSERVER (self));

  self->reload_source = 0;
  ide_shortcut_observer_reload (self);
  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
ide_shortcut_observer_queue_reload (IdeShortcutObserver *self)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SHORTCUT_OBSERVER (self));

  if (self->reload_source == 0)
    self->reload_source = g_idle_add_full (G_PRIORITY_DEFAULT,
                                           ide_shortcut_observer_reload_cb,
                                           g_object_ref (self),
                                           g_object_unref);
}

static void
ide_shortcut_observer_items_changed_cb (IdeShortcutObserver *self,
                                        guint                position,
                                        guint                removed,
                                        guint                added,
                                        GListModel          *model)
{
  g_assert (IDE_IS_SHORTCUT_OBSERVER (self));
  g_assert (G_IS_LIST_MODEL (model));

  if (removed == 0 && added == 0)
    return;

  ide_shortcut_observer_queue_reload (self);
}

static void
ide_shortcut_observer_set_model (IdeShortcutObserver *self,
                                 GListModel          *model)
{
  g_assert (IDE_IS_SHORTCUT_OBSERVER (self));
  g_assert (G_IS_LIST_MODEL (model));

  g_set_object (&self->model, model);

  g_signal_connect_object (self->model,
                           "items-changed",
                           G_CALLBACK (ide_shortcut_observer_items_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  ide_shortcut_observer_reload (self);
}

static void
ide_shortcut_observer_dispose (GObject *object)
{
  IdeShortcutObserver *self = (IdeShortcutObserver *)object;

  g_clear_handle_id (&self->reload_source, g_source_remove);
  g_clear_object (&self->model);
  g_clear_pointer (&self->map, g_hash_table_unref);

  G_OBJECT_CLASS (ide_shortcut_observer_parent_class)->dispose (object);
}

static void
ide_shortcut_observer_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeShortcutObserver *self = IDE_SHORTCUT_OBSERVER (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_shortcut_observer_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeShortcutObserver *self = IDE_SHORTCUT_OBSERVER (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      ide_shortcut_observer_set_model (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_shortcut_observer_class_init (IdeShortcutObserverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_shortcut_observer_dispose;
  object_class->get_property = ide_shortcut_observer_get_property;
  object_class->set_property = ide_shortcut_observer_set_property;

  properties[PROP_MODEL] =
    g_param_spec_object ("model", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * IdeShortcutObserver::accel-changed:
   * @self: a #IdeShortcutObserver
   * @action: the action to be performed
   * @accel: (nullable): the accelerator for the action
   *
   * This signal is emitted when an action is determined to have been changed
   * by the user or some other mechanism.
   */
  signals[ACCEL_CHANGED] =
    g_signal_new ("accel-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL,
                  ide_marshal_VOID__STRING_STRING,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                  G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (signals[ACCEL_CHANGED],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__STRING_STRINGv);
}

static void
ide_shortcut_observer_init (IdeShortcutObserver *self)
{
}

IdeShortcutObserver *
ide_shortcut_observer_new (GListModel *model)
{
  g_return_val_if_fail (G_IS_LIST_MODEL (model), NULL);

  return g_object_new (IDE_TYPE_SHORTCUT_OBSERVER,
                       "model", model,
                       NULL);
}
