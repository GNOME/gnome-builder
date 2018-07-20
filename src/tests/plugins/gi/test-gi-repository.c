/* test-gi-repository.c
 *
 * Copyright © 2018 Sébastienn Lafargue <slafargue@gnome.org>
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

#define G_LOG_DOMAIN "test-ide-gi-repository"

#include "test-gi-common.h"
#include "test-gi-utils.h"

#include <ide.h>

#include "application/ide-application-tests.h"

#include "../../../plugins/gnome-builder-plugins.h"

#include "../../../plugins/gi/ide-gi-types.h"
#include "../../../plugins/gi/ide-gi-repository.h"
#include "../../../plugins/gi/ide-gi-objects.h"

#include <libxml/xmlreader.h>
#include <string.h>

static IdeGiRepository *global_repository = NULL;

typedef enum {
  STATE_END,
  STATE_END_ELEMENT,
  STATE_PARENT,
  STATE_SIBLING,
  STATE_CHILD
} NodeState;

typedef struct
{
  GQueue          stack;
  IdeGiNamespace *ns;
  gchar          *node_name;
  gchar          *element_name;
  gboolean        return_current;
  gint            last_depth;
} GlobalState;

typedef struct
{
  gchar    *node_name;
  gchar    *element_name;
} StackItem;

static GlobalState global;

static void check_callback (IdeGiRepository  *self,
                            IdeGiCallback    *callback,
                            xmlTextReaderPtr  reader);
static void check_field    (IdeGiRepository  *self,
                            IdeGiField       *field,
                            xmlTextReaderPtr  reader);
static void check_function (IdeGiRepository  *self,
                            IdeGiFunction    *function,
                            xmlTextReaderPtr  reader);
static void check_record   (IdeGiRepository  *self,
                            IdeGiRecord      *record,
                            xmlTextReaderPtr  reader);

static StackItem *
stack_item_new (const gchar *node_name,
                const gchar *element_name)
{
  StackItem *item = g_slice_new0 (StackItem);

  item->node_name = g_strdup (node_name);
  item->element_name = g_strdup (element_name);

  return item;
}

static void
stack_item_free (StackItem *item)
{
  g_assert (item != NULL);

  g_free (item->node_name);
  g_free (item->element_name);

  g_slice_free (StackItem, item);
}

static void
init_global (GlobalState *state)
{
  g_queue_init (&global.stack);

  dzl_clear_pointer (&global.ns, ide_gi_namespace_unref);
  g_free (state->node_name);
  g_free (state->element_name);

  state->return_current = FALSE;
  state->last_depth  =  0;
}

static void
free_global (GlobalState *state)
{
  g_queue_free_full (&state->stack, (GDestroyNotify)stack_item_free);

  dzl_clear_pointer (&global.ns, ide_gi_namespace_unref);
  dzl_clear_pointer (&state->node_name, g_free);
  dzl_clear_pointer (&state->element_name, g_free);

  state->return_current = FALSE;
  state->last_depth  =  0;
}

static void
push_node (void)
{
  StackItem *item = stack_item_new (global.node_name, global.element_name);

  g_queue_push_head (&global.stack, item);
}

static StackItem *
pop_node (void)
{
  StackItem *item = g_queue_pop_head (&global.stack);

  return item;
}

static gboolean
is_stack_head_match (const gchar *node_name)
{
  StackItem *item = g_queue_peek_head (&global.stack);

  return dzl_str_equal0 (item->node_name, node_name);
}

static guchar *
get_attr (xmlTextReaderPtr  reader,
          const gchar      *name)
{
  return (guchar *)xmlTextReaderGetAttribute (reader, (guchar *)name);
}

static NodeState
get_next_node (xmlTextReaderPtr   reader,
               gchar            **node_name,
               gchar            **element_name)
{
  xmlReaderTypes type;


  while (TRUE)
    {
      g_autoxmlfree guchar *new_node_name = NULL;
      g_autoxmlfree guchar *new_element_name = NULL;
      gint new_depth;

      if (xmlTextReaderRead (reader) != 1)
        return STATE_END;

      new_node_name = xmlTextReaderName (reader);
      type = xmlTextReaderNodeType (reader);
      if (type == XML_READER_TYPE_END_ELEMENT)
        {
          g_print ("type:%d name:%s\n", type, new_node_name);
          if (is_stack_head_match ((gchar *)new_node_name))
            {
              StackItem *item = pop_node ();

              stack_item_free (item);
              return STATE_END_ELEMENT;
            }

          continue;
        }

      if (type != XML_READER_TYPE_ELEMENT)
        continue;

      new_element_name = get_attr (reader, "name");
      if (node_name)
        *node_name = g_strdup ((gchar *)new_node_name);

      if (element_name)
        *element_name = g_strdup ((gchar  *)new_element_name);

      g_print ("type:%d name:%s value:%s\n", type, new_node_name, new_element_name);

      g_free (global.node_name);
      g_free (global.element_name);
      global.node_name = g_strdup ((gchar *)new_node_name);
      global.element_name = g_strdup ((gchar *)new_element_name);

      new_depth = xmlTextReaderDepth (reader);
      if (new_depth < global.last_depth)
        {
          global.last_depth = new_depth;
          global.return_current = TRUE;
          return STATE_PARENT;
        }
      else if (new_depth == global.last_depth)
        {
          global.last_depth = new_depth;
          return STATE_SIBLING;
        }
      else
        {
          global.last_depth = new_depth;
          return STATE_CHILD;
        }
    }

  g_assert_not_reached ();
}

static gboolean
is_node_empty (xmlTextReaderPtr reader)
{
  gint ret = xmlTextReaderIsEmptyElement (reader);

  g_assert (ret != -1);

  return (ret == 1);
}

static void
assert_next_node (xmlTextReaderPtr  reader,
                  const gchar      *name)
{
  g_autoxmlfree guchar *found = NULL;
  g_autofree gchar *node_name = NULL;
  g_autofree gchar *element_name = NULL;
  NodeState state;

  state = get_next_node (reader, &node_name, &element_name);
  if (state == STATE_END || !dzl_str_equal0 (name, node_name))
    assert_message ("Node mismatch: asked:%s found:%s\n", name, found);
}

static IdeGiBase *
get_ns_root_object (IdeGiRepository *self,
                    IdeGiNamespace  *ns,
                    const gchar     *name)
{
  IdeGiVersion *version;
  IdeGiBase *base;
  g_autofree gchar *qname = NULL;
  guint major_version;
  guint minor_version;

  version = ide_gi_namespace_get_repository_version (ns);
  qname = g_strconcat (ide_gi_namespace_get_name (ns), ".", name, NULL);
  major_version = ide_gi_namespace_get_major_version (ns);
  minor_version = ide_gi_namespace_get_minor_version (ns);
  base = ide_gi_version_lookup_root_object (version, qname, major_version, minor_version);

  return base;
}

static IdeGiBase *
get_object_from_gtype (IdeGiRepository *self,
                       IdeGiNamespace  *ns,
                       const gchar     *gtype_name)
{
  IdeGiVersion *version;
  IdeGiBase *base;

  version = ide_gi_namespace_get_repository_version (ns);
  base = ide_gi_version_lookup_gtype_in_ns (version, ns, gtype_name);

  return base;
}

static void
check_common (IdeGiRepository  *self,
              IdeGiBase        *base,
              xmlTextReaderPtr  reader)
{
  g_assert (base != NULL);

  assert_attr_str (reader, "deprecated-version", ide_gi_base_get_deprecated_version (base));
  assert_attr_str (reader, "version", ide_gi_base_get_version (base));
  assert_attr_bool (reader, "introspectable", "0", ide_gi_base_is_introspectable (base));
  assert_attr_bool (reader, "deprecated", "0", ide_gi_base_is_deprecated (base));
  assert_attr_stability (reader, "stability", "Stable", ide_gi_base_stability (base));

  /* TODO: doc object */
}

static void
check_doc (IdeGiRepository  *self,
           xmlTextReaderPtr  reader,
           IdeGiBase        *parent,
           const gchar      *node_name)
{
  const guchar *text_node;
  const gchar *str;
  g_autoxmlfree guchar *end_element_name = NULL;
  xmlReaderTypes type;
  g_autoptr(IdeGiDoc) doc_object = NULL;

  if (xmlTextReaderRead (reader) != 1)
    return;

  text_node = xmlTextReaderConstValue (reader);
  type = xmlTextReaderNodeType (reader);
  g_assert (type == XML_READER_TYPE_TEXT);

  if (NULL == (doc_object = ide_gi_base_get_doc (parent)))
    return;

  if (dzl_str_equal0 (node_name, "doc"))
    str = ide_gi_doc_get_doc  (doc_object);
  else if (dzl_str_equal0 (node_name, "doc-deprecated"))
    str = ide_gi_doc_get_deprecated_version (doc_object);
  else if (dzl_str_equal0 (node_name, "doc-version"))
    str = ide_gi_doc_get_version (doc_object);
  else if (dzl_str_equal0 (node_name, "doc-stability"))
    str = ide_gi_doc_get_stability (doc_object);
  else
    g_assert_not_reached ();

  g_assert_cmpstr ((gchar *)text_node, ==, str);

  if (xmlTextReaderRead (reader) != 1)
    return;

  end_element_name = xmlTextReaderName (reader);
  type = xmlTextReaderNodeType (reader);
  g_assert (dzl_str_equal0 (end_element_name, node_name));
  g_assert (type == XML_READER_TYPE_END_ELEMENT);
}

static void
check_type (IdeGiRepository  *self,
            IdeGiTypeRef      typeref,
            xmlTextReaderPtr  reader)
{
  if (typeref.type == IDE_GI_BASIC_TYPE_CALLBACK)
    {
      g_autoptr(IdeGiBase) base = ide_gi_typeref_get_object (global.ns, typeref);
      check_callback (self, (IdeGiCallback *)base, reader);
    }
  else
    g_print ("check typeref\n");
}

static void
check_parameter_flags (IdeGiRepository  *self,
                       IdeGiParameter   *parameter,
                       xmlTextReaderPtr  reader)
{
  IdeGiParameterFlags flags = ide_gi_parameter_get_flags (parameter);

  assert_attr_bool (reader, "nullable", "0", (flags & IDE_GI_PARAMETER_FLAG_NULLABLE));
  assert_attr_bool (reader, "allow-none", "0", (flags & IDE_GI_PARAMETER_FLAG_ALLOW_NONE));
  assert_attr_bool (reader, "caller-allocates", "0", (flags & IDE_GI_PARAMETER_FLAG_CALLER_ALLOCATES));
  assert_attr_bool (reader, "optional", "0", (flags & IDE_GI_PARAMETER_FLAG_OPTIONAL));
  assert_attr_bool (reader, "skip", "0", (flags & IDE_GI_PARAMETER_FLAG_SKIP));

  /* TODO: closure, destroy */
}

static void
check_parameter (IdeGiRepository  *self,
                 IdeGiParameter   *parameter,
                 xmlTextReaderPtr  reader)
{
  g_assert (parameter != NULL);

  check_common (self, (IdeGiBase *)parameter, reader);

  assert_attr_scope (reader, "scope", "call", ide_gi_parameter_get_scope (parameter));
  assert_attr_transfer (reader, "transfer-ownership", "none", ide_gi_parameter_get_transfer_ownership (parameter));
  assert_attr_direction (reader, "direction", "in", ide_gi_parameter_get_direction (parameter));

  check_parameter_flags (self, parameter, reader);

  //g_assert_cmpstr (ide_gi_parameter_get_closure (param)); def = -1
  //g_assert_cmpstr (ide_gi_parameter_get_destroy (param)); def =  -1

  push_node ();
  while (TRUE)
    {
      g_autofree gchar *node_name = NULL;
      g_autofree gchar *element_name = NULL;
      NodeState state;

      state = get_next_node (reader, &node_name, &element_name);
      if (state == STATE_END || state == STATE_END_ELEMENT)
        return;

      if (dzl_str_equal0 (node_name, "array") || dzl_str_equal0 (node_name, "type"))
        check_type (self, ide_gi_parameter_get_typeref (parameter), reader);
      else if (dzl_str_equal0 (node_name, "doc") ||
               dzl_str_equal0 (node_name, "doc-deprecated") ||
               dzl_str_equal0 (node_name, "doc-version") ||
               dzl_str_equal0 (node_name, "doc-stability") ||
               dzl_str_equal0 (node_name, "annotation"))
        {
          check_doc (self, reader, (IdeGiBase *)parameter, node_name);
        }
      else
        assert_message ("parameter child node:%s\n", node_name);
    }
}

static void
check_parameters (IdeGiRepository  *self,
                  IdeGiBase        *base,
                  xmlTextReaderPtr  reader)
{
  IdeGiBlobType type;

  g_assert (base != NULL);

  type = ide_gi_base_get_object_type (base);
  g_assert (type == IDE_GI_BLOB_TYPE_FUNCTION ||
            type == IDE_GI_BLOB_TYPE_CONSTRUCTOR ||
            type == IDE_GI_BLOB_TYPE_VFUNC ||
            type == IDE_GI_BLOB_TYPE_RECORD ||
            type == IDE_GI_BLOB_TYPE_METHOD ||
            type == IDE_GI_BLOB_TYPE_CALLBACK ||
            type == IDE_GI_BLOB_TYPE_SIGNAL);

  push_node ();
  while (TRUE)
    {
      g_autofree gchar *node_name = NULL;
      g_autofree gchar *element_name = NULL;
      NodeState state;

      state = get_next_node (reader, &node_name, &element_name);
      if (state == STATE_END || state == STATE_END_ELEMENT)
        return;

      if (dzl_str_equal0 (node_name, "parameter") || dzl_str_equal0 (node_name, "instance-parameter"))
        {
          g_autoptr(IdeGiParameter) parameter = NULL;

          if (type == IDE_GI_BLOB_TYPE_FUNCTION ||
              type == IDE_GI_BLOB_TYPE_CONSTRUCTOR ||
              type == IDE_GI_BLOB_TYPE_VFUNC ||
              type == IDE_GI_BLOB_TYPE_RECORD ||
              type == IDE_GI_BLOB_TYPE_METHOD)
            parameter = ide_gi_function_lookup_parameter ((IdeGiFunction *)base, element_name);
          else if (type == IDE_GI_BLOB_TYPE_CALLBACK)
            parameter = ide_gi_callback_lookup_parameter ((IdeGiCallback *)base, element_name);
          else if (type == IDE_GI_BLOB_TYPE_SIGNAL)
            parameter = ide_gi_signal_lookup_parameter ((IdeGiSignal *)base, element_name);

          check_parameter (self, parameter, reader);
        }
      else
        assert_message ("parameters child node:%s\n", node_name);
    }
}

static void
check_alias (IdeGiRepository  *self,
             IdeGiAlias       *alias,
             xmlTextReaderPtr  reader)
{
  g_assert (alias != NULL);

  check_common (self, (IdeGiBase *)alias, reader);
  assert_attr_str (reader, "c:type", ide_gi_alias_get_c_type (alias));

  push_node ();
  while (TRUE)
    {
      g_autofree gchar *node_name = NULL;
      g_autofree gchar *element_name = NULL;
      NodeState state;

      state = get_next_node (reader, &node_name, &element_name);
      if (state == STATE_END || state == STATE_END_ELEMENT)
        break;

      if (dzl_str_equal0 (node_name, "type"))
        check_type (self, ide_gi_alias_get_typeref (alias), reader);
      else if (dzl_str_equal0 (node_name, "doc") ||
               dzl_str_equal0 (node_name, "doc-deprecated") ||
               dzl_str_equal0 (node_name, "doc-version") ||
               dzl_str_equal0 (node_name, "doc-stability") ||
               dzl_str_equal0 (node_name, "annotation"))
        {
          check_doc (self, reader, (IdeGiBase *)alias, node_name);
        }
      else
        assert_message ("alias child node:%s\n", node_name);
    }
}

static void
check_value (IdeGiRepository  *self,
             IdeGiValue       *value,
             xmlTextReaderPtr  reader)
{
  g_assert (value != NULL);

  check_common (self, (IdeGiBase *)value, reader);
  assert_attr_str (reader, "c:identifier", ide_gi_value_get_c_identifier (value));
  assert_attr_str (reader, "glib:nick", ide_gi_value_get_glib_nick (value));
  assert_attr_int (reader, "value", "0", ide_gi_value_get_value (value));

  push_node ();
  while (TRUE)
    {
      g_autofree gchar *node_name = NULL;
      g_autofree gchar *element_name = NULL;
      NodeState state;

      state = get_next_node (reader, &node_name, &element_name);
      if (state == STATE_END || state == STATE_END_ELEMENT)
        break;

      if (dzl_str_equal0 (node_name, "doc") ||
          dzl_str_equal0 (node_name, "doc-deprecated") ||
          dzl_str_equal0 (node_name, "doc-version") ||
          dzl_str_equal0 (node_name, "doc-stability") ||
          dzl_str_equal0 (node_name, "annotation"))
        {
          check_doc (self, reader, (IdeGiBase *)value, node_name);
        }
      else
        assert_message ("value child node:%s\n", node_name);
    }
}

static void
check_enum (IdeGiRepository  *self,
            IdeGiEnum        *enumeration,
            xmlTextReaderPtr  reader)
{
  g_assert (enumeration != NULL);

  check_common (self, (IdeGiBase *)enumeration, reader);
  assert_attr_str (reader, "c:type", ide_gi_enum_get_c_type (enumeration));
  assert_attr_str (reader, "glib:type-name", ide_gi_enum_get_g_type_name (enumeration));
  assert_attr_str (reader, "glib:get-type", ide_gi_enum_get_g_get_type (enumeration));
  assert_attr_str (reader, "glib:error-domain", ide_gi_enum_get_g_error_domain (enumeration));

  push_node ();
  while (TRUE)
    {
      g_autofree gchar *node_name = NULL;
      g_autofree gchar *element_name = NULL;
      NodeState state;

      state = get_next_node (reader, &node_name, &element_name);
      if (state == STATE_END || state == STATE_END_ELEMENT)
        break;

      if (dzl_str_equal0 (node_name, "function"))
        {
          g_autoptr(IdeGiFunction) function = ide_gi_enum_lookup_function (enumeration, (gchar *)element_name);
          check_function (self, function, reader);
        }
      else if (dzl_str_equal0 (node_name, "member"))
        {
          g_autoptr(IdeGiValue) value = ide_gi_enum_lookup_value (enumeration, (gchar *)element_name);
          check_value (self, value, reader);
        }
      else if (dzl_str_equal0 (node_name, "doc") ||
               dzl_str_equal0 (node_name, "doc-deprecated") ||
               dzl_str_equal0 (node_name, "doc-version") ||
               dzl_str_equal0 (node_name, "doc-stability") ||
               dzl_str_equal0 (node_name, "annotation"))
        {
          check_doc (self, reader, (IdeGiBase *)enumeration, node_name);
        }
      else
        assert_message ("enum child node:%s\n", node_name);
    }
}

static void
check_constant (IdeGiRepository  *self,
                IdeGiConstant    *constant,
                xmlTextReaderPtr  reader)
{
  g_assert (constant != NULL);

  check_common (self, (IdeGiBase *)constant, reader);
  assert_attr_str (reader, "c:type", ide_gi_constant_get_c_type (constant));
  assert_attr_str (reader, "c:identifier", ide_gi_constant_get_c_identifier (constant));
  assert_attr_str (reader, "value", ide_gi_constant_get_value (constant));

  push_node ();
  while (TRUE)
    {
      g_autofree gchar *node_name = NULL;
      g_autofree gchar *element_name = NULL;
      NodeState state;

      state = get_next_node (reader, &node_name, &element_name);
      if (state == STATE_END || state == STATE_END_ELEMENT)
        break;

      if (dzl_str_equal0 (node_name, "array") || dzl_str_equal0 (node_name, "type"))
        check_type (self, ide_gi_constant_get_typeref (constant), reader);
      else if (dzl_str_equal0 (node_name, "doc") ||
               dzl_str_equal0 (node_name, "doc-deprecated") ||
               dzl_str_equal0 (node_name, "doc-version") ||
               dzl_str_equal0 (node_name, "doc-stability") ||
               dzl_str_equal0 (node_name, "annotation"))
        {
          check_doc (self, reader, (IdeGiBase *)constant, node_name);
        }
      else
        assert_message ("constant child node:%s\n", node_name);
    }
}

static void
check_union (IdeGiRepository  *self,
             IdeGiUnion       *_union,
             xmlTextReaderPtr  reader)
{
  g_assert (_union != NULL);

  check_common (self, (IdeGiBase *)_union, reader);
  assert_attr_str (reader, "c:type", ide_gi_union_get_c_type (_union));
  assert_attr_str (reader, "c:symbol-prefix", ide_gi_union_get_c_symbol_prefix (_union));
  assert_attr_str (reader, "glib:get-type", ide_gi_union_get_g_get_type (_union));
  assert_attr_str (reader, "glib:type-name", ide_gi_union_get_g_type_name (_union));

  if (is_node_empty (reader))
  return;

  push_node ();
  while (TRUE)
    {
      g_autofree gchar *node_name = NULL;
      g_autofree gchar *element_name = NULL;
      NodeState state;

      state = get_next_node (reader, &node_name, &element_name);
      if (state == STATE_END || state == STATE_END_ELEMENT)
        break;

      if (dzl_str_equal0 (node_name, "constructor") ||
           dzl_str_equal0 (node_name, "function") ||
           dzl_str_equal0 (node_name, "method"))
        {
          g_autoptr(IdeGiFunction) function = ide_gi_union_lookup_function (_union, (gchar *)element_name);
          check_function (self, function, reader);
        }
      else  if (dzl_str_equal0 (node_name, "field"))
        {
          g_autoptr(IdeGiField) field = ide_gi_union_lookup_field (_union, (gchar *)element_name);
          check_field (self, field, reader);
        }
      else  if (dzl_str_equal0 (node_name, "record"))
        {
          g_autoptr(IdeGiRecord) record = ide_gi_union_lookup_record (_union, (gchar *)element_name);
          check_record (self, record, reader);
        }
      else if (dzl_str_equal0 (node_name, "doc") ||
               dzl_str_equal0 (node_name, "doc-deprecated") ||
               dzl_str_equal0 (node_name, "doc-version") ||
               dzl_str_equal0 (node_name, "doc-stability") ||
               dzl_str_equal0 (node_name, "annotation"))
        {
          check_doc (self, reader, (IdeGiBase *)_union, node_name);
        }
      else
        assert_message ("union child node:%s\n", node_name);
    }
}

static void
check_callback (IdeGiRepository  *self,
                IdeGiCallback    *callback,
                xmlTextReaderPtr  reader)
{
  g_assert (callback != NULL);

  check_common (self, (IdeGiBase *)callback, reader);
  assert_attr_bool (reader, "throws", "0", ide_gi_callback_is_throws (callback));
  assert_attr_str (reader, "c:type", ide_gi_callback_get_c_type (callback));

  push_node ();
  while (TRUE)
    {
      g_autofree gchar *node_name = NULL;
      g_autofree gchar *element_name = NULL;
      NodeState state;

      state = get_next_node (reader, &node_name, &element_name);
      if (state == STATE_END || state == STATE_END_ELEMENT)
        return;

      if (dzl_str_equal0 (node_name, "return-value"))
        check_parameter (self, ide_gi_callback_get_return_value (callback), reader);
      else  if (dzl_str_equal0 (node_name, "parameters"))
        check_parameters (self, (IdeGiBase *)callback, reader);
      else if (dzl_str_equal0 (node_name, "doc") ||
               dzl_str_equal0 (node_name, "doc-deprecated") ||
               dzl_str_equal0 (node_name, "doc-version") ||
               dzl_str_equal0 (node_name, "doc-stability") ||
               dzl_str_equal0 (node_name, "annotation"))
        {
          check_doc (self, reader, (IdeGiBase *)callback, node_name);
        }
      else
        assert_message ("callback child node:%s\n", node_name);
    }
}

static void
check_function (IdeGiRepository  *self,
                IdeGiFunction    *function,
                xmlTextReaderPtr  reader)
{
  g_assert (function != NULL);

  check_common (self, (IdeGiBase *)function, reader);
  assert_attr_bool (reader, "throws", "0", ide_gi_function_is_throws (function));
  assert_attr_str (reader, "c:identifier", ide_gi_function_get_c_identifier (function));
  assert_attr_str (reader, "shadowed-by", ide_gi_function_get_shadowed_by (function));
  assert_attr_str (reader, "shadows", ide_gi_function_get_shadows (function));
  assert_attr_str (reader, "moved-to", ide_gi_function_get_moved_to (function));
  assert_attr_str (reader, "invoker", ide_gi_function_get_invoker (function));
  //assert_attr_bool (reader, "", "0", ide_gi_function_is_setter (function));
  //assert_attr_bool (reader, "", "0", ide_gi_function_is_getter (function));

  push_node ();
  while (TRUE)
    {
      g_autofree gchar *node_name = NULL;
      g_autofree gchar *element_name = NULL;
      NodeState state;

      state = get_next_node (reader, &node_name, &element_name);
      if (state == STATE_END || state == STATE_END_ELEMENT)
        return;

      if (dzl_str_equal0 (node_name, "return-value"))
        check_parameter (self, ide_gi_function_get_return_value (function), reader);
      else  if (dzl_str_equal0 (node_name, "parameters"))
        check_parameters (self, (IdeGiBase *)function, reader);
      else if (dzl_str_equal0 (node_name, "doc") ||
               dzl_str_equal0 (node_name, "doc-deprecated") ||
               dzl_str_equal0 (node_name, "doc-version") ||
               dzl_str_equal0 (node_name, "doc-stability") ||
               dzl_str_equal0 (node_name, "annotation"))
        {
          check_doc (self, reader, (IdeGiBase *)function, node_name);
        }
      else
        assert_message ("function child node:%s\n", node_name);
    }
}

static void
check_property (IdeGiRepository  *self,
                IdeGiProperty    *property,
                xmlTextReaderPtr  reader)
{
  g_assert (property != NULL);

  check_common (self, (IdeGiBase *)property, reader);
  assert_attr_bool (reader, "0", "readable", ide_gi_property_is_readable (property));
  assert_attr_bool (reader, "0", "writable", ide_gi_property_is_writable (property));
  assert_attr_bool (reader, "0", "construct", ide_gi_property_is_construct (property));
  assert_attr_bool (reader, "0", "construct-only", ide_gi_property_is_construct_only (property));
  assert_attr_stability (reader, "stability", "Stable", ide_gi_base_stability ((IdeGiBase *)property));
  assert_attr_transfer (reader, "transfer-ownership", "none", ide_gi_property_get_transfer_ownership (property));

  push_node ();
  while (TRUE)
    {
      g_autofree gchar *node_name = NULL;
      g_autofree gchar *element_name = NULL;
      NodeState state;

      state = get_next_node (reader, &node_name, &element_name);
      if (state == STATE_END || state == STATE_END_ELEMENT)
        break;

      if (dzl_str_equal0 (node_name, "array") || dzl_str_equal0 (node_name, "type"))
        check_type (self, ide_gi_property_get_typeref (property), reader);
      else if (dzl_str_equal0 (node_name, "doc") ||
               dzl_str_equal0 (node_name, "doc-deprecated") ||
               dzl_str_equal0 (node_name, "doc-version") ||
               dzl_str_equal0 (node_name, "doc-stability") ||
               dzl_str_equal0 (node_name, "annotation"))
        {
          check_doc (self, reader, (IdeGiBase *)property, node_name);
        }
      else
        assert_message ("property child node:%s\n", node_name);
    }
}

static void
check_signal (IdeGiRepository  *self,
              IdeGiSignal      *signal,
              xmlTextReaderPtr  reader)
{
  g_assert (signal != NULL);

  check_common (self, (IdeGiBase *)signal, reader);
  assert_attr_bool (reader, "0", "action", ide_gi_signal_is_action (signal));
  assert_attr_bool (reader, "0", "no-hooks", ide_gi_signal_is_no_hooks (signal));
  assert_attr_bool (reader, "0", "no-recurse", ide_gi_signal_is_no_recurse (signal));
  assert_attr_bool (reader, "0", "detailed", ide_gi_signal_is_detailed (signal));
  assert_attr_when (reader, "when", "first", ide_gi_signal_get_run_when (signal));

  //assert_attr_str (reader, "", ide_gi_signal_get_vfunc (signal));
  //assert_attr_bool (reader, "", ide_gi_signal_has_class_closure (signal));
  //assert_attr_bool (reader, "", ide_gi_signal_is_true_stops_emit (signal));

  push_node ();
  while (TRUE)
    {
      g_autofree gchar *node_name = NULL;
      g_autofree gchar *element_name = NULL;
      NodeState state;

      state = get_next_node (reader, &node_name, &element_name);
      if (state == STATE_END || state == STATE_END_ELEMENT)
        return;

      if (dzl_str_equal0 (node_name, "return-value"))
        check_parameter (self, ide_gi_signal_get_return_value (signal), reader);
      else  if (dzl_str_equal0 (node_name, "parameters"))
        check_parameters (self, (IdeGiBase *)signal, reader);
      else if (dzl_str_equal0 (node_name, "doc") ||
               dzl_str_equal0 (node_name, "doc-deprecated") ||
               dzl_str_equal0 (node_name, "doc-version") ||
               dzl_str_equal0 (node_name, "doc-stability") ||
               dzl_str_equal0 (node_name, "annotation"))
        {
          check_doc (self, reader, (IdeGiBase *)signal, node_name);
        }
      else
        assert_message ("signal child node:%s\n", node_name);
    }
}

static void
check_field (IdeGiRepository  *self,
             IdeGiField       *field,
             xmlTextReaderPtr  reader)
{
  g_assert (field != NULL);

  check_common (self, (IdeGiBase *)field, reader);
  assert_attr_bool (reader, "0", "readable", ide_gi_field_is_readable (field));
  assert_attr_bool (reader, "0", "writable", ide_gi_field_is_writable (field));
  assert_attr_bool (reader, "0", "private", ide_gi_field_is_private (field));
  assert_attr_int (reader, "bits", "0", ide_gi_field_get_bits (field));

  push_node ();
  while (TRUE)
    {
      g_autofree gchar *node_name = NULL;
      g_autofree gchar *element_name = NULL;
      NodeState state;

      state = get_next_node (reader, &node_name, &element_name);
      if (state == STATE_END || state == STATE_END_ELEMENT)
        break;

      if (dzl_str_equal0 (node_name, "array") ||
          dzl_str_equal0 (node_name, "type") ||
          dzl_str_equal0 (node_name, "callback"))
        {
          check_type (self, ide_gi_field_get_typeref (field), reader);
        }
      else if (dzl_str_equal0 (node_name, "doc") ||
               dzl_str_equal0 (node_name, "doc-deprecated") ||
               dzl_str_equal0 (node_name, "doc-version") ||
               dzl_str_equal0 (node_name, "doc-stability") ||
               dzl_str_equal0 (node_name, "annotation"))
        {
          check_doc (self, reader, (IdeGiBase *)field, node_name);
        }
      else
        assert_message ("field child node:%s\n", node_name);
    }
}

static void
check_record (IdeGiRepository  *self,
              IdeGiRecord      *record,
              xmlTextReaderPtr  reader)
{
  g_assert (record != NULL);

  check_common (self, (IdeGiBase *)record, reader);
  assert_attr_str (reader, "c:type", ide_gi_record_get_c_type (record));
  assert_attr_bool (reader, "0", "disguised", ide_gi_record_is_disguised (record));
  assert_attr_bool (reader, "0", "foreign", ide_gi_record_is_foreign (record));
  assert_attr_str (reader, "glib:type-name", ide_gi_record_get_g_type_name (record));
  assert_attr_str (reader, "glib:get-type", ide_gi_record_get_g_get_type (record));
  assert_attr_str (reader, "c:symbol-prefix", ide_gi_record_get_c_symbol_prefix (record));
  assert_attr_str (reader, "glib:is-gtype-struct-for", ide_gi_record_get_g_is_gtype_struct_for (record));

  if (is_node_empty (reader))
    return;

  push_node ();
  while (TRUE)
    {
      g_autofree gchar *node_name = NULL;
      g_autofree gchar *element_name = NULL;
      NodeState state;

      state = get_next_node (reader, &node_name, &element_name);
      if (state == STATE_END || state == STATE_END_ELEMENT)
        return;

      if (dzl_str_equal0 (node_name, "constructor") ||
          dzl_str_equal0 (node_name, "function") ||
          dzl_str_equal0 (node_name, "method") ||
          dzl_str_equal0 (node_name, "virtual-method"))
        {
          g_autoptr(IdeGiFunction) callable = ide_gi_record_lookup_function (record, (gchar *)element_name);
          check_function (self, callable, reader);
        }
      else  if (dzl_str_equal0 (node_name, "field"))
        {
          g_autoptr(IdeGiField) field = ide_gi_record_lookup_field (record, (gchar *)element_name);
          check_field (self, field, reader);
        }
      else if (dzl_str_equal0 (node_name, "property"))
        {
          g_autoptr(IdeGiProperty) property = ide_gi_record_lookup_property (record, (gchar *)element_name);
          check_property (self, property, reader);
        }
      else  if (dzl_str_equal0 (node_name, "union"))
        {
          g_autoptr(IdeGiUnion) _union = ide_gi_record_lookup_union (record, (gchar *)element_name);
          check_union (self, _union, reader);
        }
      else  if (dzl_str_equal0 (node_name, "callback"))
        {
          g_autoptr(IdeGiCallback) callback = ide_gi_record_lookup_callback (record, (gchar *)element_name);
          check_callback (self, callback, reader);
        }
      else if (dzl_str_equal0 (node_name, "doc") ||
               dzl_str_equal0 (node_name, "doc-deprecated") ||
               dzl_str_equal0 (node_name, "doc-version") ||
               dzl_str_equal0 (node_name, "doc-stability") ||
               dzl_str_equal0 (node_name, "annotation"))
        {
          check_doc (self, reader, (IdeGiBase *)record, node_name);
        }
      else
        assert_message ("record child node:%s\n", node_name);
    }
}

static void
check_prerequisite (IdeGiRepository  *self,
                    IdeGiNamespace   *ns,
                    IdeGiBase        *base,
                    const gchar      *element_name,
                    xmlTextReaderPtr  reader)
{
  if (base != NULL)
    {
      if (NULL != strchr (element_name, '.'))
        g_assert_cmpstr (element_name, ==, ide_gi_base_get_qualified_name (base));
      else
        {
          g_autoptr(IdeGiNamespace) base_ns = ide_gi_base_get_namespace (base);

          g_assert (ns == base_ns);
          g_assert_cmpstr (element_name, ==, ide_gi_base_get_name (base));
        }
    }
}

static void
check_interface (IdeGiRepository  *self,
                 IdeGiInterface   *interface,
                 xmlTextReaderPtr  reader)
{
  g_assert (interface != NULL);

  check_common (self, (IdeGiBase *)interface, reader);
  assert_attr_str (reader, "glib:type-name", ide_gi_interface_get_g_type_name (interface));
  assert_attr_str (reader, "glib:get-type", ide_gi_interface_get_g_get_type (interface));
  //assert_attr_str (reader, "glib:type-struct", ide_gi_interface_get_g_type_struct (interface));
  assert_attr_str (reader, "c:symbol-prefix", ide_gi_interface_get_c_symbol_prefix (interface));
  assert_attr_str (reader, "c:type", ide_gi_interface_get_c_type (interface));

  push_node ();
  while (TRUE)
    {
      g_autofree gchar *node_name = NULL;
      g_autofree gchar *element_name = NULL;
      NodeState state;

      state = get_next_node (reader, &node_name, &element_name);
      if (state == STATE_END || state == STATE_END_ELEMENT)
        return;

      if (dzl_str_equal0 (node_name, "constructor") ||
          dzl_str_equal0 (node_name, "function") ||
          dzl_str_equal0 (node_name, "method") ||
          dzl_str_equal0 (node_name, "virtual-method"))
        {
          g_autoptr(IdeGiFunction) callable = ide_gi_interface_lookup_function (interface, (gchar *)element_name);
          check_function (self, callable, reader);
        }
      else if (dzl_str_equal0 (node_name, "prerequisite"))
        {
          g_autoptr(IdeGiBase) prerequisite = ide_gi_interface_lookup_prerequisite (interface, (gchar *)element_name);
          check_prerequisite (self, global.ns, prerequisite, element_name, reader);
        }
      else  if (dzl_str_equal0 (node_name, "field"))
        {
          g_autoptr(IdeGiField) field = ide_gi_interface_lookup_field (interface, (gchar *)element_name);
          check_field (self, field, reader);
        }
      else if (dzl_str_equal0 (node_name, "property"))
        {
          g_autoptr(IdeGiProperty) property = ide_gi_interface_lookup_property (interface, (gchar *)element_name);
          check_property (self, property, reader);
        }
      else  if (dzl_str_equal0 (node_name, "glib:signal"))
        {
          g_autoptr(IdeGiSignal) signal = ide_gi_interface_lookup_signal (interface, (gchar *)element_name);
          check_signal (self, signal, reader);
        }
      else  if (dzl_str_equal0 (node_name, "constant"))
        {
          g_autoptr(IdeGiConstant) constant = ide_gi_interface_lookup_constant (interface, (gchar *)element_name);
          check_constant (self, constant, reader);
        }
      else  if (dzl_str_equal0 (node_name, "callback"))
        {
          g_autoptr(IdeGiCallback) callback = ide_gi_interface_lookup_callback (interface, (gchar *)element_name);
          check_callback (self, callback, reader);
        }
      else if (dzl_str_equal0 (node_name, "doc") ||
               dzl_str_equal0 (node_name, "doc-deprecated") ||
               dzl_str_equal0 (node_name, "doc-version") ||
               dzl_str_equal0 (node_name, "doc-stability") ||
               dzl_str_equal0 (node_name, "annotation"))
        {
          check_doc (self, reader, (IdeGiBase *)interface, node_name);
        }
      else
        assert_message ("interface child node:%s\n", node_name);
    }
}

static void
check_class_parent (IdeGiRepository  *self,
                    IdeGiClass       *klass,
                    xmlTextReaderPtr  reader)
{
  g_autoxmlfree guchar *parent_name = NULL;
  const gchar *qname;

  g_assert (klass != NULL);

  qname = ide_gi_class_get_parent_qname (klass);
  parent_name = get_attr (reader, "parent");
  if (strchr ((gchar *)parent_name, '.') != NULL)
    g_assert_cmpstr (qname, ==, (gchar *)parent_name);
  else
    {
      g_autofree gchar *name = g_strconcat (ide_gi_namespace_get_name (klass->ns),
                                            ".",
                                            (gchar *)parent_name,
                                            NULL);

      g_assert_cmpstr (qname, ==, name);
    }
}

static void
check_class (IdeGiRepository  *self,
             IdeGiClass       *klass,
             xmlTextReaderPtr  reader)
{
  g_assert (klass != NULL);

  check_common (self, (IdeGiBase *)klass, reader);
  assert_attr_bool (reader, "0", "abstract", ide_gi_class_is_abstract (klass));
  assert_attr_bool (reader, "0", "glib:fundamental", ide_gi_class_is_fundamental (klass));

  assert_attr_str (reader, "glib:type-name", ide_gi_class_get_g_type_name (klass));
  assert_attr_str (reader, "glib:get-type", ide_gi_class_get_g_get_type (klass));
  assert_attr_str (reader, "glib:type-struct", ide_gi_class_get_g_type_struct (klass));
  assert_attr_str (reader, "c:symbol-prefix", ide_gi_class_get_c_symbol_prefix (klass));
  assert_attr_str (reader, "c:type", ide_gi_class_get_c_type (klass));
  check_class_parent (self, klass, reader);

  assert_attr_str (reader, "glib:ref-func", ide_gi_class_get_g_ref_func (klass));
  assert_attr_str (reader, "glib:unref-func", ide_gi_class_get_g_unref_func (klass));
  assert_attr_str (reader, "glib:set-value-func", ide_gi_class_get_g_set_value_func (klass));
  assert_attr_str (reader, "glib:get-value-func", ide_gi_class_get_g_get_value_func (klass));

  push_node ();
  while (TRUE)
    {
      g_autofree gchar *node_name = NULL;
      g_autofree gchar *element_name = NULL;
      NodeState state;

      state = get_next_node (reader, &node_name, &element_name);
      if (state == STATE_END || state == STATE_END_ELEMENT)
        return;

      if (dzl_str_equal0 (node_name, "constructor") ||
          dzl_str_equal0 (node_name, "function") ||
          dzl_str_equal0 (node_name, "method") ||
          dzl_str_equal0 (node_name, "virtual-method"))
        {
          g_autoptr(IdeGiFunction) callable = ide_gi_class_lookup_function (klass, (gchar *)element_name);
          check_function (self, callable, reader);
        }
      else if (dzl_str_equal0 (node_name, "implements"))
        {
          g_autoptr(IdeGiInterface) interface = ide_gi_class_lookup_interface (klass, (gchar *)element_name);
          check_interface (self, interface, reader);
        }
      else  if (dzl_str_equal0 (node_name, "field"))
        {
          g_autoptr(IdeGiField) field = ide_gi_class_lookup_field (klass, (gchar *)element_name);
          check_field (self, field, reader);
        }
      else if (dzl_str_equal0 (node_name, "property"))
        {
          g_autoptr(IdeGiProperty) property = ide_gi_class_lookup_property (klass, (gchar *)element_name);
          check_property (self, property, reader);
        }
      else  if (dzl_str_equal0 (node_name, "glib:signal"))
        {
          g_autoptr(IdeGiSignal) signal = ide_gi_class_lookup_signal (klass, (gchar *)element_name);
          check_signal (self, signal, reader);
        }
      else  if (dzl_str_equal0 (node_name, "constant"))
        {
          g_autoptr(IdeGiConstant) constant = ide_gi_class_lookup_constant (klass, (gchar *)element_name);
          check_constant (self, constant, reader);
        }
      else  if (dzl_str_equal0 (node_name, "union"))
        {
          g_autoptr(IdeGiUnion) _union = ide_gi_class_lookup_union (klass, (gchar *)element_name);
          check_union (self, _union, reader);
        }
      else  if (dzl_str_equal0 (node_name, "record"))
        {
          g_autoptr(IdeGiRecord) record = ide_gi_class_lookup_record (klass, (gchar *)element_name);
          check_record (self, record, reader);
        }
      else  if (dzl_str_equal0 (node_name, "callback"))
        {
          g_autoptr(IdeGiCallback) callback = ide_gi_class_lookup_callback (klass, (gchar *)element_name);
          check_callback (self, callback, reader);
        }
      else if (dzl_str_equal0 (node_name, "doc") ||
               dzl_str_equal0 (node_name, "doc-deprecated") ||
               dzl_str_equal0 (node_name, "doc-version") ||
               dzl_str_equal0 (node_name, "doc-stability") ||
               dzl_str_equal0 (node_name, "annotation"))
        {
          check_doc (self, reader, (IdeGiBase *)klass, node_name);
        }
      else
        assert_message ("class child node:%s\n", node_name);
    }
}
static void
check_c_prefixes (IdeGiRepository  *self,
                  IdeGiNamespace   *ns,
                  xmlTextReaderPtr  reader)
{
  g_autofree gchar *prefixes = NULL;
  g_autofree guchar *c_prefix =  NULL;
  g_autofree guchar *c_identifier_prefixes = NULL;

  c_prefix = get_attr (reader, "c:prefix");
  c_identifier_prefixes = get_attr (reader, "c:identifier-prefixes");

  if (dzl_str_empty0 (c_identifier_prefixes))
    prefixes  = g_strdup ((gchar *)c_prefix);
  else  if (dzl_str_empty0 (c_prefix))
    prefixes  = g_strdup ((gchar *)c_identifier_prefixes);
  else
    prefixes  = g_strconcat ((gchar *)c_identifier_prefixes, ",", c_prefix, NULL);

  g_assert_cmpstr (prefixes, ==, ide_gi_namespace_get_c_identifiers_prefixes (ns));
}

static void
check_namespace (IdeGiRepository  *self,
                 IdeGiNamespace   *ns,
                 xmlTextReaderPtr  reader)
{
  assert_attr_str (reader, "name", ide_gi_namespace_get_name (ns));
  assert_attr_str (reader, "version", ide_gi_namespace_get_version (ns));
  assert_attr_str (reader, "shared-library", ide_gi_namespace_get_shared_library (ns));
  assert_attr_str (reader, "c:symbol-prefixes", ide_gi_namespace_get_c_symbol_prefixes (ns));
  check_c_prefixes (self, ns, reader);

  push_node ();
  while (TRUE)
    {
      g_autoptr(IdeGiBase) base = NULL;
      g_autofree gchar *node_name = NULL;
      g_autofree gchar *element_name = NULL;
      NodeState state;

      state = get_next_node (reader, &node_name, &element_name);
      if (state == STATE_END || state == STATE_END_ELEMENT)
        return;

      /* TODO: convert str to element_type to switch ? */
      if (dzl_str_equal0 (node_name, "class"))
        {
          base = get_ns_root_object (self, ns, (gchar *)element_name);
          check_class (self, (IdeGiClass *)base, reader);
        }
      else if (dzl_str_equal0 (node_name, "interface"))
        {
          base = get_ns_root_object (self, ns, (gchar *)element_name);
          check_interface (self, (IdeGiInterface *)base, reader);
        }
      else if (dzl_str_equal0 (node_name, "alias"))
        {
          base = get_ns_root_object (self, ns, (gchar *)element_name);
          check_alias (self, (IdeGiAlias *)base, reader);
        }
      else if (dzl_str_equal0 (node_name, "constant"))
        {
          base = get_ns_root_object (self, ns, (gchar *)element_name);
          check_constant (self, (IdeGiConstant *)base, reader);
        }
      else if (dzl_str_equal0 (node_name, "function"))
        {
          base = get_ns_root_object (self, ns, (gchar *)element_name);
          check_function (self, (IdeGiFunction *)base, reader);
        }
      else if (dzl_str_equal0 (node_name, "callback"))
        {
          base = get_ns_root_object (self, ns, (gchar *)element_name);
          check_callback (self, (IdeGiCallback *)base, reader);
        }
      else if (dzl_str_equal0 (node_name, "record"))
        {
          base = get_ns_root_object (self, ns, (gchar *)element_name);
          check_record (self, (IdeGiRecord *)base, reader);
        }
      else if (dzl_str_equal0 (node_name, "glib:boxed"))
        {
          const guchar *gtype_name = get_attr (reader, "glib:type-name");
          base = get_object_from_gtype (self, ns, (gchar *)gtype_name);
          check_record (self, (IdeGiRecord *)base, reader);
        }
      else if (dzl_str_equal0 (node_name, "union"))
        {
          base = get_ns_root_object (self, ns, (gchar *)element_name);
          check_union (self, (IdeGiUnion *)base, reader);
        }
      else if (dzl_str_equal0 (node_name, "bitfield") || dzl_str_equal0 (node_name, "enumeration"))
        {
          base = get_ns_root_object (self, ns, (gchar *)element_name);
          check_enum (self, (IdeGiEnum *)base, reader);
        }
      else
        assert_message ("namespace child node:%s\n", node_name);
    }
}

static void
check_header (IdeGiRepository  *self,
              IdeGiNamespace   *ns,
              xmlTextReaderPtr  reader)
{
  GString *include = g_string_new (NULL);
  GString *c_include = g_string_new (NULL);
  GString *package = g_string_new (NULL);
  g_autofree gchar *include_str = NULL;
  g_autofree gchar *c_include_str = NULL;
  g_autofree gchar *package_str = NULL;
  gboolean first_include = TRUE;
  gboolean first_c_include = TRUE;
  gboolean first_package = TRUE;

  push_node ();
  while (TRUE)
    {
      g_autofree gchar *node_name = NULL;
      g_autofree gchar *element_name = NULL;
      NodeState state;

      state = get_next_node (reader, &node_name, &element_name);
      if (state == STATE_END || state == STATE_END_ELEMENT)
        goto finish;

      if (dzl_str_equal0 (node_name, "include"))
        {
          g_autoxmlfree guchar *version = get_attr (reader, "version");

          if (!first_include)
            g_string_append (include, ",");

          first_include = FALSE;
          g_string_append (include, element_name);
          g_string_append (include, ":");
          g_string_append (include, (gchar *)version);
        }
      else if (dzl_str_equal0 (node_name, "c:include"))
        {
          if (!first_c_include)
            g_string_append (c_include, ",");

          first_c_include = FALSE;
          g_string_append (c_include, element_name);
        }
      else if (dzl_str_equal0 (node_name, "package"))
        {
          if (!first_package)
            g_string_append (package, ",");

          first_package = FALSE;
          g_string_append (package, element_name);
        }
      else if (dzl_str_equal0 (node_name, "namespace"))
        check_namespace  (self, ns, reader);
      else
        assert_message ("header child node:%s\n", node_name);
    }

finish:
  include_str = g_string_free (include, FALSE);
  g_assert_cmpstr (include_str, ==, ide_gi_namespace_get_includes (ns));

  c_include_str = g_string_free (c_include, FALSE);
  g_assert_cmpstr (c_include_str, ==, ide_gi_namespace_get_c_includes (ns));

  package_str = g_string_free (package, FALSE);
  g_assert_cmpstr (package_str, ==, ide_gi_namespace_get_packages (ns));
}

static void
compare_ns (IdeGiRepository *self,
            const gchar     *file,
            IdeGiNamespace  *ns)
{
  xmlTextReaderPtr reader;
  gint ret = 0;

  g_assert (IDE_IS_GI_REPOSITORY (self));

  init_global (&global);
  global.ns = ide_gi_namespace_ref (ns);

  if (NULL != (reader = xmlNewTextReaderFilename (file)))
    {
      assert_next_node (reader, "repository");
      check_header (self, ns, reader);

      xmlFreeTextReader(reader);
      if (ret != 0)
        assert_message ("failed to parse %s\n", file);
    }
  else
    assert_message ("Unable to open %s\n", file);
}

static void
test_check_serialisation_cb (gpointer      source_object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeGiNamespace) ns = NULL;
  g_autoptr(IdeGiVersion) version = NULL;
  g_autofree gchar *file;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_MAIN_THREAD ());

  IDE_ENTRY;

  global_repository = test_gi_common_setup_finish (result, &error);
  g_assert (IDE_IS_GI_REPOSITORY (global_repository));

  version = ide_gi_repository_get_current_version (global_repository);
  g_assert (IDE_IS_GI_VERSION (version));

  ns = ide_gi_version_lookup_namespace (version, "IdeGiTest", 1, 0);
  file = g_build_filename (TEST_DATA_DIR, "gi", "IdeGiTest-1.0.gir", NULL);
  compare_ns (global_repository, file, ns);

  g_task_return_boolean (task, TRUE);
  IDE_EXIT;
}

static void
test_check_serialisation_async (GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  GTask *task;

  IDE_ENTRY;

  task = g_task_new (NULL, cancellable, callback, user_data);
  test_gi_common_setup_async (cancellable, (GAsyncReadyCallback)test_check_serialisation_cb, task);

  IDE_EXIT;
}

gint
main (gint   argc,
      gchar *argv[])
{
  static const gchar *required_plugins[] = { "buildconfig",
                                             "meson-plugin",
                                             "autotools-plugin",
                                             "directory-plugin",
                                             NULL };
  g_autoptr (IdeApplication) app = NULL;
  gboolean ret;

  g_test_init (&argc, &argv, NULL);

  ide_log_init (TRUE, NULL);
  ide_log_set_verbosity (4);

  app = ide_application_new (IDE_APPLICATION_MODE_TESTS | G_APPLICATION_NON_UNIQUE);
  ide_application_add_test (app,
                            "/Gi/repository/check_serialisation",
                            test_check_serialisation_async,
                            NULL,
                            required_plugins);

  gnome_builder_plugins_init ();
  ret = g_application_run (G_APPLICATION (app), argc, argv);

  free_global (&global);
  g_object_unref (global_repository);

  return ret;
}
