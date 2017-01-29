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
#include "ide-xml-sax.h"
#include "ide-xml-tree-builder-generic.h"
#include "ide-xml-tree-builder-ui.h"

#include "ide-xml-tree-builder.h"

struct _IdeXmlTreeBuilder
{
  IdeObject parent_instance;
};

typedef struct{
  IdeXmlSax *parser;
  GBytes    *content;
  GFile     *file;
} BuilderState;

static void
builder_state_free (BuilderState *state)
{
  g_clear_object (&state->parser);
  g_clear_pointer (&state->content, g_bytes_unref);
  g_clear_object (&state->file);
}

G_DEFINE_TYPE (IdeXmlTreeBuilder, ide_xml_tree_builder, IDE_TYPE_OBJECT)

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

static gboolean
ide_xml_tree_builder_file_is_ui (GFile       *file,
                                 const gchar *data,
                                 gsize        size)
{
  g_autofree gchar *path;
  gboolean ret = FALSE;

  g_assert (G_IS_FILE (file));
  g_assert (data != NULL);
  g_assert (size > 0);

  path = g_file_get_path (file);
  if (g_str_has_suffix (path, ".ui") || g_str_has_suffix (path, ".glade"))
    {
      XmlReader *reader;

      reader = xml_reader_new ();
      xml_reader_load_from_data (reader, data, size, NULL, NULL);
      while (xml_reader_read (reader))
        {
          if (xml_reader_get_node_type (reader) == XML_READER_TYPE_ELEMENT)
            {
              if (ide_str_equal0 (xml_reader_get_name (reader), "interface"))
                ret = TRUE;

              break;
            }
        }

      g_object_unref (reader);
    }

  return ret;
}

static void
build_tree_worker (GTask        *task,
                   gpointer      source_object,
                   gpointer      task_data,
                   GCancellable *cancellable)
{
  IdeXmlTreeBuilder *self = (IdeXmlTreeBuilder *)source_object;
  BuilderState *state = (BuilderState *)task_data;
  IdeXmlSymbolNode *root_node = NULL;
  const gchar *data;
  g_autofree gchar *uri;
  gsize size;

  g_assert (IDE_IS_XML_TREE_BUILDER (self));
  g_assert (G_IS_TASK (task));
  g_assert (state != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  data = g_bytes_get_data (state->content, &size);
  uri = g_file_get_uri (state->file);

  if (ide_xml_tree_builder_file_is_ui (state->file,data, size))
    root_node = ide_xml_tree_builder_ui_create (state->parser, state->file, data, size);
  else
    root_node = ide_xml_tree_builder_generic_create (state->parser, state->file, data, size);

  if (root_node == NULL)
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
  state->parser = ide_xml_sax_new ();
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
