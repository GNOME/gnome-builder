/* ide-tweaks-custom.c
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

#define G_LOG_DOMAIN "ide-tweaks-custom"

#include "config.h"

#include "ide-tweaks-custom.h"

struct _IdeTweaksCustom
{
  IdeTweaksItem parent_instance;
};

G_DEFINE_FINAL_TYPE (IdeTweaksCustom, ide_tweaks_custom, IDE_TYPE_TWEAKS_ITEM)

enum {
  PROP_0,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

IdeTweaksCustom *
ide_tweaks_custom_new (void)
{
  return g_object_new (IDE_TYPE_TWEAKS_CUSTOM, NULL);
}

static void
ide_tweaks_custom_dispose (GObject *object)
{
  IdeTweaksCustom *self = (IdeTweaksCustom *)object;

  G_OBJECT_CLASS (ide_tweaks_custom_parent_class)->dispose (object);
}

static void
ide_tweaks_custom_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeTweaksCustom *self = IDE_TWEAKS_CUSTOM (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_custom_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  IdeTweaksCustom *self = IDE_TWEAKS_CUSTOM (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_custom_class_init (IdeTweaksCustomClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_tweaks_custom_dispose;
  object_class->get_property = ide_tweaks_custom_get_property;
  object_class->set_property = ide_tweaks_custom_set_property;
}

static void
ide_tweaks_custom_init (IdeTweaksCustom *self)
{
}
