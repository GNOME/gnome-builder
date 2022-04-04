/* ide-editor-page.c
 *
 * Copyright 2017-2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-editor-page"

#include "config.h"

#include "ide-editor-page-private.h"

enum {
  PROP_0,
  PROP_BUFFER,
  PROP_VIEW,
  N_PROPS
};

G_DEFINE_TYPE (IdeEditorPage, ide_editor_page, IDE_TYPE_PAGE)

static GParamSpec *properties [N_PROPS];

static void
ide_editor_page_modified_changed_cb (IdeEditorPage *self,
                                     IdeBuffer     *buffer)
{
  IDE_ENTRY;

  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (IDE_IS_BUFFER (buffer));

  panel_widget_set_modified (PANEL_WIDGET (self),
                             gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (buffer)));

  IDE_EXIT;
}

static void
ide_editor_page_set_buffer (IdeEditorPage *self,
                            IdeBuffer     *buffer)
{
  IDE_ENTRY;

  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (IDE_IS_BUFFER (buffer));

  if (g_set_object (&self->buffer, buffer))
    {
      gtk_text_view_set_buffer (GTK_TEXT_VIEW (self->view), GTK_TEXT_BUFFER (buffer));

      g_signal_connect_object (buffer,
                               "modified-changed",
                               G_CALLBACK (ide_editor_page_modified_changed_cb),
                               self,
                               G_CONNECT_SWAPPED);

      g_signal_connect_object (buffer,
                               "notify::file-settings",
                               G_CALLBACK (_ide_editor_page_settings_reload),
                               self,
                               G_CONNECT_SWAPPED);

      g_object_bind_property (buffer, "title",
                              self, "title",
                              G_BINDING_SYNC_CREATE);

      ide_editor_page_modified_changed_cb (self, buffer);
      _ide_editor_page_settings_init (self);
    }

  IDE_EXIT;
}

static gboolean
ide_editor_page_grab_focus (GtkWidget *widget)
{
  return gtk_widget_grab_focus (GTK_WIDGET (IDE_EDITOR_PAGE (widget)->view));
}

static void
ide_editor_page_focus_enter_cb (IdeEditorPage           *self,
                                GtkEventControllerFocus *controller)
{
  g_autofree char *title = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_FOCUS (controller));

  title = ide_buffer_dup_title (self->buffer);
  g_debug ("Keyboard focus entered page \"%s\"", title);

  ide_page_mark_used (IDE_PAGE (self));

  IDE_EXIT;
}

static void
ide_editor_page_dispose (GObject *object)
{
  IdeEditorPage *self = (IdeEditorPage *)object;

  g_clear_object (&self->buffer_file_settings);
  g_clear_object (&self->view_file_settings);
  g_clear_object (&self->buffer);

  G_OBJECT_CLASS (ide_editor_page_parent_class)->dispose (object);
}

static void
ide_editor_page_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeEditorPage *self = IDE_EDITOR_PAGE (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      g_value_set_object (value, ide_editor_page_get_buffer (self));
      break;

    case PROP_VIEW:
      g_value_set_object (value, ide_editor_page_get_view (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_page_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeEditorPage *self = IDE_EDITOR_PAGE (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      ide_editor_page_set_buffer (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_page_class_init (IdeEditorPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_editor_page_dispose;
  object_class->get_property = ide_editor_page_get_property;
  object_class->set_property = ide_editor_page_set_property;

  widget_class->grab_focus = ide_editor_page_grab_focus;

  /**
   * IdeEditorPage:buffer:
   *
   * The #IdeBuffer that is displayed within the #IdeSourceView.
   */
  properties [PROP_BUFFER] =
    g_param_spec_object ("buffer",
                         "Buffer",
                         "The buffer to be displayed within the page",
                         IDE_TYPE_BUFFER,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeEditorPage:view:
   *
   * The #IdeSourceView contained within the page.
   */
  properties [PROP_VIEW] =
    g_param_spec_object ("view",
                         "View",
                         "The view displaying the buffer",
                         IDE_TYPE_SOURCE_VIEW,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-editor/ide-editor-page.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPage, map);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPage, map_revealer);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPage, scroller);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPage, view);
  gtk_widget_class_bind_template_callback (widget_class, ide_editor_page_focus_enter_cb);
}

static void
ide_editor_page_init (IdeEditorPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
ide_editor_page_new (IdeBuffer *buffer)
{
  g_return_val_if_fail (IDE_IS_BUFFER (buffer), NULL);

  return g_object_new (IDE_TYPE_EDITOR_PAGE,
                       "buffer", buffer,
                       NULL);
}

/**
 * ide_editor_page_get_view:
 * @self: a #IdeEditorPage
 *
 * Gets the #IdeSourceView for the page.
 *
 * Returns: (transfer none): an #IdeSourceView
 */
IdeSourceView *
ide_editor_page_get_view (IdeEditorPage *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_PAGE (self), NULL);

  return self->view;
}

/**
 * ide_editor_page_get_buffer:
 * @self: a #IdeEditorPage
 *
 * Gets the #IdeBuffer for the page.
 *
 * Returns: (transfer none): an #IdeBuffer
 */
IdeBuffer *
ide_editor_page_get_buffer (IdeEditorPage *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_PAGE (self), NULL);

  return self->buffer;
}
