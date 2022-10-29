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

#include "ide-tweaks-factory-private.h"
#include "ide-tweaks-group.h"
#include "ide-tweaks-page.h"
#include "ide-tweaks-section.h"

struct _IdeTweaksPage
{
  IdeTweaksItem parent_instance;
  char *icon_name;
  char *title;
  guint show_icon : 1;
  guint show_search : 1;
};

G_DEFINE_FINAL_TYPE (IdeTweaksPage, ide_tweaks_page, IDE_TYPE_TWEAKS_ITEM)

enum {
  PROP_0,
  PROP_HAS_SUBPAGE,
  PROP_ICON_NAME,
  PROP_SECTION,
  PROP_SHOW_ICON,
  PROP_SHOW_SEARCH,
  PROP_TITLE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

IdeTweaksPage *
ide_tweaks_page_new (void)
{
  return g_object_new (IDE_TYPE_TWEAKS_PAGE, NULL);
}

static gboolean
ide_tweaks_page_accepts (IdeTweaksItem *item,
                         IdeTweaksItem *child)
{
  return IDE_IS_TWEAKS_PAGE (child) ||
         IDE_IS_TWEAKS_FACTORY (child) ||
         IDE_IS_TWEAKS_GROUP (child) ||
         IDE_IS_TWEAKS_SECTION (child);
}

static gboolean
ide_tweaks_page_match (IdeTweaksItem  *item,
                       IdePatternSpec *spec)
{
  IdeTweaksPage *self = IDE_TWEAKS_PAGE (item);

  return ide_pattern_spec_match (spec, self->title) ||
         IDE_TWEAKS_ITEM_CLASS (ide_tweaks_page_parent_class)->match (item, spec);
}

static void
ide_tweaks_page_dispose (GObject *object)
{
  IdeTweaksPage *self = (IdeTweaksPage *)object;

  g_clear_pointer (&self->icon_name, g_free);
  g_clear_pointer (&self->title, g_free);

  G_OBJECT_CLASS (ide_tweaks_page_parent_class)->dispose (object);
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
    case PROP_HAS_SUBPAGE:
      g_value_set_boolean (value, ide_tweaks_page_get_has_subpage (self));
      break;

    case PROP_ICON_NAME:
      g_value_set_string (value, ide_tweaks_page_get_icon_name (self));
      break;

    case PROP_SECTION:
      g_value_set_object (value, ide_tweaks_page_get_section (self));
      break;

    case PROP_SHOW_ICON:
      g_value_set_boolean (value, ide_tweaks_page_get_show_icon (self));
      break;

    case PROP_SHOW_SEARCH:
      g_value_set_boolean (value, ide_tweaks_page_get_show_search (self));
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
    case PROP_ICON_NAME:
      ide_tweaks_page_set_icon_name (self, g_value_get_string (value));
      break;

    case PROP_SHOW_ICON:
      ide_tweaks_page_set_show_icon (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_SEARCH:
      ide_tweaks_page_set_show_search (self, g_value_get_boolean (value));
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
  IdeTweaksItemClass *item_class = IDE_TWEAKS_ITEM_CLASS (klass);

  object_class->dispose = ide_tweaks_page_dispose;
  object_class->get_property = ide_tweaks_page_get_property;
  object_class->set_property = ide_tweaks_page_set_property;

  item_class->accepts = ide_tweaks_page_accepts;
  item_class->match = ide_tweaks_page_match;

  properties [PROP_HAS_SUBPAGE] =
    g_param_spec_boolean ("has-subpage", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_ICON_NAME] =
    g_param_spec_string ("icon-name", NULL, NULL, NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SECTION] =
    g_param_spec_object ("section", NULL, NULL,
                         IDE_TYPE_TWEAKS_SECTION,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_SHOW_ICON] =
    g_param_spec_boolean ("show-icon", NULL, NULL,
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties[PROP_SHOW_SEARCH] =
    g_param_spec_boolean ("show-search", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL, NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_tweaks_page_init (IdeTweaksPage *self)
{
  self->show_icon = TRUE;
}

const char *
ide_tweaks_page_get_icon_name (IdeTweaksPage *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_PAGE (self), NULL);

  return self->icon_name;
}

void
ide_tweaks_page_set_icon_name (IdeTweaksPage *self,
                               const char    *icon_name)
{
  g_return_if_fail (IDE_IS_TWEAKS_PAGE (self));

  if (g_set_str (&self->icon_name, icon_name))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ICON_NAME]);
}

gboolean
ide_tweaks_page_get_show_icon (IdeTweaksPage *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_PAGE (self), FALSE);

  return self->show_icon;
}

void
ide_tweaks_page_set_show_icon (IdeTweaksPage *self,
                               gboolean       show_icon)
{
  g_return_if_fail (IDE_IS_TWEAKS_PAGE (self));

  show_icon = !!show_icon;

  if (show_icon != self->show_icon)
    {
      self->show_icon = show_icon;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SHOW_ICON]);
    }
}

gboolean
ide_tweaks_page_get_show_search (IdeTweaksPage *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_PAGE (self), FALSE);

  return self->show_search;
}

void
ide_tweaks_page_set_show_search (IdeTweaksPage *self,
                                 gboolean       show_search)
{
  g_return_if_fail (IDE_IS_TWEAKS_PAGE (self));

  show_search = !!show_search;

  if (show_search != self->show_search)
    {
      self->show_search = show_search;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SHOW_SEARCH]);
    }
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

  if (g_set_str (&self->title, title))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TITLE]);
}

/**
 * ide_tweaks_page_get_section:
 * @self: a #IdeTweaksPage
 *
 * Gets the section containing the page.
 *
 * Returns: (nullable) (transfer none): an #IdeTweaksItem or %NULL
 */
IdeTweaksItem *
ide_tweaks_page_get_section (IdeTweaksPage *self)
{
  IdeTweaksItem *item = (IdeTweaksItem *)self;

  g_return_val_if_fail (IDE_IS_TWEAKS_PAGE (self), NULL);

  while ((item = ide_tweaks_item_get_parent (item)))
    {
      if (IDE_IS_TWEAKS_PAGE (item))
        break;

      if (IDE_IS_TWEAKS_SECTION (item))
        return item;
    }

  return NULL;
}

/**
 * ide_tweaks_page_get_has_subpage:
 * @self: a #IdeTweaksPage
 *
 * Checks if @page has a subpage or a factory that can generate subpages.
 *
 * Returns: %TRUE if @self might have a subpage
 */
gboolean
ide_tweaks_page_get_has_subpage (IdeTweaksPage *self)
{
  static GType types[] = { G_TYPE_INVALID, G_TYPE_INVALID };

  g_return_val_if_fail (IDE_IS_TWEAKS_PAGE (self), FALSE);

  if G_UNLIKELY (types[0] == G_TYPE_INVALID)
    {
      types[0] = IDE_TYPE_TWEAKS_PAGE;
      types[1] = IDE_TYPE_TWEAKS_SECTION;
    }

  for (IdeTweaksItem *child = ide_tweaks_item_get_first_child (IDE_TWEAKS_ITEM (self));
       child != NULL;
       child = ide_tweaks_item_get_next_sibling (child))
    {
      for (guint i = 0; i < G_N_ELEMENTS (types); i++)
        if (G_TYPE_CHECK_INSTANCE_TYPE (child, types[i]))
          return TRUE;

      if (IDE_IS_TWEAKS_FACTORY (child) &&
          _ide_tweaks_factory_is_one_of (IDE_TWEAKS_FACTORY (child), types, G_N_ELEMENTS (types)))
        return TRUE;
    }

  return FALSE;
}
