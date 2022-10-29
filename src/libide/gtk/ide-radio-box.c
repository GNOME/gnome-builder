/* ide-radio-box.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-radio-box"

#include "config.h"

#include "ide-radio-box.h"

#define N_PER_ROW 4

typedef struct
{
  gchar           *id;
  gchar           *text;
  GtkToggleButton *button;
} IdeRadioBoxItem;

struct _IdeRadioBox
{
  GtkWidget      parent;

  GArray        *items;
  gchar         *active_id;

  GtkBox        *vbox;
  GtkBox        *hbox;
  GtkRevealer   *revealer;

  guint          has_more : 1;
};

G_DEFINE_FINAL_TYPE (IdeRadioBox, ide_radio_box, GTK_TYPE_WIDGET)

enum {
  PROP_0,
  PROP_ACTIVE_ID,
  PROP_HAS_MORE,
  PROP_SHOW_MORE,
  N_PROPS
};

enum {
  CHANGED,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static gboolean
ide_radio_box_get_has_more (IdeRadioBox *self)
{
  g_return_val_if_fail (IDE_IS_RADIO_BOX (self), FALSE);

  return self->has_more;
}

static gboolean
ide_radio_box_get_show_more (IdeRadioBox *self)
{
  g_return_val_if_fail (IDE_IS_RADIO_BOX (self), FALSE);

  return gtk_revealer_get_reveal_child (self->revealer);
}

static void
ide_radio_box_set_show_more (IdeRadioBox *self,
                             gboolean     show_more)
{
  g_return_if_fail (IDE_IS_RADIO_BOX (self));

  gtk_revealer_set_reveal_child (self->revealer, show_more);
}

static void
ide_radio_box_item_clear (IdeRadioBoxItem *item)
{
  g_free (item->id);
  g_free (item->text);
}

static void
ide_radio_box_dispose (GObject *object)
{
  IdeRadioBox *self = (IdeRadioBox *)object;
  GtkWidget *child;

  while (self->items->len > 0)
    {
      g_autofree char *id = g_strdup (g_array_index (self->items, IdeRadioBoxItem, 0).id);

      ide_radio_box_remove_item (self, id);
    }

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self))))
    gtk_widget_unparent (child);

  G_OBJECT_CLASS (ide_radio_box_parent_class)->dispose (object);
}

static void
ide_radio_box_finalize (GObject *object)
{
  IdeRadioBox *self = (IdeRadioBox *)object;

  g_clear_pointer (&self->items, g_array_unref);
  g_clear_pointer (&self->active_id, g_free);

  G_OBJECT_CLASS (ide_radio_box_parent_class)->finalize (object);
}

static void
ide_radio_box_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  IdeRadioBox *self = IDE_RADIO_BOX (object);

  switch (prop_id)
    {
    case PROP_ACTIVE_ID:
      g_value_set_string (value, ide_radio_box_get_active_id (self));
      break;

    case PROP_HAS_MORE:
      g_value_set_boolean (value, ide_radio_box_get_has_more (self));
      break;

    case PROP_SHOW_MORE:
      g_value_set_boolean (value, ide_radio_box_get_show_more (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_radio_box_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  IdeRadioBox *self = IDE_RADIO_BOX (object);

  switch (prop_id)
    {
    case PROP_ACTIVE_ID:
      ide_radio_box_set_active_id (self, g_value_get_string (value));
      break;

    case PROP_SHOW_MORE:
      ide_radio_box_set_show_more (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_radio_box_class_init (IdeRadioBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_radio_box_dispose;
  object_class->finalize = ide_radio_box_finalize;
  object_class->get_property = ide_radio_box_get_property;
  object_class->set_property = ide_radio_box_set_property;

  properties [PROP_ACTIVE_ID] =
    g_param_spec_string ("active-id",
                         "Active Id",
                         "Active Id",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_HAS_MORE] =
    g_param_spec_boolean ("has-more",
                         "Has More",
                         "Has more items to view",
                         FALSE,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SHOW_MORE] =
    g_param_spec_boolean ("show-more",
                          "Show More",
                          "Show additional items",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [CHANGED] =
    g_signal_new ("changed", G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  gtk_widget_class_set_css_name (widget_class, "radiobox");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

static void
ide_radio_box_init (IdeRadioBox *self)
{
  g_autoptr(GSimpleActionGroup) group = g_simple_action_group_new ();
  g_autoptr(GPropertyAction) action = NULL;
  GtkWidget *vbox;

  /* GPropertyAction doesn't like NULL strings */
  self->active_id = g_strdup ("");

  self->items = g_array_new (FALSE, FALSE, sizeof (IdeRadioBoxItem));
  g_array_set_clear_func (self->items, (GDestroyNotify)ide_radio_box_item_clear);

  vbox = g_object_new (GTK_TYPE_BOX,
                       "orientation", GTK_ORIENTATION_VERTICAL,
                       "visible", TRUE,
                       NULL);
  gtk_widget_set_parent (vbox, GTK_WIDGET (self));

  self->hbox = g_object_new (GTK_TYPE_BOX,
                             "orientation", GTK_ORIENTATION_HORIZONTAL,
                             "visible", TRUE,
                             NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self->hbox)), "linked");
  gtk_box_append (GTK_BOX (vbox), GTK_WIDGET (self->hbox));

  self->revealer = g_object_new (GTK_TYPE_REVEALER,
                                 "reveal-child", FALSE,
                                 "visible", TRUE,
                                 NULL);
  gtk_box_append (GTK_BOX (vbox), GTK_WIDGET (self->revealer));

  self->vbox = g_object_new (GTK_TYPE_BOX,
                             "orientation", GTK_ORIENTATION_VERTICAL,
                             "margin-top", 12,
                             "spacing", 12,
                             "visible", TRUE,
                             NULL);
  gtk_revealer_set_child (self->revealer, GTK_WIDGET (self->vbox));

  action = g_property_action_new ("active", self, "active-id");
  g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));
  gtk_widget_insert_action_group (GTK_WIDGET (self), "radiobox", G_ACTION_GROUP (group));
}

void
ide_radio_box_remove_item (IdeRadioBox *self,
                           const gchar *id)
{
  g_return_if_fail (IDE_IS_RADIO_BOX (self));
  g_return_if_fail (id != NULL);

  for (guint i = 0; i < self->items->len; i++)
    {
      IdeRadioBoxItem *item = &g_array_index (self->items, IdeRadioBoxItem, i);

      if (g_strcmp0 (id, item->id) == 0)
        {
          GtkToggleButton *button = item->button;
          GtkWidget *parent = gtk_widget_get_parent (GTK_WIDGET (button));

          g_array_remove_index_fast (self->items, i);
          gtk_box_remove (GTK_BOX (parent), GTK_WIDGET (button));

          break;
        }
    }
}

void
ide_radio_box_add_item (IdeRadioBox *self,
                        const gchar *id,
                        const gchar *text)
{
  IdeRadioBoxItem item = { 0 };
  guint precount;

  g_return_if_fail (IDE_IS_RADIO_BOX (self));
  g_return_if_fail (id != NULL);
  g_return_if_fail (text != NULL);

  precount = self->items->len;

  for (guint i = 0; i < precount; ++i)
    {
      /* Avoid duplicate items */
      if (!g_strcmp0 (g_array_index (self->items, IdeRadioBoxItem, i).id, id))
        return;
    }

  item.id = g_strdup (id);
  item.text = g_strdup (text);
  item.button = g_object_new (GTK_TYPE_TOGGLE_BUTTON,
                              "active", (g_strcmp0 (id, self->active_id) == 0),
                              "action-name", "radiobox.active",
                              "action-target", g_variant_new_string (id),
                              "hexpand", TRUE,
                              "label", text,
                              "visible", TRUE,
                              NULL);

  g_array_append_val (self->items, item);

  if (precount > 0 && (precount % N_PER_ROW) == 0)
    {
      gboolean show_more = ide_radio_box_get_show_more (self);
      gboolean visible = !self->has_more || show_more;

      self->has_more = self->items->len > N_PER_ROW;
      self->hbox = g_object_new (GTK_TYPE_BOX,
                                 "orientation", GTK_ORIENTATION_HORIZONTAL,
                                 "visible", visible,
                                 NULL);
      gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self->hbox)), "linked");
      gtk_box_append (GTK_BOX (self->vbox), GTK_WIDGET (self->hbox));
    }

  gtk_box_append (GTK_BOX (self->hbox), GTK_WIDGET (item.button));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HAS_MORE]);

  /* If this is the first item and no active id has been set,
   * then go ahead and set the active item to this one.
   */
  if (self->items->len == 1 && (!self->active_id || !*self->active_id))
    ide_radio_box_set_active_id (self, id);
}

void
ide_radio_box_set_active_id (IdeRadioBox *self,
                             const gchar *id)
{
  g_return_if_fail (IDE_IS_RADIO_BOX (self));

  if (id == NULL)
    id = "";

  if (g_set_str (&self->active_id, id))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ACTIVE_ID]);
      g_signal_emit (self, signals [CHANGED], 0);
    }
}

const gchar *
ide_radio_box_get_active_id (IdeRadioBox *self)
{
  g_return_val_if_fail (IDE_IS_RADIO_BOX (self), NULL);

  return self->active_id;
}

GtkWidget *
ide_radio_box_new (void)
{
  return g_object_new (IDE_TYPE_RADIO_BOX, NULL);
}
