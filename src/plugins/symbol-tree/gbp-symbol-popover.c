/* gbp-symbol-popover.c
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

#define G_LOG_DOMAIN "gbp-symbol-popover"

#include "config.h"

#include "gbp-symbol-popover.h"

struct _GbpSymbolPopover
{
  GtkPopover         parent_instance;

  IdeSymbolResolver *symbol_resolver;

  GtkSearchEntry    *search_entry;
  GtkListView       *list_view;
};

enum {
  PROP_0,
  PROP_SYMBOL_RESOLVER,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpSymbolPopover, gbp_symbol_popover, GTK_TYPE_POPOVER)

static GParamSpec *properties [N_PROPS];

static void
gbp_symbol_popover_activate_cb (GbpSymbolPopover *self,
                                guint             position,
                                GtkListView      *list_view)
{
  IDE_ENTRY;

  g_assert (GBP_IS_SYMBOL_POPOVER (self));
  g_assert (GTK_IS_LIST_VIEW (list_view));

  g_debug ("Activating symbol row at position %u", position);

  IDE_EXIT;
}

static void
gbp_symbol_popover_reload (GbpSymbolPopover *self)
{
  IDE_ENTRY;

  g_assert (GBP_IS_SYMBOL_POPOVER (self));

  if (self->symbol_resolver == NULL)
    {
      gtk_list_view_set_model (self->list_view, NULL);
      IDE_EXIT;
    }

  IDE_EXIT;
}

static void
gbp_symbol_popover_dispose (GObject *object)
{
  GbpSymbolPopover *self = (GbpSymbolPopover *)object;

  g_clear_object (&self->symbol_resolver);

  G_OBJECT_CLASS (gbp_symbol_popover_parent_class)->dispose (object);
}

static void
gbp_symbol_popover_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GbpSymbolPopover *self = GBP_SYMBOL_POPOVER (object);

  switch (prop_id)
    {
    case PROP_SYMBOL_RESOLVER:
      g_value_set_object (value, gbp_symbol_popover_get_symbol_resolver (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_symbol_popover_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GbpSymbolPopover *self = GBP_SYMBOL_POPOVER (object);

  switch (prop_id)
    {
    case PROP_SYMBOL_RESOLVER:
      gbp_symbol_popover_set_symbol_resolver (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_symbol_popover_class_init (GbpSymbolPopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gbp_symbol_popover_dispose;
  object_class->get_property = gbp_symbol_popover_get_property;
  object_class->set_property = gbp_symbol_popover_set_property;

  properties [PROP_SYMBOL_RESOLVER] =
    g_param_spec_object ("symbol-resolver",
                         "Symbol Resolver",
                         "The symbol resolver to build a tree from",
                         IDE_TYPE_SYMBOL_RESOLVER,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/symbol-tree/gbp-symbol-popover.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpSymbolPopover, search_entry);
  gtk_widget_class_bind_template_callback (widget_class, gbp_symbol_popover_activate_cb);
}

static void
gbp_symbol_popover_init (GbpSymbolPopover *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
gbp_symbol_popover_new (void)
{
  return g_object_new (GBP_TYPE_SYMBOL_POPOVER, NULL);
}

IdeSymbolResolver *
gbp_symbol_popover_get_symbol_resolver (GbpSymbolPopover *self)
{
  g_return_val_if_fail (GBP_IS_SYMBOL_POPOVER (self), NULL);
  g_return_val_if_fail (!self->symbol_resolver ||
                        IDE_IS_SYMBOL_RESOLVER (self->symbol_resolver), NULL);

  return self->symbol_resolver;
}

void
gbp_symbol_popover_set_symbol_resolver (GbpSymbolPopover  *self,
                                        IdeSymbolResolver *symbol_resolver)
{
  g_return_if_fail (GBP_IS_SYMBOL_POPOVER (self));
  g_return_if_fail (!symbol_resolver || IDE_IS_SYMBOL_RESOLVER (symbol_resolver));

  if (g_set_object (&self->symbol_resolver, symbol_resolver))
    {
      gbp_symbol_popover_reload (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SYMBOL_RESOLVER]);
    }
}
