/* ide-snippet-application-addin.c
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

#include "ide-snippet-application-addin.h"
#include "libide-gui.h"

struct _IdeSnippetApplicationAddin
{
  IdeObject parent_instance;
};

void application_addin_implement_iface (IdeApplicationAddinInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeSnippetApplicationAddin, ide_snippet_application_addin, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_APPLICATION_ADDIN, application_addin_implement_iface))

IdeSnippetApplicationAddin *
ide_snippet_application_addin_new (void)
{
  return g_object_new (IDE_TYPE_SNIPPET_APPLICATION_ADDIN, NULL);
}

static void
ide_snippet_application_addin_finalize (GObject *object)
{
  G_OBJECT_CLASS (ide_snippet_application_addin_parent_class)->finalize (object);
}

static void
ide_snippet_application_addin_class_init (IdeSnippetApplicationAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_snippet_application_addin_finalize;
}

static void
ide_snippet_application_addin_init (IdeSnippetApplicationAddin *self)
{
}

static void
ide_snippet_application_addin_load (IdeApplicationAddin *addin,
                                    IdeApplication      *app)
{
  IdeSnippetApplicationAddin *self = IDE_SNIPPET_APPLICATION_ADDIN (addin);
  GtkSourceSnippetManager *snippet_manager;
  g_autoptr(GStrvBuilder) builder = NULL;

  g_return_if_fail (IDE_IS_SNIPPET_APPLICATION_ADDIN (self));

  snippet_manager = gtk_source_snippet_manager_get_default ();
  builder = g_strv_builder_new ();
  g_strv_builder_add (builder, "resource:///org/gnome/builder/snippets/");

  gtk_source_snippet_manager_set_search_path (snippet_manager, (const char * const *) g_strv_builder_end (builder));
}

void
application_addin_implement_iface (IdeApplicationAddinInterface *iface)
{
  iface->load = ide_snippet_application_addin_load;
}
