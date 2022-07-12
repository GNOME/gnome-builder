/* ide-lsp-diagnostic.c
 *
 * Copyright 2022 JCWasmx86 <JCWasmx86@t-online.de>
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

#define G_LOG_DOMAIN "ide-lsp-diagnostic"

#include "config.h"

#include "ide-lsp-diagnostic.h"

typedef struct
{
  GVariant *raw;
} IdeLspDiagnosticPrivate;

enum {
  PROP_0,
  PROP_RAW,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdeLspDiagnostic, ide_lsp_diagnostic, IDE_TYPE_DIAGNOSTIC)

static GParamSpec *properties [N_PROPS];

IdeLspDiagnostic *
ide_lsp_diagnostic_new (IdeDiagnosticSeverity  severity,
                        const gchar           *message,
                        IdeLocation           *location,
                        GVariant              *raw_value)

{
  return g_object_new (IDE_TYPE_LSP_DIAGNOSTIC,
                       "severity", severity,
                       "location", location,
                       "text", message,
                       "raw", raw_value,
                       NULL);
}

static void
ide_lsp_diagnostic_finalize (GObject *object)
{
  IdeLspDiagnostic *self = (IdeLspDiagnostic *)object;
  IdeLspDiagnosticPrivate *priv = ide_lsp_diagnostic_get_instance_private (self);

  g_clear_pointer (&priv->raw, g_variant_unref);

  G_OBJECT_CLASS (ide_lsp_diagnostic_parent_class)->finalize (object);
}

static void
ide_lsp_diagnostic_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  IdeLspDiagnostic *self = IDE_LSP_DIAGNOSTIC (object);
  IdeLspDiagnosticPrivate *priv = ide_lsp_diagnostic_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_RAW:
      g_value_set_variant (value, priv->raw);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_lsp_diagnostic_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  IdeLspDiagnostic *self = (IdeLspDiagnostic *)object;
  IdeLspDiagnosticPrivate *priv = ide_lsp_diagnostic_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_RAW:
      priv->raw = g_value_dup_variant (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_lsp_diagnostic_class_init (IdeLspDiagnosticClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_lsp_diagnostic_finalize;
  object_class->get_property = ide_lsp_diagnostic_get_property;
  object_class->set_property = ide_lsp_diagnostic_set_property;

  properties [PROP_RAW] =
    g_param_spec_variant ("raw",
                          "Raw",
                          "Raw diagnostic",
                          g_variant_type_new ("a{sv}"),
                          NULL,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_lsp_diagnostic_init (IdeLspDiagnostic *self)
{
}

/**
 * ide_lsp_diagnostic_dup_raw:
 * @self: an #IdeLspDiagnostic
 *
 * Increments the reference count of the underlying diagnostic variant and
 * returns it.
 *
 * Returns: (transfer full) (nullable): a #GVariant with it's reference count incremented
 */
GVariant *
ide_lsp_diagnostic_dup_raw (IdeLspDiagnostic *self)
{
  IdeLspDiagnosticPrivate *priv = ide_lsp_diagnostic_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LSP_DIAGNOSTIC (self), NULL);

  return g_variant_ref (priv->raw);
}
