/* ide-code-index-entry.c
 *
 * Copyright © 2017 Anoop Chandu <anoopchandu96@gmail.com>
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

#define G_LOG_DOMAIN "ide-code-index-entry"

#include "symbols/ide-code-index-entry.h"

typedef struct
{
  GObject                 parent;

  gchar                  *key;
  gchar                  *name;

  IdeSymbolKind           kind;
  IdeSymbolFlags          flags;

  guint                   begin_line;
  guint                   begin_line_offset;
  guint                   end_line;
  guint                   end_line_offset;
} IdeCodeIndexEntryPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeCodeIndexEntry, ide_code_index_entry, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_KEY,
  PROP_NAME,
  PROP_KIND,
  PROP_FLAGS,
  PROP_BEGIN_LINE,
  PROP_BEGIN_LINE_OFFSET,
  PROP_END_LINE,
  PROP_END_LINE_OFFSET,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_code_index_entry_finalize (GObject *object)
{
  IdeCodeIndexEntry *self = (IdeCodeIndexEntry *)object;
  IdeCodeIndexEntryPrivate *priv = ide_code_index_entry_get_instance_private (self);

  g_clear_pointer (&priv->name, g_free);
  g_clear_pointer (&priv->key, g_free);

  G_OBJECT_CLASS (ide_code_index_entry_parent_class)->finalize (object);
}

static void
ide_code_index_entry_set_property (GObject       *object,
                                   guint          prop_id,
                                   const GValue  *value,
                                   GParamSpec    *pspec)
{
  IdeCodeIndexEntry *self = (IdeCodeIndexEntry *)object;
  IdeCodeIndexEntryPrivate *priv = ide_code_index_entry_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_KEY:
      priv->key = g_strdup (g_value_get_string (value));
      break;
    case PROP_NAME:
      priv->name = g_strdup (g_value_get_string (value));
      break;
    case PROP_KIND:
      priv->kind = g_value_get_int (value);
      break;
    case PROP_FLAGS:
      priv->flags = g_value_get_int (value);
      break;
    case PROP_BEGIN_LINE:
      priv->begin_line = g_value_get_uint (value);
      break;
    case PROP_BEGIN_LINE_OFFSET:
      priv->begin_line_offset = g_value_get_uint (value);
      break;
    case PROP_END_LINE:
      priv->end_line = g_value_get_uint (value);
      break;
    case PROP_END_LINE_OFFSET:
      priv->end_line_offset = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_code_index_entry_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  IdeCodeIndexEntry *self = (IdeCodeIndexEntry *)object;
  IdeCodeIndexEntryPrivate *priv = ide_code_index_entry_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_KEY:
      g_value_set_string (value, priv->key);
      break;
    case PROP_NAME:
      g_value_set_string (value, priv->name);
      break;
    case PROP_KIND:
      g_value_set_int (value, priv->kind);
      break;
    case PROP_FLAGS:
      g_value_set_int (value, priv->flags);
      break;
    case PROP_BEGIN_LINE:
      g_value_set_uint (value, priv->begin_line);
      break;
    case PROP_BEGIN_LINE_OFFSET:
      g_value_set_uint (value, priv->begin_line_offset);
      break;
    case PROP_END_LINE:
      g_value_set_uint (value, priv->end_line);
      break;
    case PROP_END_LINE_OFFSET:
      g_value_set_uint (value, priv->end_line_offset);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_code_index_entry_class_init (IdeCodeIndexEntryClass *klass)
{
  GObjectClass *object_class = (GObjectClass *)klass;

  object_class->finalize = ide_code_index_entry_finalize;
  object_class->set_property = ide_code_index_entry_set_property;
  object_class->get_property = ide_code_index_entry_get_property;

  properties [PROP_KEY] =
    g_param_spec_string ("key",
                         "Key",
                         "A key unique to declaration.",
                         NULL,
                         (G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  properties [PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "Name of declaration.",
                         NULL,
                         (G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  properties [PROP_KIND] =
    g_param_spec_int ("kind",
                      "Kind",
                      "Kind of declaration.",
                       G_MININT, G_MAXINT, IDE_SYMBOL_NONE,
                       (G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  properties [PROP_FLAGS] =
    g_param_spec_int ("flags",
                      "Flags",
                      "Flags of declaration.",
                       G_MININT, G_MAXINT, IDE_SYMBOL_FLAGS_NONE,
                       (G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  properties [PROP_BEGIN_LINE] =
    g_param_spec_uint ("begin-line",
                       "Begin Line",
                       "Begin Line of declaration.",
                       0, G_MAXUINT, 0,
                       (G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  properties [PROP_BEGIN_LINE_OFFSET] =
    g_param_spec_uint ("begin-line-offset",
                       "Begin Line Offset",
                       "Begin Line Offset of declaration.",
                       0, G_MAXUINT, 0,
                       (G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  properties [PROP_END_LINE] =
    g_param_spec_uint ("end-line",
                       "End Line",
                       "End Line of declaration.",
                       0, G_MAXUINT, 0,
                       (G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  properties [PROP_END_LINE_OFFSET] =
    g_param_spec_uint ("end-line-offset",
                       "End Line Offset",
                       "End Line Offset of declaration.",
                       0, G_MAXUINT, 0,
                       (G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_code_index_entry_init (IdeCodeIndexEntry *self)
{
}

gchar *
ide_code_index_entry_get_key (IdeCodeIndexEntry *self)
{
  IdeCodeIndexEntryPrivate *priv;

  g_return_val_if_fail (IDE_IS_CODE_INDEX_ENTRY (self), NULL);

  priv = ide_code_index_entry_get_instance_private (self);
  return priv->key;
}

gchar *
ide_code_index_entry_get_name (IdeCodeIndexEntry *self)
{
  IdeCodeIndexEntryPrivate *priv;

  g_return_val_if_fail (IDE_IS_CODE_INDEX_ENTRY (self), NULL);

  priv = ide_code_index_entry_get_instance_private (self);
  return priv->name;
}

IdeSymbolKind
ide_code_index_entry_get_kind (IdeCodeIndexEntry *self)
{
  IdeCodeIndexEntryPrivate *priv;

  g_return_val_if_fail (IDE_IS_CODE_INDEX_ENTRY (self), IDE_SYMBOL_NONE);

  priv = ide_code_index_entry_get_instance_private (self);
  return priv->kind;
}

IdeSymbolFlags
ide_code_index_entry_get_flags (IdeCodeIndexEntry *self)
{
  IdeCodeIndexEntryPrivate *priv;

  g_return_val_if_fail (IDE_IS_CODE_INDEX_ENTRY (self), IDE_SYMBOL_FLAGS_NONE);

  priv = ide_code_index_entry_get_instance_private (self);
  return priv->flags;
}

void
ide_code_index_entry_get_range (IdeCodeIndexEntry *self,
                                guint             *begin_line,
                                guint             *begin_line_offset,
                                guint             *end_line,
                                guint             *end_line_offset)
{
  IdeCodeIndexEntryPrivate *priv;

  g_return_if_fail (IDE_IS_CODE_INDEX_ENTRY (self));

  priv = ide_code_index_entry_get_instance_private (self);

  if (begin_line != NULL)
    *begin_line = priv->begin_line;

  if (begin_line_offset != NULL)
    *begin_line_offset = priv->begin_line_offset;

  if (end_line != NULL)
    *end_line = priv->end_line;

  if (end_line_offset != NULL)
    *end_line_offset = priv->end_line_offset;
}
