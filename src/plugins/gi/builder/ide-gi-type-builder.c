/* ide-gi-type-builder.c
 *
 * Copyright (C) 2017 Sebastien Lafargue <slafargue@gnome.org>
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

#include <ide.h>
#include <string.h>

#include "ide-gi-type-builder.h"

#include "ide-gi-doc-builder.h"
#include "ide-gi-array-builder.h"

struct _IdeGiTypeBuilder
{
  IdeGiParserObject  parent_instance;

  IdeGiDocBlob       doc_blob;
  IdeGiTypeBlob      blob;

  /* to deferenciate between type_ref_0 annd type_ref_1 position */
  guint              ref_index : 1;
  guint              has_doc_blob : 1;
};

typedef struct
{
  const gchar    *name;
  IdeGiBasicType  type;
} TypeInfo;

static const TypeInfo BASIC_TYPES_INFO [] =
{
  { "none"     , IDE_GI_BASIC_TYPE_NONE     },
  { "gboolean" , IDE_GI_BASIC_TYPE_GBOOLEAN },
  { "gchar"    , IDE_GI_BASIC_TYPE_GCHAR    },
  { "guchar"   , IDE_GI_BASIC_TYPE_GUCHAR   },
  { "gshort"   , IDE_GI_BASIC_TYPE_GSHORT   },
  { "gushort"  , IDE_GI_BASIC_TYPE_GUSHORT  },
  { "gint"     , IDE_GI_BASIC_TYPE_GINT     },
  { "guint"    , IDE_GI_BASIC_TYPE_GUINT    },
  { "glong"    , IDE_GI_BASIC_TYPE_GLONG    },
  { "gulong"   , IDE_GI_BASIC_TYPE_GULONG   },
  { "gssize"   , IDE_GI_BASIC_TYPE_GSSIZE   },
  { "gsize"    , IDE_GI_BASIC_TYPE_GSIZE    },
  { "gpointer" , IDE_GI_BASIC_TYPE_GPOINTER },
  { "gintptr"  , IDE_GI_BASIC_TYPE_GINTPTR  },
  { "guintptr" , IDE_GI_BASIC_TYPE_GUINTPTR },
  { "gint8"    , IDE_GI_BASIC_TYPE_GINT8    },
  { "guint8"   , IDE_GI_BASIC_TYPE_GUINT8   },
  { "gint16"   , IDE_GI_BASIC_TYPE_GINT16   },
  { "guint16"  , IDE_GI_BASIC_TYPE_GUINT16  },
  { "gint32"   , IDE_GI_BASIC_TYPE_GINT32   },
  { "guint32"  , IDE_GI_BASIC_TYPE_GUINT32  },
  { "gint64"   , IDE_GI_BASIC_TYPE_GINT64   },
  { "guint64"  , IDE_GI_BASIC_TYPE_GUINT64  },
  { "gfloat"   , IDE_GI_BASIC_TYPE_GFLOAT   },
  { "gdouble"  , IDE_GI_BASIC_TYPE_GDOUBLE  },
  { "GType"    , IDE_GI_BASIC_TYPE_GTYPE    },
  { "utf8"     , IDE_GI_BASIC_TYPE_GUTF8    },
  { "filename" , IDE_GI_BASIC_TYPE_FILENAME },
  { "gunichar" , IDE_GI_BASIC_TYPE_GUNICHAR },
};

G_DEFINE_TYPE (IdeGiTypeBuilder, ide_gi_type_builder, IDE_TYPE_GI_PARSER_OBJECT)

static void
ide_gi_type_builder_start_element (GMarkupParseContext  *context,
                                   const gchar          *element_name,
                                   const gchar         **attribute_names,
                                   const gchar         **attribute_values,
                                   gpointer              user_data,
                                   GError              **error)
{
  IdeGiTypeBuilder *self = (IdeGiTypeBuilder *)user_data;
  IdeGiParserResult *result;
  IdeGiPool *pool;
  IdeGiParserObject *child;
  IdeGiElementType element_type;

  g_assert (IDE_IS_GI_TYPE_BUILDER (self));
  g_assert (context != NULL);
  g_assert (element_name != NULL);
  g_assert (attribute_names != NULL);
  g_assert (attribute_values != NULL);

  result = ide_gi_parser_object_get_result (IDE_GI_PARSER_OBJECT (self));
  pool = ide_gi_parser_result_get_pool (result);
  element_type = ide_gi_parser_get_element_type (element_name);

  if (element_type & IDE_GI_PARSER_ELEMENT_MASK_TYPE)
    {
      child = ide_gi_pool_get_object (pool, element_type);

      if (!ide_gi_parser_object_parse (IDE_GI_PARSER_OBJECT (child),
                                       context,
                                       result,
                                       element_name,
                                       attribute_names,
                                       attribute_values,
                                       error))
        return;
    }
  else
    {
      g_autoptr(GFile) file = ide_gi_parser_result_get_file (result);

      ide_gi_pool_set_unhandled_element (pool, element_name);
      child = ide_gi_pool_get_current_parser_object (pool);
      ide_gi_helper_parsing_error (child, context, file);

      return;
    }
}

static void
ide_gi_type_builder_end_element (GMarkupParseContext  *context,
                                 const gchar          *element_name,
                                 gpointer              user_data,
                                 GError              **error)
{
  IdeGiTypeBuilder *self = (IdeGiTypeBuilder *)user_data;
  IdeGiParserResult *result;
  IdeGiPool *pool;
  IdeGiParserObject *child;
  IdeGiElementType element_type;
  IdeGiTypeRef *ref;

  g_assert (IDE_IS_GI_TYPE_BUILDER (self));
  g_assert (context != NULL);
  g_assert (element_name != NULL);

  result = ide_gi_parser_object_get_result (IDE_GI_PARSER_OBJECT (self));
  pool = ide_gi_parser_result_get_pool (result);
  child = ide_gi_pool_get_current_parser_object (pool);
  element_type = ide_gi_parser_get_element_type (element_name);

  if (element_type & IDE_GI_PARSER_ELEMENT_MASK_TYPE)
    {
      if (element_type & IDE_GI_PARSER_ELEMENT_MASK_DOC)
        {
          g_autofree gchar *str = ide_gi_parser_object_finish (child);
          ide_gi_helper_update_doc_blob (result, &self->doc_blob, element_type, str);
          self->has_doc_blob = TRUE;
        }
      else
        {
          ref = (self->ref_index == 0) ? &self->blob.type_ref_0 : &self->blob.type_ref_1;
          if (ref->type != IDE_GI_BASIC_TYPE_NONE)
            {
              g_autoptr(GFile) file = ide_gi_parser_result_get_file (result);
              ide_gi_helper_parsing_error_custom (IDE_GI_PARSER_OBJECT (self),
                                                  context,
                                                  file,
                                                  "type_ref already set");
            }

          if (element_type == IDE_GI_ELEMENT_TYPE_ARRAY)
            {
              IdeGiArrayBlob *blob = ide_gi_parser_object_finish (child);
              *ref = ide_gi_parser_result_add_array (result, blob);
            }
          else // IDE_GI_ELEMENT_TYPE_TYPE
            {
              IdeGiTypeBlob *blob = ide_gi_parser_object_finish (child);
              *ref = ide_gi_parser_result_add_type (result, blob);
            }

          self->ref_index = 1;
        }

      ide_gi_pool_release_object (pool);
      g_markup_parse_context_pop (context);
    }
  else if (!dzl_str_equal0 (element_name, ide_gi_pool_get_unhandled_element (pool)))
    {
      g_autoptr(GFile) file = ide_gi_parser_result_get_file (result);

      ide_gi_helper_parsing_error (child, context, file);
    }
}

static const GMarkupParser markup_parser = {
  ide_gi_type_builder_start_element,
  ide_gi_type_builder_end_element,
  NULL,
  NULL,
  NULL,
};

static gboolean
is_name_local (IdeGiParserResult  *result,
               const gchar        *name,
               gchar             **ns_out)
{
  gchar *name_ns;
  const gchar *ns;
  const gchar *pos;
  gboolean ret = TRUE;

  g_assert (IDE_IS_GI_PARSER_RESULT (result));
  g_assert (name != NULL);

  ns = ide_gi_parser_result_get_namespace (result);
  g_assert (ns != NULL);

  if ((pos = strchr (name, '.')))
    {
      g_assert (pos - name > 0);

      name_ns = g_strndup (name, pos - name);
      ret = g_str_equal (name_ns, ns);

      if (ns_out != NULL)
        *ns_out = name_ns;
      else
        g_free (name_ns);
    }
  else
    {
      if (ns_out != NULL)
        *ns_out = g_strdup (ns);
    }

  /* TODO: is generic types local ? */
  return ret;
}

static IdeGiBasicType
get_basic_type (IdeGiTypeBuilder *self,
                const gchar      *name)
{
  for (guint i = 0; i < G_N_ELEMENTS (BASIC_TYPES_INFO); i++)
    {
      if (g_strcmp0 (name, BASIC_TYPES_INFO[i].name) == 0)
        return BASIC_TYPES_INFO[i].type;
    }

  return IDE_GI_BASIC_TYPE_NONE;
}

static gboolean
ide_gi_type_builder_parse (IdeGiParserObject    *parser_object,
                           GMarkupParseContext  *context,
                           IdeGiParserResult    *result,
                           const gchar          *element_name,
                           const gchar         **attribute_names,
                           const gchar         **attribute_values,
                           GError              **error)
{
  IdeGiTypeBuilder *self = IDE_GI_TYPE_BUILDER (parser_object);
  g_autofree gchar *ns = NULL;
  const gchar *name;
  gboolean introspectable;

  g_assert (IDE_IS_GI_TYPE_BUILDER (self));
  g_assert (IDE_IS_GI_PARSER_RESULT (result));
  g_assert (g_str_equal (element_name, "type"));

  if (!ide_gi_helper_markup_collect_attributes (result,
                                                context,
                                                element_name, attribute_names, attribute_values, error,
                                                IDE_GI_MARKUP_COLLECT_BOOLEAN | IDE_GI_MARKUP_COLLECT_OPTIONAL, "0", "introspectable", &introspectable,
                                                IDE_GI_MARKUP_COLLECT_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "name", &name,
                                                IDE_GI_MARKUP_COLLECT_OFFSET32_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "c:type", &self->blob.c_type,
                                                IDE_GI_MARKUP_COLLECT_INVALID, NULL, NULL, NULL))
    return FALSE;

  self->blob.common.blob_type = IDE_GI_BLOB_TYPE_TYPE;
  self->blob.common.introspectable = introspectable;

  /* TODO: add a namespace field ? */

  self->blob.basic_type = get_basic_type (self, name);
  self->blob.common.name = ide_gi_parser_result_add_string (result, name);
  self->blob.is_local = is_name_local (result, name, &ns);

  /*  TODO: is_type_container */

  //printf ("get type name:%s is_local:%d ns:%s\n", name, self->blob.is_local, ns);

  ide_gi_parser_object_set_result (parser_object, result);
  g_markup_parse_context_push (context, &markup_parser, self);

  return TRUE;
}

static gpointer
ide_gi_type_builder_finish (IdeGiParserObject *parser_object)
{
  IdeGiTypeBuilder *self = IDE_GI_TYPE_BUILDER (parser_object);
  IdeGiParserResult *result = ide_gi_parser_object_get_result (IDE_GI_PARSER_OBJECT (self));

  if (self->has_doc_blob)
    {
      self->doc_blob.blob_type = IDE_GI_BLOB_TYPE_DOC;
      self->blob.common.doc = ide_gi_parser_result_add_doc (result, &self->doc_blob);
    }
  else
    self->blob.common.doc = -1;

  /* set IdeGitype type */
  return &self->blob;
}

static void
ide_gi_type_builder_reset (IdeGiParserObject *parser_object)
{
  IdeGiTypeBuilder *self = IDE_GI_TYPE_BUILDER (parser_object);

  memset ((gpointer)&self->blob, 0, sizeof (IdeGiTypeBlob));
  self->ref_index = 0;
}

IdeGiParserObject *
ide_gi_type_builder_new (void)
{
  IdeGiParserObject *self;

  self = IDE_GI_PARSER_OBJECT (g_object_new (IDE_TYPE_GI_TYPE_BUILDER, NULL));

  _ide_gi_parser_object_set_element_type (self, IDE_GI_ELEMENT_TYPE_TYPE);
  return self;
}

static void
ide_gi_type_builder_finalize (GObject *object)
{
  G_OBJECT_CLASS (ide_gi_type_builder_parent_class)->finalize (object);
}

static void
ide_gi_type_builder_class_init (IdeGiTypeBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeGiParserObjectClass *parent_class = IDE_GI_PARSER_OBJECT_CLASS (klass);

  object_class->finalize = ide_gi_type_builder_finalize;

  parent_class->finish = ide_gi_type_builder_finish;
  parent_class->parse = ide_gi_type_builder_parse;
  parent_class->reset = ide_gi_type_builder_reset;
}

static void
ide_gi_type_builder_init (IdeGiTypeBuilder *self)
{
}
