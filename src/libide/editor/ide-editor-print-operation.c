/* ide-editor-print-operation.c
 *
 * Copyright 2015 Paolo Borelli <pborelli@gnome.org>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "ide-editor-print-operation"

#include "config.h"

#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>

#include "ide-editor-print-operation.h"
#include "ide-editor-page.h"

struct _IdeEditorPrintOperation
{
  GtkPrintOperation         parent_instance;

  IdeSourceView            *view;
  GtkSourcePrintCompositor *compositor;
};

G_DEFINE_FINAL_TYPE (IdeEditorPrintOperation, ide_editor_print_operation, GTK_TYPE_PRINT_OPERATION)

enum {
  PROP_0,
  PROP_VIEW,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
ide_editor_print_operation_dispose (GObject *object)
{
  IdeEditorPrintOperation *self = IDE_EDITOR_PRINT_OPERATION (object);

  g_clear_object (&self->compositor);

  G_OBJECT_CLASS (ide_editor_print_operation_parent_class)->dispose (object);
}

static void
ide_editor_view_print_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  IdeEditorPrintOperation *self = IDE_EDITOR_PRINT_OPERATION (object);

  switch (prop_id)
    {
    case PROP_VIEW:
      g_value_set_object (value, self->view);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_view_print_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  IdeEditorPrintOperation *self = IDE_EDITOR_PRINT_OPERATION (object);

  switch (prop_id)
    {
    case PROP_VIEW:
      self->view = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_print_operation_begin_print (GtkPrintOperation *operation,
                                       GtkPrintContext   *context)
{
  IdeEditorPrintOperation *self = IDE_EDITOR_PRINT_OPERATION (operation);
  GtkSourceBuffer *buffer;
  guint tab_width;
  gboolean syntax_hl;

  buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->view)));

  tab_width = gtk_source_view_get_tab_width (GTK_SOURCE_VIEW (self->view));
  syntax_hl = gtk_source_buffer_get_highlight_syntax (buffer);

  self->compositor = GTK_SOURCE_PRINT_COMPOSITOR (
    g_object_new (GTK_SOURCE_TYPE_PRINT_COMPOSITOR,
                  "buffer", buffer,
                  "tab-width", tab_width,
                  "highlight-syntax", syntax_hl,
                  NULL));
}

static gboolean
ide_editor_print_operation_paginate (GtkPrintOperation *operation,
                                     GtkPrintContext   *context)
{
  IdeEditorPrintOperation *self = IDE_EDITOR_PRINT_OPERATION (operation);
  gboolean finished;

  finished = gtk_source_print_compositor_paginate (self->compositor, context);

  if (finished)
    {
      gint n_pages;

      n_pages = gtk_source_print_compositor_get_n_pages (self->compositor);
      gtk_print_operation_set_n_pages (operation, n_pages);
    }

  return finished;
}

static void
ide_editor_print_operation_draw_page (GtkPrintOperation *operation,
                                      GtkPrintContext   *context,
                                      gint               page_nr)
{
  IdeEditorPrintOperation *self = IDE_EDITOR_PRINT_OPERATION (operation);

  gtk_source_print_compositor_draw_page (self->compositor, context, page_nr);
}

static void
ide_editor_print_operation_end_print (GtkPrintOperation *operation,
                                      GtkPrintContext   *context)
{
  IdeEditorPrintOperation *self = IDE_EDITOR_PRINT_OPERATION (operation);

  g_clear_object (&self->compositor);
}

static void
ide_editor_print_operation_class_init (IdeEditorPrintOperationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkPrintOperationClass *operation_class = GTK_PRINT_OPERATION_CLASS (klass);

  object_class->dispose = ide_editor_print_operation_dispose;
  object_class->get_property = ide_editor_view_print_get_property;
  object_class->set_property = ide_editor_view_print_set_property;

  operation_class->begin_print = ide_editor_print_operation_begin_print;
  operation_class->draw_page = ide_editor_print_operation_draw_page;
  operation_class->end_print = ide_editor_print_operation_end_print;

  properties [PROP_VIEW] =
    g_param_spec_object ("view",
                         "View",
                         "The source view.",
                         IDE_TYPE_SOURCE_VIEW,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static gboolean
paginate_cb (GtkPrintOperation *operation,
             GtkPrintContext   *context,
             gpointer           user_data)
{
  return ide_editor_print_operation_paginate (operation, context);
}

static void
ide_editor_print_operation_init (IdeEditorPrintOperation *self)
{
  /* FIXME: gtk decides to call paginate only if it sees a pending signal
   * handler, even if we override the default handler.
   * So for now we connect to the signal instead of overriding the vfunc
   * See https://bugzilla.gnome.org/show_bug.cgi?id=345345
   */
  g_signal_connect (self, "paginate", G_CALLBACK (paginate_cb), NULL);
}

IdeEditorPrintOperation *
ide_editor_print_operation_new (IdeSourceView *view)
{
  g_assert (IDE_IS_SOURCE_VIEW (view));

  return g_object_new (IDE_TYPE_EDITOR_PRINT_OPERATION,
                       "view", view,
                       "allow-async", TRUE,
                       NULL);
}
