
/* ide-gi-parser-object.c
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

#define G_LOG_DOMAIN "ide-gi-parser-object"

#include "ide-gi-parser-result.h"
#include "ide-gi-pool.h"

#include "ide-gi-parser-object.h"

typedef struct
{
  IdeGiParserResult  *result;

  IdeGiElementType    type;

  guint               finished : 1;
} IdeGiParserObjectPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeGiParserObject, ide_gi_parser_object, G_TYPE_OBJECT)

gpointer
ide_gi_parser_object_finish (IdeGiParserObject *self)
{
  IdeGiParserObjectPrivate *priv = ide_gi_parser_object_get_instance_private (self);
  gpointer blob_ptr = NULL;

  g_return_val_if_fail (IDE_IS_GI_PARSER_OBJECT (self), NULL);

  if (priv->finished)
    {
      g_warning ("Parser object (%s) already finished", _ide_gi_pool_get_element_type_string (priv->type));
      return NULL;
    }

  if (IDE_GI_PARSER_OBJECT_GET_CLASS (self)->finish)
    {
      blob_ptr = IDE_GI_PARSER_OBJECT_GET_CLASS (self)->finish (self);
      priv->finished = TRUE;
    }
  else
    {
      g_warning ("Parser object (%s) doesn't have a finish method", _ide_gi_pool_get_element_type_string (priv->type));
      return NULL;
    }

  return blob_ptr;
}

void
ide_gi_parser_object_index (IdeGiParserObject *self,
                            IdeGiParserResult *result,
                            gpointer           user_data)
{
  g_return_if_fail (IDE_IS_GI_PARSER_OBJECT (self));
  g_return_if_fail (IDE_IS_GI_PARSER_RESULT (result));

  if (IDE_GI_PARSER_OBJECT_GET_CLASS (self)->index)
    IDE_GI_PARSER_OBJECT_GET_CLASS (self)->index (self, result, user_data);
}

gboolean
ide_gi_parser_object_parse (IdeGiParserObject    *self,
                            GMarkupParseContext  *context,
                            IdeGiParserResult    *result,
                            const gchar          *element_name,
                            const gchar         **attribute_names,
                            const gchar         **attribute_values,
                            GError              **error)
{
  g_return_val_if_fail (IDE_IS_GI_PARSER_OBJECT (self), FALSE);
  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (result), FALSE);
  g_return_val_if_fail (element_name != NULL, FALSE);
  g_return_val_if_fail (attribute_names != NULL, FALSE);
  g_return_val_if_fail (attribute_values != NULL, FALSE);

  if (IDE_GI_PARSER_OBJECT_GET_CLASS (self)->parse)
    return IDE_GI_PARSER_OBJECT_GET_CLASS (self)->parse (self,
                                                         context,
                                                         result,
                                                         element_name,
                                                         attribute_names,
                                                         attribute_values,
                                                         error);

  return TRUE;
}

void
ide_gi_parser_object_reset (IdeGiParserObject *self)
{
  IdeGiParserObjectPrivate *priv = ide_gi_parser_object_get_instance_private (self);

  g_return_if_fail (IDE_IS_GI_PARSER_OBJECT (self));

  priv->finished = FALSE;
  priv->result = NULL;

  if (IDE_GI_PARSER_OBJECT_GET_CLASS (self)->reset)
    return IDE_GI_PARSER_OBJECT_GET_CLASS (self)->reset (self);
}

IdeGiElementType
ide_gi_parser_object_get_element_type (IdeGiParserObject *self)
{
  IdeGiParserObjectPrivate *priv = ide_gi_parser_object_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_GI_PARSER_OBJECT (self), IDE_GI_ELEMENT_TYPE_UNKNOW);

  return priv->type;
}

const gchar *
ide_gi_parser_object_get_element_type_string (IdeGiParserObject *self)
{
  IdeGiParserObjectPrivate *priv = ide_gi_parser_object_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_GI_PARSER_OBJECT (self), NULL);

  return _ide_gi_pool_get_element_type_string (priv->type);
}

void
_ide_gi_parser_object_set_element_type (IdeGiParserObject *self,
                                        IdeGiElementType   type)
{
  IdeGiParserObjectPrivate *priv = ide_gi_parser_object_get_instance_private (self);

  g_return_if_fail (IDE_IS_GI_PARSER_OBJECT (self));
  g_return_if_fail (type != IDE_GI_ELEMENT_TYPE_UNKNOW);

  priv->type = type;
}

IdeGiParserResult *
ide_gi_parser_object_get_result (IdeGiParserObject *self)
{
  IdeGiParserObjectPrivate *priv = ide_gi_parser_object_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_GI_PARSER_OBJECT (self), NULL);
  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (priv->result) || priv->result == NULL, NULL);

  return priv->result;
}

void
ide_gi_parser_object_set_result (IdeGiParserObject *self,
                                 IdeGiParserResult *result)
{
  IdeGiParserObjectPrivate *priv = ide_gi_parser_object_get_instance_private (self);

  g_return_if_fail (IDE_IS_GI_PARSER_OBJECT (self));
  g_return_if_fail (IDE_IS_GI_PARSER_RESULT (priv->result) || priv->result == NULL);

  priv->result = result;
}

IdeGiParserObject *
ide_gi_parser_object_new (void)
{
  return g_object_new (IDE_TYPE_GI_PARSER_OBJECT, NULL);
}

static void
ide_gi_parser_object_finalize (GObject *object)
{
  G_OBJECT_CLASS (ide_gi_parser_object_parent_class)->finalize (object);
}

static void
ide_gi_parser_object_class_init (IdeGiParserObjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_gi_parser_object_finalize;
}

static void
ide_gi_parser_object_init (IdeGiParserObject *self)
{
}
