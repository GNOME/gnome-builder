/* ide-omni-search-entry.c
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

#include "ide-macros.h"
#include "ide-omni-search-entry.h"
#include "ide-omni-search-display.h"

#define SHORT_DELAY_TIMEOUT_MSEC 30
#define LONG_DELAY_TIMEOUT_MSEC  30

struct _IdeOmniSearchEntry
{
  GtkBox           parent_instance;

  /* Weak references */
  IdeWorkbench     *workbench;
  gulong           set_focus_handler;

  /* Template references */
  GtkMenuButton   *button;
  IdeOmniSearchDisplay *display;
  GtkSearchEntry  *entry;
  GtkPopover      *popover;

  guint            delay_timeout;
};

G_DEFINE_TYPE (IdeOmniSearchEntry, ide_omni_search_entry, GTK_TYPE_BOX)

GtkWidget *
ide_omni_search_entry_new (void)
{
  return g_object_new (IDE_TYPE_OMNI_SEARCH_ENTRY, NULL);
}

IdeSearchEngine *
ide_omni_search_entry_get_search_engine (IdeOmniSearchEntry *self)
{
  IdeContext *context;
  IdeSearchEngine *search_engine;

  g_return_val_if_fail (IDE_IS_OMNI_SEARCH_ENTRY (self), NULL);

  if (self->workbench == NULL)
    return NULL;

  context = ide_workbench_get_context (self->workbench);
  if (context == NULL)
      return NULL;

  search_engine = ide_context_get_search_engine (context);

  return search_engine;
}

static gboolean
ide_omni_search_entry_delay_cb (gpointer user_data)
{
  IdeOmniSearchEntry *self = user_data;
  IdeSearchEngine *search_engine;
  IdeSearchContext *context;
  const gchar *search_text;

  g_return_val_if_fail (IDE_IS_OMNI_SEARCH_ENTRY (self), G_SOURCE_REMOVE);

  self->delay_timeout = 0;

  if (self->display)
    {
      context = ide_omni_search_display_get_context (self->display);
      if (context)
        ide_search_context_cancel (context);

      search_engine = ide_omni_search_entry_get_search_engine (self);
      if (!search_engine)
        return G_SOURCE_REMOVE;

      search_text = gtk_entry_get_text (GTK_ENTRY (self->entry));
      if (!search_text)
        return G_SOURCE_REMOVE;

      /* TODO: Remove search text */
      context = ide_search_engine_search (search_engine, search_text);
      ide_omni_search_display_set_context (self->display, context);
      ide_search_context_execute (context, search_text, 7);
      g_object_unref (context);
    }

  return G_SOURCE_REMOVE;
}

static void
ide_omni_search_entry_popover_set_visible (IdeOmniSearchEntry *self,
                                           gboolean            visible)
{
  gboolean entry_has_text;

  g_return_if_fail (IDE_IS_OMNI_SEARCH_ENTRY (self));

  entry_has_text = !!(gtk_entry_get_text_length (GTK_ENTRY (self->entry)));

  if (visible == gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->button)))
    return;

  if (visible && entry_has_text)
    {
      if (!gtk_widget_has_focus (GTK_WIDGET (self->entry)))
        gtk_widget_grab_focus (GTK_WIDGET (self->entry));

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->button), TRUE);
    }
  else if (!visible)
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->button), FALSE);
    }
}

static void
ide_omni_search_entry_entry_activate (IdeOmniSearchEntry *self,
                                      GtkSearchEntry     *entry)
{
  g_return_if_fail (IDE_IS_OMNI_SEARCH_ENTRY (self));
  g_return_if_fail (GTK_IS_SEARCH_ENTRY (entry));

  ide_omni_search_display_activate (self->display);
  gtk_entry_set_text (GTK_ENTRY (self->entry), "");
}

static void
ide_omni_search_entry_entry_changed (IdeOmniSearchEntry *self,
                                     GtkSearchEntry     *entry)
{
  GtkWidget *button;
  gboolean active;
  gboolean sensitive;
  guint delay_msec = SHORT_DELAY_TIMEOUT_MSEC;

  g_return_if_fail (IDE_IS_OMNI_SEARCH_ENTRY (self));
  g_return_if_fail (GTK_IS_SEARCH_ENTRY (entry));

  button = GTK_WIDGET (self->button);
  active = gtk_widget_has_focus (GTK_WIDGET (entry)) || (self->delay_timeout != 0);
  sensitive = !!(gtk_entry_get_text_length (GTK_ENTRY (self->entry)));

  if (gtk_widget_get_sensitive (button) != sensitive)
    gtk_widget_set_sensitive (button, sensitive);

  if (active)
    ide_omni_search_entry_popover_set_visible (self, TRUE);

  if (!self->delay_timeout)
    {
      const gchar *search_text;

      search_text = gtk_entry_get_text (GTK_ENTRY (entry));
      if (search_text)
        {
          if (strlen (search_text) < 3)
            delay_msec = LONG_DELAY_TIMEOUT_MSEC;
          self->delay_timeout = g_timeout_add (delay_msec,
                                               ide_omni_search_entry_delay_cb,
                                               self);
        }
    }
}

static gboolean
ide_omni_search_entry_entry_key_press_event (IdeOmniSearchEntry *self,
                                             GdkEventKey        *key,
                                             GtkSearchEntry     *entry)
{
  g_return_val_if_fail (IDE_IS_OMNI_SEARCH_ENTRY (self), GDK_EVENT_PROPAGATE);
  g_return_val_if_fail (key, GDK_EVENT_PROPAGATE);
  g_return_val_if_fail (GTK_IS_SEARCH_ENTRY (entry), GDK_EVENT_PROPAGATE);

  switch (key->keyval)
    {
    case GDK_KEY_Escape:
      {
        ide_omni_search_entry_popover_set_visible (self, FALSE);
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
ide_omni_search_entry_display_result_activated (IdeOmniSearchEntry   *self,
                                                IdeSearchResult      *result,
                                                IdeOmniSearchDisplay *display)
{
  g_return_if_fail (IDE_IS_OMNI_SEARCH_ENTRY (self));
  g_return_if_fail (IDE_IS_SEARCH_RESULT (result));
  g_return_if_fail (IDE_IS_OMNI_SEARCH_DISPLAY (display));

  gtk_entry_set_text (GTK_ENTRY (self->entry), "");
}

static void
ide_omni_search_entry_grab_focus (GtkWidget *widget)
{
  IdeOmniSearchEntry *self = (IdeOmniSearchEntry *)widget;

  g_return_if_fail (IDE_IS_OMNI_SEARCH_ENTRY (self));

  gtk_widget_grab_focus (GTK_WIDGET (self->entry));
}

static void
ide_omni_search_entry_workbench_set_focus (IdeOmniSearchEntry *self,
                                           GtkWidget          *focus,
                                           IdeWorkbench       *workbench)
{
  g_return_if_fail (IDE_IS_OMNI_SEARCH_ENTRY (self));
  g_return_if_fail (!focus || GTK_IS_WIDGET (focus));
  g_return_if_fail (IDE_IS_WORKBENCH (workbench));

  if (!focus ||
      (!gtk_widget_is_ancestor (focus, GTK_WIDGET (self)) &&
       !gtk_widget_is_ancestor (focus, GTK_WIDGET (self->popover))))
    {
      gtk_entry_set_text (GTK_ENTRY (self->entry), "");
    }
  else
    {
      ide_omni_search_entry_popover_set_visible (self, TRUE);
    }
}

static void
ide_omni_search_entry_map (GtkWidget *widget)
{
  IdeOmniSearchEntry *self = (IdeOmniSearchEntry *)widget;
  GtkWidget *toplevel;

  g_return_if_fail (IDE_IS_OMNI_SEARCH_ENTRY (self));

  GTK_WIDGET_CLASS (ide_omni_search_entry_parent_class)->map (widget);

  gtk_widget_set_sensitive (GTK_WIDGET (self->button), FALSE);
  toplevel = gtk_widget_get_toplevel (widget);

  if (IDE_IS_WORKBENCH (toplevel))
    {
      ide_set_weak_pointer (&self->workbench, IDE_WORKBENCH (toplevel));
      self->set_focus_handler =
        g_signal_connect_object (toplevel,
                                 "set-focus",
                                 G_CALLBACK (ide_omni_search_entry_workbench_set_focus),
                                 self,
                                 G_CONNECT_SWAPPED | G_CONNECT_AFTER);
    }
}

static void
ide_omni_search_entry_unmap (GtkWidget *widget)
{
  IdeOmniSearchEntry *self = (IdeOmniSearchEntry *)widget;

  g_return_if_fail (IDE_IS_OMNI_SEARCH_ENTRY (self));

  if (self->workbench)
    {
      ide_clear_signal_handler (self->workbench, &self->set_focus_handler);
      ide_clear_weak_pointer (&self->workbench);
    }

  GTK_WIDGET_CLASS (ide_omni_search_entry_parent_class)->unmap (widget);
}

static void
ide_omni_search_entry_constructed (GObject *object)
{
  IdeOmniSearchEntry *self = (IdeOmniSearchEntry *)object;

  g_return_if_fail (IDE_IS_OMNI_SEARCH_ENTRY (self));

  G_OBJECT_CLASS (ide_omni_search_entry_parent_class)->constructed (object);

  gtk_popover_set_relative_to (self->popover, GTK_WIDGET (self->entry));

  g_signal_connect_object (self->entry,
                           "activate",
                           G_CALLBACK (ide_omni_search_entry_entry_activate),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->entry,
                           "changed",
                           G_CALLBACK (ide_omni_search_entry_entry_changed),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->entry,
                           "key-press-event",
                           G_CALLBACK (ide_omni_search_entry_entry_key_press_event),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->display,
                           "result-activated",
                           G_CALLBACK (ide_omni_search_entry_display_result_activated),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
ide_omni_search_entry_finalize (GObject *object)
{
  IdeOmniSearchEntry *self = (IdeOmniSearchEntry *)object;

  if (self->delay_timeout)
    {
      g_source_remove (self->delay_timeout);
      self->delay_timeout = 0;
    }

  G_OBJECT_CLASS (ide_omni_search_entry_parent_class)->finalize (object);
}

static void
ide_omni_search_entry_class_init (IdeOmniSearchEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = ide_omni_search_entry_constructed;
  object_class->finalize = ide_omni_search_entry_finalize;

  widget_class->grab_focus = ide_omni_search_entry_grab_focus;
  widget_class->map = ide_omni_search_entry_map;
  widget_class->unmap = ide_omni_search_entry_unmap;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-omni-search-entry.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeOmniSearchEntry, button);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniSearchEntry, display);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniSearchEntry, entry);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniSearchEntry, popover);

  g_type_ensure (IDE_TYPE_OMNI_SEARCH_DISPLAY);
}

static void
ide_omni_search_entry_init (IdeOmniSearchEntry *self)
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
