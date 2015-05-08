/* gb-command-bar-item.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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
 */

#include <glib/gi18n.h>

#include "gb-command-bar-item.h"
#include "gb-widget.h"

struct _GbCommandBarItem
{
  GtkBin           parent_instance;
  GbCommandResult *result;

  GtkWidget       *command_text;
  GtkWidget       *result_text;
  GtkWidget       *equal_label;
};

G_DEFINE_TYPE (GbCommandBarItem, gb_command_bar_item, GTK_TYPE_BIN)

enum {
  PROP_0,
  PROP_RESULT,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GtkWidget *
gb_command_bar_item_new (GbCommandResult *result)
{
  return g_object_new (GB_TYPE_COMMAND_BAR_ITEM,
                       "result", result,
                       NULL);
}

/**
 * gb_command_bar_item_get_result:
 * @item: A #GbCommandBarItem.
 *
 * Retrieves the result text widget so it can be used in the command bar
 * sizing group.
 *
 * Returns: (transfer none): The result text widget.
 */
GtkWidget *
gb_command_bar_item_get_result (GbCommandBarItem *item)
{
  g_return_val_if_fail (GB_IS_COMMAND_BAR_ITEM (item), NULL);

  return item->result_text;
}

static gboolean
string_to_boolean (GBinding     *binding,
                   const GValue *from_value,
                   GValue       *to_value,
                   gpointer      user_data)
{
  g_value_set_boolean (to_value, !!g_value_get_string (from_value));
  return TRUE;
}

static void
gb_command_bar_item_set_result (GbCommandBarItem *item,
                                GbCommandResult  *result)
{
  g_return_if_fail (GB_IS_COMMAND_BAR_ITEM (item));
  g_return_if_fail (GB_IS_COMMAND_RESULT (result));

  if (item->result != result)
    {
      g_clear_object (&item->result);

      if (result)
        {
          item->result = g_object_ref (result);
          g_object_bind_property (result, "command-text",
                                  item->command_text, "label",
                                  G_BINDING_SYNC_CREATE);
          g_object_bind_property (result, "result-text",
                                  item->result_text, "label",
                                  G_BINDING_SYNC_CREATE);
          g_object_bind_property_full (result, "result-text",
                                       item->equal_label, "visible",
                                       G_BINDING_SYNC_CREATE,
                                       string_to_boolean,
                                       NULL, NULL, NULL);
        }

      g_object_notify_by_pspec (G_OBJECT (item), gParamSpecs [PROP_RESULT]);
    }
}

static void
gb_command_bar_item_dispose (GObject *object)
{
  GbCommandBarItem *self = GB_COMMAND_BAR_ITEM (object);

  g_clear_object (&self->result);

  G_OBJECT_CLASS (gb_command_bar_item_parent_class)->dispose (object);
}

static void
gb_command_bar_item_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GbCommandBarItem *self = GB_COMMAND_BAR_ITEM (object);

  switch (prop_id)
    {
    case PROP_RESULT:
      g_value_set_object (value, gb_command_bar_item_get_result (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_command_bar_item_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GbCommandBarItem *self = GB_COMMAND_BAR_ITEM (object);

  switch (prop_id)
    {
    case PROP_RESULT:
      gb_command_bar_item_set_result (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_command_bar_item_class_init (GbCommandBarItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gb_command_bar_item_dispose;
  object_class->get_property = gb_command_bar_item_get_property;
  object_class->set_property = gb_command_bar_item_set_property;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/builder/ui/gb-command-bar-item.ui");

  GB_WIDGET_CLASS_BIND (widget_class, GbCommandBarItem, command_text);
  GB_WIDGET_CLASS_BIND (widget_class, GbCommandBarItem, result_text);
  GB_WIDGET_CLASS_BIND (widget_class, GbCommandBarItem, equal_label);

  gParamSpecs [PROP_RESULT] =
    g_param_spec_object ("result",
                         _("Result"),
                         _("The result to be visualized in the item."),
                         GB_TYPE_COMMAND_RESULT,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);
}

static void
gb_command_bar_item_init (GbCommandBarItem *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
