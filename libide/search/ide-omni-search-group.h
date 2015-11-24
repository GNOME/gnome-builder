/* ide-omni-search-display-group.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef IDE_OMNI_SEARCH_GROUP_H
#define IDE_OMNI_SEARCH_GROUP_H

#include <gtk/gtk.h>
#include <ide.h>

G_BEGIN_DECLS

#define IDE_TYPE_OMNI_SEARCH_GROUP (ide_omni_search_group_get_type())

G_DECLARE_FINAL_TYPE (IdeOmniSearchGroup, ide_omni_search_group, IDE, OMNI_SEARCH_GROUP, GtkBox)

void               ide_omni_search_group_clear         (IdeOmniSearchGroup *group);
IdeSearchProvider *ide_omni_search_group_get_provider  (IdeOmniSearchGroup *group);
void               ide_omni_search_group_add_result    (IdeOmniSearchGroup *group,
                                                        IdeSearchResult    *result);
void               ide_omni_search_group_remove_result (IdeOmniSearchGroup *group,
                                                        IdeSearchResult    *result);
void               ide_omni_search_group_unselect      (IdeOmniSearchGroup *group);
void               ide_omni_search_group_focus_first   (IdeOmniSearchGroup *group);
void               ide_omni_search_group_focus_last    (IdeOmniSearchGroup *group);
IdeSearchResult   *ide_omni_search_group_get_first     (IdeOmniSearchGroup *group);
gboolean           ide_omni_search_group_activate      (IdeOmniSearchGroup *group);

G_END_DECLS

#endif /* IDE_OMNI_SEARCH_GROUP_H */
