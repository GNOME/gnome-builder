/* ide-docs-search-model.c
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

#define G_LOG_DOMAIN "ide-docs-search-model"

#include "config.h"

#include "ide-docs-search-model.h"

#define DEFAULT_MAX_CHILDREN 3

struct _IdeDocsSearchModel
{
  GObject  parent_instance;
  GArray  *groups;
};

typedef struct
{
  IdeDocsItem *group;
  guint        expanded : 1;
} Group;

static GType
ide_docs_search_model_get_item_type (GListModel *model)
{
  return IDE_TYPE_DOCS_ITEM;
}

static guint
ide_docs_search_model_get_n_items (GListModel *model)
{
  IdeDocsSearchModel *self = (IdeDocsSearchModel *)model;
  guint n_items = 0;

  g_assert (IDE_IS_DOCS_SEARCH_MODEL (self));

  for (guint i = 0; i < self->groups->len; i++)
    {
      const Group *g = &g_array_index (self->groups, Group, i);
      guint n_children = ide_docs_item_get_n_children (g->group);

      /* Add the group title */
      n_items++;

      /* Add the items (depending on expanded state) */
      if (g->expanded)
        n_items += n_children;
      else
        n_items += MIN (n_children, DEFAULT_MAX_CHILDREN);
    }

  return n_items;
}

static gpointer
ide_docs_search_model_get_item (GListModel *model,
                                guint       position)
{
  IdeDocsSearchModel *self = (IdeDocsSearchModel *)model;

  g_assert (IDE_IS_DOCS_SEARCH_MODEL (self));
  g_assert (position < ide_docs_search_model_get_n_items (model));

  for (guint i = 0; i < self->groups->len; i++)
    {
      const Group *g = &g_array_index (self->groups, Group, i);
      guint n_children = ide_docs_item_get_n_children (g->group);

      if (position == 0)
        return g_object_ref (g->group);

      position--;

      if (!g->expanded)
        n_children = MIN (n_children, DEFAULT_MAX_CHILDREN);

      if (position >= n_children)
        {
          position -= n_children;
          continue;
        }

      return g_object_ref (ide_docs_item_get_nth_child (g->group, position));
    }

  g_return_val_if_reached (NULL);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ide_docs_search_model_get_item_type;
  iface->get_n_items = ide_docs_search_model_get_n_items;
  iface->get_item = ide_docs_search_model_get_item;
}

G_DEFINE_TYPE_WITH_CODE (IdeDocsSearchModel, ide_docs_search_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static void
clear_group_cb (gpointer data)
{
  Group *g = data;
  g_clear_object (&g->group);
}

static void
ide_docs_search_model_finalize (GObject *object)
{
  IdeDocsSearchModel *self = (IdeDocsSearchModel *)object;

  g_clear_pointer (&self->groups, g_array_unref);

  G_OBJECT_CLASS (ide_docs_search_model_parent_class)->finalize (object);
}

static void
ide_docs_search_model_class_init (IdeDocsSearchModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_docs_search_model_finalize;
}

static void
ide_docs_search_model_init (IdeDocsSearchModel *self)
{
  self->groups = g_array_new (FALSE, FALSE, sizeof (Group));
  g_array_set_clear_func (self->groups, clear_group_cb);
}

IdeDocsSearchModel *
ide_docs_search_model_new (void)
{
  return g_object_new (IDE_TYPE_DOCS_SEARCH_MODEL, NULL);
}

void
ide_docs_search_model_add_group (IdeDocsSearchModel *self,
                                 IdeDocsItem        *group)
{
  Group to_add = {0};
  guint position = 0;
  guint added;
  gint priority;

  g_return_if_fail (IDE_IS_DOCS_SEARCH_MODEL (self));
  g_return_if_fail (IDE_IS_DOCS_ITEM (group));

  if (ide_docs_item_get_n_children (group) == 0)
    return;

  to_add.group = g_object_ref (group);
  to_add.expanded = FALSE;

  priority = ide_docs_item_get_priority (group);
  added = ide_docs_item_get_n_children (group);
  if (added > DEFAULT_MAX_CHILDREN)
    added = DEFAULT_MAX_CHILDREN;

  /* Add the group header */
  added++;

  for (guint i = 0; i < self->groups->len; i++)
    {
      const Group *g = &g_array_index (self->groups, Group, i);
      guint n_children;

      if (ide_docs_item_get_priority (g->group) > priority)
        {
          g_array_insert_val (self->groups, i, to_add);
          g_list_model_items_changed (G_LIST_MODEL (self), position, 0, added);
          return;
        }

      /* Skip the group header */
      position++;

      n_children = ide_docs_item_get_n_children (g->group);

      if (g->expanded)
        position += n_children;
      else
        position += MIN (n_children, DEFAULT_MAX_CHILDREN);
    }

  g_assert (position == ide_docs_search_model_get_n_items (G_LIST_MODEL (self)));

  g_array_append_val (self->groups, to_add);
  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, added);
}

static void
ide_docs_search_model_toggle (IdeDocsSearchModel *self,
                              IdeDocsItem        *group,
                              gboolean            expanded)
{
  guint position = 0;

  g_return_if_fail (IDE_IS_DOCS_SEARCH_MODEL (self));
  g_return_if_fail (IDE_IS_DOCS_ITEM (group));

  for (guint i = 0; i < self->groups->len; i++)
    {
      Group *g = &g_array_index (self->groups, Group, i);
      guint n_children = ide_docs_item_get_n_children (g->group);
      guint removed = 0;
      guint added = 0;

      /* Skip the group header */
      position++;

      if (g->group != group)
        {
          if (g->expanded)
            position += n_children;
          else
            position += MIN (DEFAULT_MAX_CHILDREN, n_children);

          continue;
        }

      if (g->expanded == expanded)
        return;

      g->expanded = !g->expanded;

      if (g->expanded)
        {
          /* expanding */
          removed = MIN (DEFAULT_MAX_CHILDREN, n_children);
          added = n_children;
        }
      else
        {
          /* collapsing */
          removed = n_children;
          added = MIN (DEFAULT_MAX_CHILDREN, n_children);
        }

      g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
      break;
    }
}

void
ide_docs_search_model_collapse_group (IdeDocsSearchModel *self,
                                      IdeDocsItem        *group)
{
  g_return_if_fail (IDE_IS_DOCS_SEARCH_MODEL (self));
  g_return_if_fail (IDE_IS_DOCS_ITEM (group));

  ide_docs_search_model_toggle (self, group, FALSE);
}

void
ide_docs_search_model_expand_group (IdeDocsSearchModel *self,
                                    IdeDocsItem        *group)
{
  g_return_if_fail (IDE_IS_DOCS_SEARCH_MODEL (self));
  g_return_if_fail (IDE_IS_DOCS_ITEM (group));

  ide_docs_search_model_toggle (self, group, TRUE);
}
