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

#include <glib/gi18n.h>

#include <libide-foundry.h>

#include "ide-application.h"
#include "ide-gui-global.h"
#include "ide-run-button.h"
#include "ide-run-manager-private.h"

struct _IdeRunButton
{
  GtkWidget       parent_instance;
  AdwSplitButton *split_button;
  char           *run_handler_icon_name;
};

G_DEFINE_FINAL_TYPE (IdeRunButton, ide_run_button, GTK_TYPE_WIDGET)

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
          self->run_handler_icon_name = g_strdup (info->icon_name);
          g_object_set (self->split_button, "icon-name", info->icon_name, NULL);
          break;
        }
    }
}

static void
on_run_busy_state_changed_cb (IdeRunButton  *self,
                              GParamSpec    *pspec,
                              IdeRunManager *run_manager)
{
  const char *icon_name;
  const char *action_name;

  g_assert (IDE_IS_RUN_BUTTON (self));
  g_assert (IDE_IS_RUN_MANAGER (run_manager));

  if (!ide_run_manager_get_busy (run_manager))
    {
      icon_name = self->run_handler_icon_name;
      action_name = "run-manager.run";
    }
  else
    {
      icon_name = "builder-run-stop-symbolic";
      action_name = "run-manager.stop";
    }

  g_object_set (self->split_button, "icon-name", icon_name, NULL);
  gtk_actionable_set_action_name (GTK_ACTIONABLE (self->split_button), action_name);
}

static void
ide_run_button_load (IdeRunButton *self,
                     IdeContext   *context)
{
  IdeRunManager *run_manager;

  g_assert (IDE_IS_RUN_BUTTON (self));
  g_assert (IDE_IS_CONTEXT (context));

  run_manager = ide_run_manager_from_context (context);

  g_signal_connect_object (run_manager,
                           "notify::busy",
                           G_CALLBACK (on_run_busy_state_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

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

  if (ide_run_manager_get_busy (run_manager))
    {
      gtk_tooltip_set_text (tooltip, _("Stop running"));
      return TRUE;
    }

  for (iter = list; iter; iter = iter->next)
    {
      const IdeRunHandlerInfo *info = iter->data;

      if (g_strcmp0 (info->id, handler) == 0)
        {
          g_autofree char *text = NULL;

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
              gtk_tooltip_set_text (tooltip, _("Invalid project configuration"));
              return TRUE;
            }

          if (info->accel && info->title)
            text = g_strdup_printf ("%s %s", info->accel, info->title);
          else if (info->title)
            text = g_strdup (info->title);

          gtk_tooltip_set_text (tooltip, text);
        }
    }

  return FALSE;
}

static void
ide_run_button_dispose (GObject *object)
{
  IdeRunButton *self = (IdeRunButton *)object;

  g_clear_pointer ((GtkWidget **)&self->split_button, gtk_widget_unparent);

  G_OBJECT_CLASS (ide_run_button_parent_class)->dispose (object);
}

static void
ide_run_button_class_init (IdeRunButtonClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_run_button_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-gui/ui/ide-run-button.ui");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_bind_template_child (widget_class, IdeRunButton, split_button);
}

static void
ide_run_button_init (IdeRunButton *self)
{
  GMenu *menu;

  gtk_widget_init_template (GTK_WIDGET (self));

  menu = ide_application_get_menu_by_id (IDE_APPLICATION_DEFAULT, "run-menu");
  adw_split_button_set_menu_model (self->split_button, G_MENU_MODEL (menu));

  g_signal_connect_object (self->split_button,
                           "query-tooltip",
                           G_CALLBACK (ide_run_button_query_tooltip),
                           self,
                           G_CONNECT_SWAPPED);

  ide_widget_set_context_handler (self, ide_run_button_context_set);
}
