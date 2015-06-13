/* gb-file-search-provider.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include <glib/gi18n.h>

#include "gb-file-search-provider.h"
#include "gb-file-search-index.h"
#include "gb-search-display-row.h"
#include "gb-workbench.h"

struct _GbFileSearchProvider
{
  IdeObject          parent_instance;
  GbFileSearchIndex *index;
};

static void search_provider_iface_init (IdeSearchProviderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GbFileSearchProvider, gb_file_search_provider, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_SEARCH_PROVIDER,
                                                search_provider_iface_init))

static const gchar *
gb_file_search_provider_get_verb (IdeSearchProvider *provider)
{
  return _("Switch To");
}

static void
gb_file_search_provider_populate (IdeSearchProvider *provider,
                                  IdeSearchContext  *context,
                                  const gchar       *search_terms,
                                  gsize              max_results,
                                  GCancellable      *cancellable)
{
  GbFileSearchProvider *self = (GbFileSearchProvider *)provider;

  g_assert (IDE_IS_SEARCH_PROVIDER (provider));
  g_assert (IDE_IS_SEARCH_CONTEXT (context));
  g_assert (search_terms != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (self->index != NULL)
    gb_file_search_index_populate (self->index, context, provider, search_terms);

  ide_search_context_provider_completed (context, provider);
}

static void
gb_file_search_provider_build_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  GbFileSearchIndex *index = (GbFileSearchIndex *)object;
  g_autoptr(GbFileSearchProvider) self = user_data;
  GError *error = NULL;

  g_assert (GB_IS_FILE_SEARCH_INDEX (index));
  g_assert (GB_IS_FILE_SEARCH_PROVIDER (self));

  if (!gb_file_search_index_build_finish (index, result, &error))
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
    }
}

static GtkWidget *
gb_file_search_provider_create_row (IdeSearchProvider *provider,
                                    IdeSearchResult   *result)
{
  g_assert (IDE_IS_SEARCH_PROVIDER (provider));
  g_assert (IDE_IS_SEARCH_RESULT (result));

  return g_object_new (GB_TYPE_SEARCH_DISPLAY_ROW,
                       "result", result,
                       "visible", TRUE,
                       NULL);
}

static void
gb_file_search_provider_activate (IdeSearchProvider *provider,
                                  GtkWidget         *row,
                                  IdeSearchResult   *result)
{
  GtkWidget *toplevel;

  g_assert (IDE_IS_SEARCH_PROVIDER (provider));
  g_assert (GTK_IS_WIDGET (row));
  g_assert (IDE_IS_SEARCH_RESULT (result));

  toplevel = gtk_widget_get_toplevel (row);

  if (GB_IS_WORKBENCH (toplevel))
    {
      g_autofree gchar *path = NULL;
      g_autoptr(GFile) file = NULL;
      IdeContext *context;
      IdeVcs *vcs;
      GFile *workdir;

      context = gb_workbench_get_context (GB_WORKBENCH (toplevel));
      vcs = ide_context_get_vcs (context);
      workdir = ide_vcs_get_working_directory (vcs);
      g_object_get (result, "path", &path, NULL);
      file = g_file_get_child (workdir, path);

      gb_workbench_open (GB_WORKBENCH (toplevel), file);
    }
}

static gint
gb_file_search_provider_get_priority (IdeSearchProvider *provider)
{
  return 0;
}

static void
gb_file_search_provider_constructed (GObject *object)
{
  GbFileSearchProvider *self = (GbFileSearchProvider *)object;
  IdeContext *context;
  IdeVcs *vcs;
  GFile *workdir;

  context = ide_object_get_context (IDE_OBJECT (self));
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);

  self->index = g_object_new (GB_TYPE_FILE_SEARCH_INDEX,
                              "context", context,
                              "root-directory", workdir,
                              NULL);

  gb_file_search_index_build_async (self->index,
                                    NULL,
                                    gb_file_search_provider_build_cb,
                                    g_object_ref (self));

  G_OBJECT_CLASS (gb_file_search_provider_parent_class)->constructed (object);
}

static void
gb_file_search_provider_finalize (GObject *object)
{
  GbFileSearchProvider *self = (GbFileSearchProvider *)object;

  g_clear_object (&self->index);

  G_OBJECT_CLASS (gb_file_search_provider_parent_class)->finalize (object);
}

static void
gb_file_search_provider_class_init (GbFileSearchProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gb_file_search_provider_constructed;
  object_class->finalize = gb_file_search_provider_finalize;
}

static void
gb_file_search_provider_init (GbFileSearchProvider *self)
{
}

static void
search_provider_iface_init (IdeSearchProviderInterface *iface)
{
  iface->populate = gb_file_search_provider_populate;
  iface->get_verb = gb_file_search_provider_get_verb;
  iface->create_row = gb_file_search_provider_create_row;
  iface->activate = gb_file_search_provider_activate;
  iface->get_priority = gb_file_search_provider_get_priority;
}
