/* ide-workbench-message.c
 *
 * Copyright © 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-workbench-message"

#include "workbench/ide-workbench-message.h"

struct _IdeWorkbenchMessage
{
  GtkInfoBar parent_instance;

  gchar *id;

  GtkLabel *title;
  GtkLabel *subtitle;
};

enum {
  PROP_0,
  PROP_ID,
  PROP_TITLE,
  PROP_SUBTITLE,
  N_PROPS
};

G_DEFINE_TYPE (IdeWorkbenchMessage, ide_workbench_message, GTK_TYPE_INFO_BAR)

static GParamSpec *properties [N_PROPS];

static void
ide_workbench_message_finalize (GObject *object)
{
  IdeWorkbenchMessage *self = (IdeWorkbenchMessage *)object;

  g_clear_pointer (&self->id, g_free);

  G_OBJECT_CLASS (ide_workbench_message_parent_class)->finalize (object);
}

static void
ide_workbench_message_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeWorkbenchMessage *self = IDE_WORKBENCH_MESSAGE (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_string (value, ide_workbench_message_get_id (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, ide_workbench_message_get_title (self));
      break;

    case PROP_SUBTITLE:
      g_value_set_string (value, ide_workbench_message_get_subtitle (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_workbench_message_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeWorkbenchMessage *self = IDE_WORKBENCH_MESSAGE (object);

  switch (prop_id)
    {
    case PROP_ID:
      ide_workbench_message_set_id (self, g_value_get_string (value));
      break;

    case PROP_TITLE:
      ide_workbench_message_set_title (self, g_value_get_string (value));
      break;

    case PROP_SUBTITLE:
      ide_workbench_message_set_subtitle (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_workbench_message_class_init (IdeWorkbenchMessageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ide_workbench_message_finalize;
  object_class->get_property = ide_workbench_message_get_property;
  object_class->set_property = ide_workbench_message_set_property;

  properties [PROP_ID] =
    g_param_spec_string ("id", NULL, NULL, NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL, NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SUBTITLE] =
    g_param_spec_string ("subtitle", NULL, NULL, NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-workbench-message.ui");

  gtk_widget_class_bind_template_child (widget_class, IdeWorkbenchMessage, title);
  gtk_widget_class_bind_template_child (widget_class, IdeWorkbenchMessage, subtitle);
}

static void
ide_workbench_message_init (IdeWorkbenchMessage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

const gchar *
ide_workbench_message_get_id (IdeWorkbenchMessage *self)
{
  g_return_val_if_fail (IDE_IS_WORKBENCH_MESSAGE (self), NULL);

  return self->id;
}

void
ide_workbench_message_set_id (IdeWorkbenchMessage *self,
                              const gchar         *id)
{
  g_return_if_fail (IDE_IS_WORKBENCH_MESSAGE (self));

  if (g_strcmp0 (id, self->id) != 0)
    {
      g_free (self->id);
      self->id = g_strdup (id);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ID]);
    }
}

const gchar *
ide_workbench_message_get_title (IdeWorkbenchMessage *self)
{
  g_return_val_if_fail (IDE_IS_WORKBENCH_MESSAGE (self), NULL);

  return gtk_label_get_label (self->title);
}

void
ide_workbench_message_set_title (IdeWorkbenchMessage *self,
                                 const gchar         *title)
{
  g_return_if_fail (IDE_IS_WORKBENCH_MESSAGE (self));

  gtk_label_set_label (self->title, title);
  gtk_widget_set_visible (GTK_WIDGET (self->title), title || *title);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TITLE]);
}

const gchar *
ide_workbench_message_get_subtitle (IdeWorkbenchMessage *self)
{
  g_return_val_if_fail (IDE_IS_WORKBENCH_MESSAGE (self), NULL);

  return gtk_label_get_label (self->subtitle);
}

void
ide_workbench_message_set_subtitle (IdeWorkbenchMessage *self,
                                    const gchar         *subtitle)
{
  g_return_if_fail (IDE_IS_WORKBENCH_MESSAGE (self));

  gtk_label_set_label (self->subtitle, subtitle);
  gtk_widget_set_visible (GTK_WIDGET (self->subtitle), subtitle || *subtitle);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SUBTITLE]);
}

void
ide_workbench_message_add_action (IdeWorkbenchMessage *self,
                                  const gchar         *title,
                                  const gchar         *action_name)
{
  GtkWidget *button;

  g_return_if_fail (IDE_IS_WORKBENCH_MESSAGE (self));
  g_return_if_fail (title);

  button = g_object_new (GTK_TYPE_BUTTON,
                         "action-name", action_name,
                         "label", title,
                         "visible", TRUE,
                         NULL);

  gtk_container_add (GTK_CONTAINER (gtk_info_bar_get_action_area (GTK_INFO_BAR (self))), button);
}
