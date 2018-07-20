/* ide-gi-header-builder.c
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
#include <stdlib.h>
#include <string.h>

#include "ide-gi-header-builder.h"

#include "ide-gi-alias-builder.h"
#include "ide-gi-callback-builder.h"
#include "ide-gi-class-builder.h"
#include "ide-gi-constant-builder.h"
#include "ide-gi-doc-builder.h"
#include "ide-gi-enum-builder.h"
#include "ide-gi-function-builder.h"
#include "ide-gi-interface-builder.h"
#include "ide-gi-record-builder.h"
#include "ide-gi-union-builder.h"

/* TODO: handle doc elements */

struct _IdeGiHeaderBuilder
{
  IdeGiParserObject  parent_instance;

  IdeGiDocBlob       doc_blob;
  IdeGiHeaderBlob    blob;

  GString           *includes;
  GString           *c_includes;
  GString           *packages;
};

G_DEFINE_TYPE (IdeGiHeaderBuilder, ide_gi_header_builder, IDE_TYPE_GI_PARSER_OBJECT)

static void
ide_gi_header_builder_namespace_start_element (GMarkupParseContext  *context,
                                               const gchar          *element_name,
                                               const gchar         **attribute_names,
                                               const gchar         **attribute_values,
                                               gpointer              user_data,
                                               GError              **error)
{
  IdeGiHeaderBuilder *self = (IdeGiHeaderBuilder *)user_data;
  IdeGiParserResult *result;
  IdeGiPool *pool;
  IdeGiParserObject *child;
  IdeGiElementType element_type;

  g_assert (IDE_IS_GI_HEADER_BUILDER (self));
  g_assert (context != NULL);
  g_assert (element_name != NULL);
  g_assert (attribute_names != NULL);
  g_assert (attribute_values != NULL);

  result = ide_gi_parser_object_get_result (IDE_GI_PARSER_OBJECT (self));
  pool = ide_gi_parser_result_get_pool (result);
  element_type = ide_gi_parser_get_element_type (element_name);

  if (element_type & IDE_GI_PARSER_ELEMENT_MASK_NAMESPACE)
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
ide_gi_header_builder_namespace_end_element (GMarkupParseContext  *context,
                                             const gchar          *element_name,
                                             gpointer              user_data,
                                             GError              **error)
{
  IdeGiHeaderBuilder *self = (IdeGiHeaderBuilder *)user_data;
  IdeGiParserResult *result;
  IdeGiPool *pool;
  IdeGiParserObject *child;
  IdeGiElementType element_type;
  gint32 offset;

  g_assert (IDE_IS_GI_HEADER_BUILDER (self));
  g_assert (context != NULL);
  g_assert (element_name != NULL);

  result = ide_gi_parser_object_get_result (IDE_GI_PARSER_OBJECT (self));
  pool = ide_gi_parser_result_get_pool (result);
  child = ide_gi_pool_get_current_parser_object (pool);
  element_type = ide_gi_parser_get_element_type (element_name);

  if (element_type & IDE_GI_PARSER_ELEMENT_MASK_NAMESPACE)
    {
      if (element_type == IDE_GI_ELEMENT_TYPE_ALIAS)
        {
          IdeGiAliasBlob *blob = ide_gi_parser_object_finish (child);
          offset = ide_gi_parser_result_add_alias (result, blob);
          ide_gi_parser_object_index (IDE_GI_PARSER_OBJECT (child), result, GINT_TO_POINTER (offset));
        }
      else if (element_type == IDE_GI_ELEMENT_TYPE_BITFIELD)
        {
          IdeGiEnumBlob *blob = ide_gi_parser_object_finish (child);
          offset = ide_gi_parser_result_add_enum (result, blob);
          ide_gi_parser_object_index (IDE_GI_PARSER_OBJECT (child), result, GINT_TO_POINTER (offset));
        }
      else if (element_type == IDE_GI_ELEMENT_TYPE_CALLBACK)
        {
          IdeGiCallbackBlob *blob = ide_gi_parser_object_finish (child);
          IdeGiTypeRef typeref = ide_gi_parser_result_add_callback (result, blob);
          ide_gi_parser_object_index (IDE_GI_PARSER_OBJECT (child), result, GINT_TO_POINTER (typeref.offset));
        }
      else if (element_type == IDE_GI_ELEMENT_TYPE_CLASS)
        {
          IdeGiObjectBlob *blob = ide_gi_parser_object_finish (child);
          offset = ide_gi_parser_result_add_object (result, blob);
          ide_gi_parser_object_index (IDE_GI_PARSER_OBJECT (child), result, GINT_TO_POINTER (offset));
        }
      else if (element_type == IDE_GI_ELEMENT_TYPE_CONSTANT)
        {
          IdeGiConstantBlob *blob = ide_gi_parser_object_finish (child);
          offset = ide_gi_parser_result_add_constant (result, blob);
          ide_gi_parser_object_index (IDE_GI_PARSER_OBJECT (child), result, GINT_TO_POINTER (offset));
        }
      else if (element_type == IDE_GI_ELEMENT_TYPE_ENUMERATION)
        {
          IdeGiEnumBlob *blob = ide_gi_parser_object_finish (child);
          offset = ide_gi_parser_result_add_enum (result, blob);
          ide_gi_parser_object_index (IDE_GI_PARSER_OBJECT (child), result, GINT_TO_POINTER (offset));
        }
      else if (element_type == IDE_GI_ELEMENT_TYPE_FUNCTION)
        {
          IdeGiFunctionBlob *blob = ide_gi_parser_object_finish (child);
          offset = ide_gi_parser_result_add_function (result, blob);
          ide_gi_parser_object_index (IDE_GI_PARSER_OBJECT (child), result, GINT_TO_POINTER (offset));
        }
      else if (element_type == IDE_GI_ELEMENT_TYPE_GLIB_BOXED)
        {
          IdeGiRecordBlob *blob = ide_gi_parser_object_finish (child);
          offset = ide_gi_parser_result_add_record (result, blob);
          ide_gi_parser_object_index (IDE_GI_PARSER_OBJECT (child), result, GINT_TO_POINTER (offset));
        }
      else if (element_type == IDE_GI_ELEMENT_TYPE_INTERFACE)
        {
          IdeGiObjectBlob *blob = ide_gi_parser_object_finish (child);
          offset = ide_gi_parser_result_add_object (result, blob);
          ide_gi_parser_object_index (IDE_GI_PARSER_OBJECT (child), result, GINT_TO_POINTER (offset));
        }
      else if (element_type == IDE_GI_ELEMENT_TYPE_RECORD)
        {
          IdeGiRecordBlob *blob = ide_gi_parser_object_finish (child);
          offset = ide_gi_parser_result_add_record (result, blob);
          ide_gi_parser_object_index (IDE_GI_PARSER_OBJECT (child), result, GINT_TO_POINTER (offset));
        }
      else if (element_type == IDE_GI_ELEMENT_TYPE_UNION)
        {
          IdeGiUnionBlob *blob = ide_gi_parser_object_finish (child);
          offset = ide_gi_parser_result_add_union (result, blob);
          ide_gi_parser_object_index (IDE_GI_PARSER_OBJECT (child), result, GINT_TO_POINTER (offset));
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

static const GMarkupParser markup_namespace_parser = {
  ide_gi_header_builder_namespace_start_element,
  ide_gi_header_builder_namespace_end_element,
  NULL,
  NULL,
  NULL,
};

static gchar *
get_c_prefixes (const gchar *c_identifier_prefixes,
                const gchar *c_prefix)
{
  if (dzl_str_empty0 (c_identifier_prefixes))
    {
      if (dzl_str_empty0 (c_prefix))
        return NULL;

      return g_strdup (c_prefix);
    }

  if (dzl_str_empty0 (c_prefix))
    return g_strdup (c_identifier_prefixes);

  return g_strconcat (c_identifier_prefixes, ",", c_prefix, NULL);
};

static gboolean
parse_version (const gchar *version,
               guint16     *major,
               guint16     *minor,
               guint16     *micro)
{
  gchar *end;
  guint64 tmp_major = 0;
  guint64 tmp_minor = 0;
  guint64 tmp_micro = 0;

  g_assert (version != NULL);

  tmp_major = g_ascii_strtoull (version, &end, 10);
  if (tmp_major >= 0x100 || end == version)
    return FALSE;

  if (*end == '\0')
    goto next;

  if (*end != '.')
    return FALSE;

  version = end + 1;
  tmp_minor = g_ascii_strtoull (version, &end, 10);
  if (tmp_minor >= 0x100 || end == version)
    return FALSE;

  if (*end == '\0')
    goto next;

  if (*end != '.')
    return FALSE;

  version = end + 1;
  tmp_micro = g_ascii_strtoull (version, &end, 10);
  if (tmp_micro >= 0x100 ||
      end == version ||
      *end != '\0')
    return FALSE;

next:
  if (major != NULL)
    *major = tmp_major;

  if (minor != NULL)
    *minor = tmp_minor;

  if (micro != NULL)
    *micro = tmp_micro;

  return TRUE;
}

static gboolean
ide_gi_header_builder_parse_namespace (IdeGiParserObject    *parser_object,
                                       GMarkupParseContext  *context,
                                       IdeGiParserResult    *result,
                                       const gchar          *element_name,
                                       const gchar         **attribute_names,
                                       const gchar         **attribute_values,
                                       GError              **error)
{
  IdeGiHeaderBuilder *self = IDE_GI_HEADER_BUILDER (parser_object);
  g_autofree gchar *prefixes = NULL;
  const gchar *namespace = NULL;
  const gchar *nsversion = NULL;
  const gchar *shared_library = NULL;
  const gchar *c_symbol_prefixes = NULL;
  const gchar *c_identifier_prefixes = NULL;
  const gchar *c_prefix = NULL;
  guint16 major_version = 0;
  guint16 minor_version = 0;

  g_assert (IDE_IS_GI_HEADER_BUILDER (self));
  g_assert (IDE_IS_GI_PARSER_RESULT (result));
  g_assert (g_str_equal (element_name, "namespace"));

  if (!ide_gi_helper_markup_collect_attributes (result,
                                                context,
                                                element_name, attribute_names, attribute_values, error,
                                                IDE_GI_MARKUP_COLLECT_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "name", &namespace,
                                                IDE_GI_MARKUP_COLLECT_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "version", &nsversion,
                                                IDE_GI_MARKUP_COLLECT_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "shared-library", &shared_library,
                                                IDE_GI_MARKUP_COLLECT_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "c:symbol-prefixes", &c_symbol_prefixes,
                                                IDE_GI_MARKUP_COLLECT_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "c:identifier-prefixes", &c_identifier_prefixes,
                                                IDE_GI_MARKUP_COLLECT_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "c:prefix", &c_prefix,
                                                IDE_GI_MARKUP_COLLECT_INVALID, NULL, NULL, NULL))
    return FALSE;

  parse_version (nsversion, &major_version, &minor_version, NULL);

  self->blob.major_version = (guint8)major_version;
  self->blob.minor_version = (guint8)minor_version;
  self->blob.namespace = ide_gi_parser_result_add_header_string (result, namespace);
  self->blob.nsversion = ide_gi_parser_result_add_header_string (result, nsversion);
  self->blob.shared_library = ide_gi_parser_result_add_header_string (result, shared_library);
  self->blob.c_symbol_prefixes = ide_gi_parser_result_add_header_string (result, c_symbol_prefixes);

  prefixes = get_c_prefixes (c_identifier_prefixes, c_prefix);
  self->blob.c_identifier_prefixes =
   (prefixes != NULL) ? ide_gi_parser_result_add_header_string (result, prefixes) : 0;

  /* We set the header a first time, even if not complete,  because gir hierarchy rely on some of its fields */
  ide_gi_parser_result_set_header (result, &self->blob);

  g_markup_parse_context_push (context, &markup_namespace_parser, self);

  return TRUE;
}

static gboolean
ide_gi_header_builder_parse_include (IdeGiParserObject    *parser_object,
                                     GMarkupParseContext  *context,
                                     IdeGiParserResult    *result,
                                     const gchar          *element_name,
                                     const gchar        **attribute_names,
                                     const gchar         **attribute_values,
                                     GError              **error)
{
  IdeGiHeaderBuilder *self = (IdeGiHeaderBuilder *)parser_object;
  const gchar *name = NULL;
  const gchar *version = NULL;
  IdeGiElementType element_type;

  g_assert (IDE_IS_GI_HEADER_BUILDER (parser_object));
  g_assert (IDE_IS_GI_PARSER_RESULT (result));
  g_assert (g_str_equal (element_name, "c:include") || g_str_equal (element_name, "include"));

  element_type = ide_gi_parser_get_element_type (element_name);

  if (!ide_gi_helper_markup_collect_attributes (result,
                                                context,
                                                element_name, attribute_names, attribute_values, error,
                                                IDE_GI_MARKUP_COLLECT_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "name", &name,
                                                IDE_GI_MARKUP_COLLECT_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "version", &version,
                                                IDE_GI_MARKUP_COLLECT_INVALID, NULL, NULL, NULL))
    return FALSE;

  if (element_type == IDE_GI_ELEMENT_TYPE_C_INCLUDE)
    {
      if (self->c_includes == NULL)
        self->c_includes = g_string_new (NULL);
      else
        g_string_append_c (self->c_includes, ',');

      g_string_append (self->c_includes, name);
    }
  else if (element_type == IDE_GI_ELEMENT_TYPE_INCLUDE)
    {
      if (self->includes == NULL)
        self->includes = g_string_new (NULL);
      else
        g_string_append_c (self->includes, ',');

      g_string_append (self->includes, name);
      g_string_append_c (self->includes, ':');
      g_string_append (self->includes, version);
    }

  return TRUE;
}

static gboolean
ide_gi_header_builder_parse_package (IdeGiParserObject    *parser_object,
                                     GMarkupParseContext  *context,
                                     IdeGiParserResult    *result,
                                     const gchar          *element_name,
                                     const gchar         **attribute_names,
                                     const gchar         **attribute_values,
                                     GError              **error)
{
  IdeGiHeaderBuilder *self = (IdeGiHeaderBuilder *)parser_object;
  const gchar *name = NULL;

  g_assert (IDE_IS_GI_PARSER_OBJECT (parser_object));
  g_assert (IDE_IS_GI_PARSER_RESULT (result));
  g_assert (g_str_equal (element_name, "package"));

  if (!ide_gi_helper_markup_collect_attributes (result,
                                                context,
                                                element_name, attribute_names, attribute_values, error,
                                                IDE_GI_MARKUP_COLLECT_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "name", &name,
                                                IDE_GI_MARKUP_COLLECT_INVALID, NULL, NULL, NULL))
    return FALSE;

  if (self->packages == NULL)
    self->packages = g_string_new (NULL);
  else
    g_string_append_c (self->packages, ',');

  g_string_append (self->packages, name);

  return TRUE;
}

static void
ide_gi_header_builder_header_start_element (GMarkupParseContext  *context,
                                            const gchar          *element_name,
                                            const gchar         **attribute_names,
                                            const gchar         **attribute_values,
                                            gpointer              user_data,
                                            GError              **error)
{
  IdeGiHeaderBuilder *self = (IdeGiHeaderBuilder *)user_data;
  IdeGiParserResult *result;
  IdeGiPool *pool;
  IdeGiParserObject *child;
  IdeGiElementType element_type;

  g_assert (IDE_IS_GI_HEADER_BUILDER (self));
  g_assert (context != NULL);
  g_assert (element_name != NULL);
  g_assert (attribute_names != NULL);
  g_assert (attribute_values != NULL);

  result = ide_gi_parser_object_get_result (IDE_GI_PARSER_OBJECT (self));
  pool = ide_gi_parser_result_get_pool (result);
  element_type = ide_gi_parser_get_element_type (element_name);

  g_assert (IDE_IS_GI_HEADER_BUILDER (self));

  if (element_type == IDE_GI_ELEMENT_TYPE_INCLUDE ||
      element_type == IDE_GI_ELEMENT_TYPE_C_INCLUDE)
    {
      ide_gi_header_builder_parse_include (IDE_GI_PARSER_OBJECT (self),
                                           context,
                                           result,
                                           element_name,
                                           attribute_names,
                                           attribute_values,
                                           error);
      return;
    }
  else if (element_type == IDE_GI_ELEMENT_TYPE_PACKAGE)
    {
      ide_gi_header_builder_parse_package (IDE_GI_PARSER_OBJECT (self),
                                           context,
                                           result,
                                           element_name,
                                           attribute_names,
                                           attribute_values,
                                           error);
      return;
    }
  else if (element_type == IDE_GI_ELEMENT_TYPE_NAMESPACE)
    {
      ide_gi_header_builder_parse_namespace (IDE_GI_PARSER_OBJECT (self),
                                             context,
                                             result,
                                             element_name,
                                             attribute_names,
                                             attribute_values,
                                             error);
      return;
    }
  else if (element_type == IDE_GI_ELEMENT_TYPE_ENUMERATION ||
           element_type == IDE_GI_ELEMENT_TYPE_CONSTANT)
    {
      /* Seems that it's a rare case, use by at least RyGel libs */
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
ide_gi_header_builder_header_end_element (GMarkupParseContext  *context,
                                          const gchar          *element_name,
                                          gpointer              user_data,
                                          GError              **error)
{
  IdeGiHeaderBuilder *self = (IdeGiHeaderBuilder *)user_data;
  IdeGiParserResult *result;
  IdeGiPool *pool;
  IdeGiParserObject *child;
  IdeGiElementType element_type;

  g_assert (IDE_IS_GI_HEADER_BUILDER (self));
  g_assert (context != NULL);
  g_assert (element_name != NULL);

  result = ide_gi_parser_object_get_result (IDE_GI_PARSER_OBJECT (self));
  pool = ide_gi_parser_result_get_pool (result);
  child = ide_gi_pool_get_current_parser_object (pool);
  element_type = ide_gi_parser_get_element_type (element_name);

  if (element_type == IDE_GI_ELEMENT_TYPE_INCLUDE ||
      element_type == IDE_GI_ELEMENT_TYPE_C_INCLUDE ||
      element_type == IDE_GI_ELEMENT_TYPE_PACKAGE)
    {
      return;
    }
  else if (element_type == IDE_GI_ELEMENT_TYPE_NAMESPACE)
    {
      g_markup_parse_context_pop (context);
    }
  else if (element_type == IDE_GI_ELEMENT_TYPE_ENUMERATION ||
           element_type == IDE_GI_ELEMENT_TYPE_CONSTANT)
    {
      /* TODO: element finish ? */
      //ide_gi_parser_object_index (IDE_GI_PARSER_OBJECT (child), result);

      ide_gi_pool_release_object (pool);
      g_markup_parse_context_pop (context);
    }
  else if (!dzl_str_equal0 (element_name, ide_gi_pool_get_unhandled_element (pool)))
    {
      g_autoptr(GFile) file = ide_gi_parser_result_get_file (result);

      ide_gi_helper_parsing_error (child, context, file);
    }
}

static const GMarkupParser markup_header_parser = {
  ide_gi_header_builder_header_start_element,
  ide_gi_header_builder_header_end_element,
  NULL,
  NULL,
  NULL,
};

static gboolean
ide_gi_header_builder_parse_header (IdeGiParserObject    *parser_object,
                                    GMarkupParseContext  *context,
                                    IdeGiParserResult    *result,
                                    const gchar          *element_name,
                                    const gchar         **attribute_names,
                                    const gchar         **attribute_values,
                                    GError              **error)
{
  IdeGiHeaderBuilder *self = IDE_GI_HEADER_BUILDER (parser_object);
  const gchar *repo_version = NULL;

  g_assert (IDE_IS_GI_HEADER_BUILDER (self));
  g_assert (IDE_IS_GI_PARSER_RESULT (result));
  g_assert (g_str_equal (element_name, "repository"));

  if (!ide_gi_helper_markup_collect_attributes (result,
                                                context,
                                                element_name, attribute_names, attribute_values, error,
                                                IDE_GI_MARKUP_COLLECT_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "version", &repo_version,
                                                IDE_GI_MARKUP_COLLECT_INVALID, NULL, NULL, NULL))
    return FALSE;

  parse_version (repo_version,
                 &self->blob.repo_major_version,
                 &self->blob.repo_minor_version,
                 NULL);
  self->blob.blob_type = IDE_GI_BLOB_TYPE_HEADER;

  ide_gi_parser_object_set_result (parser_object, result);
  g_markup_parse_context_push (context, &markup_header_parser, self);

  return TRUE;
}

static gpointer
ide_gi_header_builder_finish_header (IdeGiParserObject *parser_object)
{
  IdeGiHeaderBuilder *self = IDE_GI_HEADER_BUILDER (parser_object);
  IdeGiParserResult *result;

  result = ide_gi_parser_object_get_result (IDE_GI_PARSER_OBJECT (self));

  if (self->c_includes != NULL)
    {
      g_autofree gchar *str = NULL;

      str = g_string_free (self->c_includes, FALSE);
      self->blob.c_includes = ide_gi_parser_result_add_header_string (result, str);
      self->c_includes = NULL;
    }

  if (self->includes != NULL)
    {
      g_autofree gchar *str = NULL;

      str = g_string_free (self->includes, FALSE);
      self->blob.includes = ide_gi_parser_result_add_header_string (result, str);
      self->includes = NULL;
    }

  if (self->packages != NULL)
    {
      g_autofree gchar *str = NULL;

      str = g_string_free (self->packages, FALSE);
      self->blob.packages = ide_gi_parser_result_add_header_string (result, str);
      self->packages = NULL;
    }

  /* TODO: those strings are already in the ro header, do we need them in the .ns too ? */
  return &self->blob;
}

static void
ide_gi_header_builder_index (IdeGiParserObject *parse_object,
                             IdeGiParserResult *result,
                             gpointer           user_data)
{
  IdeGiHeaderBuilder *self = (IdeGiHeaderBuilder *)parse_object;
  gint32 offset = GPOINTER_TO_INT (user_data);
  const gchar *name;

  g_return_if_fail (IDE_IS_GI_PARSER_OBJECT (self));
  g_return_if_fail (IDE_IS_GI_PARSER_RESULT (result));

  name = ide_gi_parser_result_get_header_string (result, self->blob.namespace);
  ide_gi_parser_result_add_global_index (result,
                                         name,
                                         offset,
                                         IDE_GI_PREFIX_TYPE_NAMESPACE,
                                         IDE_GI_BLOB_TYPE_HEADER,
                                         FALSE);

  name = ide_gi_parser_result_get_header_string (result, self->blob.packages);
  if (!dzl_str_empty0 (name))
    {
      if (strchr (name, ',') != NULL)
        {
          g_auto(GStrv) parts = g_strsplit (name, ",", -1);

          for (guint i = 0; parts[i] != NULL; i++)
            ide_gi_parser_result_add_global_index (result,
                                                   parts[i],
                                                   offset,
                                                   IDE_GI_PREFIX_TYPE_PACKAGE,
                                                   IDE_GI_BLOB_TYPE_HEADER,
                                                   FALSE);
        }
      else
        ide_gi_parser_result_add_global_index (result,
                                               name,
                                               offset,
                                               IDE_GI_PREFIX_TYPE_PACKAGE,
                                               IDE_GI_BLOB_TYPE_HEADER,
                                               FALSE);
    }

  name = ide_gi_parser_result_get_header_string (result, self->blob.c_symbol_prefixes);
  if (!dzl_str_empty0 (name))
    {
      if (strchr (name, ',') != NULL)
        {
          g_auto(GStrv) parts = g_strsplit (name, ",", -1);

          for (guint i = 0; parts[i] != NULL; i++)
            ide_gi_parser_result_add_global_index (result,
                                                   parts[i],
                                                   offset,
                                                   IDE_GI_PREFIX_TYPE_SYMBOL,
                                                   IDE_GI_BLOB_TYPE_HEADER,
                                                   FALSE);
        }
      else
        ide_gi_parser_result_add_global_index (result,
                                               name,
                                               offset,
                                               IDE_GI_PREFIX_TYPE_SYMBOL,
                                               IDE_GI_BLOB_TYPE_HEADER,
                                               FALSE);
    }

  name = ide_gi_parser_result_get_header_string (result, self->blob.c_identifier_prefixes);
  if (!dzl_str_empty0 (name))
    {
      if (strchr (name, ',') != NULL)
        {
          g_auto(GStrv) parts = g_strsplit (name, ",", -1);

          for (guint i = 0; parts[i] != NULL; i++)
            ide_gi_parser_result_add_global_index (result,
                                                   parts[i],
                                                   offset,
                                                   IDE_GI_PREFIX_TYPE_IDENTIFIER,
                                                   IDE_GI_BLOB_TYPE_HEADER,
                                                   FALSE);
        }
      else
        ide_gi_parser_result_add_global_index (result,
                                               name,
                                               offset,
                                               IDE_GI_PREFIX_TYPE_IDENTIFIER,
                                               IDE_GI_BLOB_TYPE_HEADER,
                                               FALSE);
    }
}

static void
ide_gi_header_builder_reset_header (IdeGiParserObject *parser_object)
{
  IdeGiHeaderBuilder *self = IDE_GI_HEADER_BUILDER (parser_object);

  memset ((gpointer)&self->blob, 0, sizeof (IdeGiHeaderBlob));
}

IdeGiParserObject *
ide_gi_header_builder_new (void)
{
  IdeGiParserObject *self;

  self = IDE_GI_PARSER_OBJECT (g_object_new (IDE_TYPE_GI_HEADER_BUILDER, NULL));

  _ide_gi_parser_object_set_element_type (self, IDE_GI_ELEMENT_TYPE_REPOSITORY);
  return self;
}

static void
ide_gi_header_builder_finalize (GObject *object)
{
  IdeGiHeaderBuilder *self = IDE_GI_HEADER_BUILDER (object);

  if (self->c_includes != NULL)
    {
      g_string_free (self->c_includes, TRUE);
      self->c_includes = NULL;
    }

  if (self->includes != NULL)
    {
      g_string_free (self->includes, TRUE);
      self->includes = NULL;
    }

  if (self->packages != NULL)
    {
      g_string_free (self->packages, TRUE);
      self->packages = NULL;
    }

  G_OBJECT_CLASS (ide_gi_header_builder_parent_class)->finalize (object);
}

static void
ide_gi_header_builder_class_init (IdeGiHeaderBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeGiParserObjectClass *parent_class = IDE_GI_PARSER_OBJECT_CLASS (klass);

  object_class->finalize = ide_gi_header_builder_finalize;

  parent_class->finish = ide_gi_header_builder_finish_header;
  parent_class->index = ide_gi_header_builder_index;
  parent_class->parse = ide_gi_header_builder_parse_header;
  parent_class->reset = ide_gi_header_builder_reset_header;
}

static void
ide_gi_header_builder_init (IdeGiHeaderBuilder *self)
{
}
