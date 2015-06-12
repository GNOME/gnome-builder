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

#define G_LOG_DOMAIN "ide-search-box"

#include <glib/gi18n.h>

#include "gb-glib.h"
#include "gb-scrolled-window.h"
#include "gb-search-box.h"
#include "gb-search-display.h"
#include "gb-string.h"
#include "gb-widget.h"
#include "gb-workbench.h"

/* FIXME: make search result row creation pluggable */
#include "ide-devhelp-search-result.h"

#define SHORT_DELAY_TIMEOUT_MSEC 30
#define LONG_DELAY_TIMEOUT_MSEC  30

struct _GbSearchBox
{
  GtkBox           parent_instance;

  /* Weak references */
  GbWorkbench     *workbench;
  gulong           set_focus_handler;

  /* Template references */
  GtkMenuButton   *button;
  GbSearchDisplay *display;
  GtkSearchEntry  *entry;
  GtkPopover      *popover;

  guint            delay_timeout;
};

G_DEFINE_TYPE (GbSearchBox, gb_search_box, GTK_TYPE_BOX)

GtkWidget *
gb_search_box_new (void)
{
  return g_object_new (GB_TYPE_SEARCH_BOX, NULL);
}

IdeSearchEngine *
gb_search_box_get_search_engine (GbSearchBox *self)
{
  IdeContext *context;
  IdeSearchEngine *search_engine;

  g_return_val_if_fail (GB_IS_SEARCH_BOX (self), NULL);

  if (self->workbench == NULL)
    return NULL;

  context = gb_workbench_get_context (self->workbench);
  if (context == NULL)
      return NULL;

  search_engine = ide_context_get_search_engine (context);

  return search_engine;
}

static gboolean
gb_search_box_delay_cb (gpointer user_data)
{
  GbSearchBox *self = user_data;
  IdeSearchEngine *search_engine;
  IdeSearchContext *context;
  const gchar *search_text;

  g_return_val_if_fail (GB_IS_SEARCH_BOX (self), G_SOURCE_REMOVE);

  self->delay_timeout = 0;

  context = gb_search_display_get_context (self->display);
  if (context)
    ide_search_context_cancel (context);

  search_engine = gb_search_box_get_search_engine (self);
  if (!search_engine)
    return G_SOURCE_REMOVE;

  search_text = gtk_entry_get_text (GTK_ENTRY (self->entry));
  if (!search_text)
    return G_SOURCE_REMOVE;

  /* TODO: Remove search text */
  context = ide_search_engine_search (search_engine, search_text);
  gb_search_display_set_context (self->display, context);
  ide_search_context_execute (context, search_text, 5);
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
gb_search_box_entry_focus_in (GbSearchBox   *self,
                              GdkEventFocus *focus,
                              GtkWidget     *entry)
{
  const gchar *text;

  g_return_val_if_fail (GB_IS_SEARCH_BOX (self), FALSE);
  g_return_val_if_fail (focus, FALSE);
  g_return_val_if_fail (GTK_IS_SEARCH_ENTRY (entry), FALSE);

  text = gtk_entry_get_text (GTK_ENTRY (self->entry));

  if (!gb_str_empty0 (text))
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->button), TRUE);

  return GDK_EVENT_PROPAGATE;
}

static void
gb_search_box_entry_activate (GbSearchBox    *self,
                              GtkSearchEntry *entry)
{
  g_return_if_fail (GB_IS_SEARCH_BOX (self));
  g_return_if_fail (GTK_IS_SEARCH_ENTRY (entry));

  gb_search_display_activate (self->display);
}

static void
gb_search_box_entry_changed (GbSearchBox    *self,
                             GtkSearchEntry *entry)
{
  GtkToggleButton *button;
  const gchar *text;
  gboolean active;
  guint delay_msec = SHORT_DELAY_TIMEOUT_MSEC;

  g_return_if_fail (GB_IS_SEARCH_BOX (self));
  g_return_if_fail (GTK_IS_SEARCH_ENTRY (entry));

  button = GTK_TOGGLE_BUTTON (self->button);
  text = gtk_entry_get_text (GTK_ENTRY (entry));
  active = !gb_str_empty0 (text);

  if (gtk_toggle_button_get_active (button) != active)
    gtk_toggle_button_set_active (button, active);

  if (!self->delay_timeout)
    {
      const gchar *search_text;

      search_text = gtk_entry_get_text (GTK_ENTRY (entry));
      if (search_text)
        {
          if (strlen (search_text) < 3)
            delay_msec = LONG_DELAY_TIMEOUT_MSEC;
          self->delay_timeout = g_timeout_add (delay_msec,
                                               gb_search_box_delay_cb,
                                               self);
        }
    }
}

static gboolean
gb_search_box_entry_key_press_event (GbSearchBox    *self,
                                     GdkEventKey    *key,
                                     GtkSearchEntry *entry)
{
  g_return_val_if_fail (GB_IS_SEARCH_BOX (self), GDK_EVENT_PROPAGATE);
  g_return_val_if_fail (key, GDK_EVENT_PROPAGATE);
  g_return_val_if_fail (GTK_IS_SEARCH_ENTRY (entry), GDK_EVENT_PROPAGATE);

  switch (key->keyval)
    {
    case GDK_KEY_Escape:
      {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->button), FALSE);
        gtk_widget_grab_focus (gtk_widget_get_toplevel (GTK_WIDGET (entry)));

        return GDK_EVENT_STOP;
      }
      break;

    case GDK_KEY_Tab:
    case GDK_KEY_KP_Tab:
      if ((key->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK)) != 0)
        break;
      /* Fall through */
    case GDK_KEY_Down:
    case GDK_KEY_KP_Down:
      if (gtk_widget_get_visible (GTK_WIDGET (self->popover)))
        {
          gtk_widget_grab_focus (GTK_WIDGET (self->display));
          return GDK_EVENT_STOP;
        }
      break;

    default:
      break;
    }

  return GDK_EVENT_PROPAGATE;
}

static void
gb_search_box_display_result_activated (GbSearchBox     *self,
                                        IdeSearchResult *result,
                                        GbSearchDisplay *display)
{
  GbWorkbench *workbench;

  g_return_if_fail (GB_IS_SEARCH_BOX (self));
  g_return_if_fail (IDE_IS_SEARCH_RESULT (result));
  g_return_if_fail (GB_IS_SEARCH_DISPLAY (display));

  workbench = gb_widget_get_workbench (GTK_WIDGET (self));

  /*
   * FIXME:
   *
   * This is not ideal, but we don't have time before the 3.16 release to build
   * the proper abstraction for us to keep the load hooks inside of the Builder
   * code and out of the libide code.
   *
   * After release, we should revisit this, and probably add an extension
   * point to register the handler for a given result type.
   */
  if (IDE_IS_GIT_SEARCH_RESULT (result))
    {
      g_autoptr(GFile) file = NULL;

      g_object_get (result, "file", &file, NULL);
      if (file)
        gb_workbench_open (workbench, file);
    }
  else if (IDE_IS_DEVHELP_SEARCH_RESULT (result))
    {
      g_autofree gchar *uri = NULL;

      g_object_get (result, "uri", &uri, NULL);
      //workspace = gb_workbench_get_workspace_typed (workbench, GB_TYPE_EDITOR_WORKSPACE);
      //gb_editor_workspace_show_help (workspace, uri);
    }
  else
    {
      g_warning (_("Builder does not know how to load %s"),
                 g_type_name (G_TYPE_FROM_INSTANCE (result)));
    }

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->button), FALSE);
  gtk_entry_set_text (GTK_ENTRY (self->entry), "");
}

static void
gb_search_box_button_toggled (GbSearchBox     *self,
                              GtkToggleButton *button)
{
  g_return_if_fail (GB_IS_SEARCH_BOX (self));
  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (button));

  if (gtk_toggle_button_get_active (button))
    {
      if (!gtk_widget_has_focus (GTK_WIDGET (self->entry)))
        gtk_widget_grab_focus (GTK_WIDGET (self->entry));
    }
  else
    {
      gtk_widget_hide (GTK_WIDGET (self->popover));
    }
}

static void
gb_search_box_grab_focus (GtkWidget *widget)
{
  GbSearchBox *self = (GbSearchBox *)widget;

  g_return_if_fail (GB_IS_SEARCH_BOX (self));

  gtk_widget_grab_focus (GTK_WIDGET (self->entry));
}

static void
gb_search_box_workbench_set_focus (GbSearchBox *self,
                                   GtkWidget   *focus,
                                   GbWorkbench *workbench)
{
  g_return_if_fail (GB_IS_SEARCH_BOX (self));
  g_return_if_fail (!focus || GTK_IS_WIDGET (focus));
  g_return_if_fail (GB_IS_WORKBENCH (workbench));

  if (!focus ||
      (!gtk_widget_is_ancestor (focus, GTK_WIDGET (self)) &&
       !gtk_widget_is_ancestor (focus, GTK_WIDGET (self->popover))))
    {
      gtk_entry_set_text (GTK_ENTRY (self->entry), "");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->button), FALSE);
    }
}

static void
gb_search_box_map (GtkWidget *widget)
{
  GbSearchBox *self = (GbSearchBox *)widget;
  GtkWidget *toplevel;

  g_return_if_fail (GB_IS_SEARCH_BOX (self));

  GTK_WIDGET_CLASS (gb_search_box_parent_class)->map (widget);

  toplevel = gtk_widget_get_toplevel (widget);

  if (GB_IS_WORKBENCH (toplevel))
    {
      gb_set_weak_pointer (toplevel, &self->workbench);
      self->set_focus_handler =
        g_signal_connect_object (toplevel,
                                 "set-focus",
                                 G_CALLBACK (gb_search_box_workbench_set_focus),
                                 self,
                                 G_CONNECT_SWAPPED | G_CONNECT_AFTER);
    }
}

static void
gb_search_box_unmap (GtkWidget *widget)
{
  GbSearchBox *self = (GbSearchBox *)widget;

  g_return_if_fail (GB_IS_SEARCH_BOX (self));

  if (self->workbench)
    {
      ide_clear_signal_handler (self->workbench, &self->set_focus_handler);
      ide_clear_weak_pointer (&self->workbench);
    }

  GTK_WIDGET_CLASS (gb_search_box_parent_class)->unmap (widget);
}

static void
gb_search_box_constructed (GObject *object)
{
  GbSearchBox *self = (GbSearchBox *)object;

  g_return_if_fail (GB_IS_SEARCH_BOX (self));

  G_OBJECT_CLASS (gb_search_box_parent_class)->constructed (object);

  gtk_popover_set_relative_to (self->popover, GTK_WIDGET (self->entry));

  g_signal_connect_object (self->popover,
                           "closed",
                           G_CALLBACK (gb_search_box_popover_closed),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->entry,
                           "focus-in-event",
                           G_CALLBACK (gb_search_box_entry_focus_in),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->entry,
                           "activate",
                           G_CALLBACK (gb_search_box_entry_activate),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->entry,
                           "changed",
                           G_CALLBACK (gb_search_box_entry_changed),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->entry,
                           "key-press-event",
                           G_CALLBACK (gb_search_box_entry_key_press_event),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->display,
                           "result-activated",
                           G_CALLBACK (gb_search_box_display_result_activated),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->button,
                           "toggled",
                           G_CALLBACK (gb_search_box_button_toggled),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
gb_search_box_finalize (GObject *object)
{
  GbSearchBox *self = (GbSearchBox *)object;

  if (self->delay_timeout)
    {
      g_source_remove (self->delay_timeout);
      self->delay_timeout = 0;
    }

  G_OBJECT_CLASS (gb_search_box_parent_class)->finalize (object);
}

static void
gb_search_box_class_init (GbSearchBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gb_search_box_constructed;
  object_class->finalize = gb_search_box_finalize;

  widget_class->grab_focus = gb_search_box_grab_focus;
  widget_class->map = gb_search_box_map;
  widget_class->unmap = gb_search_box_unmap;

  GB_WIDGET_CLASS_TEMPLATE (klass, "gb-search-box.ui");
  GB_WIDGET_CLASS_BIND (klass, GbSearchBox, button);
  GB_WIDGET_CLASS_BIND (klass, GbSearchBox, display);
  GB_WIDGET_CLASS_BIND (klass, GbSearchBox, entry);
  GB_WIDGET_CLASS_BIND (klass, GbSearchBox, popover);

  g_type_ensure (GB_TYPE_SEARCH_DISPLAY);
  g_type_ensure (GB_TYPE_SCROLLED_WINDOW);
}

static void
gb_search_box_init (GbSearchBox *self)
{
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
  g_object_ref (self->popover);
}
