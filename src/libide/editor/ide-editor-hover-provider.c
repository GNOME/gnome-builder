/* ide-editor-hover-provider.c
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

#include "config.h"

#define G_LOG_DOMAIN "ide-editor-hover-provider"

#include "ide-editor-hover-provider.h"

struct _IdeEditorHoverProvider
{
  GObject parent_instance;
};

static void
hover_provider_iface_init (IdeHoverProviderInterface *iface)
{
}

G_DEFINE_TYPE_WITH_CODE (IdeEditorHoverProvider, ide_editor_hover_provider, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_HOVER_PROVIDER, hover_provider_iface_init))

static void
ide_editor_hover_provider_class_init (IdeEditorHoverProviderClass *klass)
{
}

static void
ide_editor_hover_provider_init (IdeEditorHoverProvider *self)
{
}
