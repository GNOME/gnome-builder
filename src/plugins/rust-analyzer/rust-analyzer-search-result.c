/* rust-analyzer-search-result.c
 *
 * Copyright 2020 Günther Wagner <info@gunibert.de>
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

#include "rust-analyzer-search-result.h"
#include <libide-editor.h>

struct _RustAnalyzerSearchResult
{
  IdeSearchResult  parent_instance;
  IdeLocation     *location;
};

G_DEFINE_TYPE (RustAnalyzerSearchResult, rust_analyzer_search_result, IDE_TYPE_SEARCH_RESULT)

enum {
  PROP_0,
  PROP_LOCATION,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

RustAnalyzerSearchResult *
rust_analyzer_search_result_new (const gchar *title,
                                 const gchar *subtitle,
                                 IdeLocation *location,
                                 const gchar *icon_name)
{
  return g_object_new (RUST_TYPE_ANALYZER_SEARCH_RESULT,
                       "title", title,
                       "subtitle", subtitle,
                       "location", location,
                       "icon-name", icon_name,
                       // place search results before the other search providers
                       "priority", -1,
                       NULL);
}

static void
rust_analyzer_search_result_finalize (GObject *object)
{
  RustAnalyzerSearchResult *self = (RustAnalyzerSearchResult *)object;

  g_clear_object (&self->location);

  G_OBJECT_CLASS (rust_analyzer_search_result_parent_class)->finalize (object);
}

static void
rust_analyzer_search_result_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  RustAnalyzerSearchResult *self = RUST_ANALYZER_SEARCH_RESULT (object);

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
rust_analyzer_search_result_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  RustAnalyzerSearchResult *self = RUST_ANALYZER_SEARCH_RESULT (object);

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
rust_analyzer_search_result_activate (IdeSearchResult *result,
                                      GtkWidget       *last_focus)
{

  RustAnalyzerSearchResult *self = (RustAnalyzerSearchResult *)result;
  IdeWorkspace *workspace;
  IdeSurface *editor;

  g_assert (RUST_IS_ANALYZER_SEARCH_RESULT (self));
  g_assert (GTK_IS_WIDGET (last_focus));

  if (!last_focus)
    return;

  if ((workspace = ide_widget_get_workspace (last_focus)) &&
      (editor = ide_workspace_get_surface_by_name (workspace, "editor")))
    ide_editor_surface_focus_location (IDE_EDITOR_SURFACE (editor), self->location);
}

static void
rust_analyzer_search_result_class_init (RustAnalyzerSearchResultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeSearchResultClass *search_class = IDE_SEARCH_RESULT_CLASS (klass);

  object_class->finalize = rust_analyzer_search_result_finalize;
  object_class->get_property = rust_analyzer_search_result_get_property;
  object_class->set_property = rust_analyzer_search_result_set_property;
  search_class->activate = rust_analyzer_search_result_activate;

  properties [PROP_LOCATION] =
    g_param_spec_object ("location",
                         "location",
                         "Location of the symbol",
                         IDE_TYPE_LOCATION,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
rust_analyzer_search_result_init (RustAnalyzerSearchResult *self)
{
}
