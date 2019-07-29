/* ide-gui-global.h
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

#pragma once

#include <gtk/gtk.h>
#include <libide-core.h>

#include "ide-workbench.h"

G_BEGIN_DECLS

#ifdef __cplusplus
#define ide_widget_warning(instance, format, ...)                                                   \
  G_STMT_START {                                                                                    \
    IdeContext *context = ide_widget_get_context (GTK_WIDGET (instance));                           \
    ide_context_log (context, G_LOG_LEVEL_WARNING, G_LOG_DOMAIN, format __VA_OPT__(,) __VA_ARGS__); \
  } G_STMT_END
#else
#define ide_widget_warning(instance, format, ...)                                                   \
  G_STMT_START {                                                                                    \
    IdeContext *context = ide_widget_get_context (GTK_WIDGET (instance));                           \
    ide_context_log (context, G_LOG_LEVEL_WARNING, G_LOG_DOMAIN, format, ##__VA_ARGS__);            \
  } G_STMT_END
#endif

typedef void (*IdeWidgetContextHandler) (GtkWidget  *widget,
                                         IdeContext *context);

IDE_AVAILABLE_IN_3_32
void          ide_widget_set_context_handler (gpointer                  widget,
                                              IdeWidgetContextHandler   handler);
IDE_AVAILABLE_IN_3_32
IdeContext   *ide_widget_get_context         (GtkWidget                *widget);
IDE_AVAILABLE_IN_3_32
void          ide_widget_reveal_and_grab     (GtkWidget                *widget);
IDE_AVAILABLE_IN_3_32
IdeWorkbench *ide_widget_get_workbench       (GtkWidget                *widget);
IDE_AVAILABLE_IN_3_32
IdeWorkspace *ide_widget_get_workspace       (GtkWidget                *widget);
IDE_AVAILABLE_IN_3_32
gboolean      ide_gtk_show_uri_on_window     (GtkWindow                *window,
                                              const gchar              *uri,
                                              gint64                    timestamp,
                                              GError                  **error);
IDE_AVAILABLE_IN_3_32
void          ide_gtk_window_present         (GtkWindow                *window);

G_END_DECLS
