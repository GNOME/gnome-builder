/* ide-xml-service.c
 *
 * Copyright (C) 2017 Sébastien Lafargue <slafargue@gnome.org>
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

#define G_LOG_DOMAIN "ide-xml-service"

#include <egg-task-cache.h>
#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>
#include <math.h>

#include "ide-xml-service.h"
#include "xml-reader.h"

gboolean _ide_buffer_get_loading (IdeBuffer *self);

#define DEFAULT_EVICTION_MSEC (60 * 1000)

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

static void service_iface_init (IdeServiceInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (IdeXmlService, ide_xml_service, IDE_TYPE_OBJECT, 0,
                                G_IMPLEMENT_INTERFACE (IDE_TYPE_SERVICE, service_iface_init))

static IdeXmlSymbolNode *
create_node_from_reader (XmlReader *reader)
{
  const gchar *name;
  GFile *file = NULL;
  guint line = 0;
  guint line_offset = 0;

  name = xml_reader_get_name (reader);

  return ide_xml_symbol_node_new (name, file, line, line_offset);
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
ide_xml_service_walk_tree (IdeXmlService    *self,
                           XmlReader        *reader)
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

  g_assert (IDE_IS_XML_SERVICE (self));
  g_assert (XML_IS_READER (reader));

  stack = stack_new ();

  parent_node = root_node = ide_xml_symbol_node_new ("root", NULL, 0, 0);
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

static IdeXmlSymbolNode *
ide_xml_service_build_tree (IdeXmlService *self,
                            GBytes        *content,
                            GFile         *file)
{
  IdeXmlSymbolNode *root_node;
  XmlReader *reader;
  const gchar *data;
  g_autofree gchar *uri;
  gsize size;

  g_assert (IDE_IS_XML_SERVICE (self));

  data = g_bytes_get_data (content, &size);
  uri = g_file_get_uri (file);
  reader = xml_reader_new ();
  xml_reader_load_from_data (reader, data, size, uri, NULL);
  root_node = ide_xml_service_walk_tree (self, reader);

  g_object_unref (reader);

  return root_node;
}

static GBytes *
ide_xml_service_get_file_content (IdeXmlService *self,
                                  GFile         *file)
{
  IdeContext *context;
  IdeBufferManager *manager;
  IdeBuffer *buffer;
  GBytes *content = NULL;

  g_assert (IDE_IS_XML_SERVICE (self));
  g_assert (G_IS_FILE (file));

  context = ide_object_get_context (IDE_OBJECT (self));
  manager = ide_context_get_buffer_manager (context);
  if (NULL != (buffer = ide_buffer_manager_find_buffer (manager, file)))
    content = ide_buffer_get_content (buffer);

  return content;
}

static void
ide_xml_service_build_tree_cb (EggTaskCache  *cache,
                               gconstpointer  key,
                               GTask         *task,
                               gpointer       user_data)
{
  IdeXmlService *self = user_data;
  g_autofree gchar *path = NULL;
  IdeContext *context;
  IdeFile *ifile = (IdeFile *)key;
  GFile *gfile;
  IdeXmlSymbolNode *root_node;
  GBytes *content = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_XML_SERVICE (self));
  g_assert (key != NULL);
  g_assert (IDE_IS_FILE ((IdeFile *)key));
  g_assert (G_IS_TASK (task));

  gfile = ide_file_get_file (ifile);
  context = ide_object_get_context (IDE_OBJECT (self));

  if (!gfile || !(path = g_file_get_path (gfile)))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               _("File must be saved localy to parse."));
      return;
    }

  printf ("tree path:%s\n", path);
  if (NULL != (content = ide_xml_service_get_file_content (self, gfile)))
    {
      root_node = ide_xml_service_build_tree (self, content, gfile);
      g_task_return_pointer (task, root_node, g_object_unref);
    }
  else
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               _("Failed to create xml tree."));
      return;
    }

  IDE_EXIT;
}

static void
ide_xml_service_buffer_saved (IdeXmlService    *self,
                              IdeBuffer        *buffer,
                              IdeBufferManager *buffer_manager)
{
  IDE_ENTRY;

  g_assert (IDE_IS_XML_SERVICE (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));

  printf ("buffer saved:%p\n", buffer);

  IDE_EXIT;
}

static void
ide_xml_service_get_root_node_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  EggTaskCache *cache = (EggTaskCache *)object;
  g_autoptr(IdeXmlSymbolNode) ret = NULL;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  g_assert (EGG_IS_TASK_CACHE (cache));

  if (!(ret = egg_task_cache_get_finish (cache, result, &error)))
    g_task_return_error (task, error);
  else
    {
      printf ("new tree:%p\n", ret);
    g_task_return_pointer (task, g_steal_pointer (&ret), g_object_unref);
    }
}

typedef struct
{
  IdeXmlService *self;
  GTask         *task;
  GCancellable  *cancellable;
  IdeFile       *ifile;
  IdeBuffer     *buffer;
} TaskState;

static void
ide_xml_service__buffer_loaded_cb (IdeBuffer *buffer,
                                   TaskState *state)
{
  IdeXmlService *self = (IdeXmlService *)state->self;
  g_autoptr(GTask) task = state->task;

  g_assert (IDE_IS_XML_SERVICE (self));
  g_assert (G_IS_TASK (state->task));
  g_assert (state->cancellable == NULL || G_IS_CANCELLABLE (state->cancellable));
  g_assert (IDE_IS_FILE (state->ifile));
  g_assert (IDE_IS_BUFFER (state->buffer));

  printf ("buffer loaded\n");

  egg_task_cache_get_async (self->trees,
                            state->ifile,
                            TRUE,
                            state->cancellable,
                            ide_xml_service_get_root_node_cb,
                            g_steal_pointer (&task));

  g_object_unref (state->buffer);
  g_object_unref (state->ifile);
  g_slice_free (TaskState, state);
}

/**
 * ide_xml_service_get_root_node_async:
 *
 * This function is used to asynchronously retrieve the root node for
 * a particular file.
 *
 * If the root node is up to date, then no parsing will occur and the
 * existing root node will be used.
 *
 * If the root node is out of date, then the source file(s) will be
 * parsed asynchronously.
 *
 * The xml service is meant to be used with buffers, that is,
 * by extension, loaded views.
 */
void
ide_xml_service_get_root_node_async (IdeXmlService       *self,
                                     IdeFile             *file,
                                     IdeBuffer           *buffer,
                                     gint64               min_serial,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  IdeXmlSymbolNode *cached;
  g_autoptr(GTask) task = NULL;
  IdeContext *context;
  IdeBufferManager *manager;
  GFile *gfile;

  g_return_if_fail (IDE_IS_XML_SERVICE (self));
  g_return_if_fail (IDE_IS_FILE (file));
  g_return_if_fail (IDE_IS_BUFFER (buffer));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  context = ide_object_get_context (IDE_OBJECT (self));

  if (min_serial == 0)
    {
      IdeUnsavedFiles *unsaved_files;

      unsaved_files = ide_context_get_unsaved_files (context);
      min_serial = ide_unsaved_files_get_sequence (unsaved_files);
    }

  /*
   * If we have a cached unit, and it is new enough, then re-use it.
   */
  if ((cached = egg_task_cache_peek (self->trees, file)) &&
      (ide_xml_symbol_node_get_serial (cached) >= min_serial))
    {
      printf ("get cached tree:%p\n", cached);
      g_task_return_pointer (task, g_object_ref (cached), g_object_unref);
      return;
    }

  printf ("egg_task_cache_get_async\n");
  manager = ide_context_get_buffer_manager (context);
  gfile = ide_file_get_file (file);
  if (!ide_buffer_manager_has_file (manager, gfile))
    {
      TaskState *state;

      if (!_ide_buffer_get_loading (buffer))
        {
          g_task_return_new_error (task,
                                   G_IO_ERROR,
                                   G_IO_ERROR_NOT_SUPPORTED,
                                   _("Buffer loaded but not in buffer manager."));
          return;
        }

      /* Wait for the buffer to be fully loaded */
      state = g_slice_new0 (TaskState);
      state->self = self;
      state->task = g_object_ref (task);
      state->cancellable = cancellable;
      state->ifile = g_object_ref (file);
      state->buffer = g_object_ref (buffer);

      g_signal_connect (buffer,
                        "loaded",
                        G_CALLBACK (ide_xml_service__buffer_loaded_cb),
                        state);
    }
  else
    egg_task_cache_get_async (self->trees,
                              file,
                              TRUE,
                              cancellable,
                              ide_xml_service_get_root_node_cb,
                              g_object_ref (task));
}

/**
 * ide_xml_service_get_root_node_finish:
 *
 * Completes an asychronous request to get a root node for a given file.
 * See ide_xml_service_get_root_node_async() for more information.
 *
 * Returns: (transfer full): An #IdeXmlSymbolNode or %NULL up on failure.
 */
IdeXmlSymbolNode *
ide_xml_service_get_root_node_finish (IdeXmlService  *self,
                                      GAsyncResult   *result,
                                      GError        **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_XML_SERVICE (self), NULL);

  return g_task_propagate_pointer (task, error);
}

static void
ide_xml_service_context_loaded (IdeService *service)
{
  IdeBufferManager *buffer_manager;
  IdeXmlService *self = (IdeXmlService *)service;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_XML_SERVICE (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  buffer_manager = ide_context_get_buffer_manager (context);

  g_signal_connect_object (buffer_manager,
                           "buffer-saved",
                           G_CALLBACK (ide_xml_service_buffer_saved),
                           self,
                           G_CONNECT_SWAPPED);

  IDE_EXIT;
}

static void
ide_xml_service_start (IdeService *service)
{
  IdeXmlService *self = (IdeXmlService *)service;

  g_return_if_fail (IDE_IS_XML_SERVICE (self));

  self->trees = egg_task_cache_new ((GHashFunc)ide_file_hash,
                                    (GEqualFunc)ide_file_equal,
                                    g_object_ref,
                                    g_object_unref,
                                    g_object_ref,
                                    g_object_unref,
                                    DEFAULT_EVICTION_MSEC,
                                    ide_xml_service_build_tree_cb,
                                    self,
                                    NULL);

  egg_task_cache_set_name (self->trees, "xml trees cache");
}

static void
ide_xml_service_stop (IdeService *service)
{
  IdeXmlService *self = (IdeXmlService *)service;

  g_return_if_fail (IDE_IS_XML_SERVICE (self));

  if (self->cancellable && !g_cancellable_is_cancelled (self->cancellable))
    g_cancellable_cancel (self->cancellable);

  g_clear_object (&self->cancellable);
  g_clear_object (&self->trees);
}

static void
ide_xml_service_finalize (GObject *object)
{
  IdeXmlService *self = (IdeXmlService *)object;

  IDE_ENTRY;

  g_clear_object (&self->trees);
  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (ide_xml_service_parent_class)->finalize (object);

  IDE_EXIT;
}

static void
ide_xml_service_class_init (IdeXmlServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_xml_service_finalize;
}

static void
service_iface_init (IdeServiceInterface *iface)
{
  iface->context_loaded = ide_xml_service_context_loaded;
  iface->start = ide_xml_service_start;
  iface->stop = ide_xml_service_stop;
}

static void
ide_xml_service_class_finalize (IdeXmlServiceClass *klass)
{
}

static void
ide_xml_service_init (IdeXmlService *self)
{
}

void
_ide_xml_service_register_type (GTypeModule *module)
{
  ide_xml_service_register_type (module);
}

/**
 * ide_xml_service_get_cached_root_node:
 *
 * Gets the #IdeXmlSymbolNode root node for the corresponding file.
 *
 * Returns: (transfer NULL): A xml symbol node.
 */
IdeXmlSymbolNode *
ide_xml_service_get_cached_root_node (IdeXmlService *self,
                                      GFile         *file)
{
  IdeXmlSymbolNode *cached;

  g_return_val_if_fail (IDE_IS_XML_SERVICE (self), NULL);
  g_return_val_if_fail (IDE_IS_FILE (file), NULL);

  cached = egg_task_cache_peek (self->trees, file);

  return cached ? g_object_ref (cached) : NULL;
}
