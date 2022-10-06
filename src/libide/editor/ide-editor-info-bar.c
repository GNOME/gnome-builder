/* ide-editor-info-bar.c
 *
 * Copyright 2021-2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-editor-info-bar"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-gui.h>

#include "ide-editor-info-bar-private.h"

struct _IdeEditorInfoBar
{
  GtkWidget       parent_instance;

  IdeBuffer *buffer;

  GtkBox         *box;

  /* Discard widgetry */
  GtkInfoBar     *discard_infobar;
  GtkButton      *discard;
  GtkButton      *save;
  GtkLabel       *title;
  GtkLabel       *subtitle;

  /* Permission denied infobar */
  GtkInfoBar     *access_infobar;
  GtkButton      *access_subtitle;
  GtkButton      *access_title;
  GtkButton      *access_try_admin;
};

enum {
  PROP_0,
  PROP_BUFFER,
  N_PROPS
};

G_DEFINE_TYPE (IdeEditorInfoBar, ide_editor_info_bar, GTK_TYPE_WIDGET)

static GParamSpec *properties [N_PROPS];

static void
ide_editor_info_bar_update (IdeEditorInfoBar *self)
{
  g_assert (IDE_IS_EDITOR_INFO_BAR (self));
  g_assert (IDE_IS_BUFFER (self->buffer));

  /* Ignore things if we're busy to avoid flapping */
  switch (ide_buffer_get_state (self->buffer))
    {
    case IDE_BUFFER_STATE_READY:
    case IDE_BUFFER_STATE_FAILED:
      break;

    case IDE_BUFFER_STATE_LOADING:
    case IDE_BUFFER_STATE_SAVING:
    default:
      gtk_info_bar_set_revealed (self->discard_infobar, FALSE);
      return;
    }

  if (ide_buffer_get_changed_on_volume (self->buffer))
    {
      gtk_button_set_label (self->discard, _("_Discard Changes and Reload"));
      gtk_button_set_use_underline (self->discard, TRUE);
      gtk_actionable_set_action_name (GTK_ACTIONABLE (self->discard), "page.editor.reload");
      gtk_label_set_label (self->title, _("File Has Changed on Disk"));
      gtk_label_set_label (self->subtitle, _("The file has been changed by another program."));
      gtk_widget_show (GTK_WIDGET (self->discard));
      gtk_widget_hide (GTK_WIDGET (self->save));
      gtk_info_bar_set_revealed (self->discard_infobar, TRUE);
    }
  else if (ide_buffer_has_encoding_error (self->buffer))
    {
      gtk_button_set_label (self->discard, _("_Discard Changes and Reload"));
      gtk_button_set_use_underline (self->discard, TRUE);
      gtk_actionable_set_action_name (GTK_ACTIONABLE (self->discard), "page.editor.reload");
      gtk_label_set_label (self->title, _("File Contains Encoding Errors"));
      gtk_label_set_label (self->subtitle, _("The encoding used to load the file detected errors. You may select an alternate encoding from the statusbar and reload."));
      gtk_widget_show (GTK_WIDGET (self->discard));
      gtk_widget_hide (GTK_WIDGET (self->save));
      gtk_info_bar_set_revealed (self->discard_infobar, TRUE);
    }
  else
    {
      gtk_info_bar_set_revealed (self->discard_infobar, FALSE);
    }
}

static void
ide_editor_info_bar_wrap_button_label (GtkButton *button)
{
  GtkWidget *label;

  g_assert (GTK_IS_BUTTON (button));

  label = gtk_button_get_child (button);
  g_assert (GTK_IS_LABEL (label));

  gtk_label_set_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
}

static void
on_notify_cb (IdeEditorInfoBar *self,
              GParamSpec       *pspec,
              IdeBuffer        *buffer)
{
  g_assert (IDE_IS_EDITOR_INFO_BAR (self));
  g_assert (IDE_IS_BUFFER (buffer));

  ide_editor_info_bar_update (self);
}

static void
on_response_cb (IdeEditorInfoBar *self,
                int               response,
                GtkInfoBar       *infobar)
{
  g_assert (IDE_IS_EDITOR_INFO_BAR (self));
  g_assert (GTK_IS_INFO_BAR (infobar));

  gtk_info_bar_set_revealed (infobar, FALSE);
}

static void
on_try_admin_cb (IdeEditorInfoBar *self,
                 GtkButton        *button)
{
  g_assert (IDE_IS_EDITOR_INFO_BAR (self));
  g_assert (GTK_IS_BUTTON (button));

#if 0
  _ide_buffer_use_admin (self->buffer);
#endif
}

static void
on_try_again_cb (IdeEditorInfoBar *self,
                 GtkButton        *button)
{
  IdeBufferManager *buffer_manager;
  IdeContext *context;

  g_assert (IDE_IS_EDITOR_INFO_BAR (self));
  g_assert (GTK_IS_BUTTON (button));

  context = ide_widget_get_context (GTK_WIDGET (button));
  buffer_manager = ide_buffer_manager_from_context (context);

  ide_buffer_manager_load_file_async (buffer_manager,
                                      ide_buffer_get_file (self->buffer),
                                      IDE_BUFFER_OPEN_FLAGS_FORCE_RELOAD,
                                      NULL, /* TODO: Progress */
                                      NULL, NULL, NULL);
}

static void
ide_editor_info_bar_dispose (GObject *object)
{
  IdeEditorInfoBar *self = (IdeEditorInfoBar *)object;

  g_clear_object (&self->buffer);
  g_clear_pointer ((GtkWidget **)&self->box, gtk_widget_unparent);

  G_OBJECT_CLASS (ide_editor_info_bar_parent_class)->dispose (object);
}

static void
ide_editor_info_bar_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  IdeEditorInfoBar *self = IDE_EDITOR_INFO_BAR (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      g_value_set_object (value, self->buffer);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_info_bar_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  IdeEditorInfoBar *self = IDE_EDITOR_INFO_BAR (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      if (g_set_object (&self->buffer, g_value_get_object (value)))
        {
          g_object_bind_property (self->buffer, "failed",
                                  self->access_infobar, "revealed",
                                  G_BINDING_SYNC_CREATE);
          g_signal_connect_object (self->buffer,
                                   "notify::busy",
                                   G_CALLBACK (on_notify_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
          g_signal_connect_object (self->buffer,
                                   "notify::changed-on-volume",
                                   G_CALLBACK (on_notify_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
          g_signal_connect_object (self->buffer,
                                   "notify::has-encoding-error",
                                   G_CALLBACK (on_notify_cb),
                                   self,
                                   G_CONNECT_SWAPPED);

          on_notify_cb (self, NULL, self->buffer);
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_info_bar_class_init (IdeEditorInfoBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_editor_info_bar_dispose;
  object_class->get_property = ide_editor_info_bar_get_property;
  object_class->set_property = ide_editor_info_bar_set_property;

  properties [PROP_BUFFER] =
    g_param_spec_object ("buffer",
                         "Buffer",
                         "The buffer to monitor",
                         IDE_TYPE_BUFFER,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-editor/ide-editor-info-bar.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeEditorInfoBar, access_infobar);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorInfoBar, access_try_admin);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorInfoBar, access_subtitle);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorInfoBar, access_title);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorInfoBar, box);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorInfoBar, discard);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorInfoBar, discard_infobar);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorInfoBar, save);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorInfoBar, subtitle);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorInfoBar, title);
  gtk_widget_class_bind_template_callback (widget_class, on_try_admin_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_try_again_cb);
}

static void
ide_editor_info_bar_init (IdeEditorInfoBar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  /*
   * Ensure buttons with long labels can wrap text and are
   * center-justified, so the infobar can fit narrow screens.
   */
  ide_editor_info_bar_wrap_button_label (self->access_try_admin);
  ide_editor_info_bar_wrap_button_label (self->discard);

  g_signal_connect_object (self->discard_infobar,
                           "response",
                           G_CALLBACK (on_response_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

GtkWidget *
_ide_editor_info_bar_new (IdeBuffer *buffer)
{
  g_return_val_if_fail (IDE_IS_BUFFER (buffer), NULL);

  return g_object_new (IDE_TYPE_EDITOR_INFO_BAR,
                       "buffer", buffer,
                       NULL);
}
