/* ide-tweaks-widget.c
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

#define G_LOG_DOMAIN "ide-tweaks-widget"

#include "config.h"

#include "ide-tweaks-widget-private.h"

G_DEFINE_TYPE (IdeTweaksWidget, ide_tweaks_widget, IDE_TYPE_TWEAKS_ITEM)

enum {
  CREATE,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

static void
ide_tweaks_widget_class_init (IdeTweaksWidgetClass *klass)
{
  /**
   * IdeTweaksWidget::create:
   *
   * Creates a new #GtkWidget that can be inserted into the #IdeTweaksWindow
   * representing the item.
   *
   * Only the first signal handler is used.
   *
   * Returns: (transfer full) (nullable): a #GtkWidget or %NULL
   */
  signals [CREATE] =
    g_signal_new ("create",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeTweaksWidgetClass, create),
                  g_signal_accumulator_first_wins, NULL,
                  NULL,
                  GTK_TYPE_WIDGET, 0);
}

static void
ide_tweaks_widget_init (IdeTweaksWidget *self)
{
}

GtkWidget *
_ide_tweaks_widget_create (IdeTweaksWidget *self)
{
  GtkWidget *ret = NULL;

  g_return_val_if_fail (IDE_IS_TWEAKS_WIDGET (self), NULL);

  g_signal_emit (self, signals [CREATE], 0, &ret);

  g_return_val_if_fail (!ret || GTK_IS_WIDGET (ret), NULL);

  return ret;
}
