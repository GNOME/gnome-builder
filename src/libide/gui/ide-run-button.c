/* ide-run-button.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-run-button"

#include "config.h"

#include <dazzle.h>
#include <glib/gi18n.h>
#include <libide-foundry.h>

#include "ide-gui-global.h"
#include "ide-run-button.h"

#include "ide-run-manager-private.h"

struct _IdeRunButton
{
  GtkBox                parent_instance;

  GtkButton            *button;
  GtkImage             *button_image;
  DzlMenuButton        *menu_button;
  GtkButton            *stop_button;
  GtkShortcutsShortcut *run_shortcut;
  GtkLabel             *run_tooltip_message;
  DzlShortcutTooltip   *tooltip;
};

G_DEFINE_TYPE (IdeRunButton, ide_run_button, GTK_TYPE_BOX)

static void
ide_run_button_handler_set (IdeRunButton  *self,
                            GParamSpec    *pspec,
                            IdeRunManager *run_manager)
{
  const GList *list;
  const GList *iter;
  const gchar *handler;

  g_assert (IDE_IS_RUN_BUTTON (self));
  g_assert (IDE_IS_RUN_MANAGER (run_manager));

  handler = ide_run_manager_get_handler (run_manager);
  list = _ide_run_manager_get_handlers (run_manager);

  for (iter = list; iter; iter = iter->next)
    {
      const IdeRunHandlerInfo *info = iter->data;

      if (g_strcmp0 (info->id, handler) == 0)
        {
          g_object_set (self->button_image,
                        "icon-name", info->icon_name,
                        NULL);
          break;
        }
    }
}

static void
ide_run_button_load (IdeRunButton *self,
                     IdeContext   *context)
{
  IdeRunManager *run_manager;

  g_assert (IDE_IS_RUN_BUTTON (self));
  g_assert (IDE_IS_CONTEXT (context));

  run_manager = ide_run_manager_from_context (context);

  g_object_bind_property (run_manager, "busy", self->button, "visible",
                          G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
  g_object_bind_property (run_manager, "busy", self->stop_button, "visible",
                          G_BINDING_SYNC_CREATE);

  g_signal_connect_object (run_manager,
                           "notify::handler",
                           G_CALLBACK (ide_run_button_handler_set),
                           self,
                           G_CONNECT_SWAPPED);

  ide_run_button_handler_set (self, NULL, run_manager);
}

static void
ide_run_button_context_set (GtkWidget  *widget,
                            IdeContext *context)
{
  IdeRunButton *self = (IdeRunButton *)widget;

  g_assert (IDE_IS_RUN_BUTTON (self));
  g_assert (!context || IDE_IS_CONTEXT (context));

  if (context != NULL)
    ide_run_button_load (self, context);
}

static gboolean
ide_run_button_query_tooltip (IdeRunButton *self,
                              gint          x,
                              gint          y,
                              gboolean      keyboard_tooltip,
                              GtkTooltip   *tooltip,
                              GtkButton    *button)
{
  IdeRunManager *run_manager;
  const GList *list;
  const GList *iter;
  const gchar *handler;
  IdeContext *context;

  g_assert (IDE_IS_RUN_BUTTON (self));
  g_assert (GTK_IS_TOOLTIP (tooltip));
  g_assert (GTK_IS_BUTTON (button));

  context = ide_widget_get_context (GTK_WIDGET (self));
  run_manager = ide_run_manager_from_context (context);
  handler = ide_run_manager_get_handler (run_manager);
  list = _ide_run_manager_get_handlers (run_manager);

  for (iter = list; iter; iter = iter->next)
    {
      const IdeRunHandlerInfo *info = iter->data;

      if (g_strcmp0 (info->id, handler) == 0)
        {
          gboolean enabled;
          /* Figure out if the run action is enabled. If it
           * is not, then we should inform the user that
           * the project cannot be run yet because the
           * build pipeline is not yet configured. */
          g_action_group_query_action (G_ACTION_GROUP (run_manager),
                                       "run",
                                       &enabled,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL);

          if (!enabled)
            {
              gtk_tooltip_set_custom (tooltip, GTK_WIDGET (self->run_tooltip_message));
              return TRUE;
            }

          /* The shortcut tooltip will set this up after us */
          dzl_shortcut_tooltip_set_accel (self->tooltip, info->accel);
          dzl_shortcut_tooltip_set_title (self->tooltip, info->title);
        }
    }

  return FALSE;
}

static void
ide_run_button_class_init (IdeRunButtonClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-gui/ui/ide-run-button.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeRunButton, button);
  gtk_widget_class_bind_template_child (widget_class, IdeRunButton, button_image);
  gtk_widget_class_bind_template_child (widget_class, IdeRunButton, menu_button);
  gtk_widget_class_bind_template_child (widget_class, IdeRunButton, run_shortcut);
  gtk_widget_class_bind_template_child (widget_class, IdeRunButton, stop_button);
  gtk_widget_class_bind_template_child (widget_class, IdeRunButton, run_tooltip_message);
  gtk_widget_class_bind_template_child (widget_class, IdeRunButton, tooltip);
}

static void
ide_run_button_init (IdeRunButton *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->button,
                           "query-tooltip",
                           G_CALLBACK (ide_run_button_query_tooltip),
                           self,
                           G_CONNECT_SWAPPED);

  ide_widget_set_context_handler (self, ide_run_button_context_set);
}
