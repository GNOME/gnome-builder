/* ide-tweaks-subpage.c
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

#define G_LOG_DOMAIN "ide-tweaks-subpage"

#include "config.h"

#include "ide-tweaks-group.h"
#include "ide-tweaks-subpage.h"

struct _IdeTweaksSubpage
{
  IdeTweaksItem parent_instance;
  char *title;
};

G_DEFINE_FINAL_TYPE (IdeTweaksSubpage, ide_tweaks_subpage, IDE_TYPE_TWEAKS_ITEM)

enum {
  PROP_0,
  PROP_TITLE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

IdeTweaksSubpage *
ide_tweaks_subpage_new (void)
{
  return g_object_new (IDE_TYPE_TWEAKS_SUBPAGE, NULL);
}

static gboolean
ide_tweaks_subpage_accepts (IdeTweaksItem *item,
                            IdeTweaksItem *child)
{
  return IDE_IS_TWEAKS_GROUP (child);
}

static void
ide_tweaks_subpage_dispose (GObject *object)
{
  IdeTweaksSubpage *self = (IdeTweaksSubpage *)object;

  g_clear_pointer (&self->title, g_free);

  G_OBJECT_CLASS (ide_tweaks_subpage_parent_class)->dispose (object);
}

static void
ide_tweaks_subpage_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  IdeTweaksSubpage *self = IDE_TWEAKS_SUBPAGE (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      g_value_set_string (value, ide_tweaks_subpage_get_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_subpage_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  IdeTweaksSubpage *self = IDE_TWEAKS_SUBPAGE (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      ide_tweaks_subpage_set_title (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_subpage_class_init (IdeTweaksSubpageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeTweaksItemClass *item_class = IDE_TWEAKS_ITEM_CLASS (klass);

  object_class->dispose = ide_tweaks_subpage_dispose;
  object_class->get_property = ide_tweaks_subpage_get_property;
  object_class->set_property = ide_tweaks_subpage_set_property;

  item_class->accepts = ide_tweaks_subpage_accepts;

  properties [PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL, NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_tweaks_subpage_init (IdeTweaksSubpage *self)
{
}

const char *
ide_tweaks_subpage_get_title (IdeTweaksSubpage *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_SUBPAGE (self), NULL);

  return self->title;
}

void
ide_tweaks_subpage_set_title (IdeTweaksSubpage *self,
                              const char       *title)
{
  g_return_if_fail (IDE_IS_TWEAKS_SUBPAGE (self));

  if (ide_set_string (&self->title, title))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TITLE]);
}
