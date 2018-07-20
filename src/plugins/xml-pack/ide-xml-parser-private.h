/* ide-xml-parser-private.h
 *
 * Copyright 2017 Sebastien Lafargue <slafargue@gnome.org>
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

#pragma once

#include <glib.h>
#include <libxml/parser.h>

#include "ide-xml-analysis.h"
#include "ide-xml-parser.h"
#include "ide-xml-sax.h"
#include "ide-xml-stack.h"
#include "ide-xml-symbol-node.h"

G_BEGIN_DECLS

typedef gboolean (*PostProcessingCallback) (IdeXmlParser     *self,
                                            IdeXmlSymbolNode *root_node);

typedef enum
{
  BUILD_STATE_NORMAL,
  BUILD_STATE_WAIT_END_ELEMENT,
  BUILD_STATE_GET_CONTENT,
} BuildState;

typedef enum
{
  COLOR_TAG_LABEL,
  COLOR_TAG_ID,
  COLOR_TAG_STYLE_CLASS,
  COLOR_TAG_TYPE,
  COLOR_TAG_PARENT,
  COLOR_TAG_CLASS,
  COLOR_TAG_ATTRIBUTE,
} ColorTagId;

typedef enum
{
  ERROR_STATE_NONE,
  ERROR_STATE_MISSING_TAG_END,
  ERROR_STATE_MISSING_END_TAG_MISSMATCH,
} ErrorState;

struct _IdeXmlParser
{
  GObject                 parent_instance;
  GSettings              *settings;
  GArray                 *color_tags;
  PostProcessingCallback  post_processing_callback;
};

typedef struct
{
  IdeXmlParser      *self;

  GFile             *file;
  GBytes            *content;
  IdeXmlAnalysis    *analysis;
  GPtrArray         *schemas;
  GPtrArray         *diagnostics_array;
  GPtrArray         *requires_array;
  IdeXmlSax         *sax_parser;
  IdeXmlStack       *stack;
  IdeXmlSymbolNode  *root_node;

  IdeTask           *analysis_task;
  IdeXmlSymbolNode  *parent_node;
  IdeXmlSymbolNode  *current_node;
  const gchar      **attributes;
  BuildState         build_state;
  gint               current_depth;
  gint64             sequence;
  guint              modversion_count;

  ErrorState         error_state;
  gchar             *error_data_str;

  guint              file_is_ui : 1;
} ParserState;

typedef struct
{
  gchar   *runtime_id;
  gchar   *package;
  gchar   *ns_name;
  guint16  ns_major_version;
  guint16  ns_minor_version;
  guint16  mod_major_version;
  guint16  mod_minor_version;
  gint     start_line;
  gint     start_line_offset;
  gint     end_line;
  gint     end_line_offset;
} RequireEntry;

void             _ide_xml_parser_set_post_processing_callback     (IdeXmlParser           *self,
                                                                   PostProcessingCallback  callback);
IdeDiagnostic   *_ide_xml_parser_create_diagnostic                (ParserState            *state,
                                                                   const gchar            *msg,
                                                                   IdeDiagnosticSeverity   severity);
gchar           *_ide_xml_parser_get_color_tag                    (IdeXmlParser           *self,
                                                                   const gchar            *str,
                                                                   ColorTagId              id,
                                                                   gboolean                space_before,
                                                                   gboolean                space_after,
                                                                   gboolean                space_inside);
void             _ide_xml_parser_state_processing                 (IdeXmlParser           *self,
                                                                   ParserState            *state,
                                                                   const gchar            *element_name,
                                                                   IdeXmlSymbolNode       *node,
                                                                   IdeXmlSaxCallbackType   callback_type,
                                                                   gboolean                is_internal);
void             _ide_xml_parser_end_element_sax_cb               (ParserState            *state,
                                                                   const xmlChar          *name);
void             _ide_xml_parser_warning_sax_cb                   (ParserState            *state,
                                                                   const xmlChar          *name,
                                                                   ...);
void             _ide_xml_parser_error_sax_cb                     (ParserState            *state,
                                                                   const xmlChar          *name,
                                                                   ...);
void             _ide_xml_parser_fatal_error_sax_cb               (ParserState            *state,
                                                                   const xmlChar          *name,
                                                                   ...);
void             _ide_xml_parser_internal_subset_sax_cb           (ParserState            *state,
                                                                   const xmlChar          *name,
                                                                   const xmlChar          *external_id,
                                                                   const xmlChar          *system_id);
void             _ide_xml_parser_external_subset_sax_cb           (ParserState            *state,
                                                                   const xmlChar          *name,
                                                                   const xmlChar          *external_id,
                                                                   const xmlChar          *system_id);
void             _ide_xml_parser_processing_instruction_sax_cb    (ParserState            *state,
                                                                   const xmlChar          *target,
                                                                   const xmlChar          *data);
void             _ide_xml_parser_characters_sax_cb                (ParserState            *state,
                                                                   const xmlChar          *name,
                                                                   gint                    len);

G_END_DECLS
