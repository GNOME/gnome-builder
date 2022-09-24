/* ide-lsp-search-result.c
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

#define G_LOG_DOMAIN "ide-lsp-search-result"

#include "config.h"

#include "ide-lsp-search-result.h"

#include <libide-core.h>
#include <libide-editor.h>

struct _IdeLspSearchResult
{
  IdeSearchResult  parent_instance;
  IdeLocation     *location;
};

G_DEFINE_FINAL_TYPE (IdeLspSearchResult, ide_lsp_search_result, IDE_TYPE_SEARCH_RESULT)

enum {
  PROP_0,
  PROP_LOCATION,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

IdeLspSearchResult *
ide_lsp_search_result_new (const gchar *title,
                           const gchar *subtitle,
                           IdeLocation *location,
                           const gchar *icon_name)
{
  g_autoptr(GIcon) gicon = NULL;

  if (icon_name != NULL)
    gicon = g_themed_icon_new (icon_name);

  return g_object_new (IDE_TYPE_LSP_SEARCH_RESULT,
                       "title", title,
                       "subtitle", subtitle,
                       "location", location,
                       "gicon", gicon,
                       "priority", -1,
                       NULL);
}

static void
ide_lsp_search_result_finalize (GObject *object)
{
  IdeLspSearchResult *self = (IdeLspSearchResult *)object;

  g_clear_object (&self->location);

  G_OBJECT_CLASS (ide_lsp_search_result_parent_class)->finalize (object);
}

static void
ide_lsp_search_result_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeLspSearchResult *self = IDE_LSP_SEARCH_RESULT (object);

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
ide_lsp_search_result_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeLspSearchResult *self = IDE_LSP_SEARCH_RESULT (object);

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
ide_lsp_search_result_activate (IdeSearchResult *result,
                                GtkWidget       *last_focus)
{

  IdeLspSearchResult *self = (IdeLspSearchResult *)result;
  IdeWorkspace *workspace;

  g_assert (IDE_IS_LSP_SEARCH_RESULT (self));
  g_assert (GTK_IS_WIDGET (last_focus));

  if (!last_focus || !(workspace = ide_widget_get_workspace (last_focus)))
    return;

  ide_editor_focus_location (workspace, NULL, self->location);
}

static void
ide_lsp_search_result_class_init (IdeLspSearchResultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeSearchResultClass *search_class = IDE_SEARCH_RESULT_CLASS (klass);

  object_class->finalize = ide_lsp_search_result_finalize;
  object_class->get_property = ide_lsp_search_result_get_property;
  object_class->set_property = ide_lsp_search_result_set_property;
  search_class->activate = ide_lsp_search_result_activate;

  properties [PROP_LOCATION] =
    g_param_spec_object ("location",
                         "location",
                         "Location of the symbol",
                         IDE_TYPE_LOCATION,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_lsp_search_result_init (IdeLspSearchResult *self)
{
}
