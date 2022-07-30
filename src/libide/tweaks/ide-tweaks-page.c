/*
 * ide-tweaks-page.c
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

#define G_LOG_DOMAIN "ide-tweaks-page"

#include "config.h"

#include "ide-tweaks-page.h"

struct _IdeTweaksPage
{
  IdeTweaksItem parent_instance;
  char *section;
  char *title;
};

G_DEFINE_FINAL_TYPE (IdeTweaksPage, ide_tweaks_page, IDE_TYPE_TWEAKS_ITEM)

enum {
  PROP_0,
  PROP_SECTION,
  PROP_TITLE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

IdeTweaksPage *
ide_tweaks_page_new (void)
{
  return g_object_new (IDE_TYPE_TWEAKS_PAGE, NULL);
}

static void
ide_tweaks_page_finalize (GObject *object)
{
  IdeTweaksPage *self = (IdeTweaksPage *)object;

  g_clear_pointer (&self->section, g_free);
  g_clear_pointer (&self->title, g_free);

  G_OBJECT_CLASS (ide_tweaks_page_parent_class)->finalize (object);
}

static void
ide_tweaks_page_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeTweaksPage *self = IDE_TWEAKS_PAGE (object);

  switch (prop_id)
    {
    case PROP_SECTION:
      g_value_set_string (value, ide_tweaks_page_get_section (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, ide_tweaks_page_get_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_page_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeTweaksPage *self = IDE_TWEAKS_PAGE (object);

  switch (prop_id)
    {
    case PROP_SECTION:
      ide_tweaks_page_set_section (self, g_value_get_string (value));
      break;

    case PROP_TITLE:
      ide_tweaks_page_set_title (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_page_class_init (IdeTweaksPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_tweaks_page_finalize;
  object_class->get_property = ide_tweaks_page_get_property;
  object_class->set_property = ide_tweaks_page_set_property;

  properties [PROP_SECTION] =
    g_param_spec_string ("section", NULL, NULL, NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL, NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_tweaks_page_init (IdeTweaksPage *self)
{
}

const char *
ide_tweaks_page_get_section (IdeTweaksPage *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_PAGE (self), NULL);

  return self->section;
}

void
ide_tweaks_page_set_section (IdeTweaksPage *self,
                             const char    *section)
{
  g_return_if_fail (IDE_IS_TWEAKS_PAGE (self));

  if (ide_set_string (&self->section, section))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SECTION]);
}

const char *
ide_tweaks_page_get_title (IdeTweaksPage *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_PAGE (self), NULL);

  return self->title;
}

void
ide_tweaks_page_set_title (IdeTweaksPage *self,
                           const char    *title)
{
  g_return_if_fail (IDE_IS_TWEAKS_PAGE (self));

  if (ide_set_string (&self->title, title))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TITLE]);
}
