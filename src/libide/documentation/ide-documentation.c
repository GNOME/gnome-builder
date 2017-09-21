/* ide-documentation.c
 *
 * Copyright (C) 2017 Lucie Charvat <luci.charvat@gmail.com>
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

#include <glib/gi18n.h>
#include <libpeas/peas.h>

#include "ide-documentation.h"
#include "ide-documentation-provider.h"
#include "ide-documentation-proposal.h"

struct _IdeDocumentation
{
  GObject                  parent_instance;
  PeasExtensionSet        *extensions;
};

G_DEFINE_TYPE (IdeDocumentation, ide_documentation, IDE_TYPE_OBJECT)

static void
ide_documentation_finalize (GObject *object)
{
  IdeDocumentation *self = (IdeDocumentation *)object;

  g_clear_object (&self->extensions);

  G_OBJECT_CLASS (ide_documentation_parent_class)->finalize (object);
}

static void
ide_documentation_search_foreach (PeasExtensionSet *set,
                                  PeasPluginInfo   *plugin_info,
                                  PeasExtension    *exten,
                                  gpointer          user_data)
{
  IdeDocumentationProvider *provider = (IdeDocumentationProvider *) exten;
  IdeDocumentationInfo *info = user_data;

  if (ide_documentation_provider_get_context (provider) == ide_documentation_info_get_context (info))
    ide_documentation_provider_get_info (provider, info);
}

static void
ide_documentation_constructed (GObject *object)
{
  IdeDocumentation *self = (IdeDocumentation *)object;
  IdeContext *context;

  g_assert (IDE_IS_DOCUMENTATION (self));

  G_OBJECT_CLASS (ide_documentation_parent_class)->constructed (object);

  context = ide_object_get_context (IDE_OBJECT (self));

  self->extensions = peas_extension_set_new (peas_engine_get_default (),
                                             IDE_TYPE_DOCUMENTATION_PROVIDER,
                                             "context", context,
                                             NULL);
}

static void
ide_documentation_class_init (IdeDocumentationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_documentation_finalize;
  object_class->constructed = ide_documentation_constructed;
}

static void
ide_documentation_init (IdeDocumentation *self)
{
}

/**
 * ide_documentation_get_info:
 * @self: An #IdeDocumentation
 * @input: the search keyword
 * @context: the context for the request
 *
 * Requests documentation for the keyword.
 *
 * Returns: (transfer full): An #IdeDocumentationInfo
 */
IdeDocumentationInfo *
ide_documentation_get_info (IdeDocumentation        *self,
                            const gchar             *input,
                            IdeDocumentationContext  context)
{
  IdeDocumentationInfo *info;

  g_assert (IDE_IS_DOCUMENTATION (self));
  g_return_val_if_fail (input != NULL, NULL);

  info = ide_documentation_info_new (input, context);

  peas_extension_set_foreach (self->extensions,
                              ide_documentation_search_foreach,
                              info);
  return info;
}
