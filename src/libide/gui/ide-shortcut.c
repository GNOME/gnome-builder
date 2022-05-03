/* ide-shortcut.c
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

#define G_LOG_DOMAIN "ide-shortcut"

#include "config.h"

#include "ide-shortcut-private.h"

struct _IdeShortcut
{
  GObject parent_instance;
};

enum {
  PROP_0,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeShortcut, ide_shortcut, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
ide_shortcut_dispose (GObject *object)
{
  IdeShortcut *self = (IdeShortcut *)object;

  G_OBJECT_CLASS (ide_shortcut_parent_class)->dispose (object);
}

static void
ide_shortcut_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  IdeShortcut *self = IDE_SHORTCUT (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_shortcut_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  IdeShortcut *self = IDE_SHORTCUT (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_shortcut_class_init (IdeShortcutClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_shortcut_dispose;
  object_class->get_property = ide_shortcut_get_property;
  object_class->set_property = ide_shortcut_set_property;
}

static void
ide_shortcut_init (IdeShortcut *self)
{
}
