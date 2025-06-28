/*
 * gbp-manuals-search-result.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include <glib/gi18n.h>

#include <gtksourceview/gtksource.h>

#include "gbp-manuals-page.h"
#include "gbp-manuals-search-provider.h"
#include "gbp-manuals-search-result.h"
#include "gbp-manuals-workspace-addin.h"

#include "manuals-navigatable.h"

struct _GbpManualsSearchResult
{
  IdeSearchResult      parent_instance;
  ManualsSearchResult *result;
  gulong               notify_handler;
};

enum {
  PROP_0,
  PROP_RESULT,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpManualsSearchResult, gbp_manuals_search_result, IDE_TYPE_SEARCH_RESULT)

static GParamSpec *properties[N_PROPS];

static IdeSearchPreview *
gbp_manuals_search_result_load_preview (IdeSearchResult *result,
                                        IdeContext      *context)
{
  return NULL;
}

static void
do_delayed_activate (ManualsSearchResult *result,
                     GParamSpec          *pspec,
                     GbpManualsPage      *page)
{
  ManualsNavigatable *navigatable;

  g_assert (MANUALS_IS_SEARCH_RESULT (result));
  g_assert (pspec != NULL);
  g_assert (GBP_IS_MANUALS_PAGE (page));

  g_signal_handlers_disconnect_by_func (result,
                                        G_CALLBACK (do_delayed_activate),
                                        page);

  if (gtk_widget_get_root (GTK_WIDGET (page)) == NULL)
    return;

  if ((navigatable = manuals_search_result_get_item (result)))
    gbp_manuals_page_navigate_to (page, navigatable);
}

static void
gbp_manuals_search_result_activate (IdeSearchResult *result,
                                    GtkWidget       *last_focus)
{
  GbpManualsSearchResult *self = (GbpManualsSearchResult *)result;
  ManualsNavigatable *navigatable;
  IdeWorkspaceAddin *workspace_addin;
  GbpManualsPage *page;
  IdeWorkspace *workspace;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_MANUALS_SEARCH_RESULT (self));
  g_assert (GTK_IS_WIDGET (last_focus));

  workspace = ide_widget_get_workspace (GTK_WIDGET (last_focus));
  workspace_addin = ide_workspace_addin_find_by_module_name (workspace, "manuals");

  g_assert (GBP_IS_MANUALS_WORKSPACE_ADDIN (workspace_addin));

  if (ide_application_control_is_pressed (NULL))
    page = gbp_manuals_workspace_addin_add_page (GBP_MANUALS_WORKSPACE_ADDIN (workspace_addin));
  else
    page = gbp_manuals_workspace_addin_get_page (GBP_MANUALS_WORKSPACE_ADDIN (workspace_addin));

  if ((navigatable = manuals_search_result_get_item (self->result)))
    {
      gbp_manuals_page_navigate_to (page, navigatable);
    }
  else
    {
      /* We have to wait for the lazy search item to be populated. */
      g_signal_connect_object (self->result,
                               "notify::item",
                               G_CALLBACK (do_delayed_activate),
                               page,
                               0);
    }

  gtk_widget_grab_focus (GTK_WIDGET (page));
}

static gboolean
gbp_manuals_search_result_matches (IdeSearchResult *result,
                                   const char      *query)
{
  const char *title;
  guint priority;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_MANUALS_SEARCH_RESULT (result));
  g_assert (query != NULL);

  if ((title = ide_search_result_get_title (result)))
    return gtk_source_completion_fuzzy_match (title, query, &priority);

  return TRUE;
}

static void
gbp_manuals_search_result_dispose (GObject *object)
{
  GbpManualsSearchResult *self = (GbpManualsSearchResult *)object;

  g_clear_signal_handler (&self->notify_handler, self->result);
  g_clear_object (&self->result);

  G_OBJECT_CLASS (gbp_manuals_search_result_parent_class)->dispose (object);
}

static void
gbp_manuals_search_result_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GbpManualsSearchResult *self = GBP_MANUALS_SEARCH_RESULT (object);

  switch (prop_id)
    {
    case PROP_RESULT:
      g_value_set_object (value, self->result);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_manuals_search_result_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  GbpManualsSearchResult *self = GBP_MANUALS_SEARCH_RESULT (object);

  switch (prop_id)
    {
    case PROP_RESULT:
      self->result = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_manuals_search_result_class_init (GbpManualsSearchResultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeSearchResultClass *search_result_class = IDE_SEARCH_RESULT_CLASS (klass);

  object_class->dispose = gbp_manuals_search_result_dispose;
  object_class->get_property = gbp_manuals_search_result_get_property;
  object_class->set_property = gbp_manuals_search_result_set_property;

  search_result_class->matches = gbp_manuals_search_result_matches;
  search_result_class->activate = gbp_manuals_search_result_activate;
  search_result_class->load_preview = gbp_manuals_search_result_load_preview;

  properties[PROP_RESULT] =
    g_param_spec_object ("result", NULL, NULL,
                         MANUALS_TYPE_SEARCH_RESULT,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_manuals_search_result_init (GbpManualsSearchResult *self)
{
}

static void
update_when_item_changes (GbpManualsSearchResult *self,
                          GParamSpec             *pspec,
                          ManualsSearchResult    *result)
{
  ManualsNavigatable *item;

  g_assert (GBP_IS_MANUALS_SEARCH_RESULT (self));
  g_assert (MANUALS_IS_SEARCH_RESULT (result));

  if ((item = manuals_search_result_get_item (result)))
    {
      GIcon *icon = manuals_navigatable_get_icon (item);
      const char *title = manuals_navigatable_get_title (item);

      ide_search_result_set_gicon (IDE_SEARCH_RESULT (self), icon);
      ide_search_result_set_title (IDE_SEARCH_RESULT (self), title);

      g_clear_signal_handler (&self->notify_handler, self->result);
    }
}

GbpManualsSearchResult *
gbp_manuals_search_result_new (ManualsSearchResult *result)
{
  GbpManualsSearchResult *self;
  ManualsNavigatable *item;

  g_return_val_if_fail (MANUALS_IS_SEARCH_RESULT (result), NULL);

  self = g_object_new (GBP_TYPE_MANUALS_SEARCH_RESULT,
                       "subtitle", _("Open Documentation"),
                       NULL);
  self->result = result;

  if (!(item = manuals_search_result_get_item (result)))
    self->notify_handler =
      g_signal_connect_object (result,
                               "notify::item",
                               G_CALLBACK (update_when_item_changes),
                               self,
                               G_CONNECT_SWAPPED);
  else
    update_when_item_changes (self, NULL, result);

  return self;
}
