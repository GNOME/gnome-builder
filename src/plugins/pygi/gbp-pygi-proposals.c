/* gbp-pygi-proposals.c
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

#define G_LOG_DOMAIN "gbp-pygi-proposals"

#include "config.h"

#include <girepository/girepository.h>

#include <libide-core.h>

#include "gbp-pygi-proposal.h"
#include "gbp-pygi-proposals.h"

struct _GbpPygiProposals
{
  GObject parent_instance;
  GPtrArray *items;
};

static int
sort_by_name (gconstpointer a,
              gconstpointer b)
{
  const char * const *astr = a;
  const char * const *bstr = b;

  return g_strcmp0 (*astr, *bstr);
}

static GPtrArray *
get_libraries (void)
{
  static GPtrArray *items;
  static gint64 expire_at;
  gint64 now;

  now = g_get_monotonic_time ();
  if (now > expire_at)
    g_clear_pointer (&items, g_ptr_array_unref);

  if (items == NULL)
    {
      g_autoptr(GHashTable) found = g_hash_table_new (NULL, NULL);
      const char * const *search_path = gi_repository_get_search_path (ide_get_gir_repository (), NULL);

      items = g_ptr_array_new ();

      for (guint i = 0; search_path[i]; i++)
        {
          const char *path = search_path[i];
          const char *name;
          GDir *dir;

          if (!g_file_test (path, G_FILE_TEST_IS_DIR))
            continue;

          if (!(dir = g_dir_open (path, 0, NULL)))
            continue;

          while ((name = g_dir_read_name (dir)))
            {
              const char *dash = strchr (name, '-');
              g_autofree char *ns = NULL;
              const char *intern;

              if (dash == NULL || !g_str_has_suffix (name, ".typelib"))
                continue;

              ns = g_strndup (name, dash - name);
              intern = g_intern_string (ns);
              if (g_hash_table_contains (found, (char *)intern))
                continue;

              g_hash_table_add (found, (char *)intern);
              g_ptr_array_add (items, (char *)intern);
            }

          g_dir_close (dir);
        }

      g_ptr_array_sort (items, sort_by_name);
      expire_at = now + (G_USEC_PER_SEC * 5);
    }

  return items;
}

static guint
gbp_pygi_proposals_get_n_items (GListModel *model)
{
  GbpPygiProposals *self = (GbpPygiProposals *)model;
  return self->items ? self->items->len : 0;
}

static GType
gbp_pygi_proposals_get_item_type (GListModel *model)
{
  return GBP_TYPE_PYGI_PROPOSAL;
}

static gpointer
gbp_pygi_proposals_get_item (GListModel *model,
                             guint       position)
{
  GbpPygiProposals *self = (GbpPygiProposals *)model;

  if (self->items == NULL)
    return NULL;

  if (position >= self->items->len)
    return NULL;

  return gbp_pygi_proposal_new (g_ptr_array_index (self->items, position));
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_n_items = gbp_pygi_proposals_get_n_items;
  iface->get_item_type = gbp_pygi_proposals_get_item_type;
  iface->get_item = gbp_pygi_proposals_get_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpPygiProposals, gbp_pygi_proposals, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static void
gbp_pygi_proposals_dispose (GObject *object)
{
  GbpPygiProposals *self = (GbpPygiProposals *)object;

  g_clear_pointer (&self->items, g_ptr_array_unref);

  G_OBJECT_CLASS (gbp_pygi_proposals_parent_class)->dispose (object);
}

static void
gbp_pygi_proposals_class_init (GbpPygiProposalsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_pygi_proposals_dispose;
}

static void
gbp_pygi_proposals_init (GbpPygiProposals *self)
{
}

GbpPygiProposals *
gbp_pygi_proposals_new (void)
{
  return g_object_new (GBP_TYPE_PYGI_PROPOSALS, NULL);
}

void
gbp_pygi_proposals_filter (GbpPygiProposals *self,
                           const char       *word)
{
  guint old_len;
  GPtrArray *all;

  g_return_if_fail (GBP_IS_PYGI_PROPOSALS (self));

  old_len = g_list_model_get_n_items (G_LIST_MODEL (self));
  g_clear_pointer (&self->items, g_ptr_array_unref);

  self->items = g_ptr_array_new ();
  all = get_libraries ();

  for (guint i = 0; i < all->len; i++)
    {
      const char *item = g_ptr_array_index (all, i);

      if (g_str_has_prefix (item, word))
        g_ptr_array_add (self->items, (char *)item);
    }

  if (old_len != self->items->len)
    g_list_model_items_changed (G_LIST_MODEL (self), 0, old_len, self->items->len);
}
