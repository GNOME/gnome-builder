/* ide-tweaks-section.c

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

#define G_LOG_DOMAIN "ide-tweaks-section"

#include "config.h"

#include "ide-tweaks-factory.h"
#include "ide-tweaks-page.h"
#include "ide-tweaks-section.h"

struct _IdeTweaksSection
{
  IdeTweaksItem parent_instance;
  char *title;
  guint show_header : 1;
};

G_DEFINE_FINAL_TYPE (IdeTweaksSection, ide_tweaks_section, IDE_TYPE_TWEAKS_ITEM)

enum {
  PROP_0,
  PROP_SHOW_HEADER,
  PROP_TITLE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static gboolean
ide_tweaks_section_accepts (IdeTweaksItem *item,
                            IdeTweaksItem *child)
{
  g_assert (IDE_IS_TWEAKS_ITEM (item));
  g_assert (IDE_IS_TWEAKS_ITEM (child));

  return IDE_IS_TWEAKS_PAGE (child) ||
         IDE_IS_TWEAKS_FACTORY (child);
}

static void
ide_tweaks_section_dispose (GObject *object)
{
  IdeTweaksSection *self = (IdeTweaksSection *)object;

  g_clear_pointer (&self->title, g_free);

  G_OBJECT_CLASS (ide_tweaks_section_parent_class)->dispose (object);
}

static void
ide_tweaks_section_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  IdeTweaksSection *self = IDE_TWEAKS_SECTION (object);

  switch (prop_id)
    {
    case PROP_SHOW_HEADER:
      g_value_set_boolean (value, ide_tweaks_section_get_show_header (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, ide_tweaks_section_get_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_section_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  IdeTweaksSection *self = IDE_TWEAKS_SECTION (object);

  switch (prop_id)
    {
    case PROP_SHOW_HEADER:
      ide_tweaks_section_set_show_header (self, g_value_get_boolean (value));
      break;

    case PROP_TITLE:
      ide_tweaks_section_set_title (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_section_class_init (IdeTweaksSectionClass *klass)
{
  IdeTweaksItemClass *item_class = IDE_TWEAKS_ITEM_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  item_class->accepts = ide_tweaks_section_accepts;

  object_class->dispose = ide_tweaks_section_dispose;
  object_class->get_property = ide_tweaks_section_get_property;
  object_class->set_property = ide_tweaks_section_set_property;

  properties[PROP_SHOW_HEADER] =
    g_param_spec_boolean ("show-header", NULL, NULL,
                         FALSE,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_tweaks_section_init (IdeTweaksSection *self)
{
}

const char *
ide_tweaks_section_get_title (IdeTweaksSection *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_SECTION (self), NULL);

  return self->title;
}

void
ide_tweaks_section_set_title (IdeTweaksSection *self,
                              const char       *title)
{
  g_return_if_fail (IDE_IS_TWEAKS_SECTION (self));

  if (g_set_str (&self->title, title))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TITLE]);
}

gboolean
ide_tweaks_section_get_show_header (IdeTweaksSection *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_SECTION (self), FALSE);

  return self->show_header;
}

void
ide_tweaks_section_set_show_header (IdeTweaksSection *self,
                                    gboolean          show_header)
{
  g_return_if_fail (IDE_IS_TWEAKS_SECTION (self));

  show_header = !!show_header;

  if (show_header != self->show_header)
    {
      self->show_header = show_header;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SHOW_HEADER]);
    }
}

IdeTweaksSection *
ide_tweaks_section_new (void)
{
  return g_object_new (IDE_TYPE_TWEAKS_SECTION, NULL);
}
