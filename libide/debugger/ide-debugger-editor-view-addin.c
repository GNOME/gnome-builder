/* ide-debugger-editor-view-addin.c
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-debugger-editor-view-addin"

#include "ide-debugger-breakpoints.h"
#include "ide-debugger-editor-view-addin.h"
#include "ide-debugger-gutter-renderer.h"

static GHashTable *supported_languages;

struct _IdeDebuggerEditorViewAddin
{
  GObject    parent_instance;
  GPtrArray *renderers;
};

static void
ide_debugger_editor_view_addin_load_source_view (IdeEditorViewAddin *addin,
                                                 IdeSourceView      *source_view)
{
  IdeDebuggerEditorViewAddin *self = (IdeDebuggerEditorViewAddin *)addin;
  g_autoptr(IdeDebuggerBreakpoints) breakpoints = NULL;
  GtkSourceGutterRenderer *renderer;
  GtkSourceGutter *gutter;
  IdeContext *context;
  IdeBuffer *buffer;
  IdeFile *file;
  GFile *gfile;

  IDE_ENTRY;

  g_assert (IDE_IS_DEBUGGER_EDITOR_VIEW_ADDIN (addin));
  g_assert (IDE_IS_SOURCE_VIEW (source_view));

  context = ide_widget_get_context (GTK_WIDGET (source_view));
  buffer = IDE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (source_view)));
  file = ide_buffer_get_file (buffer);
  gfile = ide_file_get_file (file);

  breakpoints = g_object_new (IDE_TYPE_DEBUGGER_BREAKPOINTS,
                              "context", context,
                              "file", gfile,
                              NULL);

  gutter = gtk_source_view_get_gutter (GTK_SOURCE_VIEW (source_view), GTK_TEXT_WINDOW_LEFT);
  renderer = g_object_new (IDE_TYPE_DEBUGGER_GUTTER_RENDERER,
                           "alignment-mode", GTK_SOURCE_GUTTER_RENDERER_ALIGNMENT_MODE_FIRST,
                           "breakpoints", breakpoints,
                           "size", 16,
                           "visible", TRUE,
                           "xalign", 0.5f,
                           NULL);
  g_ptr_array_add (self->renderers, g_object_ref (renderer));
  gtk_source_gutter_insert (gutter, renderer, -1000);

  IDE_EXIT;
}

static void
ide_debugger_editor_view_addin_unload_source_view (IdeEditorViewAddin *addin,
                                                   IdeSourceView      *source_view)
{
  IdeDebuggerEditorViewAddin *self = (IdeDebuggerEditorViewAddin *)addin;
  GtkSourceGutter *gutter;

  IDE_ENTRY;

  g_assert (IDE_IS_DEBUGGER_EDITOR_VIEW_ADDIN (addin));
  g_assert (IDE_IS_SOURCE_VIEW (source_view));

  gutter = gtk_source_view_get_gutter (GTK_SOURCE_VIEW (source_view), GTK_TEXT_WINDOW_LEFT);

  for (guint i = self->renderers->len; i > 0; i--)
    {
      GtkSourceGutterRenderer *renderer = g_ptr_array_index (self->renderers, i - 1);

      if ((gpointer)source_view == (gpointer)gtk_source_gutter_renderer_get_view (renderer))
        {
          gtk_source_gutter_remove (gutter, renderer);
          g_ptr_array_remove_index (self->renderers, i - 1);
        }
    }

  IDE_EXIT;
}

static void
ide_debugger_editor_view_addin_language_changed (IdeEditorViewAddin *addin,
                                                 const gchar        *lang_id)
{
  IdeDebuggerEditorViewAddin *self = (IdeDebuggerEditorViewAddin *)addin;
  gboolean visible = FALSE;

  g_assert (IDE_IS_DEBUGGER_EDITOR_VIEW_ADDIN (self));

  visible = lang_id && g_hash_table_contains (supported_languages, lang_id);

  for (guint i = 0; i < self->renderers->len; i++)
    {
      GtkSourceGutterRenderer *renderer = g_ptr_array_index (self->renderers, i);

      gtk_source_gutter_renderer_set_visible (renderer, visible);
    }
}

static void
editor_view_addin_iface_init (IdeEditorViewAddinInterface *iface)
{
  iface->load_source_view = ide_debugger_editor_view_addin_load_source_view;
  iface->unload_source_view = ide_debugger_editor_view_addin_unload_source_view;
  iface->language_changed = ide_debugger_editor_view_addin_language_changed;
}

G_DEFINE_TYPE_EXTENDED (IdeDebuggerEditorViewAddin, ide_debugger_editor_view_addin, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_VIEW_ADDIN, editor_view_addin_iface_init))

static void
ide_debugger_editor_view_addin_finalize (GObject *object)
{
  IdeDebuggerEditorViewAddin *self = (IdeDebuggerEditorViewAddin *)object;

  g_clear_pointer (&self->renderers, g_ptr_array_unref);

  G_OBJECT_CLASS (ide_debugger_editor_view_addin_parent_class)->finalize (object);
}

static void
ide_debugger_editor_view_addin_class_init (IdeDebuggerEditorViewAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_debugger_editor_view_addin_finalize;

  supported_languages = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (supported_languages, (gchar *)"c", NULL);
  g_hash_table_insert (supported_languages, (gchar *)"chdr", NULL);
  g_hash_table_insert (supported_languages, (gchar *)"cpp", NULL);
  g_hash_table_insert (supported_languages, (gchar *)"cpphdr", NULL);
}

static void
ide_debugger_editor_view_addin_init (IdeDebuggerEditorViewAddin *self)
{
  self->renderers = g_ptr_array_new_with_free_func (g_object_unref);
}
