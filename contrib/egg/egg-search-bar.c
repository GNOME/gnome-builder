/* egg-search-bar.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "egg-search-bar"

#include <glib/gi18n.h>

#include "egg-signal-group.h"
#include "egg-search-bar.h"

typedef struct
{
  GtkRevealer    *revealer;
  GtkBox         *box;
  GtkSearchEntry *entry;
  GtkButton      *close_button;

  EggSignalGroup *window_signals;

  guint           search_mode_enabled : 1;
} EggSearchBarPrivate;

static void egg_search_bar_init_buildable (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (EggSearchBar, egg_search_bar, GTK_TYPE_BIN,
                         G_ADD_PRIVATE (EggSearchBar)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                egg_search_bar_init_buildable))

enum {
  PROP_0,
  PROP_SHOW_CLOSE_BUTTON,
  PROP_SEARCH_MODE_ENABLED,
  LAST_PROP
};

enum {
  ACTIVATE,
  REVEAL,
  LAST_SIGNAL
};

static GParamSpec *properties [LAST_PROP];
static guint       signals [LAST_SIGNAL];

static void
egg_search_bar__entry_activate (EggSearchBar   *self,
                                GtkSearchEntry *entry)
{
  g_assert (EGG_IS_SEARCH_BAR (self));
  g_assert (GTK_IS_SEARCH_ENTRY (entry));

  g_signal_emit (self, signals [ACTIVATE], 0);
}

static gboolean
is_modifier_key (const GdkEventKey *event)
{
  static const guint modifier_keyvals[] = {
    GDK_KEY_Shift_L, GDK_KEY_Shift_R, GDK_KEY_Shift_Lock,
    GDK_KEY_Caps_Lock, GDK_KEY_ISO_Lock, GDK_KEY_Control_L,
    GDK_KEY_Control_R, GDK_KEY_Meta_L, GDK_KEY_Meta_R,
    GDK_KEY_Alt_L, GDK_KEY_Alt_R, GDK_KEY_Super_L, GDK_KEY_Super_R,
    GDK_KEY_Hyper_L, GDK_KEY_Hyper_R, GDK_KEY_ISO_Level3_Shift,
    GDK_KEY_ISO_Next_Group, GDK_KEY_ISO_Prev_Group,
    GDK_KEY_ISO_First_Group, GDK_KEY_ISO_Last_Group,
    GDK_KEY_Mode_switch, GDK_KEY_Num_Lock, GDK_KEY_Multi_key,
    GDK_KEY_Scroll_Lock,
    0
  };
  const guint *ac_val;

  g_return_val_if_fail (event != NULL, FALSE);

  ac_val = modifier_keyvals;

  while (*ac_val)
    {
      if (event->keyval == *ac_val++)
        return TRUE;
    }

  return FALSE;
}

static gboolean
toplevel_key_press_event_before (EggSearchBar *self,
                                 GdkEventKey  *event,
                                 GtkWindow    *toplevel)
{
  EggSearchBarPrivate *priv = egg_search_bar_get_instance_private (self);

  g_assert (EGG_IS_SEARCH_BAR (self));
  g_assert (event != NULL);
  g_assert (GTK_IS_WINDOW (toplevel));

  switch (event->keyval)
    {
    case GDK_KEY_Escape:
      if (priv->search_mode_enabled && gtk_widget_has_focus (GTK_WIDGET (priv->entry)))
        {
          egg_search_bar_set_search_mode_enabled (self, FALSE);
          return GDK_EVENT_STOP;
        }
      break;

    default:
      break;
    }

  return GDK_EVENT_PROPAGATE;
}

static gboolean
toplevel_key_press_event_after (EggSearchBar *self,
                                GdkEventKey  *event,
                                GtkWindow    *toplevel)
{
  EggSearchBarPrivate *priv = egg_search_bar_get_instance_private (self);
  GtkWidget *entry;

  g_assert (EGG_IS_SEARCH_BAR (self));
  g_assert (event != NULL);
  g_assert (GTK_IS_WINDOW (toplevel));

  entry = GTK_WIDGET (priv->entry);

  switch (event->keyval)
    {
    case GDK_KEY_Escape:
    case GDK_KEY_Up:
    case GDK_KEY_KP_Up:
    case GDK_KEY_Down:
    case GDK_KEY_KP_Down:
    case GDK_KEY_Left:
    case GDK_KEY_KP_Left:
    case GDK_KEY_Right:
    case GDK_KEY_KP_Right:
    case GDK_KEY_Home:
    case GDK_KEY_KP_Home:
    case GDK_KEY_End:
    case GDK_KEY_KP_End:
    case GDK_KEY_Page_Up:
    case GDK_KEY_KP_Page_Up:
    case GDK_KEY_Page_Down:
    case GDK_KEY_KP_Page_Down:
    case GDK_KEY_KP_Tab:
    case GDK_KEY_Tab:
      /* ignore keynav */
      break;

    default:
      if (((event->state & GDK_MOD1_MASK) != 0) ||
          ((event->state & GDK_CONTROL_MASK) != 0) ||
          priv->search_mode_enabled ||
          is_modifier_key (event))
        break;

      egg_search_bar_set_search_mode_enabled (self, TRUE);

      return GTK_WIDGET_GET_CLASS (entry)->key_press_event (entry, event);
    }

  return GDK_EVENT_PROPAGATE;
}

static void
egg_search_bar_hierarchy_changed (GtkWidget *widget,
                                  GtkWidget *old_toplevel)
{
  EggSearchBar *self = (EggSearchBar *)widget;
  EggSearchBarPrivate *priv = egg_search_bar_get_instance_private (self);
  GtkWidget *toplevel;

  g_assert (EGG_IS_SEARCH_BAR (self));

  toplevel = gtk_widget_get_toplevel (widget);

  if (GTK_IS_WINDOW (toplevel))
    egg_signal_group_set_target (priv->window_signals, toplevel);
  else
    egg_signal_group_set_target (priv->window_signals, NULL);
}

static void
egg_search_bar_reveal (EggSearchBar *self)
{
  g_assert (EGG_IS_SEARCH_BAR (self));

  egg_search_bar_set_search_mode_enabled (self, TRUE);
}

static GObject *
egg_search_bar_get_internal_child (GtkBuildable *buildable,
                                   GtkBuilder   *builder,
                                   const gchar  *childname)
{
  EggSearchBar *self = (EggSearchBar *)buildable;
  EggSearchBarPrivate *priv = egg_search_bar_get_instance_private (self);

  g_assert (GTK_IS_BUILDABLE (buildable));
  g_assert (EGG_IS_SEARCH_BAR (self));
  g_assert (GTK_IS_BUILDER (builder));
  g_assert (childname != NULL);

  if (g_strcmp0 (childname, "entry") == 0)
    return G_OBJECT (priv->entry);
  else if (g_strcmp0 (childname, "revealer") == 0)
    return G_OBJECT (priv->revealer);

  return NULL;
}

static void
egg_search_bar_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  EggSearchBar *self = EGG_SEARCH_BAR (object);

  switch (prop_id)
    {
    case PROP_SEARCH_MODE_ENABLED:
      g_value_set_boolean (value, egg_search_bar_get_search_mode_enabled (self));
      break;

    case PROP_SHOW_CLOSE_BUTTON:
      g_value_set_boolean (value, egg_search_bar_get_show_close_button (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
egg_search_bar_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  EggSearchBar *self = EGG_SEARCH_BAR (object);

  switch (prop_id)
    {
    case PROP_SEARCH_MODE_ENABLED:
      egg_search_bar_set_search_mode_enabled (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_CLOSE_BUTTON:
      egg_search_bar_set_show_close_button (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
egg_search_bar_finalize (GObject *object)
{
  EggSearchBar *self = (EggSearchBar *)object;
  EggSearchBarPrivate *priv = egg_search_bar_get_instance_private (self);

  g_clear_object (&priv->window_signals);

  G_OBJECT_CLASS (egg_search_bar_parent_class)->finalize (object);
}

static void
egg_search_bar_class_init (EggSearchBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = egg_search_bar_finalize;
  object_class->get_property = egg_search_bar_get_property;
  object_class->set_property = egg_search_bar_set_property;

  widget_class->hierarchy_changed = egg_search_bar_hierarchy_changed;

  properties [PROP_SEARCH_MODE_ENABLED] =
    g_param_spec_boolean ("search-mode-enabled",
                          "Search Mode Enabled",
                          "Search Mode Enabled",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SHOW_CLOSE_BUTTON] =
    g_param_spec_boolean ("show-close-button",
                          "Show Close Button",
                          "Show Close Button",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  signals [ACTIVATE] =
    g_signal_new ("activate",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  signals [REVEAL] =
    g_signal_new_class_handler ("reveal",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (egg_search_bar_reveal),
                                NULL, NULL, NULL, G_TYPE_NONE, 0);

  gtk_widget_class_set_css_name (widget_class, "eggsearchbar");
}

static void
egg_search_bar_init_buildable (GtkBuildableIface *iface)
{
  iface->get_internal_child = egg_search_bar_get_internal_child;
}

static void
egg_search_bar_init (EggSearchBar *self)
{
  EggSearchBarPrivate *priv = egg_search_bar_get_instance_private (self);
  GtkStyleContext *style_context;
  GtkBox *box;

  priv->window_signals = egg_signal_group_new (GTK_TYPE_WINDOW);
  egg_signal_group_connect_object (priv->window_signals,
                                   "key-press-event",
                                   G_CALLBACK (toplevel_key_press_event_before),
                                   self,
                                   G_CONNECT_SWAPPED);
  egg_signal_group_connect_object (priv->window_signals,
                                   "key-press-event",
                                   G_CALLBACK (toplevel_key_press_event_after),
                                   self,
                                   G_CONNECT_SWAPPED | G_CONNECT_AFTER);

  priv->revealer =
    g_object_new (GTK_TYPE_REVEALER,
                  "transition-type", GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN,
                  "visible", TRUE,
                  NULL);
  /* outer box used for styling */
  box =
    g_object_new (GTK_TYPE_BOX,
                  "orientation", GTK_ORIENTATION_HORIZONTAL,
                  "visible", TRUE,
                  NULL);
  priv->box =
    g_object_new (GTK_TYPE_BOX,
                  "hexpand", TRUE,
                  "margin-bottom", 3,
                  "margin-end", 6,
                  "margin-start", 6,
                  "margin-top", 3,
                  "orientation", GTK_ORIENTATION_HORIZONTAL,
                  "visible", TRUE,
                  NULL);
  priv->entry =
    g_object_connect (g_object_new (GTK_TYPE_SEARCH_ENTRY,
                                    "placeholder-text", _("Search"),
                                    "visible", TRUE,
                                    NULL),
                      "swapped_object_signal::activate", egg_search_bar__entry_activate, self,
                      NULL);
  priv->close_button =
    g_object_new (GTK_TYPE_BUTTON,
                  "child", g_object_new (GTK_TYPE_IMAGE,
                                         "icon-name", "window-close-symbolic",
                                         "visible", TRUE,
                                         NULL),
                  "visible", FALSE,
                  NULL);

  style_context = gtk_widget_get_style_context (GTK_WIDGET (box));
  gtk_style_context_add_class (style_context, "search-bar");

  gtk_container_add (GTK_CONTAINER (priv->revealer), GTK_WIDGET (box));
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (priv->box));
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (priv->revealer));
  gtk_container_add_with_properties (GTK_CONTAINER (priv->box),
                                     GTK_WIDGET (priv->close_button),
                                     "fill", FALSE,
                                     "pack-type", GTK_PACK_END,
                                     NULL);
  gtk_box_set_center_widget (priv->box, GTK_WIDGET (priv->entry));
}

GtkWidget *
egg_search_bar_new (void)
{
  return g_object_new (EGG_TYPE_SEARCH_BAR, NULL);
}

gboolean
egg_search_bar_get_search_mode_enabled (EggSearchBar *self)
{
  EggSearchBarPrivate *priv = egg_search_bar_get_instance_private (self);

  g_return_val_if_fail (EGG_IS_SEARCH_BAR (self), FALSE);

  return priv->search_mode_enabled;
}

void
egg_search_bar_set_search_mode_enabled (EggSearchBar *self,
                                        gboolean      search_mode_enabled)
{
  EggSearchBarPrivate *priv = egg_search_bar_get_instance_private (self);

  g_return_if_fail (EGG_IS_SEARCH_BAR (self));

  search_mode_enabled = !!search_mode_enabled;

  if (search_mode_enabled != priv->search_mode_enabled)
    {
      priv->search_mode_enabled = search_mode_enabled;
      gtk_revealer_set_reveal_child (priv->revealer, search_mode_enabled);
      gtk_entry_set_text (GTK_ENTRY (priv->entry), "");
      if (search_mode_enabled)
        gtk_widget_grab_focus (GTK_WIDGET (priv->entry));

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SEARCH_MODE_ENABLED]);
    }
}

gboolean
egg_search_bar_get_show_close_button (EggSearchBar *self)
{
  EggSearchBarPrivate *priv = egg_search_bar_get_instance_private (self);

  g_return_val_if_fail (EGG_IS_SEARCH_BAR (self), FALSE);

  return gtk_widget_get_visible (GTK_WIDGET (priv->close_button));
}

void
egg_search_bar_set_show_close_button (EggSearchBar *self,
                                      gboolean      show_close_button)
{
  EggSearchBarPrivate *priv = egg_search_bar_get_instance_private (self);

  g_return_if_fail (EGG_IS_SEARCH_BAR (self));

  gtk_widget_set_visible (GTK_WIDGET (priv->close_button), show_close_button);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SHOW_CLOSE_BUTTON]);
}

/**
 * egg_search_bar_get_entry:
 *
 * Returns: (transfer none) (type Gtk.SearchEntry): A #GtkSearchEntry.
 */
GtkWidget *
egg_search_bar_get_entry (EggSearchBar *self)
{
  EggSearchBarPrivate *priv = egg_search_bar_get_instance_private (self);

  g_return_val_if_fail (EGG_IS_SEARCH_BAR (self), NULL);

  return GTK_WIDGET (priv->entry);
}
