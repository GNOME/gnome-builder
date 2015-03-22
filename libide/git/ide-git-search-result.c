/* ide-git-search-result.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>

#include "ide-git-search-result.h"

struct _IdeGitSearchResult
{
  IdeSearchResult  parent_instance;
  GFile           *file;
};

enum
{
  PROP_0,
  PROP_FILE,
  LAST_PROP
};

G_DEFINE_TYPE (IdeGitSearchResult, ide_git_search_result, IDE_TYPE_SEARCH_RESULT)

static GParamSpec *gParamSpecs [LAST_PROP];

static void
ide_git_search_result_finalize (GObject *object)
{
  IdeGitSearchResult *self = (IdeGitSearchResult *)object;

  g_clear_object (&self->file);

  G_OBJECT_CLASS (ide_git_search_result_parent_class)->finalize (object);
}

static void
ide_git_search_result_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeGitSearchResult *self = IDE_GIT_SEARCH_RESULT (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, self->file);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_git_search_result_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeGitSearchResult *self = IDE_GIT_SEARCH_RESULT (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_set_object (&self->file, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_git_search_result_class_init (IdeGitSearchResultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = ide_git_search_result_finalize;
  object_class->get_property = ide_git_search_result_get_property;
  object_class->set_property = ide_git_search_result_set_property;

  gParamSpecs [PROP_FILE] =
    g_param_spec_object ("file",
                         _("File"),
                         _("The file to be opened."),
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_FILE, gParamSpecs [PROP_FILE]);
}

static void
ide_git_search_result_init (IdeGitSearchResult *result)
{
}
