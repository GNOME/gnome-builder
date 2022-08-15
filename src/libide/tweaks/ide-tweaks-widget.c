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

typedef struct
{
  IdeTweaksWidget *cloned;
} IdeTweaksWidgetPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeTweaksWidget, ide_tweaks_widget, IDE_TYPE_TWEAKS_ITEM)

enum {
  CREATE,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

static IdeTweaksItem *
ide_tweaks_widget_copy (IdeTweaksItem *item)
{
  IdeTweaksWidget *self = (IdeTweaksWidget *)item;
  IdeTweaksWidgetPrivate *copy_priv;
  IdeTweaksItem *copy;

  g_assert (IDE_IS_TWEAKS_WIDGET (self));

  copy = IDE_TWEAKS_ITEM_CLASS (ide_tweaks_widget_parent_class)->copy (item);
  copy_priv = ide_tweaks_widget_get_instance_private (IDE_TWEAKS_WIDGET (copy));
  g_set_weak_pointer (&copy_priv->cloned, self);

  return copy;
}

static void
ide_tweaks_widget_dispose (GObject *object)
{
  IdeTweaksWidget *self = (IdeTweaksWidget *)object;
  IdeTweaksWidgetPrivate *priv = ide_tweaks_widget_get_instance_private (self);

  g_clear_weak_pointer (&priv->cloned);

  G_OBJECT_CLASS (ide_tweaks_widget_parent_class)->dispose (object);
}

static void
ide_tweaks_widget_class_init (IdeTweaksWidgetClass *klass)
{
  IdeTweaksItemClass *item_class = IDE_TWEAKS_ITEM_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  item_class->copy = ide_tweaks_widget_copy;

  object_class->dispose = ide_tweaks_widget_dispose;

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
  IdeTweaksWidgetPrivate *priv = ide_tweaks_widget_get_instance_private (self);
  GtkWidget *ret = NULL;

  g_return_val_if_fail (IDE_IS_TWEAKS_WIDGET (self), NULL);

  if (priv->cloned != NULL)
    return _ide_tweaks_widget_create (priv->cloned);

  g_signal_emit (self, signals [CREATE], 0, &ret);

  g_return_val_if_fail (!ret || GTK_IS_WIDGET (ret), NULL);

  return ret;
}
