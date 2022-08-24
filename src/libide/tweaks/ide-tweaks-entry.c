/* ide-tweaks-entry.c
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

#define G_LOG_DOMAIN "ide-tweaks-entry"

#include "config.h"

#include <adwaita.h>

#include "ide-tweaks-entry.h"

struct _IdeTweaksEntry
{
  IdeTweaksWidget parent_instance;
  char *title;
  char *property_name;
  GObject *object;
};

enum {
  PROP_0,
  PROP_OBJECT,
  PROP_PROPERTY_NAME,
  PROP_TITLE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeTweaksEntry, ide_tweaks_entry, IDE_TYPE_TWEAKS_WIDGET)

static GParamSpec *properties [N_PROPS];

static GtkWidget *
ide_tweaks_entry_create_for_item (IdeTweaksWidget *widget,
                                  IdeTweaksItem   *item)
{
  IdeTweaksEntry *info = (IdeTweaksEntry *)item;
  AdwEntryRow *row;

  g_assert (IDE_IS_TWEAKS_ENTRY (widget));
  g_assert (IDE_IS_TWEAKS_ENTRY (item));

  row = g_object_new (ADW_TYPE_ENTRY_ROW,
                      "title", info->title,
                      NULL);

  if (info->object && info->property_name)
    g_object_bind_property (info->object, info->property_name, row, "text",
                            G_BINDING_SYNC_CREATE);

  return GTK_WIDGET (row);
}

static void
ide_tweaks_entry_dispose (GObject *object)
{
  IdeTweaksEntry *self = (IdeTweaksEntry *)object;

  g_clear_object (&self->object);
  g_clear_pointer (&self->property_name, g_free);
  g_clear_pointer (&self->title, g_free);

  G_OBJECT_CLASS (ide_tweaks_entry_parent_class)->dispose (object);
}

static void
ide_tweaks_entry_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  IdeTweaksEntry *self = IDE_TWEAKS_ENTRY (object);

  switch (prop_id)
    {
    case PROP_OBJECT:
      g_value_set_object (value, ide_tweaks_entry_get_object (self));
      break;

    IDE_GET_PROPERTY_STRING (ide_tweaks_entry, title, TITLE);
    IDE_GET_PROPERTY_STRING (ide_tweaks_entry, property_name, PROPERTY_NAME);
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_entry_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  IdeTweaksEntry *self = IDE_TWEAKS_ENTRY (object);

  switch (prop_id)
    {
    case PROP_OBJECT:
      ide_tweaks_entry_set_object (self, g_value_get_object (value));
      break;

    IDE_SET_PROPERTY_STRING (ide_tweaks_entry, title, TITLE);
    IDE_SET_PROPERTY_STRING (ide_tweaks_entry, property_name, PROPERTY_NAME);
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_entry_class_init (IdeTweaksEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeTweaksWidgetClass *tweaks_widget_class = IDE_TWEAKS_WIDGET_CLASS (klass);

  object_class->dispose = ide_tweaks_entry_dispose;
  object_class->get_property = ide_tweaks_entry_get_property;
  object_class->set_property = ide_tweaks_entry_set_property;

  tweaks_widget_class->create_for_item = ide_tweaks_entry_create_for_item;

  properties[PROP_OBJECT] =
    g_param_spec_object ("object", NULL, NULL,
                         G_TYPE_OBJECT,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  IDE_DEFINE_STRING_PROPERTY ("title", NULL, G_PARAM_READWRITE, TITLE);
  IDE_DEFINE_STRING_PROPERTY ("property-name", NULL, G_PARAM_READWRITE, PROPERTY_NAME);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_tweaks_entry_init (IdeTweaksEntry *self)
{
}

IdeTweaksEntry *
ide_tweaks_entry_new (void)
{
  return g_object_new (IDE_TYPE_TWEAKS_ENTRY, NULL);
}

IDE_DEFINE_STRING_GETTER (ide_tweaks_entry, IdeTweaksEntry, IDE_TYPE_TWEAKS_ENTRY, title)
IDE_DEFINE_STRING_SETTER (ide_tweaks_entry, IdeTweaksEntry, IDE_TYPE_TWEAKS_ENTRY, title, TITLE)

IDE_DEFINE_STRING_GETTER (ide_tweaks_entry, IdeTweaksEntry, IDE_TYPE_TWEAKS_ENTRY, property_name)
IDE_DEFINE_STRING_SETTER (ide_tweaks_entry, IdeTweaksEntry, IDE_TYPE_TWEAKS_ENTRY, property_name, PROPERTY_NAME)

/**
 * ide_tweaks_entry_get_object:
 * @self: a #IdeTweaksEntry
 *
 * Returns: (transfer none) (nullable): a #GObject or %NULL
 */
GObject *
ide_tweaks_entry_get_object (IdeTweaksEntry *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_ENTRY (self), NULL);

  return self->object;
}

void
ide_tweaks_entry_set_object (IdeTweaksEntry *self,
                             GObject        *object)
{
  g_return_if_fail (IDE_IS_TWEAKS_ENTRY (self));
  g_return_if_fail (!object || G_IS_OBJECT (object));

  if (g_set_object (&self->object, object))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_OBJECT]);
}
