/* ide-python-language.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>

#include "ide-python-indenter.h"
#include "ide-python-language.h"

struct _IdePythonLanguage
{
  IdeLanguage        parent_instance;
  IdePythonIndenter *indenter;
};

static void initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_EXTENDED (IdePythonLanguage,
                        ide_python_language,
                        IDE_TYPE_LANGUAGE,
                        0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                               initable_iface_init))

static IdeIndenter *
ide_python_language_get_indenter (IdeLanguage *language)
{
  IdePythonLanguage *self = (IdePythonLanguage *)language;

  g_return_val_if_fail (IDE_IS_PYTHON_LANGUAGE (self), NULL);

  if (!self->indenter)
    {
      IdeContext *context;

      context = ide_object_get_context (IDE_OBJECT (language));
      self->indenter = g_object_new (IDE_TYPE_PYTHON_INDENTER,
                                     "context", context,
                                     NULL);
    }

  return IDE_INDENTER (self->indenter);
}

static void
ide_python_language_finalize (GObject *object)
{
  IdePythonLanguage *self = (IdePythonLanguage *)object;

  g_clear_object (&self->indenter);

  G_OBJECT_CLASS (ide_python_language_parent_class)->finalize (object);
}

static void
ide_python_language_class_init (IdePythonLanguageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeLanguageClass *language_class = IDE_LANGUAGE_CLASS (klass);

  object_class->finalize = ide_python_language_finalize;

  language_class->get_indenter = ide_python_language_get_indenter;
}

static void
ide_python_language_init (IdePythonLanguage *self)
{
}

static gboolean
ide_python_language_initable_init (GInitable     *initable,
                                   GCancellable  *cancellable,
                                   GError       **error)
{
  const gchar *id;

  g_return_val_if_fail (IDE_IS_PYTHON_LANGUAGE (initable), FALSE);

  id = ide_language_get_id (IDE_LANGUAGE (initable));

  return ((g_strcmp0 (id, "python") == 0) || (g_strcmp0 (id, "python3") == 0));
}

static void
initable_iface_init (GInitableIface *iface)
{
  iface->init = ide_python_language_initable_init;
}
