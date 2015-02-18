/* ide-language.c
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

#include "ide-diagnostician.h"
#include "ide-gca-diagnostic-provider.h"
#include "ide-highlighter.h"
#include "ide-indenter.h"
#include "ide-internal.h"
#include "ide-language.h"
#include "ide-refactory.h"
#include "ide-symbol-resolver.h"

typedef struct
{
  const gchar *id;
} IdeLanguagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeLanguage, ide_language, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_DIAGNOSTICIAN,
  PROP_HIGHLIGHTER,
  PROP_INDENTER,
  PROP_ID,
  PROP_NAME,
  PROP_REFACTORY,
  PROP_SYMBOL_RESOLVER,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];
static IdeDiagnostician *gDiagnostician;

/**
 * ide_language_get_diagnostician:
 *
 * Returns the #IdeDiagnostician for the #IdeLanguage.
 *
 * The diagnostician is responsible for querying the proper language tools to
 * diagnose issues with a particular #IdeFile.
 *
 * See ide_diagnostician_diagnose_async() for more information.
 *
 * If the #IdeLanguage does not have an #IdeDiagnostician, then %NULL is
 * returned.
 *
 * Returns: (transfer none) (nullable): An #IdeDiagnostician or %NULL.
 */
IdeDiagnostician *
ide_language_get_diagnostician (IdeLanguage *self)
{
  g_return_val_if_fail (IDE_IS_LANGUAGE (self), NULL);

  if (IDE_LANGUAGE_GET_CLASS (self)->get_diagnostician)
    return IDE_LANGUAGE_GET_CLASS (self)->get_diagnostician (self);

  return NULL;
}

static IdeDiagnostician *
ide_language_real_get_diagnostician (IdeLanguage *self)
{
  if (!gDiagnostician)
    {
      IdeDiagnosticProvider *provider;
      IdeContext *context;

      context = ide_object_get_context (IDE_OBJECT (self));
      gDiagnostician = g_object_new (IDE_TYPE_DIAGNOSTICIAN,
                                     "context", context,
                                     NULL);
      provider = g_object_new (IDE_TYPE_GCA_DIAGNOSTIC_PROVIDER,
                               "context", context,
                               NULL);
      _ide_diagnostician_add_provider (gDiagnostician, provider);
    }

  return gDiagnostician;
}

/**
 * ide_language_get_highlighter:
 *
 * Fetches the #IdeHighlighter for the #IdeLanguage.
 *
 * If @language does not provide a semantic highlighter, %NULL is returned.
 *
 * Returns: (transfer none) (nullable): An #IdeHighlighter or %NULL.
 */
IdeHighlighter *
ide_language_get_highlighter (IdeLanguage *self)
{
  g_return_val_if_fail (IDE_IS_LANGUAGE (self), NULL);

  if (IDE_LANGUAGE_GET_CLASS (self)->get_highlighter)
    return IDE_LANGUAGE_GET_CLASS (self)->get_highlighter (self);

  return NULL;
}

/**
 * ide_language_get_indenter:
 *
 * Fetches the #IdeIndenter for @language.
 *
 * If @language does not provide an #IdeIndenter, then %NULL is returned.
 *
 * Returns: (transfer none) (nullable): An #IdeIndenter or %NULL.
 */
IdeIndenter *
ide_language_get_indenter (IdeLanguage *self)
{
  g_return_val_if_fail (IDE_IS_LANGUAGE (self), NULL);

  if (IDE_LANGUAGE_GET_CLASS (self)->get_indenter)
    return IDE_LANGUAGE_GET_CLASS (self)->get_indenter (self);

  return NULL;
}

/**
 * ide_language_get_refactory:
 *
 * Fetches the refactory for @language.
 *
 * If @language does not provide an #IdeRefactory, then %NULL is returned.
 *
 * Returns: (transfer none) (nullable): An #IdeRefactory or %NULL.
 */
IdeRefactory *
ide_language_get_refactory (IdeLanguage *self)
{
  g_return_val_if_fail (IDE_IS_LANGUAGE (self), NULL);

  if (IDE_LANGUAGE_GET_CLASS (self)->get_refactory)
    return IDE_LANGUAGE_GET_CLASS (self)->get_refactory (self);

  return NULL;
}

/**
 * ide_language_get_symbol_resolver:
 *
 * Fetches the #IdeSymbolResolver for @language.
 *
 * If @language does not provide an #IdeSymbolResolver, then %NULL is returned.
 *
 * Returns: (transfer none) (nullable): An #IdeSymbolResolver or %NULL.
 */
IdeSymbolResolver *
ide_language_get_symbol_resolver (IdeLanguage *self)
{
  g_return_val_if_fail (IDE_IS_LANGUAGE (self), NULL);

  if (IDE_LANGUAGE_GET_CLASS (self)->get_symbol_resolver)
    return IDE_LANGUAGE_GET_CLASS (self)->get_symbol_resolver (self);

  return NULL;
}

/**
 * ide_language_get_id:
 *
 * Fetches the unique identifier for the language.
 *
 * Returns: A string such as "c" or "python".
 */
const gchar *
ide_language_get_id (IdeLanguage *self)
{
  IdeLanguagePrivate *priv = ide_language_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LANGUAGE (self), NULL);

  return priv->id;
}

static void
ide_language_set_id (IdeLanguage *self,
                     const gchar *id)
{
  IdeLanguagePrivate *priv = ide_language_get_instance_private (self);

  g_return_if_fail (IDE_IS_LANGUAGE (self));
  g_return_if_fail (!priv->id);

  priv->id = g_intern_string (id);
}

/**
 * ide_language_get_name:
 *
 * Fetches the display name for the language.
 *
 * Returns: A string containing the display name.
 */
const gchar *
ide_language_get_name (IdeLanguage *self)
{
  g_return_val_if_fail (IDE_IS_LANGUAGE (self), NULL);

  if (IDE_LANGUAGE_GET_CLASS (self)->get_name)
    return IDE_LANGUAGE_GET_CLASS (self)->get_name (self);

  return ide_language_get_id (self);
}

static void
ide_language_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  IdeLanguage *self = IDE_LANGUAGE (object);

  switch (prop_id)
    {
    case PROP_DIAGNOSTICIAN:
      g_value_set_object (value, ide_language_get_diagnostician (self));
      break;

    case PROP_HIGHLIGHTER:
      g_value_set_object (value, ide_language_get_highlighter (self));
      break;

    case PROP_ID:
      g_value_set_string (value, ide_language_get_id (self));
      break;

    case PROP_INDENTER:
      g_value_set_object (value, ide_language_get_indenter (self));
      break;

    case PROP_NAME:
      g_value_set_string (value, ide_language_get_name (self));
      break;

    case PROP_REFACTORY:
      g_value_set_object (value, ide_language_get_refactory (self));
      break;

    case PROP_SYMBOL_RESOLVER:
      g_value_set_object (value, ide_language_get_symbol_resolver (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_language_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  IdeLanguage *self = IDE_LANGUAGE (object);

  switch (prop_id)
    {
    case PROP_ID:
      ide_language_set_id (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_language_class_init (IdeLanguageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = ide_language_get_property;
  object_class->set_property = ide_language_set_property;

  klass->get_diagnostician = ide_language_real_get_diagnostician;

  gParamSpecs [PROP_DIAGNOSTICIAN] =
    g_param_spec_object ("diagnostician",
                         _("Diagnostician"),
                         _("The diagnostician for the language."),
                         IDE_TYPE_DIAGNOSTICIAN,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_DIAGNOSTICIAN,
                                   gParamSpecs [PROP_DIAGNOSTICIAN]);

  gParamSpecs [PROP_HIGHLIGHTER] =
    g_param_spec_object ("highlighter",
                         _("Highlighter"),
                         _("The semantic highlighter for the language."),
                         IDE_TYPE_HIGHLIGHTER,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_HIGHLIGHTER,
                                   gParamSpecs [PROP_HIGHLIGHTER]);

  gParamSpecs [PROP_ID] =
    g_param_spec_string ("id",
                         _("Id"),
                         _("The language identifier such as \"c\"."),
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_ID,
                                   gParamSpecs [PROP_ID]);

  gParamSpecs [PROP_INDENTER] =
    g_param_spec_object ("indenter",
                         _("Indenter"),
                         _("The semantic indenter for the language."),
                         IDE_TYPE_INDENTER,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_INDENTER,
                                   gParamSpecs [PROP_INDENTER]);

  gParamSpecs [PROP_NAME] =
    g_param_spec_string ("name",
                         _("Name"),
                         _("The name of the language."),
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_NAME,
                                   gParamSpecs [PROP_NAME]);

  gParamSpecs [PROP_REFACTORY] =
    g_param_spec_object ("refactory",
                         _("Refactory"),
                         _("The refactory engine for the language."),
                         IDE_TYPE_REFACTORY,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_REFACTORY,
                                   gParamSpecs [PROP_REFACTORY]);

  gParamSpecs [PROP_SYMBOL_RESOLVER] =
    g_param_spec_object ("symbol-resolver",
                         _("Symbol Resolver"),
                         _("The symbol resolver for the language."),
                         IDE_TYPE_SYMBOL_RESOLVER,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SYMBOL_RESOLVER,
                                   gParamSpecs [PROP_SYMBOL_RESOLVER]);
}

static void
ide_language_init (IdeLanguage *self)
{
}
