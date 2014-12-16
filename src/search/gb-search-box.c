/* gb-search-box.c
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

#define G_LOG_DOMAIN "search-box"

#include <glib/gi18n.h>

#include "gb-search-box.h"
#include "gb-search-display.h"
#include "gb-search-manager.h"
#include "gb-widget.h"

#define DELAY_TIMEOUT_MSEC 250

struct _GbSearchBoxPrivate
{
  GbSearchManager *search_manager;

  GtkMenuButton   *button;
  GbSearchDisplay *display;
  GtkSearchEntry  *entry;
  GtkPopover      *popover;

  guint            delay_timeout;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbSearchBox, gb_search_box, GTK_TYPE_BOX)

enum {
  PROP_0,
  PROP_SEARCH_MANAGER,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GtkWidget *
gb_search_box_new (void)
{
  return g_object_new (GB_TYPE_SEARCH_BOX, NULL);
}

GbSearchManager *
gb_search_box_get_search_manager (GbSearchBox *box)
{
  g_return_val_if_fail (GB_IS_SEARCH_BOX (box), NULL);

  return box->priv->search_manager;
}

void
gb_search_box_set_search_manager (GbSearchBox     *box,
                                  GbSearchManager *search_manager)
{
  g_return_if_fail (GB_IS_SEARCH_BOX (box));
  g_return_if_fail (!search_manager || GB_IS_SEARCH_MANAGER (search_manager));

  if (box->priv->search_manager != search_manager)
    {
      g_clear_object (&box->priv->search_manager);

      if (search_manager)
        box->priv->search_manager = g_object_ref (search_manager);

      g_object_notify_by_pspec (G_OBJECT (box),
                                gParamSpecs [PROP_SEARCH_MANAGER]);
    }
}

static gboolean
gb_search_box_delay_cb (gpointer user_data)
{
  GbSearchBox *box = user_data;
  GbSearchContext *context;
  const gchar *search_text;

  g_return_val_if_fail (GB_IS_SEARCH_BOX (box), G_SOURCE_REMOVE);

  box->priv->delay_timeout = 0;

  context = gb_search_display_get_context (box->priv->display);
  if (context)
    gb_search_context_cancel (context);

  if (!box->priv->search_manager)
    return G_SOURCE_REMOVE;

  search_text = gtk_entry_get_text (GTK_ENTRY (box->priv->entry));
  if (!search_text)
    return G_SOURCE_REMOVE;

  context = gb_search_manager_search (box->priv->search_manager, search_text);
  gb_search_display_set_context (box->priv->display, context);
  g_object_unref (context);

  return G_SOURCE_REMOVE;
}

static void
gb_search_box_popover_closed (GbSearchBox *box,
                              GtkPopover  *popover)
{
  g_return_if_fail (GB_IS_SEARCH_BOX (box));
  g_return_if_fail (GTK_IS_POPOVER (popover));

}

static gboolean
gb_search_box_entry_focus_in (GbSearchBox   *box,
                              GdkEventFocus *focus,
                              GtkWidget     *entry)
{
  g_return_val_if_fail (GB_IS_SEARCH_BOX (box), FALSE);
  g_return_val_if_fail (focus, FALSE);
  g_return_val_if_fail (GTK_IS_SEARCH_ENTRY (entry), FALSE);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (box->priv->button), TRUE);

  return GDK_EVENT_PROPAGATE;
}

static void
gb_search_box_entry_changed (GbSearchBox    *box,
                             GtkSearchEntry *entry)
{
  const gchar *search_text;

  g_return_if_fail (GB_IS_SEARCH_BOX (box));
  g_return_if_fail (GTK_IS_SEARCH_ENTRY (entry));

  if (box->priv->delay_timeout)
    {
      g_source_remove (box->priv->delay_timeout);
      box->priv->delay_timeout = 0;
    }

  search_text = gtk_entry_get_text (GTK_ENTRY (entry));

  if (search_text)
    box->priv->delay_timeout = g_timeout_add (DELAY_TIMEOUT_MSEC,
                                              gb_search_box_delay_cb,
                                              box);
}

static gboolean
gb_search_box_entry_key_press_event (GbSearchBox    *box,
                                     GdkEventKey    *key,
                                     GtkSearchEntry *entry)
{
  g_return_val_if_fail (GB_IS_SEARCH_BOX (box), GDK_EVENT_PROPAGATE);
  g_return_val_if_fail (key, GDK_EVENT_PROPAGATE);
  g_return_val_if_fail (GTK_IS_SEARCH_ENTRY (entry), GDK_EVENT_PROPAGATE);

  switch (key->keyval)
    {
    case GDK_KEY_Escape:
      {
        GtkWidget *toplevel;

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (box->priv->button),
                                      FALSE);

        toplevel = gtk_widget_get_toplevel (GTK_WIDGET (entry));
        gtk_window_set_focus (GTK_WINDOW (toplevel), NULL);

        return GDK_EVENT_STOP;
      }
      break;

    case GDK_KEY_Down:
    case GDK_KEY_KP_Down:
      gtk_widget_grab_focus (GTK_WIDGET (box->priv->display));
      return GDK_EVENT_STOP;

    default:
      break;
    }

  return GDK_EVENT_PROPAGATE;
}

static void
gb_search_box_display_result_activated (GbSearchBox     *box,
                                        GbSearchResult  *result,
                                        GbSearchDisplay *display)
{
  g_return_if_fail (GB_IS_SEARCH_BOX (box));
  g_return_if_fail (GB_IS_SEARCH_RESULT (result));
  g_return_if_fail (GB_IS_SEARCH_DISPLAY (display));

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (box->priv->button), FALSE);
  gtk_entry_set_text (GTK_ENTRY (box->priv->entry), "");
}

static void
gb_search_box_grab_focus (GtkWidget *widget)
{
  GbSearchBox *box = (GbSearchBox *)widget;

  g_return_if_fail (GB_IS_SEARCH_BOX (box));

  gtk_widget_grab_focus (GTK_WIDGET (box->priv->entry));
}

static void
gb_search_box_constructed (GObject *object)
{
  GbSearchBoxPrivate *priv;
  GbSearchBox *self = (GbSearchBox *)object;

  g_return_if_fail (GB_IS_SEARCH_BOX (self));

  priv = self->priv;

  G_OBJECT_CLASS (gb_search_box_parent_class)->constructed (object);

  gtk_popover_set_relative_to (priv->popover, GTK_WIDGET (priv->entry));

  g_signal_connect_object (priv->popover,
                           "closed",
                           G_CALLBACK (gb_search_box_popover_closed),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (priv->entry,
                           "focus-in-event",
                           G_CALLBACK (gb_search_box_entry_focus_in),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (priv->entry,
                           "changed",
                           G_CALLBACK (gb_search_box_entry_changed),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (priv->entry,
                           "key-press-event",
                           G_CALLBACK (gb_search_box_entry_key_press_event),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (priv->display,
                           "result-activated",
                           G_CALLBACK (gb_search_box_display_result_activated),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
gb_search_box_finalize (GObject *object)
{
  GbSearchBoxPrivate *priv = GB_SEARCH_BOX (object)->priv;

  if (priv->delay_timeout)
    {
      g_source_remove (priv->delay_timeout);
      priv->delay_timeout = 0;
    }

  g_clear_object (&priv->search_manager);

  G_OBJECT_CLASS (gb_search_box_parent_class)->finalize (object);
}

static void
gb_search_box_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GbSearchBox *self = GB_SEARCH_BOX (object);

  switch (prop_id)
    {
    case PROP_SEARCH_MANAGER:
      g_value_set_object (value, gb_search_box_get_search_manager (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_search_box_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GbSearchBox *self = GB_SEARCH_BOX (object);

  switch (prop_id)
    {
    case PROP_SEARCH_MANAGER:
      gb_search_box_set_search_manager (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_search_box_class_init (GbSearchBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gb_search_box_constructed;
  object_class->finalize = gb_search_box_finalize;
  object_class->get_property = gb_search_box_get_property;
  object_class->set_property = gb_search_box_set_property;

  widget_class->grab_focus = gb_search_box_grab_focus;

  gParamSpecs [PROP_SEARCH_MANAGER] =
    g_param_spec_object ("search-manager",
                         _("Search Manager"),
                         _("The search manager for the search box."),
                         GB_TYPE_SEARCH_MANAGER,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SEARCH_MANAGER,
                                   gParamSpecs [PROP_SEARCH_MANAGER]);

  GB_WIDGET_CLASS_TEMPLATE (klass, "gb-search-box.ui");
  GB_WIDGET_CLASS_BIND (klass, GbSearchBox, button);
  GB_WIDGET_CLASS_BIND (klass, GbSearchBox, display);
  GB_WIDGET_CLASS_BIND (klass, GbSearchBox, entry);
  GB_WIDGET_CLASS_BIND (klass, GbSearchBox, popover);

  g_type_ensure (GB_TYPE_SEARCH_DISPLAY);
}

static void
gb_search_box_init (GbSearchBox *self)
{
  self->priv = gb_search_box_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));

  /*
   * WORKAROUND:
   *
   * The GtkWidget template things that popover is a child of ours. When in
   * reality it is a child of the GtkMenuButton (since it owns the "popover"
   * property. Both our widget and the menu button try to call
   * gtk_widget_destroy() on it.
   *
   * https://bugzilla.gnome.org/show_bug.cgi?id=741529
   */
  g_object_ref (self->priv->popover);
}
