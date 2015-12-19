/* ide-gdk.c
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#include <string.h>

#include "ide-gdk.h"

GdkEventKey *
ide_gdk_synthesize_event_key (GdkWindow *window,
                              gunichar   ch)
{
  GdkDisplay *display;
  GdkDevice *device;
  GdkSeat *seat;
  GdkEvent *ev;
  GdkKeymapKey *keys = NULL;
  gint n_keys = 0;
  gchar str[8] = { 0 };

  g_assert (window != NULL);
  g_assert (GDK_IS_WINDOW (window));

  g_unichar_to_utf8 (ch, str);

  ev = gdk_event_new (GDK_KEY_PRESS);
  ev->key.window = g_object_ref (window);
  ev->key.send_event = TRUE;
  ev->key.time = gtk_get_current_event_time ();
  ev->key.state = 0;
  ev->key.hardware_keycode = 0;
  ev->key.group = 0;
  ev->key.is_modifier = 0;

  switch (ch)
    {
    case '\n':
      ev->key.keyval = GDK_KEY_Return;
      ev->key.string = g_strdup ("\n");
      ev->key.length = 1;
      break;

    case '\e':
      ev->key.keyval = GDK_KEY_Escape;
      ev->key.string = g_strdup ("");
      ev->key.length = 0;
      break;

    default:
      ev->key.keyval = gdk_unicode_to_keyval (ch);
      ev->key.length = strlen (str);
      ev->key.string = g_strdup (str);
      break;
    }

  gdk_keymap_get_entries_for_keyval (gdk_keymap_get_default (),
                                     ev->key.keyval,
                                     &keys,
                                     &n_keys);

  if (n_keys > 0)
    {
      ev->key.hardware_keycode = keys [0].keycode;
      ev->key.group = keys [0].group;
      if (keys [0].level == 1)
        ev->key.state |= GDK_SHIFT_MASK;
      g_free (keys);
    }

  display = gdk_window_get_display (ev->any.window);
  seat = gdk_display_get_default_seat (display);
  device = gdk_seat_get_keyboard (seat);
  gdk_event_set_device (ev, device);

  return &ev->key;
}

GdkEventKey *
ide_gdk_synthesize_event_keyval (GdkWindow *window,
                                 guint      keyval)
{
  GdkDisplay *display;
  GdkDevice *device;
  GdkEvent *ev;
  GdkSeat *seat;
  GdkKeymapKey *keys = NULL;
  gint n_keys = 0;
  gchar str[8] = { 0 };
  gunichar ch;

  g_assert (window != NULL);
  g_assert (GDK_IS_WINDOW (window));

  ch = gdk_keyval_to_unicode (keyval);
  g_unichar_to_utf8 (ch, str);

  ev = gdk_event_new (GDK_KEY_PRESS);
  ev->key.window = g_object_ref (window);
  ev->key.send_event = TRUE;
  ev->key.time = gtk_get_current_event_time ();
  ev->key.state = 0;
  ev->key.hardware_keycode = 0;
  ev->key.group = 0;
  ev->key.is_modifier = 0;
  ev->key.keyval = keyval;
  ev->key.string = g_strdup (str);
  ev->key.length = strlen (str);

  gdk_keymap_get_entries_for_keyval (gdk_keymap_get_default (),
                                     ev->key.keyval,
                                     &keys,
                                     &n_keys);

  if (n_keys > 0)
    {
      ev->key.hardware_keycode = keys [0].keycode;
      ev->key.group = keys [0].group;
      if (keys [0].level == 1)
        ev->key.state |= GDK_SHIFT_MASK;
      g_free (keys);
    }

  display = gdk_window_get_display (ev->any.window);
  seat = gdk_display_get_default_seat (display);
  device = gdk_seat_get_keyboard (seat);
  gdk_event_set_device (ev, device);

  return &ev->key;
}
