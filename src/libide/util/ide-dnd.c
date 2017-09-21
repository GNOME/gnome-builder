/* ide-dnd.c
 *
 * Copyright (C) 2015 Dimitris Zenios <dimitris.zenios@gmail.com>
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

#include "ide-dnd.h"

/**
 * ide_dnd_get_uri_list:
 * @selection_data: the #GtkSelectionData from drag_data_received
 *
 * Create a list of valid uri's from a uri-list drop.
 *
 * Returns: (transfer full): a string array which will hold the uris or
 *   %NULL if there were no valid uris. g_strfreev should be used when
 *   the string array is no longer used
 */
gchar **
ide_dnd_get_uri_list (GtkSelectionData *selection_data)
{
  const gchar *data;

  g_return_val_if_fail (selection_data, NULL);
  g_return_val_if_fail (gtk_selection_data_get_length (selection_data) > 0, NULL);

  data = (const gchar *)gtk_selection_data_get_data (selection_data);

  return g_uri_list_extract_uris (data);
}
