/* ide-gi-pool.c
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

#include "builder/ide-gi-alias-builder.h"
#include "builder/ide-gi-array-builder.h"
#include "builder/ide-gi-callback-builder.h"
#include "builder/ide-gi-class-builder.h"
#include "builder/ide-gi-constant-builder.h"
#include "builder/ide-gi-doc-builder.h"
#include "builder/ide-gi-enum-builder.h"
#include "builder/ide-gi-field-builder.h"
#include "builder/ide-gi-function-builder.h"
#include "builder/ide-gi-header-builder.h"
#include "builder/ide-gi-interface-builder.h"
#include "builder/ide-gi-member-builder.h"
#include "builder/ide-gi-parameters-builder.h"
#include "builder/ide-gi-property-builder.h"
#include "builder/ide-gi-record-builder.h"
#include "builder/ide-gi-signal-builder.h"
#include "builder/ide-gi-type-builder.h"
#include "builder/ide-gi-union-builder.h"

#include "ide-gi-pool.h"

typedef IdeGiParserObject * (*PoolBuilderFunc) (void);

typedef struct _PoolBuilderEntry
{
  guint32          max_objects;
  PoolBuilderFunc  func;
  const gchar     *name;
} PoolBuilderEntry;

static PoolBuilderEntry builder_entries[] = {
  { 0,  NULL,                          "unknow"             },

  { 10, ide_gi_alias_builder_new,      "alias"              },
  { 10, ide_gi_doc_builder_new,        "annotation"         },
  { 10, ide_gi_array_builder_new,      "array"              },
  { 10, NULL,                          "attributes"         },
  { 10, ide_gi_enum_builder_new,       "bitfield"           },
  { 10, ide_gi_callback_builder_new,   "callback"           },
  { 10, NULL,                          "c:include"          },
  { 10, ide_gi_class_builder_new,      "class"              },
  { 10, ide_gi_constant_builder_new,   "constant"           },
  { 10, ide_gi_function_builder_new,   "constructor"        },
  { 10, ide_gi_doc_builder_new,        "doc"                },
  { 10, ide_gi_doc_builder_new,        "doc-deprecated"     },
  { 10, ide_gi_doc_builder_new,        "doc-stability"      },
  { 10, ide_gi_doc_builder_new,        "doc-version"        },
  { 10, ide_gi_enum_builder_new,       "enumeration"        },
  { 10, ide_gi_field_builder_new,      "field"              },
  { 10, ide_gi_function_builder_new,   "function"           },
  { 10, ide_gi_record_builder_new,     "glib:boxed"         },
  { 10, ide_gi_signal_builder_new,     "glib:signal"        },
  { 10, NULL,                          "implements"         },
  { 10, NULL,                          "include"            },
  { 10, NULL,                          "instance-parameter" },
  { 10, ide_gi_interface_builder_new,  "interface"          },
  { 10, ide_gi_member_builder_new,     "member"             },
  { 10, ide_gi_function_builder_new,   "method"             },
  { 10, NULL,                          "namespace"          },
  { 10, NULL,                          "package"            },
  { 10, NULL,                          "parameter"          },
  { 10, ide_gi_parameters_builder_new, "parameters"         },
  { 10, NULL,                          "prerequisite"       },
  { 10, ide_gi_property_builder_new,   "property"           },
  { 10, ide_gi_record_builder_new,     "record"             },
  { 10, ide_gi_header_builder_new,     "repository"         },
  { 10, ide_gi_parameters_builder_new, "return-value"       },
  { 10, ide_gi_type_builder_new,       "type"               },
  { 10, ide_gi_union_builder_new,      "union"              },
  { 10, NULL,                          "varargs"            },
  { 10, ide_gi_function_builder_new,   "virtual-method"     },

  { 10, NULL,                          "last"               },
};

struct _IdeGiPool
{
  GObject parent_instance;

  GQueue *queue;
  GSList *builder_lists[G_N_ELEMENTS (builder_entries)];

  gchar  *unhandled_element;

  guint   reuse : 1;
  guint   thread_aware : 1;
};

G_DEFINE_TYPE (IdeGiPool, ide_gi_pool, G_TYPE_OBJECT)

static inline IdeGiParserObject *
create_object (guint index)
{
  PoolBuilderFunc func;

  g_assert (index != 0);

  func = builder_entries[index].func;
  if (func == NULL)
    {
      g_warning ("No builder for this element type: %s", builder_entries[index].name);
      return NULL;
    }

  return func();
}

IdeGiParserObject *
ide_gi_pool_get_object (IdeGiPool        *self,
                        IdeGiElementType  type)
{
  IdeGiParserObject *obj;
  guint index;

  g_return_val_if_fail (IDE_IS_GI_POOL (self), NULL);
  g_return_val_if_fail (__builtin_popcountll (type) == 1, NULL);

  index = __builtin_ffsll (type);

  if (!self->reuse)
    {
      obj = create_object (index);
    }
  else
    {
      GSList *list = self->builder_lists[index];

      if (list == NULL)
        obj = create_object (index);
      else
        {
          obj = list->data;
          ide_gi_parser_object_reset (obj);

          self->builder_lists[index] = g_slist_delete_link (list, list);
        }
    }

  g_queue_push_head (self->queue, obj);

  return obj;
}

gboolean
ide_gi_pool_release_object (IdeGiPool *self)
{
  IdeGiParserObject *obj;
  gboolean ret = TRUE;

  g_return_val_if_fail (IDE_IS_GI_POOL (self), FALSE);

  obj = g_queue_pop_head (self->queue);

  if (!self->reuse)
    g_object_unref (obj);
  else
    {
      GSList *list;
      IdeGiElementType type;
      guint index;

      type = ide_gi_parser_object_get_element_type (obj);
      g_assert (__builtin_popcountll (type) == 1);

      index = __builtin_ffsll (type);
      list = self->builder_lists[index];

      self->builder_lists[index] = g_slist_prepend (list, obj);
    }

  return ret;
}

IdeGiParserObject *
ide_gi_pool_get_current_parser_object (IdeGiPool *self)
{
  g_return_val_if_fail (IDE_IS_GI_POOL (self), NULL);

  return g_queue_peek_head (self->queue);
}

/* Be aware that some objects embed several parsers and
 * that the element type reported will be the main one.
 */
IdeGiParserObject *
ide_gi_pool_get_parent_parser_object (IdeGiPool *self)
{
  IdeGiParserObject  *object = NULL;

  g_return_val_if_fail (IDE_IS_GI_POOL (self), NULL);

  if (g_queue_get_length (self->queue) > 1)
    object = g_queue_peek_nth (self->queue, 1);

  return object;
}

void
ide_gi_pool_set_unhandled_element (IdeGiPool   *self,
                                   const gchar *element)
{
  g_return_if_fail (IDE_IS_GI_POOL (self));

  dzl_clear_pointer (&self->unhandled_element, g_free);
  if (element != NULL)
    self->unhandled_element = g_strdup (element);
}

const gchar *
ide_gi_pool_get_unhandled_element (IdeGiPool *self)
{
  g_return_val_if_fail (IDE_IS_GI_POOL (self), NULL);

  return self->unhandled_element;
}

IdeGiPool *
ide_gi_pool_new (gboolean reuse)
{
  IdeGiPool *self;

  self = g_object_new (IDE_TYPE_GI_POOL, NULL);

  self->reuse = reuse;
  return self;
}

static void
ide_gi_pool_finalize (GObject *object)
{
  IdeGiPool *self = (IdeGiPool *)object;

  dzl_clear_pointer (&self->unhandled_element, g_free);

  g_queue_free_full (self->queue, g_object_unref);
  self->queue = NULL;

  for (guint i = 0; i < G_N_ELEMENTS (builder_entries); i++)
    g_slist_free (self->builder_lists[i]);

  G_OBJECT_CLASS (ide_gi_pool_parent_class)->finalize (object);
}

static void
ide_gi_pool_class_init (IdeGiPoolClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_gi_pool_finalize;
}

static void
ide_gi_pool_init (IdeGiPool *self)
{
  self->queue = g_queue_new ();
}

const gchar *
_ide_gi_pool_get_element_type_string (IdeGiElementType type)
{
  guint index;

  g_assert (__builtin_popcountll (type) == 1);

  index = __builtin_ffsll (type);

  return builder_entries[index].name;
}
