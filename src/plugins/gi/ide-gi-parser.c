/* ide-gi-parser.c
 *
 * Copyright (C) 2017 Sebastien Lafargue <slafargue@gnome.org>
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-gi-parser"

#include <ide.h>

#include "ide-gi-blob.h"

#include "ide-gi-parser-object.h"
#include "ide-gi-parser-result.h"

#include "radix-tree/ide-gi-radix-tree-builder.h"

#include "ide-gi-parser.h"

struct _IdeGiParser
{
  GObject    parent_instance;

  IdeGiPool *pool;
};

G_DEFINE_TYPE (IdeGiParser, ide_gi_parser, G_TYPE_OBJECT)

static GHashTable *element_table;
static gboolean    global_init_done;

/* Keep in sync with ide-gi-parser.h IdeGiElementType */
static const gchar *element_names[] = {
  "unknow",
  "alias",
  "annotation",
  "array",
  "attributes",
  "bitfield",
  "callback",
  "c:include",
  "class",
  "constant",
  "constructor",
  "doc",
  "doc-deprecated",
  "doc-stability",
  "doc-version",
  "enumeration",
  "field",
  "function",
  "glib:boxed",
  "glib:signal",
  "implements",
  "include",
  "instance-parameter",
  "interface",
  "member",
  "method",
  "namespace",
  "package",
  "parameter",
  "parameters",
  "prerequisite",
  "property",
  "record",
  "repository",
  "return-value",
  "type",
  "union",
  "varargs",
  "virtual-method",
  NULL
};

/* Allow a fast conversion between an element name and its IDE_GI_ELEMENT_TYPE_* value */
gboolean
ide_gi_parser_global_init (void)
{
  guint64 bit = 1;

  g_atomic_int_set (&global_init_done, TRUE);

  if (element_table == NULL)
    {
      element_table = g_hash_table_new (g_str_hash, g_str_equal);

      g_hash_table_insert (element_table, (gchar *)element_names[0], 0);
      for (guint i = 1; element_names[i]; i++, bit <<= 1)
        g_hash_table_insert (element_table, (gchar *)element_names[i], GUINT_TO_POINTER (bit));

      return TRUE;
    }

  return FALSE;
}

gboolean
ide_gi_parser_global_cleanup (void)
{
  g_atomic_int_set (&global_init_done, FALSE);

  if (element_table != NULL)
    {
      g_hash_table_unref (element_table);
      element_table = NULL;

      return TRUE;
    }

  return FALSE;
}

IdeGiElementType
ide_gi_parser_get_element_type (const gchar *element_name)
{
  g_return_val_if_fail (element_name != NULL, IDE_GI_ELEMENT_TYPE_UNKNOW);

  return (IdeGiElementType)g_hash_table_lookup (element_table, element_name);
}

/* We use the position of the set bit in the type
 * to get back the element name.
 */
const gchar *
ide_gi_parser_get_element_type_string (IdeGiElementType type)
{
  gint index;

  g_return_val_if_fail (__builtin_popcountll (type) == 1, NULL);

  index = __builtin_ffsll (type);
  return element_names[index];
}

void
ide_gi_parser_set_pool (IdeGiParser *self,
                        IdeGiPool   *pool)
{
  g_return_if_fail (IDE_IS_GI_PARSER (self));
  g_return_if_fail (IDE_IS_GI_POOL (pool));

  if (self->pool != pool)
    {
      g_clear_object (&self->pool);
      self->pool = g_object_ref (pool);
    }
}

IdeGiPool *
ide_gi_parser_get_pool (IdeGiParser *self)
{
  g_return_val_if_fail (IDE_IS_GI_PARSER (self), NULL);

  return self->pool;
}

static void
ide_gi_parser_start_element (GMarkupParseContext  *context,
                             const gchar          *element_name,
                             const gchar         **attribute_names,
                             const gchar         **attribute_values,
                             gpointer              user_data,
                             GError              **error)
{
  IdeGiParserResult *result = user_data;
  IdeGiPool *pool;
  IdeGiParserObject *child;

  g_assert (context != NULL);
  g_assert (element_name != NULL);
  g_assert (attribute_names != NULL);
  g_assert (attribute_values != NULL);
  g_assert (result != NULL);

  if (g_str_equal (element_name, "repository"))
    {
      pool = ide_gi_parser_result_get_pool (result);
      child = ide_gi_pool_get_object (pool, IDE_GI_ELEMENT_TYPE_REPOSITORY);
      ide_gi_parser_object_parse (IDE_GI_PARSER_OBJECT (child),
                                  context,
                                  result,
                                  element_name,
                                  attribute_names,
                                  attribute_values,
                                  error);
    }
}

static void
ide_gi_parser_end_element (GMarkupParseContext  *context,
                           const gchar          *element_name,
                           gpointer              user_data,
                           GError              **error)
{
  IdeGiParserResult *result = user_data;
  IdeGiPool *pool;
  IdeGiParserObject *child;
  IdeGiHeaderBlob *blob;

  g_assert (context != NULL);
  g_assert (element_name != NULL);

  if (g_str_equal (element_name, "repository"))
    {
      pool = ide_gi_parser_result_get_pool (result);
      child = ide_gi_pool_get_current_parser_object (pool);

      blob = ide_gi_parser_object_finish (child);
      ide_gi_parser_object_index (IDE_GI_PARSER_OBJECT (child), result, GINT_TO_POINTER (0));
      ide_gi_parser_result_set_header (result, blob);

      ide_gi_pool_release_object (pool);
      g_markup_parse_context_pop (context);
    }
}

static const GMarkupParser markup_parser = {
  ide_gi_parser_start_element,
  ide_gi_parser_end_element,
  NULL,
  NULL,
  NULL,
};

IdeGiParserResult *
ide_gi_parser_parse_file (IdeGiParser   *self,
                          GFile         *file,
                          GCancellable  *cancellable,
                          GError       **error)
{
  g_autoptr(GMarkupParseContext) context = NULL;
  g_autoptr(IdeGiParserResult) result = NULL;
  g_autofree gchar *content = NULL;
  gsize content_len = 0;

  g_return_val_if_fail (IDE_IS_GI_PARSER (self), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);

  if (!g_file_load_contents (file, cancellable, &content, &content_len, NULL, error))
    return NULL;

  result = ide_gi_parser_result_new (file);
  ide_gi_parser_result_set_parser (result, self);
  ide_gi_parser_result_set_pool (result, self->pool);

  context = g_markup_parse_context_new (&markup_parser, G_MARKUP_PREFIX_ERROR_POSITION, result, NULL);

  if (!g_markup_parse_context_parse (context, content, content_len, error))
    return NULL;

  if (!g_markup_parse_context_end_parse (context, error))
    return NULL;

  return g_steal_pointer (&result);
}

static void
ide_gi_parser_finalize (GObject *object)
{
  IdeGiParser *self = (IdeGiParser *)object;

  g_clear_object (&self->pool);

  G_OBJECT_CLASS (ide_gi_parser_parent_class)->finalize (object);
}

static void
ide_gi_parser_class_init (IdeGiParserClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  ide_gi_parser_global_init ();

  object_class->finalize = ide_gi_parser_finalize;
}

static void
ide_gi_parser_init (IdeGiParser *self)
{
}

IdeGiParser *
ide_gi_parser_new (void)
{
  return g_object_new (IDE_TYPE_GI_PARSER, NULL);
}
