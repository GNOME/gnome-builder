/* gstyle-rename-popover.c
 *
 * Copyright 2016 Sebastien Lafargue <slafargue@gnome.org>
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

#include "gstyle-rename-popover.h"
#include "gstyle-utils.h"

struct _GstyleRenamePopover
{
  GtkPopover    parent_instance;

  GtkEntry     *entry;
  GtkButton    *button;
  GtkLabel     *label;
  GtkLabel     *message;
};

G_DEFINE_TYPE (GstyleRenamePopover, gstyle_rename_popover, GTK_TYPE_POPOVER)

enum {
  PROP_0,
  PROP_LABEL,
  PROP_MESSAGE,
  PROP_NAME,
  N_PROPS
};

enum {
  RENAMED,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL];
static GParamSpec *properties [N_PROPS];

static gboolean
check_text_validity (GstyleRenamePopover *self,
                     const gchar         *txt)
{
  gunichar ch;

  if (gstyle_str_empty0 (txt))
    return FALSE;

  do
    {
      ch = g_utf8_get_char (txt);
      if (!g_unichar_isgraph (ch) && ch != ' ')
        return FALSE;

      txt = g_utf8_find_next_char (txt, NULL);
    }
  while (txt != NULL && *txt != '\0');

  return TRUE;
}

static void
gstyle_rename_popover_entry_changed_cb (GstyleRenamePopover *self,
                                        GtkEntry            *entry)
{
  const gchar *txt;
  gboolean sensitive;

  g_assert (GSTYLE_IS_RENAME_POPOVER (self));
  g_assert (GTK_IS_ENTRY (entry));

  txt = gtk_entry_get_text (entry);
  sensitive = check_text_validity (self, txt);
  gtk_widget_set_sensitive (GTK_WIDGET (self->button), sensitive);
}

static void
entry_validation (GstyleRenamePopover *self)
{
  const gchar *txt;

  g_assert (GSTYLE_IS_RENAME_POPOVER (self));

  txt = gtk_entry_get_text (self->entry);
  if (check_text_validity (self, txt))
    {
      g_signal_emit_by_name (self, "renamed", txt);
      g_signal_emit_by_name (self, "closed");
      gtk_popover_popdown (GTK_POPOVER (self));
    }
  else
    gtk_widget_set_sensitive (GTK_WIDGET (self->button), FALSE);
}

static void
gstyle_rename_popover_entry_activate_cb (GstyleRenamePopover *self,
                                         GtkEntry            *entry)
{
  g_assert (GSTYLE_IS_RENAME_POPOVER (self));
  g_assert (GTK_IS_ENTRY (entry));

  entry_validation (self);
}

static void
gstyle_rename_popover_button_clicked_cb (GstyleRenamePopover *self,
                                         GtkButton           *button)
{
  g_assert (GSTYLE_IS_RENAME_POPOVER (self));
  g_assert (GTK_IS_BUTTON (button));

  entry_validation (self);
}

void
gstyle_rename_popover_set_label (GstyleRenamePopover *self,
                                 const gchar         *label)
{
  g_assert (GSTYLE_IS_RENAME_POPOVER (self));
  g_assert (label != NULL);

  if (g_strcmp0 (gtk_label_get_text (self->label), label) != 0)
    {
      if (gstyle_str_empty0 (label))
        gtk_label_set_text (self->label, "");
      else
        gtk_label_set_text (self->label, label);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LABEL]);
    }
}

const gchar *
gstyle_rename_popover_get_label (GstyleRenamePopover *self)
{
  g_assert (GSTYLE_IS_RENAME_POPOVER (self));

  return gtk_label_get_text (self->label);
}

void
gstyle_rename_popover_set_message (GstyleRenamePopover *self,
                                   const gchar         *message)
{
  g_assert (GSTYLE_IS_RENAME_POPOVER (self));
  g_assert (message != NULL);

  if (g_strcmp0 (gtk_label_get_text (self->message), message) != 0)
    {
      if (gstyle_str_empty0 (message))
        gtk_label_set_text (self->message, "");
      else
        gtk_label_set_text (self->message, message);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MESSAGE]);
    }
}

const gchar *
gstyle_rename_popover_get_message (GstyleRenamePopover *self)
{
  g_assert (GSTYLE_IS_RENAME_POPOVER (self));

  return gtk_label_get_text (self->message);
}

void
gstyle_rename_popover_set_name (GstyleRenamePopover *self,
                                const gchar         *name)
{
  g_assert (GSTYLE_IS_RENAME_POPOVER (self));

  if (g_strcmp0 (gtk_entry_get_text (self->entry), name) != 0)
    {
      if (name == NULL || gstyle_str_empty0 (name))
        gtk_entry_set_text (self->entry, "");
      else
        gtk_entry_set_text (self->entry, name);

      gtk_editable_select_region (GTK_EDITABLE (self->entry), 0, -1);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_NAME]);
    }
}

const gchar *
gstyle_rename_popover_get_name (GstyleRenamePopover *self)
{
  g_assert (GSTYLE_IS_RENAME_POPOVER (self));

  return gtk_entry_get_text (self->entry);
}

GstyleRenamePopover *
gstyle_rename_popover_new (void)
{
  return g_object_new (GSTYLE_TYPE_RENAME_POPOVER, NULL);
}

static void
gstyle_rename_popover_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GstyleRenamePopover *self = GSTYLE_RENAME_POPOVER (object);

  switch (prop_id)
    {
    case PROP_LABEL:
      g_value_set_string (value, gstyle_rename_popover_get_label (self));
      break;

    case PROP_MESSAGE:
      g_value_set_string (value, gstyle_rename_popover_get_message (self));
      break;

    case PROP_NAME:
      g_value_set_string (value, gstyle_rename_popover_get_name (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gstyle_rename_popover_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GstyleRenamePopover *self = GSTYLE_RENAME_POPOVER (object);

  switch (prop_id)
    {
    case PROP_LABEL:
      gstyle_rename_popover_set_label (self, g_value_get_string (value));
      break;

    case PROP_MESSAGE:
      gstyle_rename_popover_set_message (self, g_value_get_string (value));
      break;

    case PROP_NAME:
      gstyle_rename_popover_set_name (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gstyle_rename_popover_class_init (GstyleRenamePopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = gstyle_rename_popover_get_property;
  object_class->set_property = gstyle_rename_popover_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libgstyle/ui/gstyle-rename-popover.ui");
  gtk_widget_class_bind_template_child (widget_class, GstyleRenamePopover, button);
  gtk_widget_class_bind_template_child (widget_class, GstyleRenamePopover, entry);
  gtk_widget_class_bind_template_child (widget_class, GstyleRenamePopover, label);
  gtk_widget_class_bind_template_child (widget_class, GstyleRenamePopover, message);

  properties [PROP_LABEL] = g_param_spec_string ("label",
                                                 "label",
                                                 "Popover label text",
                                                 "",
                                                 G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties [PROP_MESSAGE] = g_param_spec_string ("message",
                                                   "message",
                                                   "Popover message text",
                                                   "",
                                                   G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties [PROP_NAME] = g_param_spec_string ("name",
                                                "name",
                                                "Popover entry name",
                                                "",
                                                G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [RENAMED] = g_signal_new ("renamed",
                                    G_TYPE_FROM_CLASS (klass),
                                    G_SIGNAL_RUN_LAST,
                                   0,
                                   NULL, NULL, NULL,
                                   G_TYPE_NONE,
                                   1,
                                   G_TYPE_STRING);
}

static void
gstyle_rename_popover_init (GstyleRenamePopover *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->entry,
                           "changed",
                           G_CALLBACK (gstyle_rename_popover_entry_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->entry,
                           "activate",
                           G_CALLBACK (gstyle_rename_popover_entry_activate_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->button,
                           "clicked",
                           G_CALLBACK (gstyle_rename_popover_button_clicked_cb),
                           self,
                           G_CONNECT_SWAPPED);
}
