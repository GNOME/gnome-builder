/* ide-vala-language.c
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

#include "ide-vala-language.h"

struct _IdeValaLanguage
{
  IdeLanguage parent_instance;
};

static void initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeValaLanguage,
                         ide_vala_language,
                         IDE_TYPE_LANGUAGE,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                initable_iface_init))

static gboolean
ide_vala_language_initable_init (GInitable     *initable,
                                 GCancellable  *cancellable,
                                 GError       **error)
{
  const gchar *id;

  g_return_val_if_fail (IDE_IS_VALA_LANGUAGE (initable), FALSE);

  id = ide_language_get_id (IDE_LANGUAGE (initable));

  return (g_strcmp0 (id, "vala") == 0);
}

static void
initable_iface_init (GInitableIface *iface)
{
  iface->init = ide_vala_language_initable_init;
}

static void
ide_vala_language_class_init (IdeValaLanguageClass *klass)
{
}

static void
ide_vala_language_init (IdeValaLanguage *self)
{
}
