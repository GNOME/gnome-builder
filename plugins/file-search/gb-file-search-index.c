/* gb-file-search-index.c
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

#include <fuzzy.h>
#include <glib/gi18n.h>

#include "gb-file-search-index.h"

struct _GbFileSearchIndex
{
  GObject       parent_instance;

  GFile        *root_directory;
  GFileMonitor *file_monitor;
  Fuzzy        *fuzzy;
};

G_DEFINE_TYPE (GbFileSearchIndex, gb_file_search_index, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ROOT_DIRECTORY,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static void
gb_file_search_index_set_root_directory (GbFileSearchIndex *self,
                                         GFile             *root_directory)
{
  g_return_if_fail (GB_IS_FILE_SEARCH_INDEX (self));
  g_return_if_fail (!root_directory || G_IS_FILE (root_directory));

  if (g_set_object (&self->root_directory, root_directory))
    {
      g_clear_pointer (&self->fuzzy, fuzzy_unref);

      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_ROOT_DIRECTORY]);
    }
}

static void
gb_file_search_index_finalize (GObject *object)
{
  GbFileSearchIndex *self = (GbFileSearchIndex *)object;

  g_clear_object (&self->root_directory);
  g_clear_object (&self->file_monitor);

  G_OBJECT_CLASS (gb_file_search_index_parent_class)->finalize (object);
}

static void
gb_file_search_index_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GbFileSearchIndex *self = GB_FILE_SEARCH_INDEX (object);

  switch (prop_id)
    {
    case PROP_ROOT_DIRECTORY:
      g_value_set_object (value, self->root_directory);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_file_search_index_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GbFileSearchIndex *self = GB_FILE_SEARCH_INDEX (object);

  switch (prop_id)
    {
    case PROP_ROOT_DIRECTORY:
      gb_file_search_index_set_root_directory (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_file_search_index_class_init (GbFileSearchIndexClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_file_search_index_finalize;
  object_class->get_property = gb_file_search_index_get_property;
  object_class->set_property = gb_file_search_index_set_property;

  gParamSpecs [PROP_ROOT_DIRECTORY] =
    g_param_spec_object ("root-directory",
                         _("Root Directory"),
                         _("Root Directory"),
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);
}

static void
gb_file_search_index_init (GbFileSearchIndex *self)
{
}
