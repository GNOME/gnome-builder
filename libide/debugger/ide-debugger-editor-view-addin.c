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

#include "ide-context.h"
#include "ide-debug.h"

#include "debugger/ide-debug-manager.h"
#include "debugger/ide-debugger-editor-view-addin.h"
#include "debugger/ide-debugger-gutter-renderer.h"
#include "files/ide-file.h"
#include "util/ide-gtk.h"

struct _IdeDebuggerEditorViewAddin
{
  GObject parent_instance;

  IdeEditorView *view;
  IdeDebuggerGutterRenderer *renderer;
};

static void
ide_debugger_editor_view_addin_load (IdeEditorViewAddin *addin,
                                     IdeEditorView      *view)
{
  IdeDebuggerEditorViewAddin *self = (IdeDebuggerEditorViewAddin *)addin;
  g_autoptr(IdeDebuggerBreakpoints) breakpoints = NULL;
  IdeDebugManager *debug_manager;
  IdeSourceView *source_view;
  GtkSourceGutter *gutter;
  IdeContext *context;
  IdeBuffer *buffer;
  IdeFile *file;
  GFile *gfile;

  IDE_ENTRY;

  g_assert (IDE_IS_DEBUGGER_EDITOR_VIEW_ADDIN (self));
  g_assert (IDE_IS_EDITOR_VIEW (view));

  self->view = view;

  context = ide_widget_get_context (GTK_WIDGET (view));
  debug_manager = ide_context_get_debug_manager (context);

  buffer = ide_editor_view_get_buffer (view);
  file = ide_buffer_get_file (buffer);
  gfile = ide_file_get_file (file);

  /* Install the breakpoints gutter */
  breakpoints = ide_debug_manager_get_breakpoints_for_file (debug_manager, gfile);
  source_view = ide_editor_view_get_view (view);
  gutter = gtk_source_view_get_gutter (GTK_SOURCE_VIEW (source_view), GTK_TEXT_WINDOW_LEFT);
  self->renderer = g_object_new (IDE_TYPE_DEBUGGER_GUTTER_RENDERER,
                                 "debug-manager", debug_manager,
                                 "breakpoints", breakpoints,
                                 "size", 16,
                                 "xpad", 1,
                                 NULL);
  gtk_source_gutter_insert (gutter, GTK_SOURCE_GUTTER_RENDERER (self->renderer), -100);

  /* TODO: Monitor IdeBuffer:file? */

  IDE_EXIT;
}

static void
ide_debugger_editor_view_addin_unload (IdeEditorViewAddin *addin,
                                       IdeEditorView      *view)
{
  IdeDebuggerEditorViewAddin *self = (IdeDebuggerEditorViewAddin *)addin;
  IdeSourceView *source_view;
  GtkSourceGutter *gutter;

  IDE_ENTRY;

  g_assert (IDE_IS_DEBUGGER_EDITOR_VIEW_ADDIN (self));
  g_assert (IDE_IS_EDITOR_VIEW (view));

  source_view = ide_editor_view_get_view (view);
  gutter = gtk_source_view_get_gutter (GTK_SOURCE_VIEW (source_view), GTK_TEXT_WINDOW_LEFT);
  gtk_source_gutter_remove (gutter, GTK_SOURCE_GUTTER_RENDERER (self->renderer));

  self->renderer = NULL;
  self->view = NULL;

  IDE_EXIT;
}

static void
editor_view_addin_iface_init (IdeEditorViewAddinInterface *iface)
{
  iface->load = ide_debugger_editor_view_addin_load;
  iface->unload = ide_debugger_editor_view_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (IdeDebuggerEditorViewAddin, ide_debugger_editor_view_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_VIEW_ADDIN,
                                                editor_view_addin_iface_init))

static void
ide_debugger_editor_view_addin_class_init (IdeDebuggerEditorViewAddinClass *klass)
{
}

static void
ide_debugger_editor_view_addin_init (IdeDebuggerEditorViewAddin *self)
{
}
