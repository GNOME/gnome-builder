/* gbp-snippet-application-addin.c
 *
 * Copyright 2022 Günther Wagner <info@gunibert.de>
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

#define G_LOG_DOMAIN "gbp-snippet-application-addin"

#include "config.h"

#include <libide-gui.h>

#include "gbp-snippet-application-addin.h"

struct _GbpSnippetApplicationAddin
{
  GObject parent_instance;
};

static void
gbp_snippet_application_addin_load (IdeApplicationAddin *addin,
                                    IdeApplication      *app)
{
  GbpSnippetApplicationAddin *self = (GbpSnippetApplicationAddin *)addin;
  GtkSourceSnippetManager *manager;
  char **search_path;
  gsize len;

  IDE_ENTRY;

  g_assert (GBP_IS_SNIPPET_APPLICATION_ADDIN (self));
  g_assert (IDE_IS_APPLICATION (app));

  manager = gtk_source_snippet_manager_get_default ();

  search_path = g_strdupv ((char **)gtk_source_snippet_manager_get_search_path (manager));
  len = g_strv_length (search_path);
  search_path = g_realloc_n (search_path, len + 2, sizeof (char **));
  search_path[len++] = g_strdup ("resource:///org/gnome/builder/snippets/");
  search_path[len] = NULL;

  gtk_source_snippet_manager_set_search_path (manager,
                                              (const char * const *)search_path);

  g_strfreev (search_path);

  IDE_EXIT;
}

static void
application_addin_iface_init (IdeApplicationAddinInterface *iface)
{
  iface->load = gbp_snippet_application_addin_load;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpSnippetApplicationAddin, gbp_snippet_application_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_APPLICATION_ADDIN, application_addin_iface_init))

static void
gbp_snippet_application_addin_class_init (GbpSnippetApplicationAddinClass *klass)
{
}

static void
gbp_snippet_application_addin_init (GbpSnippetApplicationAddin *self)
{
}

