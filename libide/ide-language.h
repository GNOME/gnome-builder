/* ide-language.h
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

#ifndef IDE_LANGUAGE_H
#define IDE_LANGUAGE_H

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_LANGUAGE            (ide_language_get_type())
#define IDE_LANGUAGE_EXTENSION_POINT "org.gnome.libide.extensions.language"

G_DECLARE_DERIVABLE_TYPE (IdeLanguage, ide_language, IDE, LANGUAGE, IdeObject)

struct _IdeLanguageClass
{
  IdeObjectClass parent;

  IdeDiagnostician  *(*get_diagnostician)   (IdeLanguage *self);
  IdeHighlighter    *(*get_highlighter)     (IdeLanguage *self);
  IdeIndenter       *(*get_indenter)        (IdeLanguage *self);
  const gchar       *(*get_name)            (IdeLanguage *self);
  IdeRefactory      *(*get_refactory)       (IdeLanguage *self);
  IdeSymbolResolver *(*get_symbol_resolver) (IdeLanguage *self);
};

IdeDiagnostician  *ide_language_get_diagnostician   (IdeLanguage *self);
IdeHighlighter    *ide_language_get_highlighter     (IdeLanguage *self);
const gchar       *ide_language_get_id              (IdeLanguage *self);
IdeIndenter       *ide_language_get_indenter        (IdeLanguage *self);
const gchar       *ide_language_get_name            (IdeLanguage *self);
IdeRefactory      *ide_language_get_refactory       (IdeLanguage *self);
IdeSymbolResolver *ide_language_get_symbol_resolver (IdeLanguage *self);

G_END_DECLS

#endif /* IDE_LANGUAGE_H */
