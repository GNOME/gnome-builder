/* gbp-devhelp-docs-provider.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-devhelp-docs-provider"

#include "config.h"

#include <libide-sourceview.h>
#include <libide-threading.h>

#include "devhelp2-parser.h"
#include "gbp-devhelp-docs-provider.h"

struct _GbpDevhelpDocsProvider
{
  GObject parent_instance;

  GPtrArray *parsers;
};

static void
gbp_devhelp_docs_provider_populate_async (IdeDocsProvider     *provider,
                                          IdeDocsItem         *item,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data)
{
  GbpDevhelpDocsProvider *self = (GbpDevhelpDocsProvider *)provider;
  g_autoptr(IdeTask) task = NULL;

  g_assert (GBP_IS_DEVHELP_DOCS_PROVIDER (self));
  g_assert (IDE_IS_DOCS_ITEM (item));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_devhelp_docs_provider_populate_async);
  ide_task_set_task_data (task, g_object_ref (item), g_object_unref);

  if (ide_docs_item_is_root (item))
    {
      g_autoptr(IdeDocsItem) child = NULL;

      child = ide_docs_item_new ();
      ide_docs_item_set_title (child, "Books");
      ide_docs_item_set_kind (child, IDE_DOCS_ITEM_KIND_COLLECTION);

      ide_docs_item_append (item, child);
    }

  ide_task_return_boolean (task, TRUE);
}

static gboolean
gbp_devhelp_docs_provider_populate_finish (IdeDocsProvider  *provider,
                                           GAsyncResult     *result,
                                           GError          **error)
{
  g_assert (GBP_IS_DEVHELP_DOCS_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
gbp_devhelp_docs_provider_search_async (IdeDocsProvider     *provider,
                                        IdeDocsQuery        *query,
                                        IdeDocsItem         *results,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  GbpDevhelpDocsProvider *self = (GbpDevhelpDocsProvider *)provider;
  g_autoptr(GListStore) store = g_list_store_new (IDE_TYPE_DOCS_ITEM);
  g_autoptr(IdeTask) task = NULL;
  IdeDocsItem *api;
  const gchar *text;

  g_assert (GBP_IS_DEVHELP_DOCS_PROVIDER (self));
  g_assert (IDE_IS_DOCS_QUERY (query));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_devhelp_docs_provider_search_async);
  ide_task_set_task_data (task, g_object_ref (query), g_object_unref);

  api = ide_docs_item_find_child_by_id (results, "api");
  g_assert (IDE_IS_DOCS_ITEM (api));

  text = ide_docs_query_get_fuzzy (query);

  if (self->parsers != NULL && !ide_str_empty0 (text))
    {
      g_autofree gchar *needle = g_utf8_casefold (text, -1);

      for (guint i = 0; i < self->parsers->len; i++)
        {
          Devhelp2Parser *parser = g_ptr_array_index (self->parsers, i);
          g_autoptr(IdeDocsItem) group = NULL;
          guint priority = G_MAXINT;

          if (parser->book.title == NULL)
            continue;

          group = ide_docs_item_new ();
          ide_docs_item_set_title (group, parser->book.title);
          ide_docs_item_set_kind (group, IDE_DOCS_ITEM_KIND_BOOK);

          for (guint j = 0; j < parser->keywords->len; j++)
            {
              const Keyword *kw = &g_array_index (parser->keywords, Keyword, j);
              guint prio = G_MAXINT;

              if (ide_completion_fuzzy_match (kw->name, needle, &prio))
                {
                  g_autoptr(IdeDocsItem) child = ide_docs_item_new ();
                  g_autofree gchar *highlight = ide_completion_fuzzy_highlight (kw->name, text);

                  ide_docs_item_set_title (child, kw->name);
                  ide_docs_item_set_display_name (child, highlight);
                  ide_docs_item_set_kind (child, kw->kind);
                  ide_docs_item_set_priority (group, prio);
                  ide_docs_item_append (group, child);

                  priority = MIN (prio, priority);
                }
            }

          ide_docs_item_sort_by_priority (group);
          ide_docs_item_set_priority (group, priority);

          if (ide_docs_item_has_child (group))
            ide_docs_item_append (api, group);
        }
    }

  ide_task_return_boolean (task, TRUE);
}

static gboolean
gbp_devhelp_docs_provider_search_finish (IdeDocsProvider  *provider,
                                         GAsyncResult     *result,
                                         GError          **error)
{
  g_assert (GBP_IS_DEVHELP_DOCS_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
docs_provider_iface_init (IdeDocsProviderInterface *iface)
{
  iface->populate_async = gbp_devhelp_docs_provider_populate_async;
  iface->populate_finish = gbp_devhelp_docs_provider_populate_finish;
  iface->search_async = gbp_devhelp_docs_provider_search_async;
  iface->search_finish = gbp_devhelp_docs_provider_search_finish;
}

static void
gbp_devhelp_docs_provider_init_async (GAsyncInitable      *initable,
                                      gint                 io_priority,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  GbpDevhelpDocsProvider *self = (GbpDevhelpDocsProvider *)initable;
  g_autoptr(IdeTask) task = NULL;
  static const gchar *dirs[] = {
    "/app/share/gtk-doc/html",
    "/usr/share/gtk-doc/html",
  };

  g_assert (GBP_IS_DEVHELP_DOCS_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_devhelp_docs_provider_init_async);

  self->parsers = g_ptr_array_new_with_free_func ((GDestroyNotify)devhelp2_parser_free);

  for (guint i = 0; i < G_N_ELEMENTS (dirs); i++)
    {
      g_autoptr(GDir) dir = g_dir_open (dirs[i], 0, NULL);
      const gchar *name;

      if (dir == NULL)
        continue;

      while ((name = g_dir_read_name (dir)))
        {
          g_autofree gchar *index = g_strdup_printf ("%s.devhelp2", name);
          g_autofree gchar *path = g_build_filename (dirs[i], name, index, NULL);
          Devhelp2Parser *parser;

          if (!g_file_test (path, G_FILE_TEST_EXISTS))
            continue;

          parser = devhelp2_parser_new ();

          if (devhelp2_parser_parse_file (parser, path, NULL))
            g_ptr_array_add (self->parsers, parser);
          else
            devhelp2_parser_free (parser);
        }
    }

  ide_task_return_boolean (task, TRUE);
}

static gboolean
gbp_devhelp_docs_provider_init_finish (GAsyncInitable  *initable,
                                       GAsyncResult    *result,
                                       GError         **error)
{
  g_assert (GBP_IS_DEVHELP_DOCS_PROVIDER (initable));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = gbp_devhelp_docs_provider_init_async;
  iface->init_finish = gbp_devhelp_docs_provider_init_finish;
}

G_DEFINE_TYPE_WITH_CODE (GbpDevhelpDocsProvider, gbp_devhelp_docs_provider, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_iface_init)
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_DOCS_PROVIDER, docs_provider_iface_init))

static void
gbp_devhelp_docs_provider_finalize (GObject *object)
{
  GbpDevhelpDocsProvider *self = (GbpDevhelpDocsProvider *)object;

  g_clear_pointer (&self->parsers, g_ptr_array_unref);

  G_OBJECT_CLASS (gbp_devhelp_docs_provider_parent_class)->finalize (object);
}

static void
gbp_devhelp_docs_provider_class_init (GbpDevhelpDocsProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_devhelp_docs_provider_finalize;
}

static void
gbp_devhelp_docs_provider_init (GbpDevhelpDocsProvider *self)
{
}
