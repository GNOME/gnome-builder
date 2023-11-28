/* ide-xml-rng-parser.c
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* Based on the relaxng.c libxml2 code.
 * No error checks because already done in the validation step.
 * Whole refactoring to match the GNOME Builder needs.
 */

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/uri.h>

#include "ide-xml-rng-define.h"
#include "ide-xml-rng-grammar.h"
#include "ide-xml-rng-parser.h"

static const guchar *HREF_PROP = (const guchar *)"href";
static const guchar *NS_PROP = (const guchar *)"ns";
static const guchar *xmlRelaxNGNs = (const guchar *)"http://relaxng.org/ns/structure/1.0";

static gboolean          ide_xml_rng_parser_cleanup (IdeXmlRngParser    *self,
                                                     xmlNode            *root);
static void              parse_define               (IdeXmlRngParser    *self,
                                                     xmlNode            *nodes);
static void              parse_include              (IdeXmlRngParser    *self,
                                                     xmlNode            *nodes);
static IdeXmlRngDefine  *parse_name_class           (IdeXmlRngParser    *self,
                                                     xmlNode            *node,
                                                     IdeXmlRngDefine    *current);
static IdeXmlRngDefine  *parse_element              (IdeXmlRngParser    *self,
                                                     xmlNode            *node);
static IdeXmlRngDefine  *parse_patterns             (IdeXmlRngParser    *self,
                                                     xmlNode            *nodes,
                                                     gboolean            group_elements);
static IdeXmlRngGrammar *parse_grammar              (IdeXmlRngParser    *self,
                                                     xmlNode            *node);
static IdeXmlRngDefine  *parse_pattern              (IdeXmlRngParser    *self,
                                                     xmlNode            *node);
static IdeXmlSchema     *parse_document             (IdeXmlRngParser    *self,
                                                     xmlNode            *root);

typedef enum
{
  XML_RNG_FLAGS_IN_ATTRIBUTE     = 1 << 0,
  XML_RNG_FLAGS_IN_ONEORMORE     = 1 << 1,
  XML_RNG_FLAGS_IN_LIST          = 1 << 2,
  XML_RNG_FLAGS_IN_DATAEXCEPT    = 1 << 3,
  XML_RNG_FLAGS_IN_START         = 1 << 4,
  XML_RNG_FLAGS_IN_OOMGROUP      = 1 << 5,
  XML_RNG_FLAGS_IN_OOMINTERLEAVE = 1 << 6,
  XML_RNG_FLAGS_IN_EXTERNALREF   = 1 << 7,
  XML_RNG_FLAGS_IN_ANYEXCEPT     = 1 << 8,
  XML_RNG_FLAGS_IN_NSEXCEPT      = 1 << 9
} XmlRngFlags;

typedef enum
{
  XML_RNG_COMBINE_UNDEF          = 1 << 0,
  XML_RNG_COMBINE_CHOICE         = 1 << 1,
  XML_RNG_COMBINE_INTERLEAVE     = 1 << 2
} XmlRngCombine;

typedef struct _XmlDocument
{
  gchar           *href;
  xmlDoc          *doc;
  IdeXmlRngDefine *content;
  IdeXmlSchema    *schema;

  guint            is_external_ref : 1;
} XmlDocument;

struct _IdeXmlRngParser
{
  GObject            parent_instance;

  GArray            *xml_externalref_docs;
  GQueue             xml_externalref_docs_stack;

  GArray            *xml_include_docs;
  GQueue             xml_include_docs_stack;

  IdeXmlRngGrammar  *grammars;
  IdeXmlRngGrammar  *parent_grammar;

  IdeXmlRngDefine   *current_def;

  IdeXmlHashTable   *interleaves;
  gint               interleaves_count;

  XmlRngFlags        flags;
};

G_DEFINE_FINAL_TYPE (IdeXmlRngParser, ide_xml_rng_parser, G_TYPE_OBJECT)

static inline void
_autofree_cleanup_xmlFree (void *p)
{
  void **pp = (void**)p;
  xmlFree (*pp);
}

#define g_autoxmlfree __attribute__((cleanup(_autofree_cleanup_xmlFree)))

G_GNUC_UNUSED static void
xml_document_free (XmlDocument *doc)
{
  g_assert (doc != NULL);

  g_free (doc->href);

  if (doc->doc != NULL)
    xmlFreeDoc (doc->doc);

  if (doc->content != NULL)
    ide_xml_rng_define_unref (doc->content);

  if (doc->schema != NULL)
    ide_xml_schema_unref (doc->schema);
}

static void
_xmlfree (xmlChar **xmlstr)
{
  g_assert (xmlstr != NULL);

  if (*xmlstr != NULL)
    {
      xmlFree (*xmlstr);
      *xmlstr = NULL;
    }
}

static inline guchar *
_strip (guchar *name)
{
  if (name != NULL)
    g_strstrip ((gchar *)name);

  return name;
}

static inline gboolean
is_valid_rng_node (xmlNode     *node,
                   const gchar *name)
{
  return (node != NULL &&
          node->ns != NULL &&
          node->type == XML_ELEMENT_NODE &&
          ide_str_equal0 (node->name, name) &&
          ide_str_equal0 (node->ns->href, xmlRelaxNGNs));
}

static inline gboolean
is_blank_rng_node (xmlNode *node)
{
  const gchar *str;
  gunichar ch;

  g_assert (node != NULL);

  if (node->content == NULL)
    return TRUE;

  str = (const gchar *)node->content;
  ch = g_utf8_get_char (str);
  while (ch != 0)
    {
      if (!g_unichar_isspace(ch))
        return FALSE;

      str = g_utf8_next_char (str);
      ch = g_utf8_get_char (str);
    }

  return TRUE;
}

static inline void
remove_node (xmlNode *node)
{
  g_assert (node != NULL);

  xmlUnlinkNode(node);
  xmlFreeNode(node);
}

static guchar *
get_node_closest_ns (xmlNode *node)
{
  guchar *ns;

  g_assert (node != NULL);

  while (NULL == (ns = xmlGetProp(node, NS_PROP)))
    {
      node = node->parent;
      if (node == NULL || node->type != XML_ELEMENT_NODE)
        break;
    }

  return ns;
}

static gint
compare_href_func (XmlDocument *xml_doc,
                   gchar       *href)
{
  return !ide_str_equal0 (href, xml_doc->href);
}

static XmlDocument *
xml_include_docs_stack_lookup (IdeXmlRngParser *self,
                               const gchar     *url)
{
  GList *l;

  g_assert (IDE_IS_XML_RNG_PARSER (self));

  l = g_queue_find_custom (&self->xml_include_docs_stack, url, (GCompareFunc)compare_href_func);
  return (l != NULL) ? l->data : NULL;
}

static XmlDocument *
xml_externalref_docs_stack_lookup (IdeXmlRngParser *self,
                                   const gchar     *url)
{
  GList *l;

  g_assert (IDE_IS_XML_RNG_PARSER (self));

  l = g_queue_find_custom (&self->xml_externalref_docs_stack, url, (GCompareFunc)compare_href_func);
  return (l != NULL) ? l->data : NULL;
}

static guchar *
build_url (xmlDoc  *doc,
           xmlNode *node)
{
  g_autoxmlfree guchar *href = NULL;
  g_autoxmlfree guchar *base = NULL;

  if (NULL == (href = xmlGetProp(node, HREF_PROP)) ||
      NULL == (base = xmlNodeGetBase(doc, node)))
    return NULL;

  return xmlBuildURI(href, base);
}

static void
ide_xml_remove_redefine (IdeXmlRngParser *self,
                         xmlNode         *children,
                         const gchar     *name)
{
  xmlNode *node, *node_next, *inc_root;
  XmlDocument *inc_doc;

  g_assert (IDE_IS_XML_RNG_PARSER (self));

  node = children;
  while (node != NULL)
    {
      node_next = node->next;
      if (name == NULL && is_valid_rng_node (node, "start"))
        remove_node (node);
      else if (name != NULL && is_valid_rng_node (node, "define"))
        {
          g_autoxmlfree guchar *name_tmp = _strip (xmlGetProp (node, (const guchar *)"name"));

          if (name_tmp != NULL)
            {
              if (ide_str_equal0 (name, name_tmp))
                remove_node (node);
            }
        }
      else if (name != NULL && is_valid_rng_node (node, "include"))
        {
          inc_doc = node->psvi;
          if (inc_doc != NULL &&
              inc_doc->doc != NULL &&
              inc_doc->doc->children != NULL &&
              ide_str_equal0 (inc_doc->doc->children->name, "grammar"))
            {
              inc_root = xmlDocGetRootElement (inc_doc->doc);
              ide_xml_remove_redefine (self, inc_root->children, name);
            }
        }

      node = node_next;
    }
}

static XmlDocument *
load_include (IdeXmlRngParser *self,
              xmlNode         *node,
              const gchar     *url)
{
  g_autoxmlfree guchar *ns = NULL;
  xmlDoc *href_doc;
  XmlDocument xml_doc, *xml_doc_ref;
  xmlNode *root, *current;

  g_assert (IDE_IS_XML_RNG_PARSER (self));
  g_assert (node != NULL);
  g_assert (!ide_str_empty0 (url));

  if (NULL != (xml_include_docs_stack_lookup (self, url)) ||
      NULL == (ns = get_node_closest_ns (node)) ||
      NULL == (href_doc = xmlReadFile (url, NULL, 0)))
    return NULL;

  xml_doc.is_external_ref = TRUE;
  xml_doc.href = g_strdup (url);
  xml_doc.doc = href_doc;

  g_array_append_val (self->xml_include_docs, xml_doc);
  xml_doc_ref = &g_array_index (self->xml_include_docs, XmlDocument, self->xml_include_docs->len - 1);

  g_queue_push_head (&self->xml_include_docs_stack, xml_doc_ref);
  if (NULL != (root = xmlDocGetRootElement(href_doc)))
    {
      ide_xml_rng_parser_cleanup (self, root);
      if (xmlHasProp(root, NS_PROP))
        xmlSetProp(root, NS_PROP, ns);
    }

  g_queue_pop_head (&self->xml_include_docs_stack);

  if (root != NULL && !is_valid_rng_node (root, "grammar"))
    return NULL;

  current = node->children;
  while (current != NULL)
    {
      if (is_valid_rng_node (root, "start"))
        ide_xml_remove_redefine (self, root->children, NULL);
      else if (is_valid_rng_node (root, "define"))
        {
          g_autoxmlfree guchar *name = _strip (xmlGetProp (current, (const guchar *)"name"));

          if (name != NULL)
            ide_xml_remove_redefine (self, root->children, (const gchar *)name);
        }

      current = current->next;
    }

  return xml_doc_ref;
}

/* TODO: Get an error code to differenciate between recurse and can't load ? */
static XmlDocument *
load_externalref (IdeXmlRngParser *self,
                  xmlNode         *node,
                  const gchar     *url)
{
  g_autoxmlfree guchar *ns = NULL;
  xmlDoc *href_doc;
  XmlDocument xml_doc, *xml_doc_ref;
  xmlNode *root;

  g_assert (IDE_IS_XML_RNG_PARSER (self));
  g_assert (node != NULL);
  g_assert (!ide_str_empty0 (url));

  if (NULL != (xml_externalref_docs_stack_lookup (self, url)) ||
      NULL == (ns = get_node_closest_ns (node)) ||
      NULL == (href_doc = xmlReadFile (url, NULL, 0)))
    return NULL;

  xml_doc.is_external_ref = TRUE;
  xml_doc.href = g_strdup (url);
  xml_doc.doc = href_doc;

  g_array_append_val (self->xml_externalref_docs, xml_doc);
  xml_doc_ref = &g_array_index (self->xml_externalref_docs, XmlDocument, self->xml_externalref_docs->len - 1);

  g_queue_push_head (&self->xml_externalref_docs_stack, xml_doc_ref);
  if (NULL != (root = xmlDocGetRootElement(href_doc)))
    {
      ide_xml_rng_parser_cleanup (self, root);
      if (xmlHasProp(root, NS_PROP))
        xmlSetProp(root, NS_PROP, ns);
    }

  g_queue_pop_head (&self->xml_externalref_docs_stack);

  return xml_doc_ref;
}

static gboolean
ide_xml_rng_parser_cleanup (IdeXmlRngParser *self,
                            xmlNode         *root)
{
  xmlNode *current, *delete = NULL;
  XmlDocument *ext_doc;
  g_autoxmlfree guchar *href_url = NULL;
  gboolean ret = FALSE;

  g_assert (IDE_IS_XML_RNG_PARSER (self));

  current = root;
  while (current != NULL)
    {
      if (delete != NULL)
        {
          remove_node (delete);
          delete = NULL;
        }

      if (current->type == XML_ELEMENT_NODE)
        {
          if (current->ns == NULL || !ide_str_equal0 (current->ns->href, xmlRelaxNGNs))
            {
              delete = current;
              goto next_node;
            }
          else
            {
              if (ide_str_equal0 (current->name, "externalRef"))
                {
                  if (NULL == (href_url = build_url (root->doc, current)) ||
                      NULL == (ext_doc = load_externalref (self, current, (const gchar *)href_url)))
                    {
                      delete = current;
                      goto next_node;
                    }

                  current->psvi = ext_doc;
                }
              else if (ide_str_equal0 (current->name, "include"))
                {
                  if (NULL == (href_url = build_url (root->doc, current)) ||
                      NULL == (ext_doc = load_include (self, current, (const gchar *)href_url)))
                    {
                      delete = current;
                      goto next_node;
                    }

                  current->psvi = ext_doc;
                }
              else if (ide_str_equal0 (current->name, "element") ||
                       ide_str_equal0 (current->name, "attribute"))
                {
                  xmlNode *text_node;
                  xmlNode *doc_node;
                  g_autoxmlfree guchar *ns = NULL;
                  g_autoxmlfree guchar *name = xmlGetProp (current, (const guchar *)"name");

                  if (name != NULL)
                    {
                      if (current->children == NULL)
                        text_node = xmlNewChild (current, current->ns, (const guchar *)"name", name);
                      else
                        {
                          doc_node = xmlNewDocNode (current->doc, current->ns, (const guchar *)"name", NULL);
                          xmlAddPrevSibling (current->children, doc_node);
                          text_node = xmlNewText (name);
                          xmlAddChild (doc_node, text_node);
                          text_node = doc_node;
                        }

                      xmlUnsetProp (current, (const guchar *)"name");
                      if (NULL != (ns = xmlGetProp (current, NS_PROP)))
                        xmlSetProp (text_node, NS_PROP, ns);
                      else if (ide_str_equal0 (current->name, "attribute"))
                        xmlSetProp (text_node, NS_PROP, (const guchar *)"");
                    }
                }
              else if (ide_str_equal0 (current->name, "name") ||
                       ide_str_equal0 (current->name, "nsname") ||
                       ide_str_equal0 (current->name, "value"))
                {
                  if (xmlHasProp (current, NS_PROP) == NULL)
                    {
                      xmlNode *node = NULL;
                      g_autoxmlfree guchar *ns = NULL;

                      node = current->parent;
                      while (node != NULL && node->type == XML_ELEMENT_NODE)
                        {
                          if (NULL == (ns = xmlGetProp (node, NS_PROP)))
                            break;

                          node = node->parent;
                        }

                      if (ns == NULL)
                        xmlSetProp (current, NS_PROP, (const guchar *)"");
                      else
                        xmlSetProp (current, NS_PROP, ns);
                    }

                  if (ide_str_equal0 (current->name, "name"))
                    {
                      g_autoxmlfree guchar *name = NULL;
                      g_autoxmlfree guchar *local = NULL;
                      g_autoxmlfree guchar *prefix = NULL;

                      if (NULL != (name = xmlNodeGetContent (current)) &&
                          NULL != (local = xmlSplitQName2 (name, &prefix)))
                        {
                          xmlNsPtr ns_node;

                          if (NULL != (ns_node = xmlSearchNs (current->doc, current, prefix)))
                            {
                              xmlSetProp (current, NS_PROP, ns_node->href);
                              xmlNodeSetContent (current, local);
                            }
                        }
                    }
                }
              else if (ide_str_equal0 (current->name, "except") && current != root)
                {
                    if (current->parent != NULL &&
                        (ide_str_equal0 (current->parent->name, "anyName") ||
                         ide_str_equal0 (current->parent->name, "nsName")))
                      {
                        ide_xml_rng_parser_cleanup (self, current);
                        goto next_node;
                      }
                }

              if (ide_str_equal0 (current->name, "div"))
                {
                  g_autoxmlfree guchar *ns = NULL;
                  xmlNode *child;
                  xmlNode *next_node;
                  xmlNode *ins;

                  ns = xmlGetProp (current, NS_PROP);
                  child = current->children;
                  ins = current;
                  while (child != NULL)
                    {
                      if (ns != NULL && !xmlHasProp (child, NS_PROP))
                        xmlSetProp (child, NS_PROP, ns);

                      next_node = child->next;
                      xmlUnlinkNode (child);
                      ins = xmlAddNextSibling (ins, child);
                      child = next_node;
                    }

                  if (current->nsDef != NULL && current->parent != NULL)
                    {
                      xmlNsPtr parDef = (xmlNsPtr)&current->parent->nsDef;

                      while (parDef->next != NULL)
                        parDef = parDef->next;

                      parDef->next = current->nsDef;
                      current->nsDef = NULL;
                    }

                  delete = current;
                  goto next_node;
                }
            }
        }
      else if (current->type == XML_TEXT_NODE || current->type == XML_CDATA_SECTION_NODE)
        {
          if (is_blank_rng_node (current))
            {
              if (current->parent != NULL && current->parent->type == XML_ELEMENT_NODE)
                {
                  if (!ide_str_equal0 (current->parent->name, "value") &&
                      !ide_str_equal0 (current->parent->name, "param"))
                    delete = current;
                }
              else
                {
                  delete = current;
                  goto next_node;
                }
            }
        }
      else
        {
          delete = current;
          goto next_node;
        }

      if (current->children != NULL &&
          current->children->type != XML_ENTITY_DECL &&
          current->children->type != XML_ENTITY_REF_NODE &&
          current->children->type != XML_ENTITY_NODE)
        {
          current = current->children;
          continue;
        }

next_node:
      if (current->next == NULL)
        {
          do
            {
              current = current->parent;
              if (current == NULL || current == root)
                goto next_step;

              if (current->next != NULL)
                {
                  current = current->next;
                  break;
                }
            } while (current != NULL);
        }
      else
        current = current->next;
    }

next_step:
  if (delete != NULL)
    remove_node (delete);

  ret = TRUE;
  return ret;
}

static IdeXmlRngDefine *
parse_except_name_class (IdeXmlRngParser *self,
                         xmlNode         *node,
                         gboolean         is_attribute)
{
  IdeXmlRngDefine *def, *current, *last = NULL;
  g_autoptr (IdeXmlRngDefine) tmp_def = NULL;
  IdeXmlRngDefineType type;
  xmlNode *child;

  g_assert (IDE_IS_XML_RNG_PARSER (self));
  g_assert (node != NULL);

  if (!is_valid_rng_node (node, "except") || node->children == NULL)
    return NULL;

  def = ide_xml_rng_define_new (node, self->current_def, NULL, IDE_XML_RNG_DEFINE_EXCEPT);
  child = node->children;
  while (child != NULL)
    {
      type = (is_attribute) ? IDE_XML_RNG_DEFINE_ATTRIBUTE : IDE_XML_RNG_DEFINE_ELEMENT;
      current = ide_xml_rng_define_new (child, def, NULL, type);
      if (NULL != (tmp_def = parse_name_class (self, child, current)))
        {
          if (last == NULL)
            def->content = current;
          else
            last->next = current;

          last = current;
        }

      child = child->next;
    }

  return def;
}

static IdeXmlRngDefine *
parse_name_class (IdeXmlRngParser *self,
                  xmlNode         *node,
                  IdeXmlRngDefine *current)
{
  IdeXmlRngDefine *def, *tmp_def = NULL, *parent;

  g_assert (IDE_IS_XML_RNG_PARSER (self));
  g_assert (node != NULL);

  def = current;
  if (is_valid_rng_node (node, "name") ||
      is_valid_rng_node (node, "anyName") ||
      is_valid_rng_node (node, "nsName"))
    {
      if (current->type != IDE_XML_RNG_DEFINE_ELEMENT && current->type != IDE_XML_RNG_DEFINE_ATTRIBUTE)
        {
          if (self->flags & XML_RNG_FLAGS_IN_ATTRIBUTE)
            def = ide_xml_rng_define_new (node, current, NULL, IDE_XML_RNG_DEFINE_ATTRIBUTE);
          else
            def = ide_xml_rng_define_new (node, current, NULL, IDE_XML_RNG_DEFINE_ELEMENT);
        }
    }

  parent = self->current_def;
  self->current_def = def;

  if (is_valid_rng_node (node, "name"))
    {
      _xmlfree (&def->name);
      _xmlfree (&def->ns);

      def->name = _strip (xmlNodeGetContent(node));
      def->ns = xmlGetProp (node, NS_PROP);
    }
  else if (is_valid_rng_node (node, "anyName"))
    {
      _xmlfree (&def->name);
      _xmlfree (&def->ns);

      if (node->children != NULL)
        {
          g_assert (def->name_class == NULL);
          def->name_class = parse_except_name_class (self,
                                                     node->children,
                                                     current->type == IDE_XML_RNG_DEFINE_ATTRIBUTE);
        }
    }
  else if (is_valid_rng_node (node, "nsName"))
    {
      _xmlfree (&def->name);

      def->ns = xmlGetProp(node, NS_PROP);
      if (node->children != NULL)
        {
          g_assert (def->name_class == NULL);
          def->name_class = parse_except_name_class (self,
                                                     node->children,
                                                     current->type == IDE_XML_RNG_DEFINE_ATTRIBUTE);
        }
    }
  else if (is_valid_rng_node (node, "choice"))
    {
      xmlNode *child;
      IdeXmlRngDefine *last = NULL, *choice_parent;

      def = ide_xml_rng_define_new (node, current, NULL, IDE_XML_RNG_DEFINE_CHOICE);
      if (NULL != (child = node->children))
        {
          choice_parent = self->current_def;
          self->current_def = def;

          while (child != NULL)
            {
              if (NULL != (tmp_def = parse_name_class (self, child, def)))
                {
                  if (last == NULL)
                    last = def->name_class = tmp_def;
                  else
                    {
                      last->next = tmp_def;
                      last = tmp_def;
                    }
                }

              child = child->next;
            }

          self->current_def = choice_parent;
        }
    }
  else
    {
      def = NULL;
      goto end;
    }

  if (def != current)
    {
      if (current->name_class == NULL)
        current->name_class = def;
      else
        {
          tmp_def = current->name_class;
          while (tmp_def->next != NULL)
            tmp_def = tmp_def->next;

          tmp_def->next = def;
        }

      goto end;
    }
  else
    ide_xml_rng_define_ref (def);

end:
  self->current_def = parent;
  return def;
}

static IdeXmlRngDefine *
parse_data (IdeXmlRngParser *self,
            xmlNode         *node)
{
  IdeXmlRngDefine *def = NULL;

  g_assert (IDE_IS_XML_RNG_PARSER (self));
  g_assert (node != NULL);

  /* TODO: not done for now */

  return def;
}

static IdeXmlRngDefine *
parse_value (IdeXmlRngParser *self,
             xmlNode         *node)
{
  IdeXmlRngDefine *def = NULL;
  guchar *name;

  g_assert (IDE_IS_XML_RNG_PARSER (self));
  g_assert (node != NULL);

  /* TODO: datatype library part */
  name = _strip (xmlNodeGetContent(node));
  def = ide_xml_rng_define_new (node, self->current_def, name, IDE_XML_RNG_DEFINE_VALUE);

  return def;
}

/* TODO: we don't compute the interleaves for now */
static IdeXmlRngDefine *
parse_interleave (IdeXmlRngParser *self,
                  xmlNode         *node)
{
  IdeXmlRngDefine *def, *current, *last = NULL, *parent;
  xmlNode *child;

  g_assert (IDE_IS_XML_RNG_PARSER (self));
  g_assert (node != NULL);

  if (NULL == (child = node->children))
    return NULL;

  parent = self->current_def;
  def = ide_xml_rng_define_new (node, parent, NULL, IDE_XML_RNG_DEFINE_INTERLEAVE);
  self->current_def = def;

  while (child != NULL)
    {
      if (is_valid_rng_node (node, "element"))
        current = parse_element (self, child);
      else
        current = parse_pattern (self, child);

      if (current != NULL)
        {
          if (last == NULL)
            def->content = current;
          else
            last->next = current;

          last = current;
        }

      child = child->next;
    }

  self->current_def = parent;
  return def;
}

static void
import_ref_func (const gchar *name,
                 gpointer     value,
                 gpointer     data)
{
  IdeXmlRngParser *self = (IdeXmlRngParser *)data;
  IdeXmlRngDefine *def = (IdeXmlRngDefine *)value;

  g_assert (IDE_IS_XML_RNG_PARSER (self));
  g_assert (def != NULL);

  def->is_external_ref = TRUE;
  ide_xml_hash_table_add (self->grammars->refs, name, def);
}

/* TODO: check that we don't change the same table we read ? */
static void
parse_import_refs (IdeXmlRngParser  *self,
                   IdeXmlRngGrammar *grammar)
{
  g_assert (grammar != NULL);
  g_assert (self->grammars != NULL);

  if (grammar->refs == NULL)
    return;

  ide_xml_hash_table_full_scan (grammar->refs, import_ref_func, self);
}

/* TODO: should we free the XmlDocument ? */
static IdeXmlRngDefine *
parse_externalref (IdeXmlRngParser *self,
                   xmlNode         *node)
{
  g_autoptr (IdeXmlRngDefine) def = NULL;
  XmlDocument *doc = NULL;
  xmlNode *root, *tmp;
  g_autoxmlfree xmlChar *ns = NULL;
  XmlRngFlags old_flags;
  gboolean has_new_ns = FALSE;

  g_assert (IDE_IS_XML_RNG_PARSER (self));
  g_assert (node != NULL);

  if (NULL != (doc = node->psvi))
    {
      def = ide_xml_rng_define_new (node, self->current_def, NULL, IDE_XML_RNG_DEFINE_EXTERNALREF);
      if (doc->content == NULL)
        {
          if (NULL == (root = xmlDocGetRootElement(doc->doc)))
            return NULL;

          if (NULL == (ns = xmlGetProp(root, NS_PROP)))
            {
              tmp = node;
              while (tmp != NULL && tmp->type == XML_ELEMENT_NODE)
                {
                  if (NULL != (ns = xmlGetProp(tmp, NS_PROP)))
                    break;

                  tmp = tmp->parent;
                }

              if (ns != NULL)
                {
                  xmlSetProp(root, NS_PROP, ns);
                  has_new_ns = TRUE;
                }
            }

          old_flags = self->flags;
          self->flags |= XML_RNG_FLAGS_IN_EXTERNALREF;
          doc->schema = parse_document (self, root);
          self->flags = old_flags;

          if (doc->schema != NULL && doc->schema->top_grammar != NULL)
            {
              doc->content = doc->schema->top_grammar->start_defines;
              parse_import_refs (self, doc->schema->top_grammar);
            }

          if (has_new_ns)
            xmlUnsetProp(root, NS_PROP);
        }

      def->content = doc->content;
      ide_xml_rng_define_propagate_parent (def->content, def);
    }

  return g_steal_pointer (&def);
}


static IdeXmlRngDefine *
parse_attribute (IdeXmlRngParser *self,
                 xmlNode         *node)
{
  IdeXmlRngDefine *def = NULL, *current, *parent;
  xmlNode *child;
  XmlRngFlags old_flags;

  g_assert (IDE_IS_XML_RNG_PARSER (self));
  g_assert (node != NULL);

  def = ide_xml_rng_define_new (node, self->current_def, NULL, IDE_XML_RNG_DEFINE_ATTRIBUTE);
  if (NULL == (child = node->children))
    return def;

  old_flags = self->flags;
  self->flags |= XML_RNG_FLAGS_IN_ATTRIBUTE;
  if (NULL != (current = parse_name_class (self, child, def)))
    {
      ide_xml_rng_define_unref (current);
      child = child->next;
    }

  if (child != NULL)
    {
      parent = self->current_def;
      self->current_def = def;
      if (NULL != (current = parse_pattern (self, child)))
        switch (current->type)
          {
          case IDE_XML_RNG_DEFINE_EMPTY:
          case IDE_XML_RNG_DEFINE_NOTALLOWED:
          case IDE_XML_RNG_DEFINE_TEXT:
          case IDE_XML_RNG_DEFINE_ELEMENT:
          case IDE_XML_RNG_DEFINE_DATATYPE:
          case IDE_XML_RNG_DEFINE_VALUE:
          case IDE_XML_RNG_DEFINE_LIST:
          case IDE_XML_RNG_DEFINE_REF:
          case IDE_XML_RNG_DEFINE_PARENTREF:
          case IDE_XML_RNG_DEFINE_EXTERNALREF:
          case IDE_XML_RNG_DEFINE_DEFINE:
          case IDE_XML_RNG_DEFINE_ZEROORMORE:
          case IDE_XML_RNG_DEFINE_ONEORMORE:
          case IDE_XML_RNG_DEFINE_OPTIONAL:
          case IDE_XML_RNG_DEFINE_CHOICE:
          case IDE_XML_RNG_DEFINE_GROUP:
          case IDE_XML_RNG_DEFINE_INTERLEAVE:
          case IDE_XML_RNG_DEFINE_ATTRIBUTE:
            def->content = current;
            break;

          case IDE_XML_RNG_DEFINE_START:
          case IDE_XML_RNG_DEFINE_PARAM:
          case IDE_XML_RNG_DEFINE_EXCEPT:
          case IDE_XML_RNG_DEFINE_NOOP:
            ide_xml_rng_define_unref (current);
            break;

          case IDE_XML_RNG_DEFINE_ATTRIBUTES_GROUP:
          default:
            g_assert_not_reached ();
          }

      self->current_def = parent;
    }

  self->flags = old_flags;
  return def;
}

static IdeXmlRngDefine *
parse_pattern (IdeXmlRngParser *self,
               xmlNode         *node)
{
  IdeXmlRngDefine *def = NULL, *parent;
  IdeXmlRngGrammar *grammar, *old_grammar, *parent_grammar;
  g_autoxmlfree xmlChar *name = NULL;

  g_assert (IDE_IS_XML_RNG_PARSER (self));

  if (node == NULL)
    return NULL;

  parent = self->current_def;

  if (is_valid_rng_node (node, "element"))
    def = parse_element (self, node);
  else if (is_valid_rng_node (node, "attribute"))
    def = parse_attribute (self, node);
  else if (is_valid_rng_node (node, "empty"))
    def = ide_xml_rng_define_new (node, parent, NULL, IDE_XML_RNG_DEFINE_EMPTY);
  else if (is_valid_rng_node (node, "text"))
    def = ide_xml_rng_define_new (node, parent, NULL, IDE_XML_RNG_DEFINE_TEXT);
  else if (is_valid_rng_node (node, "zeroOrMore"))
    {
      if (node->children != NULL)
        {
          def = ide_xml_rng_define_new (node, parent, NULL, IDE_XML_RNG_DEFINE_ZEROORMORE);
          self->current_def = def;
          def->content = parse_patterns (self, node->children, TRUE);
        }
    }
  else if (is_valid_rng_node (node, "oneOrMore"))
    {
      if (node->children != NULL)
        {
          def = ide_xml_rng_define_new (node, parent, NULL, IDE_XML_RNG_DEFINE_ONEORMORE);
          self->current_def = def;
          def->content = parse_patterns (self, node->children, TRUE);
        }
    }
  else if (is_valid_rng_node (node, "optional"))
    {
      if (node->children != NULL)
        {
          def = ide_xml_rng_define_new (node, parent, NULL, IDE_XML_RNG_DEFINE_OPTIONAL);
          self->current_def = def;
          def->content = parse_patterns (self, node->children, TRUE);
        }
    }
  else if (is_valid_rng_node (node, "choice"))
    {
      if (node->children != NULL)
        {
          def = ide_xml_rng_define_new (node, parent, NULL, IDE_XML_RNG_DEFINE_CHOICE);
          self->current_def = def;
          def->content = parse_patterns (self, node->children, FALSE);
        }
    }
  else if (is_valid_rng_node (node, "group"))
    {
      if (node->children != NULL)
        {
          def = ide_xml_rng_define_new (node, parent, NULL, IDE_XML_RNG_DEFINE_GROUP);
          self->current_def = def;
          def->content = parse_patterns (self, node->children, FALSE);
        }
    }
  else if (is_valid_rng_node (node, "ref"))
    {
      name = _strip (xmlGetProp (node, (const guchar *)"name"));
      if (!ide_str_empty0 ((gchar *)name))
        {
          def = ide_xml_rng_define_new (node, parent, name, IDE_XML_RNG_DEFINE_REF);
          ide_xml_hash_table_add (self->grammars->refs, (gchar *)def->name, ide_xml_rng_define_ref (def));
        }
    }
  else if (is_valid_rng_node (node, "data"))
    def = parse_data (self, node);
  else if (is_valid_rng_node (node, "value"))
    def = parse_value (self, node);
  else if (is_valid_rng_node (node, "list"))
    {
      if (node->children != NULL)
        {
          def = ide_xml_rng_define_new (node, parent, NULL, IDE_XML_RNG_DEFINE_LIST);
          self->current_def = def;
          def->content = parse_patterns (self, node->children, FALSE);
        }
    }
  else if (is_valid_rng_node (node, "interleave"))
    def = parse_interleave (self, node);
  else if (is_valid_rng_node (node, "externalref"))
    def = parse_externalref (self, node);
  else if (is_valid_rng_node (node, "notAllowed"))
    def = ide_xml_rng_define_new (node, parent, NULL, IDE_XML_RNG_DEFINE_NOTALLOWED);
  else if (is_valid_rng_node (node, "grammar"))
    {
      if (self->grammars != NULL)
        parent_grammar = self->parent_grammar;

      self->parent_grammar = old_grammar = self->grammars;

      grammar = parse_grammar (self, node->children);
      def = grammar->start_defines;
      if (old_grammar != NULL)
        {
          self->grammars = old_grammar;
          self->parent_grammar = parent_grammar;
        }
    }
  else if (is_valid_rng_node (node, "parentref"))
    {
      if (self->parent_grammar == NULL)
        return NULL;

      name = _strip (xmlGetProp (node, (const guchar *)"name"));
      def = ide_xml_rng_define_new  (node, parent, name, IDE_XML_RNG_DEFINE_PARENTREF);
      if (def->name != NULL)
        ide_xml_hash_table_add (self->parent_grammar->refs, (gchar *)def->name, ide_xml_rng_define_ref (def));
    }
  else if (is_valid_rng_node (node, "mixed"))
    {
      if (node->children != NULL)
        {
          IdeXmlRngDefine *tmp_def;

          def = parse_interleave (self, node);
          if (def->content != NULL && def->content->next != NULL)
            {
              tmp_def = ide_xml_rng_define_new (node, def, NULL, IDE_XML_RNG_DEFINE_GROUP);
              tmp_def->content = def->content;
              ide_xml_rng_define_propagate_parent (def->content, tmp_def);
              def->content = tmp_def;
            }

          tmp_def = ide_xml_rng_define_new (node, def, NULL, IDE_XML_RNG_DEFINE_TEXT);
          tmp_def->next = def->content;
          def->content = tmp_def;
        }
    }

  self->current_def = parent;
  return def;
}

static IdeXmlRngDefine *
parse_element (IdeXmlRngParser *self,
               xmlNode         *node)
{
  g_autoptr (IdeXmlRngDefine) def = NULL;
  IdeXmlRngDefine *current, *last = NULL, *parent;
  xmlNode *child;

  g_assert (IDE_IS_XML_RNG_PARSER (self));
  g_assert (node != NULL);

  if (NULL == (child = node->children))
    return NULL;

  parent = self->current_def;
  def = ide_xml_rng_define_new (node, parent, NULL, IDE_XML_RNG_DEFINE_ELEMENT);
  self->current_def = def;

  if (NULL != (current = parse_name_class (self, child, def)))
    {
      ide_xml_rng_define_unref (current);
      child = child->next;
    }

  if (child == NULL)
    return NULL;

  while (child != NULL)
    {
      if (NULL != (current = parse_pattern (self, child)))
        {
          switch (current->type)
            {
            case IDE_XML_RNG_DEFINE_EMPTY:
            case IDE_XML_RNG_DEFINE_NOTALLOWED:
            case IDE_XML_RNG_DEFINE_TEXT:
            case IDE_XML_RNG_DEFINE_ELEMENT:
            case IDE_XML_RNG_DEFINE_DATATYPE:
            case IDE_XML_RNG_DEFINE_VALUE:
            case IDE_XML_RNG_DEFINE_LIST:
            case IDE_XML_RNG_DEFINE_REF:
            case IDE_XML_RNG_DEFINE_PARENTREF:
            case IDE_XML_RNG_DEFINE_EXTERNALREF:
            case IDE_XML_RNG_DEFINE_DEFINE:
            case IDE_XML_RNG_DEFINE_ZEROORMORE:
            case IDE_XML_RNG_DEFINE_ONEORMORE:
            case IDE_XML_RNG_DEFINE_OPTIONAL:
            case IDE_XML_RNG_DEFINE_CHOICE:
            case IDE_XML_RNG_DEFINE_GROUP:
            case IDE_XML_RNG_DEFINE_INTERLEAVE:
              if (last == NULL)
                def->content = last = current;
              else
                {
                  if (last->type == IDE_XML_RNG_DEFINE_ELEMENT && def->content == last)
                    {
                      def->content = ide_xml_rng_define_new (node, def, NULL, IDE_XML_RNG_DEFINE_GROUP);
                      def->content->content = last;
                      last->parent = def->content;

                      self->current_def = def->content;
                    }

                  last->next = current;
                  last = last->next;
                }

              break;

            case IDE_XML_RNG_DEFINE_ATTRIBUTE:
              current->next = def->attributes;
              def->attributes = current;
              break;

            case IDE_XML_RNG_DEFINE_START:
            case IDE_XML_RNG_DEFINE_PARAM:
            case IDE_XML_RNG_DEFINE_EXCEPT:
            case IDE_XML_RNG_DEFINE_NOOP:
              break;

            case IDE_XML_RNG_DEFINE_ATTRIBUTES_GROUP:
            default:
              g_assert_not_reached ();
            }
        }

      child = child->next;
    }

  self->current_def = parent;

  return g_steal_pointer (&def);
}

static IdeXmlRngDefine *
parse_patterns (IdeXmlRngParser *self,
                xmlNode         *nodes,
                gboolean         group_elements)
{
  IdeXmlRngDefine *def = NULL, *last = NULL, *current, *parent;

  g_assert (IDE_IS_XML_RNG_PARSER (self));

  parent = self->current_def;
  while (nodes != NULL)
    {
      if (is_valid_rng_node (nodes, "element"))
        {
          current = parse_element (self, nodes);
          if (def == NULL)
            def = last = current;
          else
            {
              if (group_elements && def->type == IDE_XML_RNG_DEFINE_ELEMENT && def == last)
                {
                  def = ide_xml_rng_define_new (nodes, parent, NULL, IDE_XML_RNG_DEFINE_GROUP);
                  def->content = last;
                  last->parent = def;

                  self->current_def = def;
                }

              last->next = current;
              last = current;
            }
        }
      else
        {
          if (NULL != (current = parse_pattern (self, nodes)))
            {
              if (def == NULL)
                def = last = current;
              else
                {
                  last->next = current;
                  last = current;
                }
            }
        }

      nodes = nodes->next;
    }

  self->current_def = parent;
  return def;
}

static void
parse_start (IdeXmlRngParser *self,
             xmlNode         *nodes)
{
  IdeXmlRngDefine *def, *parent;

  g_assert (IDE_IS_XML_RNG_PARSER (self));
  g_assert (nodes != NULL);

  parent = self->current_def;

  if (is_valid_rng_node (nodes, "empty"))
    def = ide_xml_rng_define_new (nodes, parent, NULL, IDE_XML_RNG_DEFINE_EMPTY);
  else if (is_valid_rng_node (nodes, "notAllowed"))
    def = ide_xml_rng_define_new (nodes, parent, NULL, IDE_XML_RNG_DEFINE_NOTALLOWED);
  else
    def = parse_patterns (self, nodes, TRUE);

  if (self->grammars->start_defines != NULL)
    ide_xml_rng_define_append (self->grammars->start_defines, def);
  else
    self->grammars->start_defines = def;
}

static void
parse_grammar_content (IdeXmlRngParser *self,
                       xmlNode         *nodes)
{
  IdeXmlRngDefine *parent;

  g_assert (IDE_IS_XML_RNG_PARSER (self));

  parent = self->current_def;
  self->current_def = NULL;

  while (nodes != NULL)
    {
      if (is_valid_rng_node (nodes, "start"))
        {
          g_assert (nodes->children != NULL);
          parse_start (self, nodes->children);
        }
      else if (is_valid_rng_node (nodes, "define"))
        parse_define (self, nodes);
      else if (is_valid_rng_node (nodes, "include"))
        parse_include (self, nodes);
      else
        g_assert_not_reached ();

      nodes = nodes->next;
    }

  self->current_def = parent;
}

static void
parse_define (IdeXmlRngParser *self,
              xmlNode         *nodes)
{
  IdeXmlRngDefine *def, *parent;
  g_autoxmlfree xmlChar *name = NULL;

  g_assert (IDE_IS_XML_RNG_PARSER (self));
  g_assert (nodes != NULL);

  if (NULL != (name = _strip (xmlGetProp (nodes, (const guchar *)"name"))))
    {
      parent = self->current_def;

      def = ide_xml_rng_define_new (nodes, NULL, name, IDE_XML_RNG_DEFINE_DEFINE);
      self->current_def = def;

      if (nodes->children != NULL)
        def->content = parse_patterns (self, nodes->children, FALSE);

      ide_xml_hash_table_add (self->grammars->defines, (gchar *)name, def);

      self->current_def = parent;
    }
}

static void
parse_include (IdeXmlRngParser *self,
               xmlNode         *node)
{
  XmlDocument *include;
  xmlNode *root;

  g_assert (IDE_IS_XML_RNG_PARSER (self));
  g_assert (node != NULL);

  if (NULL != (include = node->psvi) &&
      NULL != (root = xmlDocGetRootElement(include->doc)) &&
      ide_str_equal0 (root->name, "grammar") &&
      root->children != NULL)
    {
      parse_grammar_content (self, root->children);
      if (node->children != NULL)
        parse_grammar_content (self, node->children);
    }
}

static void
merge_starts (IdeXmlRngParser  *self,
              IdeXmlRngGrammar *grammar)
{
  IdeXmlRngDefine *starts, *current;
  xmlChar *combine;
  g_autofree gchar *name = NULL;
  XmlRngCombine combine_type = XML_RNG_COMBINE_UNDEF;

  g_assert (IDE_IS_XML_RNG_PARSER (self));
  g_assert (grammar != NULL);

  starts = current = grammar->start_defines;
  if (starts == NULL || starts->next == NULL)
    return;

  while (current != NULL)
    {
      combine = NULL;
      if (current->node != NULL &&
          current->node->parent != NULL &&
          ide_str_equal0 (current->node->parent->name, "start"))
        {
          if (NULL != (combine = xmlGetProp(current->node->parent, (const guchar *)"combine")))
            {
              if (ide_str_equal0 (combine, "choice"))
                {
                  if (combine_type == XML_RNG_COMBINE_UNDEF)
                    combine_type = XML_RNG_COMBINE_CHOICE;
                }
              else if (ide_str_equal0 (combine, "interleave"))
                {
                  if (combine_type == XML_RNG_COMBINE_UNDEF)
                    combine_type = XML_RNG_COMBINE_INTERLEAVE;
                }

              xmlFree (combine);
            }
        }

      current = current->next;
    }

  if (combine_type == XML_RNG_COMBINE_UNDEF)
    combine_type = XML_RNG_COMBINE_INTERLEAVE;

  if (combine_type == XML_RNG_COMBINE_CHOICE)
    current = ide_xml_rng_define_new (starts->node, NULL, NULL, IDE_XML_RNG_DEFINE_CHOICE);
  else
    {
      name = g_strdup_printf ("interleaved%d", ++self->interleaves_count);
      ide_xml_hash_table_add (self->interleaves, name, current);
      current = ide_xml_rng_define_new (starts->node, NULL, NULL, IDE_XML_RNG_DEFINE_INTERLEAVE);
    }

  current->content = grammar->start_defines;
  ide_xml_rng_define_propagate_parent (grammar->start_defines, current);

  grammar->start_defines = current;
}

static void
merge_defines_func (const gchar *name,
                    GPtrArray   *array,
                    gpointer     data)
{
  IdeXmlRngParser *self = (IdeXmlRngParser *)data;
  IdeXmlRngDefine *current, *def, *tmp_def, *tmp2_def, *last = NULL;
  xmlChar *combine;
  XmlRngCombine combine_type = XML_RNG_COMBINE_UNDEF;

  g_assert (IDE_IS_XML_RNG_PARSER (self));
  g_assert (array != NULL);

  if (array->len == 1)
    return;

  for (gint i = 0; i <array->len; ++i)
    {
      current = g_ptr_array_index (array, i);
      if (NULL != (combine = xmlGetProp(current->node, (const guchar *)"combine")))
        {
          if (ide_str_equal0 (combine, "choice"))
            {
              if (combine_type == XML_RNG_COMBINE_UNDEF)
                combine_type = XML_RNG_COMBINE_CHOICE;
            }
          else if (ide_str_equal0 (combine, "interleave"))
            {
              if (combine_type == XML_RNG_COMBINE_UNDEF)
                combine_type = XML_RNG_COMBINE_INTERLEAVE;
            }

          xmlFree (combine);
        }
    }

  if (combine_type == XML_RNG_COMBINE_UNDEF)
    combine_type = XML_RNG_COMBINE_INTERLEAVE;

  def = g_ptr_array_index (array, 0);
  if (combine_type == XML_RNG_COMBINE_CHOICE)
    current = ide_xml_rng_define_new (def->node, NULL, NULL, IDE_XML_RNG_DEFINE_CHOICE);
  else
    current = ide_xml_rng_define_new (def->node, NULL, NULL, IDE_XML_RNG_DEFINE_INTERLEAVE);

  for (gint i = 0; i <array->len; ++i)
    {
      tmp_def = g_ptr_array_index (array, i);
      if (tmp_def->content != NULL)
        {
          if (tmp_def->content->next != NULL)
            {
              tmp2_def = ide_xml_rng_define_new (def->node, NULL, NULL, IDE_XML_RNG_DEFINE_GROUP);
              tmp2_def->content = tmp_def->content;
              tmp_def->content->parent = tmp2_def;
            }
          else
            tmp2_def = tmp_def->content;

          if (last == NULL)
            {
              current->content = tmp2_def;
              tmp2_def->parent = current;
            }
          else
            last->next = tmp2_def;

          last = tmp2_def;
        }

      tmp_def->content = current;
      current->parent = tmp_def;
    }

  def->content = current;
  current->parent = def;

  if (combine_type == XML_RNG_COMBINE_INTERLEAVE)
    {
      name = g_strdup_printf ("interleaved%d", ++self->interleaves_count);
      ide_xml_hash_table_add (self->interleaves, name, current);
    }
}

static void
merge_refs_func (const gchar *name,
                 GPtrArray   *array,
                 gpointer     data)
{
  IdeXmlRngParser *self = (IdeXmlRngParser *)data;
  IdeXmlRngGrammar *grammar;
  IdeXmlRngDefine *ref, *def;
  GPtrArray *def_array;

  g_assert (IDE_IS_XML_RNG_PARSER (self));
  g_assert (array != NULL);

  grammar = self->grammars;
  ref = g_ptr_array_index (array, 0);

  if (grammar == NULL ||
      grammar->defines == NULL ||
      ref->content != NULL)
    return;

  /* TODO: grammar->defines need to be linked by hash  ? */
  if (NULL != (def_array = ide_xml_hash_table_lookup (grammar->defines, name)))
    {
      def = g_ptr_array_index (def_array, 0);
      for (gint i = 0; i <array->len; ++i)
        {
          ref = g_ptr_array_index (array, i);
          if (ref->is_external_ref)
            continue;

          ref->content = def;
        }
    }
}

static IdeXmlRngGrammar *
parse_grammar (IdeXmlRngParser *self,
               xmlNode         *nodes)
{
  IdeXmlRngGrammar *grammar, *old_grammar;

  g_assert (IDE_IS_XML_RNG_PARSER (self));

  if (nodes == NULL)
    return NULL;

  grammar = ide_xml_rng_grammar_new ();
  if (self->grammars != NULL)
    self->grammars->next = ide_xml_rng_grammar_ref (grammar);
  else
    self->grammars = ide_xml_rng_grammar_ref (grammar);

  old_grammar = self->grammars;
  self->grammars = grammar;

  parse_grammar_content (self, nodes);

  self->grammars = grammar;
  merge_starts (self, grammar);
  if (grammar->defines != NULL)
    ide_xml_hash_table_array_scan (grammar->defines, merge_defines_func, self);

  if (grammar->refs != NULL)
    ide_xml_hash_table_array_scan (grammar->refs, merge_refs_func, self);

  self->grammars = old_grammar;

  return grammar;
}

static IdeXmlRngDefine *
unlink_define (IdeXmlRngParser *self,
               IdeXmlRngDefine *current,
               IdeXmlRngDefine *parent,
               IdeXmlRngDefine *prev)
{
  g_assert (IDE_IS_XML_RNG_PARSER (self));
  g_assert (current != NULL);

  if (prev != NULL)
    prev->next = current->next;
  else
    {
      if (parent != NULL)
        {
          if (parent->content == current)
            parent->content = current->next;
          else if (parent->attributes == current)
            parent->attributes = current->next;
          else if (parent->name_class == current)
            parent->name_class = current->next;
        }
      else
        {
          current->type = IDE_XML_RNG_DEFINE_NOOP;
          prev = current;
        }

      /* TODO: free and remove the def from global structs ? */
    }

  return prev;
}

static gboolean
generate_attributes (IdeXmlRngParser *self,
                     IdeXmlRngDefine *def)
{
  IdeXmlRngDefine *current = def, *parent = NULL, *tmp_def;

  g_assert (IDE_IS_XML_RNG_PARSER (self));

  while (current != NULL)
    {
      if (current->type == IDE_XML_RNG_DEFINE_ELEMENT ||
          current->type == IDE_XML_RNG_DEFINE_TEXT ||
          current->type == IDE_XML_RNG_DEFINE_DATATYPE ||
          current->type == IDE_XML_RNG_DEFINE_PARAM ||
          current->type == IDE_XML_RNG_DEFINE_LIST ||
          current->type == IDE_XML_RNG_DEFINE_VALUE ||
          current->type == IDE_XML_RNG_DEFINE_EMPTY)
        return FALSE;

      if (current->type == IDE_XML_RNG_DEFINE_CHOICE ||
          current->type == IDE_XML_RNG_DEFINE_INTERLEAVE ||
          current->type == IDE_XML_RNG_DEFINE_GROUP ||
          current->type == IDE_XML_RNG_DEFINE_ONEORMORE ||
          current->type == IDE_XML_RNG_DEFINE_ZEROORMORE ||
          current->type == IDE_XML_RNG_DEFINE_OPTIONAL ||
          current->type == IDE_XML_RNG_DEFINE_PARENTREF ||
          current->type == IDE_XML_RNG_DEFINE_EXTERNALREF ||
          current->type == IDE_XML_RNG_DEFINE_REF ||
          current->type == IDE_XML_RNG_DEFINE_DEFINE)
        {
          if (current->content != NULL)
            {
              parent = current;
              current = current->content;
              tmp_def = current;
              while (tmp_def != NULL)
                {
                  tmp_def->parent = parent;
                  tmp_def = tmp_def->next;
                }

              continue;
            }
        }

      if (current == def)
        break;

      if (current->next != NULL)
        {
          current = current->next;
          continue;
        }

      do
        {
          if (NULL == (current = current->parent))
            break;

          if (current == def)
            return TRUE;

          if (current->next != NULL)
            {
              current = current->next;
              break;
            }
        } while (current != NULL);
    }

  return TRUE;
}

static void
simplify (IdeXmlRngParser *self,
          IdeXmlRngDefine *current,
          IdeXmlRngDefine *parent)
{
  IdeXmlRngDefine *prev = NULL;

  g_assert (IDE_IS_XML_RNG_PARSER (self));

  while (current != NULL)
    {
      if (current->type == IDE_XML_RNG_DEFINE_REF || current->type == IDE_XML_RNG_DEFINE_PARENTREF)
        {
          if (!current->is_ref_simplified)
            {
              current->is_ref_simplified = TRUE;
              simplify (self, current->content, current);
            }
        }
      else if (current->type == IDE_XML_RNG_DEFINE_NOTALLOWED)
        {
          current->parent = parent;
          if (parent != NULL &&
              (parent->type == IDE_XML_RNG_DEFINE_ATTRIBUTE ||
               parent->type == IDE_XML_RNG_DEFINE_LIST ||
               parent->type == IDE_XML_RNG_DEFINE_GROUP ||
               parent->type == IDE_XML_RNG_DEFINE_INTERLEAVE ||
               parent->type == IDE_XML_RNG_DEFINE_ONEORMORE ||
               parent->type == IDE_XML_RNG_DEFINE_ZEROORMORE))
            {
              parent->type = IDE_XML_RNG_DEFINE_NOTALLOWED;
              break;
            }

          if (parent != NULL && parent->type == IDE_XML_RNG_DEFINE_CHOICE)
            prev = unlink_define (self, current, parent, prev);
          else
            prev = current;
        }
      else if (current->type == IDE_XML_RNG_DEFINE_EMPTY)
        {
          current->parent = parent;
          if (parent != NULL &&
              (parent->type == IDE_XML_RNG_DEFINE_ONEORMORE || parent->type == IDE_XML_RNG_DEFINE_ZEROORMORE))
            {
              parent->type = IDE_XML_RNG_DEFINE_EMPTY;
              break;
            }
          if (parent != NULL &&
              (parent->type == IDE_XML_RNG_DEFINE_GROUP || parent->type == IDE_XML_RNG_DEFINE_INTERLEAVE))
            {
              prev = unlink_define (self, current, parent, prev);
            }
          else
            prev = current;
        }
      else
        {
          current->parent = parent;
          if (current->content != NULL)
            simplify (self, current->content, current);

          if (current->type != IDE_XML_RNG_DEFINE_VALUE && current->attributes != NULL)
            simplify (self, current->attributes, current);

          if (current->name_class != NULL)
            simplify (self, current->name_class, current);

          if (current->type == IDE_XML_RNG_DEFINE_ELEMENT)
            {
              IdeXmlRngDefine *tmp_def, *tmp_content;

              while (current->content != NULL)
                {
                  if (generate_attributes (self, current->content))
                    {
                      tmp_def = current->content;
                      current->content = tmp_def->next;
                      tmp_def->next = current->attributes;
                      current->attributes = tmp_def;
                    }
                  else
                    break;
                }

              tmp_content = current->content;
              while (tmp_content != NULL && tmp_content->next != NULL)
                {
                  tmp_def = tmp_content->next;
                  if (generate_attributes (self, tmp_def))
                    {
                      tmp_content->next = tmp_def->next;
                      tmp_def->next = current->attributes;
                      current->attributes = tmp_def;
                    }
                  else
                    tmp_content = tmp_def;
                }
            }

          if (current->type == IDE_XML_RNG_DEFINE_GROUP || current->type == IDE_XML_RNG_DEFINE_INTERLEAVE)
            {
              if (current->content == NULL)
                  current->type = IDE_XML_RNG_DEFINE_EMPTY;
              else if (current->content->next == NULL)
                {
                  if (parent == NULL && prev == NULL)
                    current->type = IDE_XML_RNG_DEFINE_NOOP;
                  else if (prev == NULL)
                    {
                      parent->content = current->content;
                      current->content->next = current->next;
                      current = current->content;
                    }
                  else
                    {
                      current->content->next = current->next;
                      prev->next = current->content;
                      current = current->content;
                    }
                }
            }

          if (current->type == IDE_XML_RNG_DEFINE_EXCEPT &&
              current->content != NULL &&
              current->content->type == IDE_XML_RNG_DEFINE_NOTALLOWED)
            {
              prev = unlink_define (self, current, parent, prev);
            }
          else if (current->type == IDE_XML_RNG_DEFINE_NOTALLOWED)
            {
              if (parent != NULL &&
                  (parent->type == IDE_XML_RNG_DEFINE_ATTRIBUTE ||
                   parent->type == IDE_XML_RNG_DEFINE_LIST ||
                   parent->type == IDE_XML_RNG_DEFINE_GROUP ||
                   parent->type == IDE_XML_RNG_DEFINE_INTERLEAVE ||
                   parent->type == IDE_XML_RNG_DEFINE_ONEORMORE ||
                   parent->type == IDE_XML_RNG_DEFINE_ZEROORMORE))
                {
                  parent->type = IDE_XML_RNG_DEFINE_NOTALLOWED;
                  break;
                }

              if (parent != NULL && parent->type == IDE_XML_RNG_DEFINE_CHOICE)
                prev = unlink_define (self, current, parent, prev);
              else
                prev = current;
            }
          else if (current->type == IDE_XML_RNG_DEFINE_EMPTY)
            {
              if (parent != NULL &&
                  (parent->type == IDE_XML_RNG_DEFINE_ONEORMORE ||
                   parent->type == IDE_XML_RNG_DEFINE_ZEROORMORE))
                {
                    parent->type = IDE_XML_RNG_DEFINE_EMPTY;
                    break;
                }

              if (parent != NULL &&
                  (parent->type == IDE_XML_RNG_DEFINE_GROUP ||
                   parent->type == IDE_XML_RNG_DEFINE_INTERLEAVE ||
                   parent->type == IDE_XML_RNG_DEFINE_CHOICE))
                {
                  prev = unlink_define (self, current, parent, prev);
                }
              else
                prev = current;
          }
          else
            prev = current;
        }

      current = current->next;
    }
}

static IdeXmlSchema *
parse_document (IdeXmlRngParser *self,
                xmlNode         *root)
{
  IdeXmlSchema *schema;
  IdeXmlRngGrammar *old_grammar;

  g_assert (IDE_IS_XML_RNG_PARSER (self));
  g_assert (root != NULL);

  schema = ide_xml_schema_new ();

  if (is_valid_rng_node (root, "grammar"))
    schema->top_grammar = parse_grammar (self, root->children);
  else
    {
      schema->top_grammar = ide_xml_rng_grammar_new ();
      if (NULL != (old_grammar = self->grammars))
        ide_xml_rng_grammar_add_child (self->grammars, schema->top_grammar);

      self->grammars = schema->top_grammar;
      parse_start (self, root);

      if (old_grammar != NULL)
        self->grammars = old_grammar;
    }

  if (schema->top_grammar->start_defines != NULL)
    {
      if (!(self->flags & XML_RNG_FLAGS_IN_EXTERNALREF))
        {
          /* simplify sets the nodes parent but in case we don't use it
           * parent is also set when parsing.
           */
          simplify (self, schema->top_grammar->start_defines, NULL);
          while (schema->top_grammar->start_defines != NULL &&
                 schema->top_grammar->start_defines->type == IDE_XML_RNG_DEFINE_NOOP &&
                 schema->top_grammar->start_defines->next != NULL)
            {
              schema->top_grammar->start_defines = schema->top_grammar->start_defines->content;
            }
        }
    }

  return schema;
}

/* No rules or cycles checks are done because the parser is meant
 * to be run on a valid rng syntax.
 */
IdeXmlSchema *
ide_xml_rng_parser_parse (IdeXmlRngParser *self,
                          const gchar     *schema_data,
                          gsize            schema_size,
                          GFile           *file)
{
  IdeXmlSchema *schema = NULL;
  g_autofree gchar *url = NULL;
  xmlDoc *doc;
  xmlNode *root;
  gint options;

  g_return_val_if_fail (IDE_IS_XML_RNG_PARSER (self), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  url = g_file_get_uri (file);
  options = XML_PARSE_RECOVER | XML_PARSE_NOERROR | XML_PARSE_NOWARNING;
  if (NULL != (doc = xmlReadMemory (schema_data, schema_size, url, NULL, options)))
  {
    if (NULL == (root = xmlDocGetRootElement (doc)) ||
        !ide_xml_rng_parser_cleanup (self, root))
      goto end;

    schema = parse_document (self, root);
    /* TODO: transfer other self fields to schema */

    /* TODO: code at L7546 */
  }

end:
  if (doc != NULL)
    xmlFreeDoc (doc);

  return schema;
}

IdeXmlRngParser *
ide_xml_rng_parser_new (void)
{
  return g_object_new (IDE_TYPE_XML_RNG_PARSER, NULL);
}

static void
ide_xml_rng_parser_finalize (GObject *object)
{
  IdeXmlRngParser *self = (IdeXmlRngParser *)object;

  g_array_free (self->xml_externalref_docs, TRUE);
  g_array_free (self->xml_include_docs, TRUE);

  G_OBJECT_CLASS (ide_xml_rng_parser_parent_class)->finalize (object);
}

static void
ide_xml_rng_parser_class_init (IdeXmlRngParserClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_xml_rng_parser_finalize;
}

static void
ide_xml_rng_parser_init (IdeXmlRngParser *self)
{
  g_queue_init (&self->xml_externalref_docs_stack);
  g_queue_init (&self->xml_include_docs_stack);

  self->xml_externalref_docs = g_array_new (FALSE, TRUE, sizeof (XmlDocument));
  self->xml_include_docs = g_array_new (FALSE, TRUE, sizeof (XmlDocument));
}
