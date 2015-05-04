/* ide-devhelp-search-result.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include "ide-devhelp-search-result.h"

struct _IdeDevhelpSearchResult
{
  IdeSearchResult  parent_instance;
  gchar           *uri;
};

enum
{
  PROP_0,
  PROP_URI,
  LAST_PROP
};

G_DEFINE_TYPE (IdeDevhelpSearchResult, ide_devhelp_search_result, IDE_TYPE_SEARCH_RESULT)

static GParamSpec *gParamSpecs [LAST_PROP];

static void
ide_devhelp_search_result_finalize (GObject *object)
{
  IdeDevhelpSearchResult *self = (IdeDevhelpSearchResult *)object;

  g_clear_pointer (&self->uri, g_free);

  G_OBJECT_CLASS (ide_devhelp_search_result_parent_class)->finalize (object);
}

static void
ide_devhelp_search_result_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  IdeDevhelpSearchResult *self = IDE_DEVHELP_SEARCH_RESULT (object);

  switch (prop_id)
    {
    case PROP_URI:
      g_value_set_string (value, self->uri);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_devhelp_search_result_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  IdeDevhelpSearchResult *self = IDE_DEVHELP_SEARCH_RESULT (object);

  switch (prop_id)
    {
    case PROP_URI:
      self->uri = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_devhelp_search_result_class_init (IdeDevhelpSearchResultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_devhelp_search_result_finalize;
  object_class->get_property = ide_devhelp_search_result_get_property;
  object_class->set_property = ide_devhelp_search_result_set_property;

  gParamSpecs [PROP_URI] =
    g_param_spec_string ("uri",
                         _("URI"),
                         _("The URI to the Devhelp document."),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);
}

static void
ide_devhelp_search_result_init (IdeDevhelpSearchResult *result)
{
}
