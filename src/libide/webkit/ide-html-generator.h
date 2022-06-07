/* ide-html-generator.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#include <libide-core.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define IDE_TYPE_HTML_GENERATOR (ide_html_generator_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeHtmlGenerator, ide_html_generator, IDE, HTML_GENERATOR, GObject)

struct _IdeHtmlGeneratorClass
{
  GObjectClass parent_class;

  void    (*invalidate)      (IdeHtmlGenerator     *self);
  void    (*generate_async)  (IdeHtmlGenerator     *self,
                              GCancellable         *cancellable,
                              GAsyncReadyCallback   callback,
                              gpointer              user_data);
  GBytes *(*generate_finish) (IdeHtmlGenerator     *self,
                              GAsyncResult         *result,
                              GError              **error);
};

IDE_AVAILABLE_IN_ALL
const char       *ide_html_generator_get_base_uri    (IdeHtmlGenerator     *self);
IDE_AVAILABLE_IN_ALL
void              ide_html_generator_set_base_uri    (IdeHtmlGenerator     *self,
                                                      const char           *base_uri);
IDE_AVAILABLE_IN_ALL
void              ide_html_generator_invalidate      (IdeHtmlGenerator     *self);
IDE_AVAILABLE_IN_ALL
void              ide_html_generator_generate_async  (IdeHtmlGenerator     *self,
                                                      GCancellable         *cancellable,
                                                      GAsyncReadyCallback   callback,
                                                      gpointer              user_data);
IDE_AVAILABLE_IN_ALL
GBytes           *ide_html_generator_generate_finish (IdeHtmlGenerator     *self,
                                                      GAsyncResult         *result,
                                                      GError              **error);
IDE_AVAILABLE_IN_ALL
IdeHtmlGenerator *ide_html_generator_new_for_buffer  (GtkTextBuffer        *buffer);

G_END_DECLS
