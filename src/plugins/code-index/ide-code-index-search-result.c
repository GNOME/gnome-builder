/* ide-code-index-search-result.c
 *
 * Copyright 2017 Anoop Chandu <anoopchandu96@gmail.com>
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

#define G_LOG_DOMAIN "ide-code-index-search-result"

#include "config.h"

#include <libide-code.h>
#include <libide-editor.h>

#include "ide-code-index-search-result.h"

struct _IdeCodeIndexSearchResult
{
  IdeSearchResult  parent;
  IdeLocation     *location;
};

G_DEFINE_FINAL_TYPE (IdeCodeIndexSearchResult, ide_code_index_search_result, IDE_TYPE_SEARCH_RESULT)

enum {
  PROP_0,
  PROP_LOCATION,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_code_index_search_result_activate (IdeSearchResult *result,
                                       GtkWidget       *last_focus)
{
  IdeCodeIndexSearchResult *self = (IdeCodeIndexSearchResult *)result;
  g_autoptr(PanelPosition) position = NULL;
  IdeWorkspace *workspace;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CODE_INDEX_SEARCH_RESULT (self));
  g_assert (GTK_IS_WIDGET (last_focus));

  workspace = ide_widget_get_workspace (last_focus);
  position = panel_position_new ();

  ide_editor_focus_location (workspace, position, self->location);

  IDE_EXIT;
}

static IdeSearchPreview *
ide_code_index_search_result_load_preview (IdeSearchResult *result,
                                           IdeContext      *context)
{
  IdeCodeIndexSearchResult *self = (IdeCodeIndexSearchResult *)result;
  IdeSearchPreview *preview = NULL;
  GFile *file;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CODE_INDEX_SEARCH_RESULT (self));

  if ((file = ide_location_get_file (self->location)))
    {
      preview = ide_file_search_preview_new (file);
      ide_file_search_preview_scroll_to (IDE_FILE_SEARCH_PREVIEW (preview),
                                         self->location);
    }

  IDE_RETURN (preview);
}

static void
ide_code_index_search_result_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  IdeCodeIndexSearchResult *self = (IdeCodeIndexSearchResult *)object;

  switch (prop_id)
    {
    case PROP_LOCATION:
      g_value_set_object (value, self->location);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_code_index_search_result_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  IdeCodeIndexSearchResult *self = (IdeCodeIndexSearchResult *)object;

  switch (prop_id)
    {
    case PROP_LOCATION:
      self->location = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_code_index_search_result_finalize (GObject *object)
{
  IdeCodeIndexSearchResult *self = (IdeCodeIndexSearchResult *)object;

  g_clear_object (&self->location);

  G_OBJECT_CLASS (ide_code_index_search_result_parent_class)->finalize (object);
}

static void
ide_code_index_search_result_class_init (IdeCodeIndexSearchResultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeSearchResultClass *result_class = IDE_SEARCH_RESULT_CLASS (klass);

  object_class->get_property = ide_code_index_search_result_get_property;
  object_class->set_property = ide_code_index_search_result_set_property;
  object_class->finalize = ide_code_index_search_result_finalize;

  result_class->activate = ide_code_index_search_result_activate;
  result_class->load_preview = ide_code_index_search_result_load_preview;

  properties [PROP_LOCATION] =
    g_param_spec_object ("location",
                         "location",
                         "Location of symbol.",
                         IDE_TYPE_LOCATION,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_code_index_search_result_init (IdeCodeIndexSearchResult *self)
{
}

IdeCodeIndexSearchResult *
ide_code_index_search_result_new (const char  *title,
                                  const char  *subtitle,
                                  GIcon       *gicon,
                                  IdeLocation *location,
                                  float        score)
{
  g_autofree char *etitle = g_markup_escape_text (title, -1);

  return g_object_new (IDE_TYPE_CODE_INDEX_SEARCH_RESULT,
                       "title", etitle,
                       "subtitle", subtitle,
                       "gicon", gicon,
                       "location", location,
                       "score", score,
                       NULL);
}
