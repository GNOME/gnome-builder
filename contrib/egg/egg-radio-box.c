/* egg-radio-box.c
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

#define G_LOG_DOMAIN "egg-radio-box"

#include "egg-radio-box.h"

/*
 * XXX: Ideally we would manage all the size requests ourselves. However,
 *      that takes some more work to do correctly (and support stuff like
 *      linked, etc).
 */
#define N_PER_ROW 4

typedef struct
{
  gchar           *id;
  gchar           *text;
  GtkToggleButton *button;
} EggRadioBoxItem;

typedef struct
{
  GArray        *items;
  GSimpleAction *active_action;

  GtkBox        *vbox;
  GtkBox        *hbox;

  guint          n_in_hbox;
  guint          has_more : 1;
  guint          show_more : 1;
} EggRadioBoxPrivate;

static void buildable_iface_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_EXTENDED (EggRadioBox, egg_radio_box, GTK_TYPE_BIN, 0,
                        G_ADD_PRIVATE (EggRadioBox)
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, buildable_iface_init))

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
egg_radio_box_get_has_more (EggRadioBox *self)
{
  EggRadioBoxPrivate *priv = egg_radio_box_get_instance_private (self);

  g_return_val_if_fail (EGG_IS_RADIO_BOX (self), FALSE);

  return priv->has_more;
}

static gboolean
egg_radio_box_get_show_more (EggRadioBox *self)
{
  EggRadioBoxPrivate *priv = egg_radio_box_get_instance_private (self);

  g_return_val_if_fail (EGG_IS_RADIO_BOX (self), FALSE);

  return priv->show_more;
}

static void
show_first_item (GtkWidget *widget,
                 gpointer   user_data)
{
  gboolean *toggled = user_data;

  if (*toggled == TRUE)
    gtk_widget_hide (widget);
  else
    *toggled = TRUE;
}

static void
egg_radio_box_set_show_more (EggRadioBox *self,
                             gboolean     show_more)
{
  EggRadioBoxPrivate *priv = egg_radio_box_get_instance_private (self);
  gboolean toggled = FALSE;

  g_return_if_fail (EGG_IS_RADIO_BOX (self));

  if (show_more)
    gtk_widget_show_all (GTK_WIDGET (priv->vbox));
  else
    gtk_container_foreach (GTK_CONTAINER (priv->vbox),
                           show_first_item,
                           &toggled);

  priv->show_more = !!show_more;
}

static void
egg_radio_box_item_clear (EggRadioBoxItem *item)
{
  g_free (item->id);
  g_free (item->text);
}

static void
egg_radio_box_active_action (GSimpleAction *action,
                             GVariant      *variant,
                             gpointer       user_data)
{
  EggRadioBox *self = user_data;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (variant != NULL);
  g_assert (g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING));
  g_assert (EGG_IS_RADIO_BOX (self));

  egg_radio_box_set_active_id (self, g_variant_get_string (variant, NULL));
}

static void
egg_radio_box_finalize (GObject *object)
{
  EggRadioBox *self = (EggRadioBox *)object;
  EggRadioBoxPrivate *priv = egg_radio_box_get_instance_private (self);

  g_clear_pointer (&priv->items, g_array_unref);
  g_clear_object (&priv->active_action);

  G_OBJECT_CLASS (egg_radio_box_parent_class)->finalize (object);
}

static void
egg_radio_box_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  EggRadioBox *self = EGG_RADIO_BOX (object);

  switch (prop_id)
    {
    case PROP_ACTIVE_ID:
      g_value_take_string (value, egg_radio_box_get_active_id (self));
      break;

    case PROP_HAS_MORE:
      g_value_set_boolean (value, egg_radio_box_get_has_more (self));
      break;

    case PROP_SHOW_MORE:
      g_value_set_boolean (value, egg_radio_box_get_show_more (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
egg_radio_box_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  EggRadioBox *self = EGG_RADIO_BOX (object);

  switch (prop_id)
    {
    case PROP_ACTIVE_ID:
      egg_radio_box_set_active_id (self, g_value_get_string (value));
      break;

    case PROP_SHOW_MORE:
      egg_radio_box_set_show_more (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
egg_radio_box_class_init (EggRadioBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = egg_radio_box_finalize;
  object_class->get_property = egg_radio_box_get_property;
  object_class->set_property = egg_radio_box_set_property;

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
}

static void
egg_radio_box_init (EggRadioBox *self)
{
  EggRadioBoxPrivate *priv = egg_radio_box_get_instance_private (self);
  g_autoptr(GSimpleActionGroup) group = g_simple_action_group_new ();

  priv->items = g_array_new (FALSE, FALSE, sizeof (EggRadioBoxItem));
  g_array_set_clear_func (priv->items, (GDestroyNotify)egg_radio_box_item_clear);

  priv->vbox = g_object_new (GTK_TYPE_BOX,
                             "orientation", GTK_ORIENTATION_VERTICAL,
                             "spacing", 12,
                             "visible", TRUE,
                             NULL);
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (priv->vbox));

  priv->active_action = g_simple_action_new_stateful ("active",
                                                      G_VARIANT_TYPE_STRING,
                                                      g_variant_new_string (""));
  g_signal_connect (priv->active_action,
                    "change-state",
                    G_CALLBACK (egg_radio_box_active_action),
                    self);
  g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (priv->active_action));
  gtk_widget_insert_action_group (GTK_WIDGET (self), "radiobox", G_ACTION_GROUP (group));
}

void
egg_radio_box_add_item (EggRadioBox *self,
                        const gchar *id,
                        const gchar *text)
{
  EggRadioBoxPrivate *priv = egg_radio_box_get_instance_private (self);
  EggRadioBoxItem item = { 0 };
  g_autofree gchar *active_id = NULL;

  g_return_if_fail (EGG_IS_RADIO_BOX (self));
  g_return_if_fail (id != NULL);
  g_return_if_fail (text != NULL);

  active_id = egg_radio_box_get_active_id (self);

  item.id = g_strdup (id);
  item.text = g_strdup (text);
  item.button = g_object_new (GTK_TYPE_TOGGLE_BUTTON,
                              "action-name", "radiobox.active",
                              "action-target", g_variant_new_string (id),
                              "active", (g_strcmp0 (id, active_id) == 0),
                              "label", text,
                              "visible", TRUE,
                              NULL);

  g_array_append_val (priv->items, item);

  if (priv->n_in_hbox % N_PER_ROW == 0)
    {
      priv->n_in_hbox = 0;
      priv->has_more = priv->hbox != NULL;
      priv->hbox = g_object_new (GTK_TYPE_BOX,
                                 "orientation", GTK_ORIENTATION_HORIZONTAL,
                                 "visible", !priv->has_more || priv->show_more,
                                 NULL);
      gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (priv->hbox)), "linked");
      gtk_container_add (GTK_CONTAINER (priv->vbox), GTK_WIDGET (priv->hbox));
    }

  gtk_container_add_with_properties (GTK_CONTAINER (priv->hbox), GTK_WIDGET (item.button),
                                     "expand", TRUE,
                                     NULL);

  priv->n_in_hbox++;

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HAS_MORE]);

  /* If this is the first item and no active id has been set,
   * then go ahead and set the active item to this one.
   */
  if (priv->items->len == 1 && (!active_id || !*active_id))
    egg_radio_box_set_active_id (self, id);
}

void
egg_radio_box_set_active_id (EggRadioBox *self,
                             const gchar *id)
{
  EggRadioBoxPrivate *priv = egg_radio_box_get_instance_private (self);

  g_return_if_fail (EGG_IS_RADIO_BOX (self));
  g_return_if_fail (id != NULL);

  g_simple_action_set_state (priv->active_action, g_variant_new_string (id));

  g_signal_emit (self, signals [CHANGED], 0);
}

gchar *
egg_radio_box_get_active_id (EggRadioBox *self)
{
  EggRadioBoxPrivate *priv = egg_radio_box_get_instance_private (self);
  g_autoptr(GVariant) state = NULL;

  g_return_val_if_fail (EGG_IS_RADIO_BOX (self), NULL);

  state = g_action_get_state (G_ACTION (priv->active_action));

  if (state != NULL)
    return g_variant_dup_string (state, NULL);

  return NULL;
}

GtkWidget *
egg_radio_box_new (void)
{
  return g_object_new (EGG_TYPE_RADIO_BOX, NULL);
}

typedef struct
{
  EggRadioBox *self;
  gchar       *id;
  GString     *text;
} ItemParserData;

static void
item_parser_start_element (GMarkupParseContext  *context,
                           const gchar          *element_name,
                           const gchar         **attribute_names,
                           const gchar         **attribute_values,
                           gpointer              user_data,
                           GError              **error)
{
  ItemParserData *parser_data = user_data;

  g_assert (context != NULL);
  g_assert (element_name != NULL);
  g_assert (parser_data != NULL);

  if (g_strcmp0 (element_name, "item") == 0)
    {
      const gchar *translatable = NULL;

      g_clear_pointer (&parser_data->id, g_free);
      g_string_truncate (parser_data->text, 0);

      if (!g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
                                        G_MARKUP_COLLECT_STRDUP, "id", &parser_data->id,
                                        G_MARKUP_COLLECT_STRING, "translatable", &translatable,
                                        G_MARKUP_COLLECT_INVALID))
        return;
    }
}

static void
item_parser_end_element (GMarkupParseContext  *context,
                         const gchar          *element_name,
                         gpointer              user_data,
                         GError              **error)
{
  ItemParserData *parser_data = user_data;

  g_assert (context != NULL);
  g_assert (element_name != NULL);
  g_assert (parser_data != NULL);

  if (g_strcmp0 (element_name, "item") == 0)
    {
      if (parser_data->id && parser_data->text != NULL)
        egg_radio_box_add_item (parser_data->self, parser_data->id, parser_data->text->str);
    }
}

static void
item_parser_text (GMarkupParseContext  *context,
                  const gchar          *text,
                  gsize                 text_len,
                  gpointer              user_data,
                  GError              **error)
{
  ItemParserData *parser_data = user_data;

  g_assert (parser_data != NULL);

  if (parser_data->text == NULL)
    parser_data->text = g_string_new (NULL);

  g_string_append_len (parser_data->text, text, text_len);
}

static GMarkupParser ItemParser = {
  item_parser_start_element,
  item_parser_end_element,
  item_parser_text,
};

static gboolean
egg_radio_box_custom_tag_start (GtkBuildable  *buildable,
                                GtkBuilder    *builder,
                                GObject       *child,
                                const gchar   *tagname,
                                GMarkupParser *parser,
                                gpointer      *data)
{
  EggRadioBox *self = (EggRadioBox *)buildable;

  g_assert (EGG_IS_RADIO_BOX (self));
  g_assert (GTK_IS_BUILDER (builder));
  g_assert (tagname != NULL);
  g_assert (parser != NULL);
  g_assert (data != NULL);

  if (g_strcmp0 (tagname, "items") == 0)
    {
      ItemParserData *parser_data;

      parser_data = g_slice_new0 (ItemParserData);
      parser_data->self = self;

      *parser = ItemParser;
      *data = parser_data;

      return TRUE;
    }

  return FALSE;
}

static void
egg_radio_box_custom_finished (GtkBuildable *buildable,
                               GtkBuilder   *builder,
                               GObject      *child,
                               const gchar  *tagname,
                               gpointer      user_data)
{
  EggRadioBox *self = (EggRadioBox *)buildable;

  g_assert (EGG_IS_RADIO_BOX (self));
  g_assert (GTK_IS_BUILDER (builder));
  g_assert (tagname != NULL);

  if (g_strcmp0 (tagname, "items") == 0)
    {
      ItemParserData *parser_data = user_data;

      g_free (parser_data->id);
      g_string_free (parser_data->text, TRUE);
      g_slice_free (ItemParserData, parser_data);
    }
}

static void
buildable_iface_init (GtkBuildableIface *iface)
{
  iface->custom_tag_start = egg_radio_box_custom_tag_start;
  iface->custom_finished = egg_radio_box_custom_finished;
}
