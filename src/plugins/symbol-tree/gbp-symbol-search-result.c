/* gbp-symbol-search-result.c
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

#define G_LOG_DOMAIN "gbp-symbol-search-result"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-editor.h>
#include <libide-gui.h>
#include <libide-sourceview.h>

#include "gbp-symbol-search-result.h"

struct _GbpSymbolSearchResult
{
  IdeSearchResult  parent_instance;
  IdeSymbolNode   *node;
  GFile           *file;
};

enum {
  PROP_0,
  PROP_FILE,
  PROP_NODE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpSymbolSearchResult, gbp_symbol_search_result, IDE_TYPE_SEARCH_RESULT)

static GParamSpec *properties [N_PROPS];

static void
gbp_symbol_search_result_preview_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  IdeSymbolNode *node = (IdeSymbolNode *)object;
  g_autoptr(IdeFileSearchPreview) preview = user_data;
  g_autoptr(IdeLocation) location = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SYMBOL_NODE (node));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_FILE_SEARCH_PREVIEW (preview));

  if ((location = ide_symbol_node_get_location_finish (node, result, &error)))
    ide_file_search_preview_scroll_to (preview, location);

  IDE_EXIT;
}

static IdeSearchPreview *
gbp_symbol_search_result_load_preview (IdeSearchResult *result,
                                       IdeContext      *context)
{
  GbpSymbolSearchResult *self = (GbpSymbolSearchResult *)result;
  IdeSearchPreview *preview;

  IDE_ENTRY;

  g_assert (IDE_IS_SEARCH_RESULT (result));

  if (self->file == NULL)
    IDE_RETURN (NULL);

  preview = ide_file_search_preview_new (self->file);

  ide_symbol_node_get_location_async (self->node,
                                      NULL,
                                      gbp_symbol_search_result_preview_cb,
                                      g_object_ref (preview));

  IDE_RETURN (preview);
}

static void
gbp_symbol_search_result_get_location_cb (GObject      *object,
                                          GAsyncResult *result,
                                          gpointer      user_data)
{
  IdeSymbolNode *node = (IdeSymbolNode *)object;
  g_autoptr(IdeWorkspace) workspace = user_data;
  g_autoptr(IdeLocation) location = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SYMBOL_NODE (node));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_WORKSPACE (workspace));

  if ((location = ide_symbol_node_get_location_finish (node, result, &error)))
    ide_editor_focus_location (workspace, NULL, location);
  else
    ide_object_message (ide_workspace_get_context (workspace),
                        "Failed to locate location for symbol");

  IDE_EXIT;
}

static void
gbp_symbol_search_result_activate (IdeSearchResult *result,
                                   GtkWidget       *last_focus)
{
  GbpSymbolSearchResult *self = (GbpSymbolSearchResult *)result;
  IdeWorkspace *workspace;

  IDE_ENTRY;

  g_assert (IDE_IS_SEARCH_RESULT (result));
  g_assert (GTK_IS_WIDGET (last_focus));

  workspace = ide_widget_get_workspace (last_focus);

  ide_symbol_node_get_location_async (self->node,
                                      NULL,
                                      gbp_symbol_search_result_get_location_cb,
                                      g_object_ref (workspace));

  IDE_EXIT;
}

static void
gbp_symbol_search_result_set_node (GbpSymbolSearchResult *self,
                                   IdeSymbolNode         *node)
{
  g_assert (GBP_IS_SYMBOL_SEARCH_RESULT (self));
  g_assert (IDE_IS_SYMBOL_NODE (node));

  self->node = g_object_ref (node);

  ide_search_result_set_gicon (IDE_SEARCH_RESULT (self), ide_symbol_node_get_gicon (node));
  ide_search_result_set_title (IDE_SEARCH_RESULT (self), ide_symbol_node_get_name (node));
  ide_search_result_set_use_markup (IDE_SEARCH_RESULT (self), ide_symbol_node_get_use_markup (node));
}

static gboolean
gbp_symbol_search_result_matches (IdeSearchResult *result,
                                  const char      *query)
{
  GbpSymbolSearchResult *self = (GbpSymbolSearchResult *)result;
  g_autofree char *display_name = NULL;
  const char *name;
  guint prio;

  g_assert (GBP_IS_SYMBOL_SEARCH_RESULT (self));
  g_assert (IDE_IS_SYMBOL_NODE (self->node));

  if (query == NULL)
    return TRUE;

  name = ide_symbol_node_get_name (self->node);
  if (name && gtk_source_completion_fuzzy_match (name, query, &prio))
    return TRUE;

  g_object_get (self->node, "display-name", &display_name, NULL);
  if (display_name && gtk_source_completion_fuzzy_match (display_name, query, &prio))
    return TRUE;

  return FALSE;
}

static void
gbp_symbol_search_result_dispose (GObject *object)
{
  GbpSymbolSearchResult *self = (GbpSymbolSearchResult *)object;

  g_clear_object (&self->file);
  g_clear_object (&self->node);

  G_OBJECT_CLASS (gbp_symbol_search_result_parent_class)->dispose (object);
}

static void
gbp_symbol_search_result_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GbpSymbolSearchResult *self = GBP_SYMBOL_SEARCH_RESULT (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, self->file);
      break;

    case PROP_NODE:
      g_value_set_object (value, gbp_symbol_search_result_get_node (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_symbol_search_result_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GbpSymbolSearchResult *self = GBP_SYMBOL_SEARCH_RESULT (object);

  switch (prop_id)
    {
    case PROP_FILE:
      self->file = g_value_dup_object (value);
      break;

    case PROP_NODE:
      gbp_symbol_search_result_set_node (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_symbol_search_result_class_init (GbpSymbolSearchResultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeSearchResultClass *search_result_class = IDE_SEARCH_RESULT_CLASS (klass);

  object_class->dispose = gbp_symbol_search_result_dispose;
  object_class->get_property = gbp_symbol_search_result_get_property;
  object_class->set_property = gbp_symbol_search_result_set_property;

  search_result_class->activate = gbp_symbol_search_result_activate;
  search_result_class->matches = gbp_symbol_search_result_matches;
  search_result_class->load_preview = gbp_symbol_search_result_load_preview;

  properties [PROP_FILE] =
    g_param_spec_object ("file", NULL, NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_NODE] =
    g_param_spec_object ("node", NULL, NULL,
                         IDE_TYPE_SYMBOL_NODE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_symbol_search_result_init (GbpSymbolSearchResult *self)
{
}

GbpSymbolSearchResult *
gbp_symbol_search_result_new (IdeSymbolNode *node,
                              GFile         *file)
{
  g_autofree char *subtitle = NULL;

  g_return_val_if_fail (IDE_IS_SYMBOL_NODE (node), NULL);
  g_return_val_if_fail (!file || G_IS_FILE (file), NULL);

  if (file != NULL)
    {
      g_autofree char *basename = g_file_get_basename (file);
      g_autofree char *escaped = g_markup_escape_text (basename, -1);

      /* translators: "In Page" refers to the title of the page which contains the search result */
      subtitle = g_strdup_printf ("<span fgalpha='32767'>%s</span> %s", _("In Page"), escaped);
    }

  return g_object_new (GBP_TYPE_SYMBOL_SEARCH_RESULT,
                       "file", file,
                       "node", node,
                       "subtitle", subtitle,
                       NULL);
}

IdeSymbolNode *
gbp_symbol_search_result_get_node (GbpSymbolSearchResult *self)
{
  g_return_val_if_fail (GBP_IS_SYMBOL_SEARCH_RESULT (self), NULL);

  return self->node;
}
