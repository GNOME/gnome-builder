/* ide-xml-tree-builder.c
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

#include <egg-task-cache.h>
#include <glib/gi18n.h>

#include "xml-reader.h"

#include "ide-xml-tree-builder.h"

typedef GPtrArray IdeXmlStack;

static inline IdeXmlStack *
stack_new (void)
{
  return g_ptr_array_new ();
}

static inline void
stack_push (IdeXmlStack *stack,
            gpointer     ptr)
{
  g_ptr_array_add (stack, ptr);
}

static inline gpointer
stack_pop (IdeXmlStack *stack)
{
  gint end = stack->len - 1;

  return (end < 0) ? NULL : g_ptr_array_remove_index_fast (stack, end);
}

static inline gint
stack_is_empty (IdeXmlStack *stack)
{
  return (stack->len == 0);
}

static inline void
stack_destroy (IdeXmlStack *stack)
{
  g_ptr_array_unref (stack);
}

static inline gsize
stack_get_size (IdeXmlStack *stack)
{
  return stack->len;
}

struct _IdeXmlService
{
  IdeObject         parent_instance;

  EggTaskCache     *trees;
  GCancellable     *cancellable;
};
struct _IdeXmlTreeBuilder
{
  IdeObject parent_instance;
};

typedef struct{
  XmlReader *reader;
  GBytes    *content;
  GFile     *file;
} BuilderState;

static void
builder_state_free (BuilderState *state)
{
  g_clear_object (&state->reader);
  g_clear_pointer (&state->content, g_bytes_unref);
  g_clear_object (&state->file);
}

G_DEFINE_TYPE (IdeXmlTreeBuilder, ide_xml_tree_builder, IDE_TYPE_OBJECT)

static IdeXmlSymbolNode *
create_node_from_reader (XmlReader *reader)
{
  const gchar *name;
  GFile *file = NULL;
  guint line = 0;
  guint line_offset = 0;

  name = xml_reader_get_name (reader);

  return ide_xml_symbol_node_new (name, IDE_SYMBOL_UI_OBJECT,
                                  file, line, line_offset);
}

static void
print_node (IdeXmlSymbolNode *node,
            guint             depth)
{
  g_autofree gchar *spacer;

  spacer = g_strnfill (depth, '\t');
  printf ("%s%s (%i)\n",
          spacer,
          ide_symbol_node_get_name (IDE_SYMBOL_NODE (node)),
          depth);
}

static IdeXmlSymbolNode *
ide_xml_service_walk_tree (IdeXmlTreeBuilder  *self,
                           XmlReader          *reader)
{
  IdeXmlStack *stack;
  IdeXmlSymbolNode *root_node;
  IdeXmlSymbolNode *parent_node;
  IdeXmlSymbolNode *current_node;
  IdeXmlSymbolNode *previous_node = NULL;
  xmlReaderTypes type;
  gint depth = 0;
  gint current_depth = 0;
  gboolean is_empty;

  g_assert (IDE_IS_XML_TREE_BUILDER (self));
  g_assert (XML_IS_READER (reader));

  stack = stack_new ();

  parent_node = root_node = ide_xml_symbol_node_new ("root", IDE_SYMBOL_NONE,
                                                     NULL, 0, 0);
  stack_push (stack, parent_node);

  while (xml_reader_read (reader))
    {
      type = xml_reader_get_node_type (reader);
      if ( type == XML_READER_TYPE_ELEMENT)
        {
          current_node = create_node_from_reader (reader);
          depth = xml_reader_get_depth (reader);
          is_empty = xml_reader_is_empty_element (reader);

          /* TODO: take end elements into account and use:
           * || ABS (depth - current_depth) > 1
           */
          if (depth < 0)
            {
              g_warning ("Wrong xml element depth, current:%i new:%i\n", current_depth, depth);
              break;
            }

          if (depth > current_depth)
            {
              ++current_depth;
              stack_push (stack, parent_node);

              g_assert (previous_node != NULL);
              parent_node = previous_node;
              ide_xml_symbol_node_take_child (parent_node, current_node);
            }
          else if (depth < current_depth)
            {
              --current_depth;
              parent_node = stack_pop (stack);
              if (parent_node == NULL)
                {
                  g_warning ("Xml nodes stack empty\n");
                  break;
                }

              g_assert (parent_node != NULL);
              ide_xml_symbol_node_take_child (parent_node, current_node);
            }
          else
            {
              ide_xml_symbol_node_take_child (parent_node, current_node);
            }

          previous_node = current_node;
          print_node (current_node, depth);
        }
    }

  printf ("stack size:%li\n", stack_get_size (stack));

  stack_destroy (stack);

  return root_node;
}

static GBytes *
ide_xml_service_get_file_content (IdeXmlTreeBuilder *self,
                                  GFile             *file)
{
  IdeContext *context;
  IdeBufferManager *manager;
  IdeBuffer *buffer;
  GBytes *content = NULL;

  g_assert (IDE_IS_XML_TREE_BUILDER (self));
  g_assert (G_IS_FILE (file));

  context = ide_object_get_context (IDE_OBJECT (self));
  manager = ide_context_get_buffer_manager (context);

  printf ("found buffer:%p\n", ide_buffer_manager_find_buffer (manager, file));

  if (NULL != (buffer = ide_buffer_manager_find_buffer (manager, file)))
    content = ide_buffer_get_content (buffer);

  return content;
}

static void
build_tree_worker (GTask        *task,
                   gpointer      source_object,
                   gpointer      task_data,
                   GCancellable *cancellable)
{
  IdeXmlTreeBuilder *self = (IdeXmlTreeBuilder *)source_object;
  BuilderState *state = (BuilderState *)task_data;
  IdeXmlSymbolNode *root_node;
  const gchar *data;
  g_autofree gchar *uri;
  gsize size;

  g_assert (IDE_IS_XML_TREE_BUILDER (self));
  g_assert (G_IS_TASK (task));
  g_assert (state != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  printf ("build_tree_worker thread\n");

  data = g_bytes_get_data (state->content, &size);
  uri = g_file_get_uri (state->file);
  xml_reader_load_from_data (state->reader, data, size, uri, NULL);

  if (NULL == (root_node = ide_xml_service_walk_tree (self, state->reader)))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               _("Failed to create xml tree."));
      return;
    }

  g_task_return_pointer (task, root_node, g_object_unref);
}

void
ide_xml_tree_builder_build_tree_async (IdeXmlTreeBuilder   *self,
                                       GFile               *file,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  BuilderState *state;
  GBytes *content = NULL;

  g_return_if_fail (IDE_IS_XML_TREE_BUILDER (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_xml_tree_builder_build_tree_async);

  if (NULL == (content = ide_xml_service_get_file_content (self, file)))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               _("Failed to create xml tree."));
      return;
    }

  state = g_slice_new0 (BuilderState);
  state->reader = xml_reader_new ();
  state->content = g_bytes_ref (content);
  state->file = g_object_ref (file);

  g_task_set_task_data (task, state, (GDestroyNotify)builder_state_free);
  g_task_run_in_thread (task, build_tree_worker);
}

IdeXmlSymbolNode *
ide_xml_tree_builder_build_tree_finish (IdeXmlTreeBuilder  *self,
                                        GAsyncResult       *result,
                                        GError            **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_XML_TREE_BUILDER (self), NULL);
  g_return_val_if_fail (G_IS_TASK (task), NULL);

  return g_task_propagate_pointer (task, error);
}

IdeXmlTreeBuilder *
ide_xml_tree_builder_new (void)
{
  return g_object_new (IDE_TYPE_XML_TREE_BUILDER, NULL);
}

static void
ide_xml_tree_builder_finalize (GObject *object)
{
  G_OBJECT_CLASS (ide_xml_tree_builder_parent_class)->finalize (object);
}

static void
ide_xml_tree_builder_class_init (IdeXmlTreeBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_xml_tree_builder_finalize;
}

static void
ide_xml_tree_builder_init (IdeXmlTreeBuilder *self)
{
}
