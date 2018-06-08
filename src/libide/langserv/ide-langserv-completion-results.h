/* ide-langserv-completion-results.h
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define IDE_TYPE_LANGSERV_COMPLETION_RESULTS (ide_langserv_completion_results_get_type())

IDE_AVAILABLE_IN_3_30
G_DECLARE_FINAL_TYPE (IdeLangservCompletionResults, ide_langserv_completion_results, IDE, LANGSERV_COMPLETION_RESULTS, GObject)

IDE_AVAILABLE_IN_3_30
IdeLangservCompletionResults *ide_langserv_completion_results_new      (GVariant                     *results);
IDE_AVAILABLE_IN_3_30
void                          ide_langserv_completion_results_refilter (IdeLangservCompletionResults *self,
                                                                        const gchar                  *typed_text);

G_END_DECLS
