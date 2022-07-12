/* ide-preferences-window.h
 *
 * Copyright 2017-2022 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

#include <libide-core.h>

G_BEGIN_DECLS

typedef struct _IdePreferenceItemEntry IdePreferenceItemEntry;

typedef void (*IdePreferenceCallback) (const char                   *page_name,
                                       const IdePreferenceItemEntry *entry,
                                       AdwPreferencesGroup          *group,
                                       gpointer                      user_data);

typedef struct
{
  const char *parent;
  const char *section;
  const char *name;
  const char *icon_name;
  int priority;
  const char *title;
} IdePreferencePageEntry;

typedef struct
{
  const char *page;
  const char *name;
  int priority;
  const char *title;
} IdePreferenceGroupEntry;

struct _IdePreferenceItemEntry
{
  const char *page;
  const char *group;
  const char *name;

  int priority;

  IdePreferenceCallback callback;

  /* Callback specific data */

  /* Title/Subtitle for helper functions */
  const char *title;
  const char *subtitle;

  /* Schema info for helper functions */
  const char *schema_id;
  const char *path;
  const char *key;
  const char *value;

  /*< private >*/
  gconstpointer user_data;
};

typedef enum
{
  IDE_PREFERENCES_MODE_EMPTY,
  IDE_PREFERENCES_MODE_APPLICATION,
  IDE_PREFERENCES_MODE_PROJECT,
} IdePreferencesMode;

#define IDE_TYPE_PREFERENCES_WINDOW (ide_preferences_window_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdePreferencesWindow, ide_preferences_window, IDE, PREFERENCES_WINDOW, AdwApplicationWindow)

IDE_AVAILABLE_IN_ALL
GtkWidget          *ide_preferences_window_new        (IdePreferencesMode             mode,
                                                       IdeContext                    *context);
IDE_AVAILABLE_IN_ALL
IdePreferencesMode  ide_preferences_window_get_mode   (IdePreferencesWindow          *self);
IDE_AVAILABLE_IN_ALL
IdeContext         *ide_preferences_window_get_context (IdePreferencesWindow          *self);
IDE_AVAILABLE_IN_ALL
void                ide_preferences_window_set_page   (IdePreferencesWindow          *self,
                                                       const char                    *page);
IDE_AVAILABLE_IN_ALL
void                ide_preferences_window_add_pages  (IdePreferencesWindow          *self,
                                                       const IdePreferencePageEntry  *pages,
                                                       gsize                          n_pages,
                                                       const char                    *translation_domain);
IDE_AVAILABLE_IN_ALL
void                ide_preferences_window_add_group  (IdePreferencesWindow          *self,
                                                       const char                    *page,
                                                       const char                    *name,
                                                       int                            priority,
                                                       const char                    *title);
IDE_AVAILABLE_IN_ALL
void                ide_preferences_window_add_groups (IdePreferencesWindow          *self,
                                                       const IdePreferenceGroupEntry *groups,
                                                       gsize                          n_groups,
                                                       const char                    *translation_domain);
IDE_AVAILABLE_IN_ALL
void                ide_preferences_window_add_items  (IdePreferencesWindow          *self,
                                                       const IdePreferenceItemEntry  *items,
                                                       gsize                          n_items,
                                                       gpointer                       user_data,
                                                       GDestroyNotify                 user_data_destroy);
IDE_AVAILABLE_IN_ALL
void                ide_preferences_window_add_item   (IdePreferencesWindow          *self,
                                                       const char                    *page,
                                                       const char                    *group,
                                                       const char                    *name,
                                                       int                            priority,
                                                       IdePreferenceCallback          callback,
                                                       gpointer                       user_data,
                                                       GDestroyNotify                 user_data_destroy);
IDE_AVAILABLE_IN_ALL
void                ide_preferences_window_add_toggle (IdePreferencesWindow          *self,
                                                       const IdePreferenceItemEntry  *item);
IDE_AVAILABLE_IN_ALL
void                ide_preferences_window_add_spin   (IdePreferencesWindow          *self,
                                                       const IdePreferenceItemEntry  *item);
IDE_AVAILABLE_IN_ALL
void                ide_preferences_window_add_check  (IdePreferencesWindow          *self,
                                                       const IdePreferenceItemEntry  *item);
IDE_AVAILABLE_IN_ALL
void                ide_preferences_window_toggle     (const char                    *page_name,
                                                       const IdePreferenceItemEntry  *entry,
                                                       AdwPreferencesGroup           *group,
                                                       gpointer                       user_data);
IDE_AVAILABLE_IN_ALL
void                ide_preferences_window_check      (const char                    *page_name,
                                                       const IdePreferenceItemEntry  *entry,
                                                       AdwPreferencesGroup           *group,
                                                       gpointer                       user_data);
IDE_AVAILABLE_IN_ALL
void                ide_preferences_window_spin       (const char                    *page_name,
                                                       const IdePreferenceItemEntry  *entry,
                                                       AdwPreferencesGroup           *group,
                                                       gpointer                       user_data);
IDE_AVAILABLE_IN_ALL
void                ide_preferences_window_font       (const char                    *page_name,
                                                       const IdePreferenceItemEntry  *entry,
                                                       AdwPreferencesGroup           *group,
                                                       gpointer                       user_data);
IDE_AVAILABLE_IN_ALL
void                ide_preferences_window_combo      (const char                    *page_name,
                                                       const IdePreferenceItemEntry  *entry,
                                                       AdwPreferencesGroup           *group,
                                                       gpointer                       user_data);

G_END_DECLS
