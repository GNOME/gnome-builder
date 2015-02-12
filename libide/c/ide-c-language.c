/* ide-c-language.c
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

#include "ide-c-indenter.h"
#include "ide-c-language.h"
#include "ide-clang-diagnostic-provider.h"
#include "ide-clang-highlighter.h"
#include "ide-clang-symbol-resolver.h"
#include "ide-diagnostician.h"
#include "ide-private.h"

typedef struct
{
  IdeDiagnostician  *diagnostician;
  IdeHighlighter    *highlighter;
  IdeIndenter       *indenter;
  IdeRefactory      *refactory;
  IdeSymbolResolver *symbol_resolver;
} IdeCLanguagePrivate;

static void _g_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_EXTENDED (IdeCLanguage, ide_c_language, IDE_TYPE_LANGUAGE, 0,
                        G_ADD_PRIVATE (IdeCLanguage)
                        G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                               _g_initable_iface_init))

static IdeDiagnostician *
ide_c_language_get_diagnostician (IdeLanguage *language)
{
  IdeCLanguage *self = (IdeCLanguage *)language;
  IdeCLanguagePrivate *priv = ide_c_language_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_C_LANGUAGE (self), NULL);

  return priv->diagnostician;
}

static IdeHighlighter *
ide_c_language_get_highlighter (IdeLanguage *language)
{
  IdeCLanguage *self = (IdeCLanguage *)language;
  IdeCLanguagePrivate *priv = ide_c_language_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_C_LANGUAGE (self), NULL);

  return priv->highlighter;
}

static IdeIndenter *
ide_c_language_get_indenter (IdeLanguage *language)
{
  IdeCLanguage *self = (IdeCLanguage *)language;
  IdeCLanguagePrivate *priv = ide_c_language_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_C_LANGUAGE (self), NULL);

  return priv->indenter;
}

static IdeRefactory *
ide_c_language_get_refactory (IdeLanguage *language)
{
  IdeCLanguage *self = (IdeCLanguage *)language;
  IdeCLanguagePrivate *priv = ide_c_language_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_C_LANGUAGE (self), NULL);

  return priv->refactory;
}

static IdeSymbolResolver *
ide_c_language_get_symbol_resolver (IdeLanguage *language)
{
  IdeCLanguage *self = (IdeCLanguage *)language;
  IdeCLanguagePrivate *priv = ide_c_language_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_C_LANGUAGE (self), NULL);

  return priv->symbol_resolver;
}

static const gchar *
ide_c_language_get_name (IdeLanguage *self)
{
  return _("C");
}

static void
ide_c_language_dispose (GObject *object)
{
  IdeCLanguage *self = (IdeCLanguage *)object;
  IdeCLanguagePrivate *priv = ide_c_language_get_instance_private (self);

  g_clear_object (&priv->diagnostician);
  g_clear_object (&priv->highlighter);
  g_clear_object (&priv->indenter);
  g_clear_object (&priv->refactory);
  g_clear_object (&priv->symbol_resolver);

  G_OBJECT_CLASS (ide_c_language_parent_class)->dispose (object);
}

static void
ide_c_language_class_init (IdeCLanguageClass *klass)
{
  IdeLanguageClass *language_class = IDE_LANGUAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  language_class->get_diagnostician = ide_c_language_get_diagnostician;
  language_class->get_highlighter = ide_c_language_get_highlighter;
  language_class->get_indenter = ide_c_language_get_indenter;
  language_class->get_refactory = ide_c_language_get_refactory;
  language_class->get_symbol_resolver = ide_c_language_get_symbol_resolver;
  language_class->get_name = ide_c_language_get_name;

  object_class->dispose = ide_c_language_dispose;
}

static void
ide_c_language_init (IdeCLanguage *self)
{
}

static gboolean
ide_c_language_initiable_init (GInitable     *initable,
                               GCancellable  *cancellable,
                               GError       **error)
{
  IdeCLanguage *self = (IdeCLanguage *)initable;
  IdeCLanguagePrivate *priv = ide_c_language_get_instance_private (self);
  const gchar *id;

  g_return_val_if_fail (IDE_IS_C_LANGUAGE (self), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  id = ide_language_get_id (IDE_LANGUAGE (self));

  if ((g_strcmp0 (id, "c") == 0) || (g_strcmp0 (id, "chdr") == 0))
    {
      IdeContext *context;
      IdeDiagnosticProvider *provider;

      context = ide_object_get_context (IDE_OBJECT (initable));

      /*
       * Create our diagnostician using clang as a backend.
       */
      priv->diagnostician = g_object_new (IDE_TYPE_DIAGNOSTICIAN,
                                          "context", context,
                                          NULL);
      provider = g_object_new (IDE_TYPE_CLANG_DIAGNOSTIC_PROVIDER,
                               "context", context,
                               NULL);
      _ide_diagnostician_add_provider (priv->diagnostician, provider);
      g_clear_object (&provider);

      /*
       * Create our highlighter that will use clang for semantic highlighting.
       */
      priv->highlighter = g_object_new (IDE_TYPE_CLANG_HIGHLIGHTER,
                                        "context", context,
                                        NULL);

      /*
       * Create our indenter to provide as-you-type indentation.
       */
      priv->indenter = g_object_new (IDE_TYPE_C_INDENTER,
                                     "context", context,
                                     NULL);

      /*
       * TODO: Refactory design (rename local, extract method, etc).
       */

      /*
       * Create our symbol resolver to help discover symbols within a file
       * as well as what symbol is at "location X".
       */
      priv->symbol_resolver = g_object_new (IDE_TYPE_CLANG_SYMBOL_RESOLVER,
                                            "context", context,
                                            NULL);

      return TRUE;
    }

  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_NOT_SUPPORTED,
               _("Language id does not match a C language."));

  return FALSE;
}

static void
_g_initable_iface_init (GInitableIface *iface)
{
  iface->init = ide_c_language_initiable_init;
}
