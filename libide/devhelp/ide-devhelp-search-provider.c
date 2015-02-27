/* ide-devhelp-search-provider.c
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

#include "ide-devhelp-search-provider.h"

#include <ctype.h>
#include <glib/gi18n.h>
#include <devhelp/devhelp.h>

#include "ide-search-reducer.h"
#include "ide-search-result.h"
#include "ide-search-context.h"

typedef struct
{
  DhBookManager  *book_manager;
  DhKeywordModel *keywords_model;
} IdeDevhelpSearchProviderPrivate;

struct _IdeDevhelpSearchProvider
{
  IdeSearchProvider                parent;

  /*< private >*/
  IdeDevhelpSearchProviderPrivate *priv;
};

typedef struct
{
  IdeSearchContext *context;
  gchar            *search_terms;
  gsize             max_results;
} PopulateState;

G_DEFINE_TYPE_WITH_PRIVATE (IdeDevhelpSearchProvider,
                            ide_devhelp_search_provider,
                            IDE_TYPE_SEARCH_PROVIDER)

static GQuark      gQuarkLink;

static void
populate_get_matches_cb (GObject      *source_object,
                         GAsyncResult *res,
                         gpointer      user_data)
{
  IdeDevhelpSearchProviderPrivate *priv;
  IdeContext* context;
  PopulateState *state;
  g_auto(IdeSearchReducer) reducer = { 0 };
  GtkTreeIter iter;
  gboolean valid;
  gint count = 0;
  gint total;

  priv = IDE_DEVHELP_SEARCH_PROVIDER (source_object)->priv;
  state = (PopulateState*) user_data;
  context = ide_object_get_context (IDE_OBJECT (state->context));

  dh_keyword_model_filter (priv->keywords_model, state->search_terms, NULL, NULL);

  total = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (priv->keywords_model),
                                          NULL);
  if (state->max_results != 0)
    total = MIN (total, state->max_results);

  /* initialize our reducer, which helps us prevent creating unnecessary
   * objects that will simply be discarded */
  ide_search_reducer_init (&reducer,
                           state->context,
                           IDE_SEARCH_PROVIDER (source_object),
                           state->max_results);

  valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->keywords_model),
                                         &iter);
  while (valid)
    {
      g_autoptr(IdeSearchResult) result = NULL;
      DhLink *link = NULL;
      g_autofree gchar *name = NULL;
      gfloat score = .0;

      gtk_tree_model_get (GTK_TREE_MODEL (priv->keywords_model), &iter,
                          DH_KEYWORD_MODEL_COL_NAME, &name,
                          DH_KEYWORD_MODEL_COL_LINK, &link,
                          -1);

      valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->keywords_model),
                                        &iter);

      count++;
      score = (total - count) / (gfloat) (total + 1);
      if (!ide_search_reducer_accepts (&reducer, score))
        continue;

      if (!g_str_is_ascii (name))
        {
          gchar *ascii_name = g_str_to_ascii (name, NULL);
          g_free (name);
          name = ascii_name;
        }

      if ((dh_link_get_flags (link) & DH_LINK_FLAGS_DEPRECATED) != 0)
        {
          gchar *italic_name = g_strdup_printf ("<i>%s</i>", name);
          g_free (name);
          name = italic_name;
        }

      result = ide_search_result_new (context,
                                      name,
                                      dh_link_get_book_name (link),
                                      score);
      g_object_set_qdata_full (G_OBJECT (result),
                               gQuarkLink,
                               dh_link_get_uri (link),
                               g_free);

      /* push the result through the search reducer */
      ide_search_reducer_push (&reducer, result);
    }

  ide_search_context_provider_completed (state->context,
                                         IDE_SEARCH_PROVIDER (source_object));

  g_free (state->search_terms);
  g_object_unref (state->context);
  g_slice_free (PopulateState, state);
}

void
ide_devhelp_search_provider_populate (IdeSearchProvider *provider,
                                      IdeSearchContext  *context,
                                      const gchar       *search_terms,
                                      gsize              max_results,
                                      GCancellable      *cancellable)
{
  PopulateState *state;
  g_autoptr(GTask) task = NULL;

  state = g_new0 (PopulateState, 1);
  state->context = g_object_ref (context);
  state->search_terms = g_strdup (search_terms);
  state->max_results = max_results;

  task = g_task_new (provider, cancellable, populate_get_matches_cb, state);
  g_task_return_pointer (task, NULL, NULL);
}

static void
ide_devhelp_search_provider_constructed (GObject *object)
{
  IdeDevhelpSearchProvider *self = IDE_DEVHELP_SEARCH_PROVIDER (object);

  dh_book_manager_populate (self->priv->book_manager);
  dh_keyword_model_set_words (self->priv->keywords_model, self->priv->book_manager);
}

static void
ide_devhelp_search_provider_finalize (GObject *object)
{
  IdeDevhelpSearchProvider *self = IDE_DEVHELP_SEARCH_PROVIDER (object);

  g_clear_object (&self->priv->book_manager);

  G_OBJECT_CLASS (ide_devhelp_search_provider_parent_class)->finalize (object);
}

static void
ide_devhelp_search_provider_class_init (IdeDevhelpSearchProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeSearchProviderClass *provider_class = IDE_SEARCH_PROVIDER_CLASS (klass);

  object_class->constructed = ide_devhelp_search_provider_constructed;
  object_class->finalize = ide_devhelp_search_provider_finalize;

  provider_class->populate = ide_devhelp_search_provider_populate;

  gQuarkLink = g_quark_from_static_string ("LINK");
}

static void
ide_devhelp_search_provider_init (IdeDevhelpSearchProvider *self)
{
  self->priv = ide_devhelp_search_provider_get_instance_private (self);
  self->priv->book_manager = dh_book_manager_new ();
  self->priv->keywords_model = dh_keyword_model_new ();
}
