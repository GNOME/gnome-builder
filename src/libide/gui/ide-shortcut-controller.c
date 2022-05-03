/* ide-shortcut-controller.c
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

#define G_LOG_DOMAIN "ide-shortcut-controller"

#include "config.h"

#include "ide-gui-global.h"
#include "ide-shortcut-controller-private.h"
#include "ide-shortcut-model-private.h"

static gboolean
has_context_property (GObject *object)
{
  GParamSpec *pspec;

  if (object == NULL)
    return FALSE;

  if (!(pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (object), "context")))
    return FALSE;

  return G_IS_PARAM_SPEC_OBJECT (pspec) && g_type_is_a (pspec->value_type, IDE_TYPE_CONTEXT);
}


GtkEventController *
ide_shortcut_controller_new_for_window (GtkWindow *window)
{
  g_autoptr(IdeShortcutModel) model = NULL;
  IdeContext *context;

  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

  model = ide_shortcut_model_new ();
  context = ide_widget_get_context (GTK_WIDGET (window));

  if (has_context_property (G_OBJECT (window)))
    g_object_bind_property (window, "context", model, "context", 0);

  if (context != NULL)
    ide_shortcut_model_set_context (model, context);

  return gtk_shortcut_controller_new_for_model (G_LIST_MODEL (model));
}
