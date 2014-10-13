/* gb-preferences-page.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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
 */

#include <glib/gi18n.h>

#include "gb-preferences-page.h"
#include "gb-string.h"

struct _GbPreferencesPagePrivate
{
  gchar *title;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbPreferencesPage, gb_preferences_page,
                            GTK_TYPE_BIN)

enum {
  PROP_0,
  PROP_TITLE,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

const gchar *
gb_preferences_page_get_title (GbPreferencesPage *page)
{
  g_return_val_if_fail (GB_IS_PREFERENCES_PAGE (page), NULL);

  return page->priv->title;
}

void
gb_preferences_page_set_title (GbPreferencesPage *page,
                               const gchar       *title)
{
  g_return_if_fail (GB_IS_PREFERENCES_PAGE (page));

  if (!gb_str_equal0 (title, page->priv->title))
    {
      g_free (page->priv->title);
      page->priv->title = g_strdup (title);
      g_object_notify_by_pspec (G_OBJECT (page),
                                gParamSpecs [PROP_TITLE]);
    }
}

static void
gb_preferences_page_finalize (GObject *object)
{
  GbPreferencesPagePrivate *priv = GB_PREFERENCES_PAGE (object)->priv;

  g_clear_pointer (&priv->title, g_free);

  G_OBJECT_CLASS (gb_preferences_page_parent_class)->finalize (object);
}

static void
gb_preferences_page_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GbPreferencesPage *self = GB_PREFERENCES_PAGE (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      g_value_set_string (value, gb_preferences_page_get_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_preferences_page_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GbPreferencesPage *self = GB_PREFERENCES_PAGE (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      gb_preferences_page_set_title (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_preferences_page_class_init (GbPreferencesPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_preferences_page_finalize;
  object_class->get_property = gb_preferences_page_get_property;
  object_class->set_property = gb_preferences_page_set_property;

  gParamSpecs [PROP_TITLE] =
    g_param_spec_string ("title",
                         _("Title"),
                         _("The title for the preferences page."),
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_TITLE,
                                   gParamSpecs [PROP_TITLE]);
}

static void
gb_preferences_page_init (GbPreferencesPage *self)
{
  self->priv = gb_preferences_page_get_instance_private (self);
}
