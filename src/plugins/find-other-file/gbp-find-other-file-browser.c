/* gbp-find-other-file-browser.c
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

#define G_LOG_DOMAIN "gbp-find-other-file-browser"

#include "config.h"

#include "gbp-find-other-file-browser.h"
#include "gbp-found-file.h"

struct _GbpFindOtherFileBrowser
{
  GObject    parent_instance;
  GPtrArray *items;
  GFile     *file;
  GFile     *root;
};

enum {
  PROP_0,
  PROP_ROOT,
  PROP_FILE,
  N_PROPS
};

static GType
gbp_find_other_file_browser_get_item_type (GListModel *model)
{
  return G_TYPE_FILE;
}

static guint
gbp_find_other_file_browser_get_n_items (GListModel *model)
{
  return GBP_FIND_OTHER_FILE_BROWSER (model)->items->len;
}

static gpointer
gbp_find_other_file_browser_get_item (GListModel *model,
                                      guint       position)
{
  GbpFindOtherFileBrowser *self = GBP_FIND_OTHER_FILE_BROWSER (model);

  if (position < self->items->len)
    return g_object_ref (g_ptr_array_index (self->items, position));

  return NULL;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = gbp_find_other_file_browser_get_item_type;
  iface->get_n_items = gbp_find_other_file_browser_get_n_items;
  iface->get_item = gbp_find_other_file_browser_get_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpFindOtherFileBrowser, gbp_find_other_file_browser, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static GParamSpec *properties [N_PROPS];

static void
gbp_find_other_file_browser_reload (GbpFindOtherFileBrowser *self)
{
  guint old_len = 0;
  guint new_len = 0;

  g_assert (GBP_IS_FIND_OTHER_FILE_BROWSER (self));

  old_len = self->items->len;

  if (old_len)
    g_ptr_array_remove_range (self->items, 0, old_len);

  if (self->root != NULL &&
      self->file != NULL &&
      g_file_has_prefix (self->file, self->root))
    {
      GFile *parent = g_file_get_parent (self->file);

      while (parent != NULL &&
             (g_file_has_prefix (parent, self->root) ||
              g_file_equal (parent, self->root)))
        {
          g_ptr_array_add (self->items, parent);
          parent = g_file_get_parent (parent);
        }

      g_clear_object (&parent);
    }

  new_len = self->items->len;

  if (old_len || new_len)
    g_list_model_items_changed (G_LIST_MODEL (self), 0, old_len, new_len);
}

static void
gbp_find_other_file_browser_dispose (GObject *object)
{
  GbpFindOtherFileBrowser *self = (GbpFindOtherFileBrowser *)object;

  g_clear_object (&self->file);
  g_clear_object (&self->root);
  g_clear_pointer (&self->items, g_ptr_array_unref);

  G_OBJECT_CLASS (gbp_find_other_file_browser_parent_class)->dispose (object);
}

static void
gbp_find_other_file_browser_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  GbpFindOtherFileBrowser *self = GBP_FIND_OTHER_FILE_BROWSER (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, self->file);
      break;

    case PROP_ROOT:
      g_value_set_object (value, self->root);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_find_other_file_browser_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  GbpFindOtherFileBrowser *self = GBP_FIND_OTHER_FILE_BROWSER (object);

  switch (prop_id)
    {
    case PROP_FILE:
      gbp_find_other_file_browser_set_file (self, g_value_get_object (value));
      break;

    case PROP_ROOT:
      gbp_find_other_file_browser_set_root (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_find_other_file_browser_class_init (GbpFindOtherFileBrowserClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_find_other_file_browser_dispose;
  object_class->get_property = gbp_find_other_file_browser_get_property;
  object_class->set_property = gbp_find_other_file_browser_set_property;

  properties [PROP_FILE] =
    g_param_spec_object ("file", NULL, NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_ROOT] =
    g_param_spec_object ("root", NULL, NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_find_other_file_browser_init (GbpFindOtherFileBrowser *self)
{
  self->items = g_ptr_array_new_with_free_func (g_object_unref);
}

void
gbp_find_other_file_browser_set_file (GbpFindOtherFileBrowser *self,
                                      GFile                   *file)
{
  g_return_if_fail (GBP_IS_FIND_OTHER_FILE_BROWSER (self));
  g_return_if_fail (!file || G_IS_FILE (file));

  if (g_set_object (&self->file, file))
    {
      gbp_find_other_file_browser_reload (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FILE]);
    }
}

void
gbp_find_other_file_browser_set_root (GbpFindOtherFileBrowser *self,
                                      GFile                   *root)
{
  g_return_if_fail (GBP_IS_FIND_OTHER_FILE_BROWSER (self));
  g_return_if_fail (!root || G_IS_FILE (root));

  if (g_set_object (&self->root, root))
    {
      gbp_find_other_file_browser_reload (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ROOT]);
    }
}

GbpFindOtherFileBrowser *
gbp_find_other_file_browser_new (void)
{
  return g_object_new (GBP_TYPE_FIND_OTHER_FILE_BROWSER, NULL);
}
