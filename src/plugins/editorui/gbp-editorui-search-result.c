/* gbp-editorui-search-result.c
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-editorui-search-result"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-sourceview.h>
#include <libide-gui.h>

#include "gbp-editorui-search-result.h"

struct _GbpEditoruiSearchResult
{
  IdeSearchResult parent_instance;
  GtkSourceStyleScheme *scheme;
};

enum {
  PROP_0,
  PROP_NAME,
  PROP_SCHEME,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpEditoruiSearchResult, gbp_editorui_search_result, IDE_TYPE_SEARCH_RESULT)

static GParamSpec *properties [N_PROPS];

static void
gbp_editorui_search_result_set_scheme (GbpEditoruiSearchResult *self,
                                       GtkSourceStyleScheme    *scheme)
{
  g_autofree char *title = NULL;
  const char *name;

  g_assert (GBP_IS_EDITORUI_SEARCH_RESULT (self));
  g_assert (GTK_SOURCE_IS_STYLE_SCHEME (scheme));

  g_set_object (&self->scheme, scheme);

  name = gtk_source_style_scheme_get_name (scheme);
  title = g_strdup_printf (_("Switch to %s style"), name);

  ide_search_result_set_title (IDE_SEARCH_RESULT (self), title);
}

static void
gbp_editorui_search_result_activate (IdeSearchResult *search_result,
                                     GtkWidget       *last_focus)
{
  GbpEditoruiSearchResult *self = (GbpEditoruiSearchResult *)search_result;
  const char *scheme_id;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_EDITORUI_SEARCH_RESULT (self));

  scheme_id = gtk_source_style_scheme_get_id (self->scheme);

  ide_application_set_style_scheme (IDE_APPLICATION_DEFAULT, scheme_id);

  /* We have to force light/dark here or we may not get consistent results
   * from the style-scheme when loading.
   */
  adw_style_manager_set_color_scheme (adw_style_manager_get_default (),
                                      ide_source_style_scheme_is_dark (self->scheme) ?
                                       ADW_COLOR_SCHEME_FORCE_DARK : ADW_COLOR_SCHEME_FORCE_LIGHT);

  IDE_EXIT;
}

static void
gbp_editorui_search_result_finalize (GObject *object)
{
  GbpEditoruiSearchResult *self = (GbpEditoruiSearchResult *)object;

  g_clear_object (&self->scheme);

  G_OBJECT_CLASS (gbp_editorui_search_result_parent_class)->finalize (object);
}

static void
gbp_editorui_search_result_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  GbpEditoruiSearchResult *self = GBP_EDITORUI_SEARCH_RESULT (object);

  switch (prop_id)
    {
    case PROP_NAME:
      if (self->scheme)
        g_value_set_static_string (value, g_intern_string (gtk_source_style_scheme_get_name (self->scheme)));
      break;

    case PROP_SCHEME:
      g_value_set_object (value, self->scheme);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_editorui_search_result_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  GbpEditoruiSearchResult *self = GBP_EDITORUI_SEARCH_RESULT (object);

  switch (prop_id)
    {
    case PROP_SCHEME:
      gbp_editorui_search_result_set_scheme (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_editorui_search_result_class_init (GbpEditoruiSearchResultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeSearchResultClass *search_result_class = IDE_SEARCH_RESULT_CLASS (klass);

  object_class->finalize = gbp_editorui_search_result_finalize;
  object_class->get_property = gbp_editorui_search_result_get_property;
  object_class->set_property = gbp_editorui_search_result_set_property;

  search_result_class->activate = gbp_editorui_search_result_activate;

  properties [PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SCHEME] =
    g_param_spec_object ("scheme", NULL, NULL,
                         GTK_SOURCE_TYPE_STYLE_SCHEME,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_editorui_search_result_init (GbpEditoruiSearchResult *self)
{
  static GIcon *icon;

  if (icon == NULL)
    icon = g_themed_icon_new ("preferences-color-symbolic");

  ide_search_result_set_gicon (IDE_SEARCH_RESULT (self), icon);
  ide_search_result_set_subtitle (IDE_SEARCH_RESULT (self),
                                  _("Switch application and editor theme"));
}

IdeSearchResult *
gbp_editorui_search_result_new (GtkSourceStyleScheme *scheme)
{
  g_return_val_if_fail (GTK_SOURCE_IS_STYLE_SCHEME (scheme), NULL);

  return g_object_new (GBP_TYPE_EDITORUI_SEARCH_RESULT,
                       "scheme", scheme,
                       NULL);
}
