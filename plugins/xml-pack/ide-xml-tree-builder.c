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

#include <dazzle.h>
#include <glib/gi18n.h>
#include <string.h>

#include "ide-xml-sax.h"
#include "ide-xml-tree-builder-generic.h"
#include "ide-xml-tree-builder-ui.h"

#include "ide-xml-tree-builder.h"

typedef struct _ColorTag
{
  gchar *name;
  gchar *fg;
  gchar *bg;
} ColorTag;

static void
color_tag_free (gpointer *data)
{
  ColorTag *tag = (ColorTag *)data;

  g_free (tag->name);
  g_free (tag->fg);
  g_free (tag->bg);
}

/* Keep it in sync with ColorTagId enum */
static ColorTag default_color_tags [] =
{
  { "label",       "#000000", "#D5E7FC" }, // COLOR_TAG_LABEL
  { "id",          "#000000", "#D9E7BD" }, // COLOR_TAG_ID
  { "style-class", "#000000", "#DFCD9B" }, // COLOR_TAG_STYLE_CLASS
  { "type",        "#000000", "#F4DAC3" }, // COLOR_TAG_TYPE
  { "parent",      "#000000", "#DEBECF" }, // COLOR_TAG_PARENT
  { "class",       "#000000", "#FFEF98" }, // COLOR_TAG_CLASS
  { "attribute",   "#000000", "#F0E68C" }, // COLOR_TAG_ATTRIBUTE
  { NULL },
};

struct _IdeXmlTreeBuilder
{
  IdeObject  parent_instance;

  GSettings *settings;
  GArray    *color_tags;
};

typedef struct{
  IdeXmlSax *parser;
  GBytes    *content;
  GFile     *file;
  gint64     sequence;
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
ide_xml_tree_builder_get_file_content (IdeXmlTreeBuilder *self,
                                       GFile             *file,
                                       gint64            *sequence)
{
  IdeContext *context;
  IdeBufferManager *manager;
  IdeUnsavedFiles *unsaved_files;
  IdeUnsavedFile *uf;
  IdeBuffer *buffer;
  gint64 sequence_tmp = -1;
  GBytes *content = NULL;

  g_assert (IDE_IS_XML_TREE_BUILDER (self));
  g_assert (G_IS_FILE (file));

  context = ide_object_get_context (IDE_OBJECT (self));
  manager = ide_context_get_buffer_manager (context);

  if (NULL != (buffer = ide_buffer_manager_find_buffer (manager, file)))
    {
      content = ide_buffer_get_content (buffer);

      unsaved_files = ide_context_get_unsaved_files (context);
      if (NULL != (uf = ide_unsaved_files_get_unsaved_file (unsaved_files, file)))
        sequence_tmp = ide_unsaved_file_get_sequence (uf);
    }

  if (sequence != NULL)
    *sequence = sequence_tmp;

  return content;
}

static gboolean
ide_xml_tree_builder_file_is_ui (GFile       *file,
                                 const gchar *data,
                                 gsize        size)
{
  g_autofree gchar *path = NULL;
  g_autofree gchar *buffer = NULL;
  gsize buffer_size;

  g_assert (G_IS_FILE (file));
  g_assert (data != NULL);
  g_assert (size > 0);

  path = g_file_get_path (file);
  if (g_str_has_suffix (path, ".ui") || g_str_has_suffix (path, ".glade"))
    {
      buffer_size = (size < 256) ? size : 256;
      buffer = g_strndup (data, buffer_size);
      if (NULL != (strstr (buffer, "<interface>")))
        return TRUE;
    }

  return FALSE;
}

static void
build_tree_worker (GTask        *task,
                   gpointer      source_object,
                   gpointer      task_data,
                   GCancellable *cancellable)
{
  IdeXmlTreeBuilder *self = (IdeXmlTreeBuilder *)source_object;
  BuilderState *state = (BuilderState *)task_data;
  IdeXmlAnalysis *analysis = NULL;
  const gchar *data;
  gsize size;

  g_assert (IDE_IS_XML_TREE_BUILDER (self));
  g_assert (G_IS_TASK (task));
  g_assert (state != NULL);
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  data = g_bytes_get_data (state->content, &size);

  if (ide_xml_tree_builder_file_is_ui (state->file, data, size))
    analysis = ide_xml_tree_builder_ui_create (self, state->parser, state->file, data, size);
  else
    analysis = ide_xml_tree_builder_generic_create (self, state->parser, state->file, data, size);

  if (analysis == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               _("Failed to create the XML tree."));
      return;
    }

  ide_xml_analysis_set_sequence (analysis, state->sequence);
  g_task_return_pointer (task, analysis, (GDestroyNotify)ide_xml_analysis_unref);
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
  gint64 sequence;

  g_return_if_fail (IDE_IS_XML_TREE_BUILDER (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_xml_tree_builder_build_tree_async);

  if (NULL == (content = ide_xml_tree_builder_get_file_content (self, file, &sequence)))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               _("Failed to create the XML tree."));
      return;
    }

  state = g_slice_new0 (BuilderState);
  state->parser = ide_xml_sax_new ();
  state->content = content;
  state->file = g_object_ref (file);
  state->sequence = sequence;

  g_task_set_task_data (task, state, (GDestroyNotify)builder_state_free);
  g_task_run_in_thread (task, build_tree_worker);
}

IdeXmlAnalysis *
ide_xml_tree_builder_build_tree_finish (IdeXmlTreeBuilder  *self,
                                        GAsyncResult       *result,
                                        GError            **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_XML_TREE_BUILDER (self), NULL);
  g_return_val_if_fail (G_IS_TASK (task), NULL);

  return g_task_propagate_pointer (task, error);
}

gchar *
ide_xml_tree_builder_get_color_tag (IdeXmlTreeBuilder *self,
                                    const gchar       *str,
                                    ColorTagId         id,
                                    gboolean           space_before,
                                    gboolean           space_after,
                                    gboolean           space_inside)
{
  ColorTag *tag;

  g_assert (IDE_IS_XML_TREE_BUILDER (self));
  g_assert (self->color_tags != NULL);
  g_assert (!ide_str_empty0 (str));

  tag = &g_array_index (self->color_tags, ColorTag, id);
  return g_strdup_printf ("%s<span foreground=\"%s\" background=\"%s\">%s%s%s</span>%s",
                          space_before ? " " : "",
                          tag->fg,
                          tag->bg,
                          space_inside ? " " : "",
                          str,
                          space_inside ? " " : "",
                          space_after ? " " : "");
}

static void
init_color_tags (IdeXmlTreeBuilder *self)
{
  g_autofree gchar *scheme_name = NULL;
  GtkSourceStyleSchemeManager *manager;
  GtkSourceStyleScheme *scheme;
  gchar *tag_name;
  GtkSourceStyle *style;
  gchar *foreground;
  gchar *background;
  ColorTag tag;
  ColorTag *tag_ptr;
  gboolean tag_set;

  g_assert (IDE_IS_XML_TREE_BUILDER (self));

  scheme_name = g_settings_get_string (self->settings, "style-scheme-name");
  manager = gtk_source_style_scheme_manager_get_default ();
  scheme = gtk_source_style_scheme_manager_get_scheme (manager, scheme_name);

  g_array_remove_range (self->color_tags, 0, self->color_tags->len);
  tag_ptr = default_color_tags;
  while (tag_ptr->fg != NULL)
    {
      tag_set = FALSE;
      if (scheme != NULL)
        {
          tag_name = g_strconcat ("symboltree::", tag_ptr->name, NULL);
          if (NULL != (style = gtk_source_style_scheme_get_style (scheme, tag_name)))
            {
              g_object_get (style, "foreground", &foreground, NULL);
              g_object_get (style, "background", &background, NULL);
              if (foreground != NULL && background != NULL)
                {
                  tag_set = TRUE;
                  tag.name = g_strdup (tag_ptr->name);
                  tag.fg = g_steal_pointer (&foreground);
                  tag.bg = g_steal_pointer (&background);
                }

              g_free (foreground);
              g_free (background);
            }

          g_free (tag_name);
        }

      if (!tag_set)
        {
          tag.name = g_strdup (tag_ptr->name);
          tag.fg = g_strdup (tag_ptr->fg);
          tag.bg = g_strdup (tag_ptr->bg);
        }

      g_array_append_val (self->color_tags, tag);
      ++tag_ptr;
    }
}

static void
editor_settings_changed_cb (IdeXmlTreeBuilder *self,
                            gchar             *key,
                            GSettings         *settings)
{
  g_assert (IDE_IS_XML_TREE_BUILDER (self));

  if (ide_str_equal0 (key, "style-scheme-name"))
    init_color_tags (self);
}

IdeXmlTreeBuilder *
ide_xml_tree_builder_new (void)
{
  return g_object_new (IDE_TYPE_XML_TREE_BUILDER, NULL);
}

static void
ide_xml_tree_builder_finalize (GObject *object)
{
  IdeXmlTreeBuilder *self = (IdeXmlTreeBuilder *)object;

  g_clear_pointer (&self->color_tags, g_array_unref);
  g_clear_object (&self->settings);

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
  self->color_tags = g_array_new (TRUE, TRUE, sizeof (ColorTag));
  g_array_set_clear_func (self->color_tags, (GDestroyNotify)color_tag_free);

  self->settings = g_settings_new ("org.gnome.builder.editor");
  g_signal_connect_swapped (self->settings,
                            "changed",
                            G_CALLBACK (editor_settings_changed_cb),
                            self);

  init_color_tags (self);
}
