/* gbp-vim-command.c
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

#define G_LOG_DOMAIN "gbp-vim-command"

#include "config.h"

#include <libide-gui.h>
#include <libide-sourceview.h>

#include "gbp-vim-command.h"
#include "gb-vim.h"

struct _GbpVimCommand
{
  IdeObject  parent_instance;
  GtkWidget *active_widget;
  gchar     *typed_text;
  gchar     *command;
  gchar     *description;
  gint       priority;
};

static gint
gbp_vim_command_get_priority (IdeCommand *command)
{
  return GBP_VIM_COMMAND (command)->priority;
}

static gchar *
gbp_vim_command_get_title (IdeCommand *command)
{
  GbpVimCommand *self = (GbpVimCommand *)command;

  return g_strdup (self->command);
}

static gchar *
gbp_vim_command_get_subtitle (IdeCommand *command)
{
  GbpVimCommand *self = (GbpVimCommand *)command;

  return g_strdup (self->description);
}

static void
gbp_vim_command_run_async (IdeCommand          *command,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  GbpVimCommand *self = (GbpVimCommand *)command;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_VIM_COMMAND (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_vim_command_run_async);

  if (!gb_vim_execute (self->active_widget, self->typed_text, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);
}

static gboolean
gbp_vim_command_run_finish (IdeCommand    *command,
                            GAsyncResult  *result,
                            GError       **error)
{
  g_assert (GBP_IS_VIM_COMMAND (command));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
command_iface_init (IdeCommandInterface *iface)
{
  iface->get_title = gbp_vim_command_get_title;
  iface->get_subtitle = gbp_vim_command_get_subtitle;
  iface->run_async = gbp_vim_command_run_async;
  iface->run_finish = gbp_vim_command_run_finish;
  iface->get_priority = gbp_vim_command_get_priority;
}

G_DEFINE_TYPE_WITH_CODE (GbpVimCommand, gbp_vim_command, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_COMMAND, command_iface_init))

static void
gbp_vim_command_finalize (GObject *object)
{
  GbpVimCommand *self = (GbpVimCommand *)object;

  g_clear_pointer (&self->typed_text, g_free);
  g_clear_pointer (&self->command, g_free);
  g_clear_pointer (&self->description, g_free);
  g_clear_object (&self->active_widget);

  G_OBJECT_CLASS (gbp_vim_command_parent_class)->finalize (object);
}

static void
gbp_vim_command_class_init (GbpVimCommandClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_vim_command_finalize;
}

static void
gbp_vim_command_init (GbpVimCommand *self)
{
}

GbpVimCommand *
gbp_vim_command_new (GtkWidget   *active_widget,
                     const gchar *typed_text,
                     const gchar *command,
                     const gchar *description)
{
  g_autoptr(GbpVimCommand) ret = NULL;
  guint priority = G_MAXINT;

  ret = g_object_new (GBP_TYPE_VIM_COMMAND, NULL);
  ret->active_widget = g_object_ref (active_widget);
  ret->typed_text = g_strdup (typed_text);
  ret->command = g_strdup (command);
  ret->description = g_strdup (description);

  ide_completion_fuzzy_match (command, typed_text, &priority);
  ret->priority = priority;

  return g_steal_pointer (&ret);
}
