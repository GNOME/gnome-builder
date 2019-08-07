/* gbp-gaction-command.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-gaction-command"

#include "config.h"

#include "gbp-gaction-command.h"

struct _GbpGactionCommand
{
  IdeObject  parent_instance;
  GtkWidget *widget;
  gchar     *group;
  gchar     *name;
  GVariant  *param;
  gchar     *title;
  gint       priority;
};

static void
gbp_gaction_command_run_async (IdeCommand          *command,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  GbpGactionCommand *self = (GbpGactionCommand *)command;
  g_autoptr(IdeTask) task = NULL;

  g_assert (GBP_IS_GACTION_COMMAND (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_gaction_command_run_async);

  if (self->widget != NULL)
    dzl_gtk_widget_action (self->widget, self->group, self->name, self->param);

  ide_task_return_boolean (task, TRUE);
}

static gboolean
gbp_gaction_command_run_finish (IdeCommand    *command,
                                GAsyncResult  *result,
                                GError       **error)
{
  g_assert (GBP_IS_GACTION_COMMAND (command));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static gchar *
gbp_gaction_command_get_title (IdeCommand *command)
{
  GbpGactionCommand *self = (GbpGactionCommand *)command;

  g_assert (GBP_IS_GACTION_COMMAND (self));

  return g_strdup (self->title);
}

static gint
gbp_gaction_command_get_priority (IdeCommand *command)
{
  return GBP_GACTION_COMMAND (command)->priority;
}

static void
command_iface_init (IdeCommandInterface *iface)
{
  iface->run_async = gbp_gaction_command_run_async;
  iface->run_finish = gbp_gaction_command_run_finish;
  iface->get_title = gbp_gaction_command_get_title;
  iface->get_priority = gbp_gaction_command_get_priority;
}

G_DEFINE_TYPE_WITH_CODE (GbpGactionCommand, gbp_gaction_command, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_COMMAND, command_iface_init))

static void
gbp_gaction_command_finalize (GObject *object)
{
  GbpGactionCommand *self = (GbpGactionCommand *)object;

  g_clear_pointer (&self->group, g_free);
  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->param, g_variant_unref);
  g_clear_pointer (&self->title, g_free);

  if (self->widget != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->widget,
                                            G_CALLBACK (gtk_widget_destroyed),
                                            &self->widget);
      self->widget = NULL;
    }

  G_OBJECT_CLASS (gbp_gaction_command_parent_class)->finalize (object);
}

static gchar *
gbp_gaction_command_repr (IdeObject *object)
{
  GbpGactionCommand *self = (GbpGactionCommand *)object;

  return g_strdup_printf ("%s action=%s.%s",
                          G_OBJECT_TYPE_NAME (self),
                          self->group,
                          self->name);
}

static void
gbp_gaction_command_class_init (GbpGactionCommandClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->finalize = gbp_gaction_command_finalize;

  i_object_class->repr = gbp_gaction_command_repr;
}

static void
gbp_gaction_command_init (GbpGactionCommand *self)
{
}

GbpGactionCommand *
gbp_gaction_command_new (GtkWidget   *widget,
                         const gchar *group,
                         const gchar *name,
                         GVariant    *param,
                         const gchar *title,
                         gint         priority)
{
  GbpGactionCommand *self;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (group != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);

  self = g_object_new (GBP_TYPE_GACTION_COMMAND, NULL);
  self->widget = widget;
  self->group = g_strdup (group);
  self->name = g_strdup (name);
  self->param = param ? g_variant_ref_sink (param) : NULL;
  self->title = g_strdup (title);
  self->priority = priority;

  g_signal_connect (self->widget,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->widget);

  return g_steal_pointer (&self);
}
