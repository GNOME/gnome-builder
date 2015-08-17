/* gb-editor-print-operation.c
 *
 * Copyright (C) 2015 Paolo Borelli <pborelli@gnome.org>
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

#define G_LOG_DOMAIN "gb-editor-print-operation"

#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>
#include <ide.h>

#include "gb-editor-print-operation.h"
#include "gb-editor-view.h"

struct _GbEditorPrintOperation
{
  GtkPrintOperation parent_instance;

  IdeSourceView *view;
  GtkSourcePrintCompositor *compositor;
};

G_DEFINE_TYPE (GbEditorPrintOperation, gb_editor_print_operation, GTK_TYPE_PRINT_OPERATION)

enum {
  PROP_0,
  PROP_VIEW,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static void
gb_editor_print_operation_dispose (GObject *object)
{
  GbEditorPrintOperation *self = GB_EDITOR_PRINT_OPERATION (object);

  g_clear_object (&self->compositor);

  G_OBJECT_CLASS (gb_editor_print_operation_parent_class)->dispose (object);
}

static void
gb_editor_view_print_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GbEditorPrintOperation *self = GB_EDITOR_PRINT_OPERATION (object);

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
gb_editor_view_print_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GbEditorPrintOperation *self = GB_EDITOR_PRINT_OPERATION (object);

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
gb_editor_print_operation_begin_print (GtkPrintOperation *operation,
                                       GtkPrintContext   *context)
{
  GbEditorPrintOperation *self = GB_EDITOR_PRINT_OPERATION (operation);
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
gb_editor_print_operation_paginate (GtkPrintOperation *operation,
                                    GtkPrintContext   *context)
{
  GbEditorPrintOperation *self = GB_EDITOR_PRINT_OPERATION (operation);
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
gb_editor_print_operation_draw_page (GtkPrintOperation *operation,
                                     GtkPrintContext   *context,
                                     gint               page_nr)
{
  GbEditorPrintOperation *self = GB_EDITOR_PRINT_OPERATION (operation);

  gtk_source_print_compositor_draw_page (self->compositor, context, page_nr);
}

static void
gb_editor_print_operation_end_print (GtkPrintOperation *operation,
                                     GtkPrintContext   *context)
{
  GbEditorPrintOperation *self = GB_EDITOR_PRINT_OPERATION (operation);

  g_clear_object (&self->compositor);
}

static void
gb_editor_print_operation_class_init (GbEditorPrintOperationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkPrintOperationClass *operation_class = GTK_PRINT_OPERATION_CLASS (klass);

  object_class->dispose = gb_editor_print_operation_dispose;
  object_class->get_property = gb_editor_view_print_get_property;
  object_class->set_property = gb_editor_view_print_set_property;

  operation_class->begin_print = gb_editor_print_operation_begin_print;
  operation_class->draw_page = gb_editor_print_operation_draw_page;
  operation_class->end_print = gb_editor_print_operation_end_print;

  gParamSpecs [PROP_VIEW] =
    g_param_spec_object ("view",
                         _("View"),
                         _("The source view."),
                         IDE_TYPE_SOURCE_VIEW,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);
}

static gboolean
paginate_cb (GtkPrintOperation *operation,
             GtkPrintContext   *context,
             gpointer           user_data)
{
  return gb_editor_print_operation_paginate (operation, context);
}

static void
gb_editor_print_operation_init (GbEditorPrintOperation *self)
{
  /* FIXME: gtk decides to call paginate only if it sees a pending signal
   * handler, even if we override the default handler.
   * So for now we connect to the signal instead of overriding the vfunc
   * See https://bugzilla.gnome.org/show_bug.cgi?id=345345
   */
  g_signal_connect (self, "paginate", G_CALLBACK (paginate_cb), NULL);
}

GbEditorPrintOperation *
gb_editor_print_operation_new (IdeSourceView *view)
{
  g_assert (IDE_IS_SOURCE_VIEW (view));

  return g_object_new (GB_TYPE_EDITOR_PRINT_OPERATION,
                       "view", view,
                       "allow-async", TRUE,
                       NULL);
}
