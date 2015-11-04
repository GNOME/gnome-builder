/* egg-date-time.c
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>

#include "egg-date-time.h"

/**
 * egg_date_time_format_for_display:
 * @self: A #GDateTime
 *
 * Helper function to "humanize" a #GDateTime into a relative time
 * relationship string.
 *
 * Returns: (transfer full): A newly allocated string describing the
 *   date and time imprecisely such as "Yesterday".
 */
gchar *
egg_date_time_format_for_display (GDateTime *self)
{
  GDateTime *now;
  GTimeSpan diff;
  gint years;

  /*
   * TODO:
   *
   * There is probably a lot more we can do here to be friendly for
   * various locales, but this will get us started.
   */

  g_return_val_if_fail (self != NULL, NULL);

  now = g_date_time_new_now_utc ();
  diff = g_date_time_difference (now, self) / G_USEC_PER_SEC;

  if (diff < 0)
    return g_strdup ("");
  else if (diff < (60 * 45))
    return g_strdup (_("Just now"));
  else if (diff < (60 * 90))
    return g_strdup (_("An hour ago"));
  else if (diff < (60 * 60 * 24 * 2))
    return g_strdup (_("Yesterday"));
  else if (diff < (60 * 60 * 24 * 7))
    return g_date_time_format (self, "%A");
  else if (diff < (60 * 60 * 24 * 365))
    return g_date_time_format (self, "%B");
  else if (diff < (60 * 60 * 24 * 365 * 1.5))
    return g_strdup (_("About a year ago"));

  years = MAX (2, diff / (60 * 60 * 24 * 365));

  return g_strdup_printf (ngettext ("About %u year ago", "About %u years ago", years), years);
}
