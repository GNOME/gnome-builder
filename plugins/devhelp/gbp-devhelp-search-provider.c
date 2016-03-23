/* gbp-devhelp-search-provider.c
 *
 * Copyright (C) 2015 Erick Pérez Castellanos <erick.red@gmail.com>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "devhelp-search"

#include <ctype.h>
#include <devhelp/devhelp.h>
#include <glib/gi18n.h>
#include <ide.h>
#include <libpeas/peas.h>

#include "gbp-devhelp-panel.h"
#include "gbp-devhelp-search-provider.h"
#include "gbp-devhelp-search-result.h"

struct _GbpDevhelpSearchProvider
{
  IdeObject          parent;

  DhBookManager     *book_manager;
  DhKeywordModel    *keywords_model;
};

static void search_provider_iface_init (IdeSearchProviderInterface *iface);

G_DEFINE_TYPE_EXTENDED (GbpDevhelpSearchProvider,
                        gbp_devhelp_search_provider,
                        IDE_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_SEARCH_PROVIDER,
                                               search_provider_iface_init))

static void
gbp_devhelp_search_provider_populate (IdeSearchProvider *provider,
                                      IdeSearchContext  *context,
                                      const gchar       *search_terms,
                                      gsize              max_results,
                                      GCancellable      *cancellable)
{
  GbpDevhelpSearchProvider *self = (GbpDevhelpSearchProvider *)provider;
  g_auto(IdeSearchReducer) reducer = { 0 };
  IdeContext *idecontext;
  GtkTreeIter iter;
  gboolean valid;
  gint count = 0;
  gint total;

  g_assert (GBP_IS_DEVHELP_SEARCH_PROVIDER (self));
  g_assert (IDE_IS_SEARCH_CONTEXT (context));
  g_assert (search_terms);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (search_terms [0] == '\0')
    {
      ide_search_context_provider_completed (context, provider);
      return;
    }

  idecontext = ide_object_get_context (IDE_OBJECT (provider));

  dh_keyword_model_filter (self->keywords_model, search_terms, NULL, NULL);

  ide_search_reducer_init (&reducer, context, provider, max_results);

  valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->keywords_model), &iter);
  total = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (self->keywords_model), NULL);

  while (valid)
    {
      g_autoptr(IdeSearchResult) result = NULL;
      g_autofree gchar *name = NULL;
      DhLink *link = NULL;
      gfloat score = (total - count) / (gfloat)total;

      gtk_tree_model_get (GTK_TREE_MODEL (self->keywords_model), &iter,
                          DH_KEYWORD_MODEL_COL_NAME, &name,
                          DH_KEYWORD_MODEL_COL_LINK, &link,
                          -1);

      valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (self->keywords_model), &iter);

      /* we traverse from best to worst, so just break */
      if (!ide_search_reducer_accepts (&reducer, score))
        break;

      count++;

      if ((dh_link_get_flags (link) & DH_LINK_FLAGS_DEPRECATED) != 0)
        {
          gchar *italic_name = g_strdup_printf ("<i>%s</i>", name);
          g_free (name);
          name = italic_name;
        }

      result = g_object_new (GBP_TYPE_DEVHELP_SEARCH_RESULT,
                             "context", idecontext,
                             "provider", provider,
                             "title", name,
                             "subtitle", dh_link_get_book_name (link),
                             "score", score,
                             "uri", dh_link_get_uri (link),
                             NULL);

      ide_search_reducer_push (&reducer, result);
    }

  ide_search_context_provider_completed (context, provider);
}

static const gchar *
gbp_devhelp_search_provider_get_verb (IdeSearchProvider *provider)
{
  return _("Documentation");
}

static void
gbp_devhelp_search_provider_constructed (GObject *object)
{
  GbpDevhelpSearchProvider *self = GBP_DEVHELP_SEARCH_PROVIDER (object);

  dh_book_manager_populate (self->book_manager);
  dh_keyword_model_set_words (self->keywords_model, self->book_manager);
}

static GtkWidget *
gbp_devhelp_search_provider_create_row (IdeSearchProvider *provider,
                                       IdeSearchResult   *result)
{
  g_assert (IDE_IS_SEARCH_PROVIDER (provider));
  g_assert (IDE_IS_SEARCH_RESULT (result));

  return g_object_new (IDE_TYPE_OMNI_SEARCH_ROW,
                       "icon-name", "devhelp-symbolic",
                       "result", result,
                       "visible", TRUE,
                       NULL);
}

static void
gbp_devhelp_search_provider_activate (IdeSearchProvider *provider,
                                      GtkWidget         *row,
                                      IdeSearchResult   *result)
{
  IdePerspective *editor;
  GbpDevhelpPanel *panel;
  GtkWidget *toplevel;
  GtkWidget *pane;
  gchar *uri;

  g_return_if_fail (GBP_IS_DEVHELP_SEARCH_PROVIDER (provider));
  g_return_if_fail (GTK_IS_WIDGET (row));
  g_return_if_fail (IDE_IS_SEARCH_RESULT (result));

  toplevel = gtk_widget_get_toplevel (row);

  if (!IDE_IS_WORKBENCH (toplevel))
    return;

  editor = ide_workbench_get_perspective_by_name (IDE_WORKBENCH (toplevel), "editor");
  g_assert (editor != NULL);

  pane = pnl_dock_bin_get_right_edge (PNL_DOCK_BIN (editor));
  g_assert (pane != NULL);

  panel = ide_widget_find_child_typed (pane, GBP_TYPE_DEVHELP_PANEL);
  g_assert (panel != NULL);

  g_object_get (result, "uri", &uri, NULL);

  if (panel != NULL)
    {
      gbp_devhelp_panel_set_uri (panel, uri);
      ide_workbench_focus (IDE_WORKBENCH (toplevel), GTK_WIDGET (panel));
    }

  g_free (uri);
}

static gint
gbp_devhelp_search_provider_get_priority (IdeSearchProvider *provider)
{
  return 100;
}

static void
gbp_devhelp_search_provider_finalize (GObject *object)
{
  GbpDevhelpSearchProvider *self = GBP_DEVHELP_SEARCH_PROVIDER (object);

  g_clear_object (&self->book_manager);

  G_OBJECT_CLASS (gbp_devhelp_search_provider_parent_class)->finalize (object);
}

static void
gbp_devhelp_search_provider_class_init (GbpDevhelpSearchProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gbp_devhelp_search_provider_constructed;
  object_class->finalize = gbp_devhelp_search_provider_finalize;
}

static void
gbp_devhelp_search_provider_init (GbpDevhelpSearchProvider *self)
{
  self->book_manager = dh_book_manager_new ();
  self->keywords_model = dh_keyword_model_new ();
}

static void
search_provider_iface_init (IdeSearchProviderInterface *iface)
{
  iface->create_row = gbp_devhelp_search_provider_create_row;
  iface->get_verb = gbp_devhelp_search_provider_get_verb;
  iface->populate = gbp_devhelp_search_provider_populate;
  iface->activate = gbp_devhelp_search_provider_activate;
  iface->get_priority = gbp_devhelp_search_provider_get_priority;
}
