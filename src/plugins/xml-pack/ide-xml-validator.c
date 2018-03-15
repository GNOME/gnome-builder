/* ide-xml-validator.c
 *
 * Copyright © 2017 Sebastien Lafargue <slafargue@gnome.org>
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

#include <glib.h>

#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/relaxng.h>
#include <libxml/tree.h>
#include <libxml/valid.h>
#include <libxml/xmlschemas.h>

#include "ide-xml-validator.h"

struct _IdeXmlValidator
{
  IdeObject         parent_instance;

  GPtrArray        *diagnostics_array;
  xmlDtd           *dtd;
  xmlRelaxNG       *rng;
  xmlSchema        *xml_schema;

  IdeXmlSchemaKind  kind;
  guint             dtd_use_subsets : 1;
};

typedef struct _ValidState
{
  IdeXmlValidator  *self;
  IdeXmlSchemaKind  kind;
  xmlValidCtxt     *dtd_valid_context;
  xmlDoc           *doc;
} ValidState;

G_DEFINE_TYPE (IdeXmlValidator, ide_xml_validator, IDE_TYPE_OBJECT)

IdeXmlSchemaKind
ide_xml_validator_get_kind (IdeXmlValidator *self)
{
  g_return_val_if_fail (IDE_IS_XML_VALIDATOR (self), SCHEMA_KIND_NONE);

  return self->kind;
}

static IdeDiagnostic *
create_diagnostic (IdeXmlValidator        *self,
                   GFile                  *file,
                   xmlError               *error,
                   IdeDiagnosticSeverity   severity)
{
  IdeContext *context;
  IdeDiagnostic *diagnostic;
  g_autoptr(IdeSourceLocation) loc = NULL;
  g_autoptr(IdeFile) ifile = NULL;
  gint line;

  g_assert (IDE_IS_XML_VALIDATOR (self));
  g_assert (G_IS_FILE (file));
  g_assert (error != NULL);

  context = ide_object_get_context (IDE_OBJECT (self));
  ifile = ide_file_new (context, file);
  line = (error->line > 0) ? error->line - 1 : 0;
  loc = ide_source_location_new (ifile, line, 0, 0);

  diagnostic = ide_diagnostic_new (severity, error->message, loc);

  return diagnostic;
}

static void
ide_xml_valid_error (ValidState  *state,
                     const gchar *msg,
                     ...)
{
  IdeXmlValidator *self = state->self;
  IdeDiagnostic *diagnostic;
  g_autoptr (GFile) file = NULL;

  g_assert (IDE_IS_XML_VALIDATOR (self));

  file = g_file_new_for_uri ((gchar *)state->doc->URL);
  diagnostic = create_diagnostic (self,
                                  file,
                                  xmlGetLastError (),
                                  IDE_DIAGNOSTIC_ERROR);

  g_ptr_array_add (self->diagnostics_array, diagnostic);
}

static void
ide_xml_valid_warning (ValidState  *state,
                       const gchar *msg,
                       ...)
{
  IdeXmlValidator *self = state->self;
  IdeDiagnostic *diagnostic;
  g_autoptr (GFile) file = NULL;

  g_assert (IDE_IS_XML_VALIDATOR (self));

  file = g_file_new_for_uri ((gchar *)state->doc->URL);
  diagnostic = create_diagnostic (self,
                                  file,
                                  xmlGetLastError (),
                                  IDE_DIAGNOSTIC_WARNING);

  g_ptr_array_add (self->diagnostics_array, diagnostic);
}

/**
 * ide_xml_validator_validate:
 * @self: a #IdeXmlValidator instance
 * @doc: the xmldoc to validate
 * @diagnostics: (out) (nullable): a location to store a #IdeDiagnostics object
 *
 * Returns: %TRUE if the validation succeeded, %FALSE otherwise
 */
gboolean
ide_xml_validator_validate (IdeXmlValidator   *self,
                            xmlDoc            *doc,
                            IdeDiagnostics   **diagnostics)
{
  xmlValidCtxt *dtd_valid_context;
  xmlSchemaValidCtxt *xml_schema_valid_context;
  xmlRelaxNGValidCtxt *rng_valid_context;
  ValidState state;
  gboolean ret = FALSE;

  g_assert (IDE_IS_XML_VALIDATOR (self));
  g_assert (doc != NULL);
  g_assert ((diagnostics != NULL && *diagnostics == NULL) || diagnostics == NULL);

  xmlLineNumbersDefault (1);

  state.self = self;
  state.doc = doc;
  state.kind = self->kind;

  if (self->kind == SCHEMA_KIND_DTD)
    {
      if (NULL == (dtd_valid_context = xmlNewValidCtxt ()))
        goto end;

      state.dtd_valid_context = dtd_valid_context;
      dtd_valid_context->userData = &state;
      dtd_valid_context->error = (xmlValidityErrorFunc)ide_xml_valid_error;
      dtd_valid_context->warning = (xmlValidityWarningFunc)ide_xml_valid_warning;

      if (self->dtd_use_subsets)
        ret = xmlValidateDocument (dtd_valid_context, doc);
      else
        ret = xmlValidateDtd (dtd_valid_context, doc, self->dtd);

      xmlFreeValidCtxt (dtd_valid_context);
    }
  else if (self->kind == SCHEMA_KIND_XML_SCHEMA)
    {
      if (NULL == (xml_schema_valid_context = xmlSchemaNewValidCtxt (self->xml_schema)))
        goto end;

      xmlSchemaSetValidErrors (xml_schema_valid_context,
                               (xmlSchemaValidityErrorFunc)ide_xml_valid_error,
                               (xmlSchemaValidityWarningFunc)ide_xml_valid_warning,
                               &state);

      ret = xmlSchemaValidateDoc (xml_schema_valid_context, doc);
      xmlSchemaFreeValidCtxt (xml_schema_valid_context);
    }
  else if (self->kind == SCHEMA_KIND_RNG)
    {
      if (NULL == (rng_valid_context = xmlRelaxNGNewValidCtxt (self->rng)))
        goto end;

      xmlRelaxNGSetValidErrors (rng_valid_context,
                                (xmlRelaxNGValidityErrorFunc)ide_xml_valid_error,
                                (xmlRelaxNGValidityWarningFunc)ide_xml_valid_warning,
                                &state);

      ret = xmlRelaxNGValidateDoc (rng_valid_context, doc);
      xmlRelaxNGFreeValidCtxt (rng_valid_context);
    }
  else
    g_assert_not_reached ();

end:
  if (diagnostics != NULL)
    *diagnostics = ide_diagnostics_new (self->diagnostics_array);
  else
    g_clear_pointer (&self->diagnostics_array, g_ptr_array_unref);

  self->diagnostics_array = g_ptr_array_new_with_free_func ((GDestroyNotify)ide_diagnostic_unref);

  return ret;
}

/* For VALIDATOR_KIND_DTD, if data == NULL, the document
 * subsets (internal/external) will be used for the validation.
 */
gboolean
ide_xml_validator_set_schema (IdeXmlValidator  *self,
                              IdeXmlSchemaKind  kind,
                              const gchar      *data,
                              gsize             size)
{
  xmlDoc *dtd_doc;
  xmlRelaxNGParserCtxt *rng_parser;
  xmlSchemaParserCtxt *schema_parser;
  gboolean ret = FALSE;

  g_assert (IDE_IS_XML_VALIDATOR (self));

  if (kind == SCHEMA_KIND_DTD)
    {
      if (data == NULL)
        {
          self->dtd_use_subsets = TRUE;
          ret = TRUE;
        }

      if (NULL != (dtd_doc = xmlParseMemory (data, size)))
        {
          if (NULL != (self->dtd = xmlNewDtd (dtd_doc, NULL, NULL, NULL)))
            ret = TRUE;

          xmlFreeDoc (dtd_doc);
        }
    }
  else if (kind == SCHEMA_KIND_RNG)
    {
      if (NULL != (rng_parser = xmlRelaxNGNewMemParserCtxt (data, size)) &&
          NULL != (self->rng = xmlRelaxNGParse (rng_parser)))
        ret = TRUE;
    }
  else if (kind == SCHEMA_KIND_XML_SCHEMA)
    {
      if (NULL != (schema_parser = xmlSchemaNewMemParserCtxt (data, size)) &&
          NULL != (self->xml_schema = xmlSchemaParse (schema_parser)))
        ret = TRUE;
    }
  else
    g_assert_not_reached ();

  self->kind = (ret) ? kind : SCHEMA_KIND_NONE;

  return ret;
}

IdeXmlValidator *
ide_xml_validator_new (IdeContext *context)
{
  return g_object_new (IDE_TYPE_XML_VALIDATOR,
                       "context", context,
                       NULL);
}

static void
ide_xml_validator_finalize (GObject *object)
{
  IdeXmlValidator *self = (IdeXmlValidator *)object;

  g_clear_pointer (&self->dtd, xmlFreeDtd);
  g_clear_pointer (&self->rng, xmlRelaxNGFree);
  g_clear_pointer (&self->xml_schema, xmlSchemaFree);
  g_clear_pointer (&self->diagnostics_array, g_ptr_array_unref);

  G_OBJECT_CLASS (ide_xml_validator_parent_class)->finalize (object);
}

static void
ide_xml_validator_class_init (IdeXmlValidatorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_xml_validator_finalize;
}

static void
ide_xml_validator_init (IdeXmlValidator *self)
{
  self->diagnostics_array = g_ptr_array_new_with_free_func ((GDestroyNotify)ide_diagnostic_unref);
}
