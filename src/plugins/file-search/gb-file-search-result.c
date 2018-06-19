/* gb-file-search-result.c
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "gb-file-search-result"

#include "gb-file-search-result.h"

struct _GbFileSearchResult
{
  IdeSearchResult  parent_instance;

  IdeContext      *context;
  gchar           *path;
};

G_DEFINE_TYPE (GbFileSearchResult, gb_file_search_result, IDE_TYPE_SEARCH_RESULT)

enum {
  PROP_0,
  PROP_CONTEXT,
  PROP_PATH,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static IdeSourceLocation *
gb_file_search_result_get_source_location (IdeSearchResult *result)
{
  GbFileSearchResult *self = (GbFileSearchResult *)result;
  g_autoptr(GFile) file = NULL;
  g_autoptr(IdeFile) ifile = NULL;
  IdeVcs *vcs;
  GFile *workdir;

  g_return_val_if_fail (GB_IS_FILE_SEARCH_RESULT (self), NULL);

  vcs = ide_context_get_vcs (self->context);
  workdir = ide_vcs_get_working_directory (vcs);
  file = g_file_get_child (workdir, self->path);
  ifile = ide_file_new (self->context, file);

  return ide_source_location_new (ifile, 0, 0, 0);
}

static void
gb_file_search_result_finalize (GObject *object)
{
  GbFileSearchResult *self = (GbFileSearchResult *)object;

  dzl_clear_weak_pointer (&self->context);
  dzl_clear_pointer (&self->path, g_free);

  G_OBJECT_CLASS (gb_file_search_result_parent_class)->finalize (object);
}

static void
gb_file_search_result_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GbFileSearchResult *self = (GbFileSearchResult *)object;

  switch (prop_id)
    {
    case PROP_PATH:
      g_value_set_string (value, self->path);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_file_search_result_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GbFileSearchResult *self = (GbFileSearchResult *)object;

  switch (prop_id)
    {
    case PROP_CONTEXT:
      dzl_set_weak_pointer (&self->context, g_value_get_object (value));
      break;

    case PROP_PATH:
      self->path = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_file_search_result_class_init (GbFileSearchResultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeSearchResultClass *result_class = IDE_SEARCH_RESULT_CLASS (klass);

  object_class->finalize = gb_file_search_result_finalize;
  object_class->get_property = gb_file_search_result_get_property;
  object_class->set_property = gb_file_search_result_set_property;

  result_class->get_source_location = gb_file_search_result_get_source_location;

  properties [PROP_CONTEXT] =
    g_param_spec_object ("context",
                         "Context",
                         "The context for the result",
                         IDE_TYPE_CONTEXT,
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_PATH] =
    g_param_spec_string ("path",
                         "Path",
                         "The relative path to the file.",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
gb_file_search_result_init (GbFileSearchResult *self)
{
}
