/* ide-gi-helper.c
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

#include <dazzle.h>
#include <ide.h>

#include "ide-gi-parser-object.h"

#include "ide-gi-helper.h"

void
ide_gi_helper_update_doc_blob (IdeGiParserResult *result,
                               IdeGiDocBlob      *blob,
                               IdeGiElementType   element_type,
                               const gchar       *str)
{
  guint32 offset;

  g_return_if_fail (IDE_IS_GI_PARSER_RESULT (result));
  g_return_if_fail (blob != NULL);

  if (dzl_str_empty0 (str))
    return;

  /* TODO: finish the annotation strings handling */
  if (element_type == IDE_GI_ELEMENT_TYPE_ANNOTATION)
    {
      offset = ide_gi_parser_result_add_annotation_string (result, str);
      blob->n_attributes++;

      if (blob->attributes == 0)
        blob->attributes = offset;
    }
  else
    {
      offset = ide_gi_parser_result_add_doc_string (result, str);

      if (element_type == IDE_GI_ELEMENT_TYPE_DOC)
        blob->doc = offset;
      else if (element_type == IDE_GI_ELEMENT_TYPE_DOC_DEPRECATED)
        blob->doc_deprecated = offset;
      else if (element_type == IDE_GI_ELEMENT_TYPE_DOC_STABILITY)
        blob->doc_stability = offset;
      else if (element_type == IDE_GI_ELEMENT_TYPE_DOC_VERSION)
        blob->doc_version = offset;
      else
        g_assert_not_reached ();
    }
}

static inline gboolean
markup_parse_boolean_internal (const gchar *string,
                               gboolean    *value)
{
  gchar const * const falses[] = { "false", "f", "no", "n", "0" };
  gchar const * const trues[] = { "true", "t", "yes", "y", "1" };

  for (guint i = 0; i < G_N_ELEMENTS (falses); i++)
    {
      if (g_ascii_strcasecmp (string, falses[i]) == 0)
        {
          if (value != NULL)
            *value = FALSE;

          return TRUE;
        }
    }

  for (guint i = 0; i < G_N_ELEMENTS (trues); i++)
    {
      if (g_ascii_strcasecmp (string, trues[i]) == 0)
        {
          if (value != NULL)
            *value = TRUE;

          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
markup_parse_boolean (const char  *string,
                      const gchar *def,
                      gboolean    *value)
{
  if (!markup_parse_boolean_internal (string, value))
    {
      if (!markup_parse_boolean_internal (def, value))
        *value = FALSE;

      return FALSE;
    }

  return TRUE;
}

/* TODO: better error system, show warning only when debug activated.*/

/* Modified version of g_markup_collect_attributes */
gboolean
ide_gi_helper_markup_collect_attributes (IdeGiParserResult       *result,
                                         GMarkupParseContext     *context,
                                         const gchar             *element_name,
                                         const gchar            **attribute_names,
                                         const gchar            **attribute_values,
                                         GError                 **error,
                                         IdeGiMarkupCollectType   first_type,
                                         const gchar             *default_value,
                                         const gchar             *first_attr,
                                         ...)
{
  IdeGiMarkupCollectType type = first_type;
  const gchar *def = default_value;
  const gchar *attr = first_attr;
  va_list ap;
  guint i;

  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (result), FALSE);
  g_return_val_if_fail (context != NULL, FALSE);
  g_return_val_if_fail (element_name != NULL, FALSE);
  g_return_val_if_fail (attribute_names != NULL, FALSE);
  g_return_val_if_fail (attribute_values != NULL, FALSE);
  g_return_val_if_fail (error != NULL, FALSE);
  g_return_val_if_fail (default_value != NULL, FALSE);

  va_start (ap, first_attr);
  while (type != IDE_GI_MARKUP_COLLECT_INVALID)
    {
      gboolean mandatory = !(type & IDE_GI_MARKUP_COLLECT_OPTIONAL);
      const gchar *value;

      type &= (IDE_GI_MARKUP_COLLECT_OPTIONAL - 1);

      if (type == IDE_GI_MARKUP_COLLECT_TRISTATE)
        mandatory = FALSE;

      /* search for the matching name/value in the array */
      for (i = 0; attribute_names[i]; i++)
        if (!strcmp (attribute_names[i], attr))
          break;


      value = attribute_values[i];
      if (value == NULL)
        {
          if (mandatory)
            {
              g_set_error (error, G_MARKUP_ERROR,
                           G_MARKUP_ERROR_MISSING_ATTRIBUTE,
                           "element '%s' requires attribute '%s'",
                           element_name, attr);

              va_end (ap);
              goto failure;
            }
          else
            value = def;
        }


      switch (type)
        {
        case IDE_GI_MARKUP_COLLECT_STRING:
          {
            const char **str_ptr = va_arg (ap, const char **);

            if (str_ptr != NULL)
              *str_ptr = value;
          }

          break;

        case IDE_GI_MARKUP_COLLECT_OFFSET32_STRING:
          {
            guint32 *guint32_ptr = va_arg (ap, guint32 *);

            if (guint32_ptr != NULL)
              *guint32_ptr = ide_gi_parser_result_add_string (result, value);
          }

          break;

        case IDE_GI_MARKUP_COLLECT_OFFSET32_DOC_STRING:
          {
            guint32 *guint32_ptr = va_arg (ap, guint32 *);

            if (guint32_ptr != NULL)
              *guint32_ptr = ide_gi_parser_result_add_doc_string (result, value);
          }

          break;

        case IDE_GI_MARKUP_COLLECT_STABILITY:
          {
            IdeGiStability *stability_ptr = va_arg (ap, IdeGiStability *);

            if (stability_ptr != NULL)
              {
                if (g_strcmp0 (value, "Stable") == 0)
                  *stability_ptr = IDE_GI_STABILITY_STABLE;
                else if (g_strcmp0 (value, "Unstable") == 0)
                  *stability_ptr = IDE_GI_STABILITY_UNSTABLE;
                else  if (g_strcmp0 (value, "Private") == 0)
                  *stability_ptr = IDE_GI_STABILITY_PRIVATE;
                else
                  g_assert_not_reached ();
              }
          }

          break;

        case IDE_GI_MARKUP_COLLECT_SCOPE:
          {
            IdeGiScope *scope_ptr = va_arg (ap, IdeGiScope *);

            if (scope_ptr != NULL)
              {
                if (g_strcmp0 (value, "call") == 0)
                  *scope_ptr = IDE_GI_SCOPE_CALL;
                else if (g_strcmp0 (value, "async") == 0)
                  *scope_ptr = IDE_GI_SCOPE_ASYNC;
                else if (g_strcmp0 (value, "notified") == 0)
                  *scope_ptr = IDE_GI_SCOPE_NOTIFIED;
                else
                  g_assert_not_reached ();
              }
          }

          break;

        case IDE_GI_MARKUP_COLLECT_DIRECTION:
          {
            IdeGiDirection *direction_ptr = va_arg (ap, IdeGiDirection *);

            if (direction_ptr != NULL)
              {
                if (g_strcmp0 (value, "in") == 0)
                  *direction_ptr = IDE_GI_DIRECTION_IN;
                else if (g_strcmp0 (value, "out") == 0)
                  *direction_ptr = IDE_GI_DIRECTION_OUT;
                else if (g_strcmp0 (value, "inout") == 0)
                  *direction_ptr = IDE_GI_DIRECTION_INOUT;
                else
                  g_assert_not_reached ();
              }
          }

          break;

        case IDE_GI_MARKUP_COLLECT_TRANSFER_OWNERSHIP:
          {
            IdeGiTransferOwnership *transfer_ptr = va_arg (ap, IdeGiTransferOwnership *);

            if (transfer_ptr != NULL)
              {
                if (g_strcmp0 (value, "none") == 0)
                  *transfer_ptr = IDE_GI_TRANSFER_OWNERSHIP_NONE;
                else if (g_strcmp0 (value, "container") == 0)
                  *transfer_ptr = IDE_GI_TRANSFER_OWNERSHIP_CONTAINER;
                else if (g_strcmp0 (value, "full") == 0)
                  *transfer_ptr = IDE_GI_TRANSFER_OWNERSHIP_FULL;
                else if (g_strcmp0 (value, "floating") == 0)
                  *transfer_ptr = IDE_GI_TRANSFER_OWNERSHIP_FLOATING;
                else
                  g_assert_not_reached ();
              }
          }

          break;

        case IDE_GI_MARKUP_COLLECT_SIGNAL_WHEN:
          {
            IdeGiSignalWhen *when_ptr = va_arg (ap, IdeGiSignalWhen *);

            if (when_ptr != NULL)
              {
                if (g_strcmp0 (value, "first") == 0)
                  *when_ptr = IDE_GI_SIGNAL_WHEN_FIRST;
                else if (g_strcmp0 (value, "last") == 0)
                  *when_ptr = IDE_GI_SIGNAL_WHEN_LAST;
                else if (g_strcmp0 (value, "cleanup") == 0)
                  *when_ptr = IDE_GI_SIGNAL_WHEN_CLEANUP;
                else
                  g_assert_not_reached ();
              }
          }

          break;

        case IDE_GI_MARKUP_COLLECT_UINT64:
          {
            guint64 *guint64_ptr = va_arg (ap, guint64 *);

            if (guint64_ptr != NULL)
              *guint64_ptr = g_ascii_strtoull (value, NULL, 10);
          }

          break;

        case IDE_GI_MARKUP_COLLECT_INT64:
          {
            gint64 *gint64_ptr = va_arg (ap, gint64 *);

            if (gint64_ptr != NULL)
              *gint64_ptr = g_ascii_strtoll (value, NULL, 10);
          }

          break;

        case IDE_GI_MARKUP_COLLECT_BOOLEAN:
        case IDE_GI_MARKUP_COLLECT_TRISTATE:
            {
              gboolean *bool_ptr = va_arg (ap, gboolean *);

              if (value == NULL)
                {
                  if (bool_ptr != NULL)
                    {
                      if (type == IDE_GI_MARKUP_COLLECT_TRISTATE)
                        *bool_ptr = -1;
                      else /* IDE_GI_MARKUP_COLLECT_BOOLEAN */
                        *bool_ptr = FALSE;
                    }
                }
              else
                {
                  if (!markup_parse_boolean (value, def, bool_ptr))
                    {
                      if (mandatory)
                        {
                          g_set_error (error, G_MARKUP_ERROR,
                                       G_MARKUP_ERROR_INVALID_CONTENT,
                                       "element '%s', attribute '%s', value '%s' "
                                       "cannot be parsed as a boolean value",
                                       element_name, attr, value);
                          va_end (ap);
                          goto failure;
                        }
                      else
                        {
                          gint line;
                          gint offset;

                          /* TODO: add file info */
                          g_markup_parse_context_get_position (context, &line, &offset);
                          g_debug ("Error on line %d char %d:"
                                   "element '%s', attribute '%s', value '%s' "
                                   "cannot be parsed as a boolean value",
                                   line, offset, element_name, attr, value);

                          /* Fix some errors in RygelCore, GXml, Tracker and Gee
                           * using some false boolean strings for deprecated attribute
                           */
                          if (!g_strcmp0 (attr, "deprecated"))
                            *bool_ptr = TRUE;
                          else
                            {
                              if (!markup_parse_boolean_internal (def, bool_ptr))
                                *bool_ptr = FALSE;
                            }
                        }
                    }
                }
            }

          break;

        case IDE_GI_MARKUP_COLLECT_INVALID:
        case IDE_GI_MARKUP_COLLECT_OPTIONAL:
        default:
          g_assert_not_reached ();
        }

      type = va_arg (ap, IdeGiMarkupCollectType);
      def = va_arg (ap, const char *);
      attr = va_arg (ap, const char *);
   }

  va_end (ap);

  return TRUE;

failure:
  type = first_type;
  attr = first_attr;
  def = default_value;

  va_start (ap, first_attr);
  while (type != IDE_GI_MARKUP_COLLECT_INVALID)
    {
      gpointer ptr = va_arg (ap, gpointer);

      if (ptr != NULL)
        {
          switch (type & (IDE_GI_MARKUP_COLLECT_OPTIONAL - 1))
            {
            case IDE_GI_MARKUP_COLLECT_STRING:
              *(char **)ptr = NULL;
              break;

            case IDE_GI_MARKUP_COLLECT_BOOLEAN:
              *(gboolean *)ptr = FALSE;
              break;

            case IDE_GI_MARKUP_COLLECT_UINT64:
              *(guint64 *)ptr = FALSE;
              break;

            case IDE_GI_MARKUP_COLLECT_INT64:
              *(gint64 *)ptr = FALSE;
              break;

            case IDE_GI_MARKUP_COLLECT_STABILITY:
              *(IdeGiStability *)ptr = IDE_GI_STABILITY_STABLE;
              break;

            case IDE_GI_MARKUP_COLLECT_SCOPE:
              *(IdeGiScope *)ptr = IDE_GI_SCOPE_CALL;
              break;

            case IDE_GI_MARKUP_COLLECT_DIRECTION:
              *(IdeGiDirection *)ptr = IDE_GI_DIRECTION_IN;
              break;

            case IDE_GI_MARKUP_COLLECT_TRANSFER_OWNERSHIP:
              *(IdeGiTransferOwnership *)ptr = IDE_GI_TRANSFER_OWNERSHIP_NONE;
              break;

            case IDE_GI_MARKUP_COLLECT_SIGNAL_WHEN:
              *(IdeGiSignalWhen *)ptr = IDE_GI_SIGNAL_WHEN_FIRST;
              break;

            case IDE_GI_MARKUP_COLLECT_TRISTATE:
              *(gboolean *)ptr = -1;
              break;

            case IDE_GI_MARKUP_COLLECT_OFFSET32_STRING:
            case IDE_GI_MARKUP_COLLECT_OFFSET32_DOC_STRING:
              *(guint32 *)ptr = 0;
              break;

            default:
              break;
            }
        }

      type = va_arg (ap, IdeGiMarkupCollectType);
      def = va_arg (ap, const char *);
      attr = va_arg (ap, const char *);
    }

  va_end (ap);

  return FALSE;
}

void
ide_gi_helper_parsing_error_custom (IdeGiParserObject   *parser_object,
                                    GMarkupParseContext *context,
                                    GFile               *file,
                                    const gchar         *message)
{
  g_autofree gchar *filename = NULL;
  const gchar *element;
  const gchar *type_str;
  gint line;
  gint col;

  g_assert (IDE_IS_GI_PARSER_OBJECT (parser_object));
  g_assert (context != NULL);
  g_assert (G_IS_FILE (file));
  g_assert (message != NULL);

  g_markup_parse_context_get_position (context, &line, &col);
  element = g_markup_parse_context_get_element (context);
  filename = g_file_get_path (file);
  type_str = ide_gi_parser_object_get_element_type_string (parser_object);

  g_debug ("In type:%s '%s':<%s> in:%s at (%d:%d)",
            type_str,
            message,
            element,
            filename,
            line, col);
}

void
ide_gi_helper_parsing_error (IdeGiParserObject   *parser_object,
                             GMarkupParseContext *context,
                             GFile               *file)
{
  ide_gi_helper_parsing_error_custom (parser_object,
                                      context,
                                      file,
                                      "unhandled or Wrong end element");
}


