/* ide-run-tool.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-run-tool"

#include "config.h"

#include <glib/gi18n.h>

#include "ide-marshal.h"

#include "ide-pipeline.h"
#include "ide-run-command.h"
#include "ide-run-context.h"
#include "ide-run-tool-private.h"

typedef struct
{
  IdeSubprocess *subprocess;
  char *icon_name;
} IdeRunToolPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (IdeRunTool, ide_run_tool, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ICON_NAME,
  N_PROPS
};

enum {
  STARTED,
  STOPPED,
  N_SIGNALS
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

static void
ide_run_tool_real_force_exit (IdeRunTool *self)
{
  IdeRunToolPrivate *priv = ide_run_tool_get_instance_private (self);

  g_assert (IDE_IS_RUN_TOOL (self));

  if (priv->subprocess == NULL)
    return;

  ide_object_message (IDE_OBJECT (self),
                      _("Forcing subprocess %s to exit"),
                      ide_subprocess_get_identifier (priv->subprocess));

  ide_subprocess_force_exit (priv->subprocess);
}

static void
ide_run_tool_real_send_signal (IdeRunTool *self,
                               int         signum)
{
  IdeRunToolPrivate *priv = ide_run_tool_get_instance_private (self);

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_RUN_TOOL (self));

  ide_object_message (IDE_OBJECT (self),
                      _("Sending signal %d to subprocess %s"),
                      signum,
                      ide_subprocess_get_identifier (priv->subprocess));

  if (priv->subprocess != NULL)
    ide_subprocess_send_signal (priv->subprocess, signum);
}

static void
ide_run_tool_destroy (IdeObject *object)
{
  IdeRunTool *self = (IdeRunTool *)object;
  IdeRunToolPrivate *priv = ide_run_tool_get_instance_private (self);

  g_clear_object (&priv->subprocess);
  g_clear_pointer (&priv->icon_name, g_free);

  IDE_OBJECT_CLASS (ide_run_tool_parent_class)->destroy (object);
}

static void
ide_run_tool_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  IdeRunTool *self = IDE_RUN_TOOL (object);

  switch (prop_id)
    {
    case PROP_ICON_NAME:
      g_value_set_string (value, ide_run_tool_get_icon_name (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_run_tool_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  IdeRunTool *self = IDE_RUN_TOOL (object);

  switch (prop_id)
    {
    case PROP_ICON_NAME:
      ide_run_tool_set_icon_name (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_run_tool_class_init (IdeRunToolClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->get_property = ide_run_tool_get_property;
  object_class->set_property = ide_run_tool_set_property;

  i_object_class->destroy = ide_run_tool_destroy;

  klass->force_exit = ide_run_tool_real_force_exit;
  klass->send_signal = ide_run_tool_real_send_signal;

  signals[STARTED] =
    g_signal_new ("started",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeRunToolClass, started),
                  NULL, NULL,
                  ide_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, IDE_TYPE_SUBPROCESS);
  g_signal_set_va_marshaller (signals [STARTED],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__OBJECTv);

  signals[STOPPED] =
    g_signal_new ("stopped",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeRunToolClass, stopped),
                  NULL, NULL,
                  ide_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  g_signal_set_va_marshaller (signals [STOPPED],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__VOIDv);
}

static void
ide_run_tool_init (IdeRunTool *self)
{
}

void
ide_run_tool_force_exit (IdeRunTool *self)
{
  g_return_if_fail (IDE_IS_RUN_TOOL (self));

  IDE_RUN_TOOL_GET_CLASS (self)->force_exit (self);
}

void
ide_run_tool_send_signal (IdeRunTool *self,
                          int         signum)
{
  g_return_if_fail (IDE_IS_RUN_TOOL (self));

  IDE_RUN_TOOL_GET_CLASS (self)->send_signal (self, signum);
}

void
ide_run_tool_prepare_to_run (IdeRunTool    *self,
                             IdePipeline   *pipeline,
                             IdeRunCommand *run_command,
                             IdeRunContext *run_context)
{
  g_return_if_fail (IDE_IS_RUN_TOOL (self));
  g_return_if_fail (IDE_IS_PIPELINE (pipeline));
  g_return_if_fail (IDE_IS_RUN_COMMAND (run_command));
  g_return_if_fail (IDE_IS_RUN_CONTEXT (run_context));

  if (IDE_RUN_TOOL_GET_CLASS (self)->prepare_to_run)
    IDE_RUN_TOOL_GET_CLASS (self)->prepare_to_run (self, pipeline, run_command, run_context);
}

void
_ide_run_tool_emit_started (IdeRunTool    *self,
                            IdeSubprocess *subprocess)
{
  IdeRunToolPrivate *priv = ide_run_tool_get_instance_private (self);

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_RUN_TOOL (self));

  g_debug ("%s started", G_OBJECT_TYPE_NAME (self));
  g_set_object (&priv->subprocess, subprocess);
  g_signal_emit (self, signals[STARTED], 0, subprocess);

  IDE_EXIT;
}

void
_ide_run_tool_emit_stopped (IdeRunTool *self)
{
  IdeRunToolPrivate *priv = ide_run_tool_get_instance_private (self);

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_RUN_TOOL (self));

  g_debug ("%s stopped", G_OBJECT_TYPE_NAME (self));
  g_clear_object (&priv->subprocess);
  g_signal_emit (self, signals[STOPPED], 0);

  IDE_EXIT;
}

const char *
ide_run_tool_get_icon_name (IdeRunTool *self)
{
  IdeRunToolPrivate *priv = ide_run_tool_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_RUN_TOOL (self), NULL);

  return priv->icon_name;
}

void
ide_run_tool_set_icon_name (IdeRunTool *self,
                            const char *icon_name)
{
  IdeRunToolPrivate *priv = ide_run_tool_get_instance_private (self);

  g_return_if_fail (IDE_IS_RUN_TOOL (self));

  if (!g_set_str (&priv->icon_name, icon_name))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON_NAME]);
}
