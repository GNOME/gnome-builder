/* ide-shortcut-bundle.c
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

#define G_LOG_DOMAIN "ide-shortcut-bundle"

#include "config.h"

#include <json-glib/json-glib.h>

#include "ide-shortcut-bundle-private.h"
#include "ide-shortcut-private.h"

struct _IdeShortcutBundle
{
  GObject    parent_instance;
  GPtrArray *items;
};

typedef struct
{
  const char *key;
  const char *when;
  const char *command;
  const char *action;
} Shortcut;

static guint
ide_shortcut_bundle_get_n_items (GListModel *model)
{
  return IDE_SHORTCUT_BUNDLE (model)->items->len;
}

static gpointer
ide_shortcut_bundle_get_item (GListModel *model,
                              guint       position)
{
  IdeShortcutBundle *self = IDE_SHORTCUT_BUNDLE (model);

  if (position >= self->items->len)
    return NULL;

  return NULL;
}

static GType
ide_shortcut_bundle_get_item_type (GListModel *model)
{
  return IDE_TYPE_SHORTCUT;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_n_items = ide_shortcut_bundle_get_n_items;
  iface->get_item = ide_shortcut_bundle_get_item;
  iface->get_item_type = ide_shortcut_bundle_get_item_type;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeShortcutBundle, ide_shortcut_bundle, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static void
shortcut_free (gpointer data)
{
  g_slice_free (Shortcut, data);
}

static void
ide_shortcut_bundle_dispose (GObject *object)
{
  G_OBJECT_CLASS (ide_shortcut_bundle_parent_class)->dispose (object);
}

static void
ide_shortcut_bundle_class_init (IdeShortcutBundleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_shortcut_bundle_dispose;
}

static void
ide_shortcut_bundle_init (IdeShortcutBundle *self)
{
  self->items = g_ptr_array_new_with_free_func (shortcut_free);
}

IdeShortcutBundle *
ide_shortcut_bundle_new (void)
{
  return g_object_new (IDE_TYPE_SHORTCUT_BUNDLE, NULL);
}

static gboolean
get_string_member (JsonObject  *obj,
                   const char  *name,
                   const char **value,
                   GError     **error)
{
  JsonNode *node;
  const char *str;

  *value = NULL;

  if (!json_object_has_member (obj, name))
    return TRUE;

  node = json_object_get_member (obj, name);

  if (!JSON_NODE_HOLDS_VALUE (node))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "Key \"%s\" contains something other than a string",
                   name);
      return FALSE;
    }

  str = json_node_get_string (node);

  if (str != NULL && strlen (str) > 1024)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "Implausible string found, bailing. Length %"G_GSIZE_FORMAT,
                   strlen (str));
      return FALSE;
    }

  *value = g_intern_string (str);

  return TRUE;
}

static gboolean
populate_from_object (IdeShortcutBundle  *self,
                      JsonNode           *node,
                      GError            **error)
{
  JsonObject *obj;
  Shortcut shortcut = {0};

  g_assert (IDE_IS_SHORTCUT_BUNDLE (self));
  g_assert (node != NULL);
  g_assert (JSON_NODE_HOLDS_OBJECT (node));

  obj = json_node_get_object (node);

  if (!get_string_member (obj, "key", &shortcut.key, error) ||
      !get_string_member (obj, "when", &shortcut.when, error) ||
      !get_string_member (obj, "command", &shortcut.command, error) ||
      !get_string_member (obj, "action", &shortcut.action, error))
    return FALSE;

  g_ptr_array_add (self->items, g_slice_dup (Shortcut, &shortcut));

  return TRUE;
}

static gboolean
populate_from_array (IdeShortcutBundle  *self,
                     JsonNode           *node,
                     GError            **error)
{
  JsonArray *ar;
  guint n_items;

  g_assert (IDE_IS_SHORTCUT_BUNDLE (self));
  g_assert (node != NULL);
  g_assert (JSON_NODE_HOLDS_ARRAY (node));

  ar = json_node_get_array (node);
  n_items = json_array_get_length (ar);

  for (guint i = 0; i < n_items; i++)
    {
      JsonNode *element = json_array_get_element (ar, i);

      if (JSON_NODE_HOLDS_ARRAY (element))
        {
          if (!populate_from_array (self, element, error))
            return FALSE;
        }
      else if (JSON_NODE_HOLDS_OBJECT (element))
        {
          if (!populate_from_object (self, element, error))
            return FALSE;
        }
      else
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_DATA,
                       "Somthing other than an object found within array");
          return FALSE;
        }
    }

  return TRUE;
}

gboolean
ide_shortcut_bundle_parse (IdeShortcutBundle  *self,
                           GFile              *file,
                           GError            **error)
{
  g_autoptr(JsonParser) parser = NULL;
  g_autofree char *data = NULL;
  JsonNode *root;
  gsize len = 0;

  g_return_val_if_fail (IDE_IS_SHORTCUT_BUNDLE (self), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  if (!g_file_load_contents (file, NULL, &data, &len, NULL, error))
    return FALSE;

  /* TODO: We sort of want to look like keybindings.json style, which could
   * mean some munging for trailing , and missing [].
   */

  parser = json_parser_new ();

  if (!json_parser_load_from_data (parser, data, len, error))
    return FALSE;

  /* Nothing to do if the contents are empty */
  if (!(root = json_parser_get_root (parser)))
    return TRUE;

  /* In case we get arrays containing arrays, try to handle them gracefully
   * and unscrew this terribly defined file format by VSCode.
   */
  if (JSON_NODE_HOLDS_ARRAY (root))
    return populate_from_array (self, root, error);
  else if (JSON_NODE_HOLDS_OBJECT (root))
    return populate_from_object (self, root, error);

  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_INVALID_DATA,
               "Got something other than an array or object");

  return FALSE;
}
