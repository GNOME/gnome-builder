/* ide-xml-hover-provider.c
 *
 * Copyright 2018 Sebastien Lafargue <slafargue@gnome.org>
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

#define G_LOG_DOMAIN "ide-xml-hover-provider"

#include <dazzle.h>
#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>

#include "../gi/ide-gi-objects.h"
#include "../gi/ide-gi-service.h"

#include "ide-xml-detail.h"
#include "ide-xml-position.h"
#include "ide-xml-service.h"
#include "ide-xml-symbol-node.h"
#include "ide-xml-types.h"
#include "ide-xml-utils.h"

#include "ide-xml-hover-provider.h"

#define XML_HOVER_PROVIDER_PRIORITY 200

struct _IdeXmlHoverProvider
{
  GObject parent_instance;
};

typedef struct
{
  IdeHoverContext *context;
  IdeGiService    *gi_service;
  IdeFile         *ifile;
  gint             line;
  gint             line_offset;
} HoverState;

static void
hover_state_free (HoverState *state)
{
  g_assert (state != NULL);

  g_clear_object (&state->context);
  g_clear_object (&state->ifile);
  g_clear_object (&state->gi_service);

  g_slice_free (HoverState, state);
}

static void
add_proposal (IdeXmlHoverProvider *self,
              IdeHoverContext     *context,
              IdeGiBase           *base)
{
  g_autoptr(IdeGiDoc) doc = NULL;

  if (base != NULL && (doc = ide_gi_base_get_doc (base)))
    {
      const gchar *text = ide_gi_doc_get_doc (doc);
      GtkWidget *widget = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                                        "expand", TRUE,
                                        "width-request", 500,
                                        "max-content-height", 200,
                                        "propagate-natural-height", TRUE,
                                        "valign", GTK_ALIGN_START,
                                        NULL);
      GtkWidget *label = g_object_new (GTK_TYPE_LABEL,
                                       "label", text,
                                       "valign", GTK_ALIGN_START,
                                       "halign", GTK_ALIGN_START,
                                       "visible", TRUE,
                                       NULL);

      gtk_container_add (GTK_CONTAINER (widget), label);
      ide_hover_context_add_widget (context,
                                    XML_HOVER_PROVIDER_PRIORITY,
                                    _("GI"),
                                    widget);
    }
}

static gboolean
property_walker_func (IdeGiBase   *object,
                      const gchar *name,
                      gpointer     data)
{
  IdeGiBase **base = (IdeGiBase **)data;
  guint n_props;

  n_props = ide_gi_class_get_n_properties ((IdeGiClass *)object);
  for (guint i = 0; i < n_props; i++)
    {
      g_autoptr(IdeGiProperty) prop = ide_gi_class_get_property ((IdeGiClass *)object, i);
      const gchar *found_name = ide_gi_base_get_name ((IdeGiBase *)prop);

      if (dzl_str_equal0 (name, found_name))
        {
          *base = (IdeGiBase *)g_steal_pointer (&prop);
          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
signal_walker_func (IdeGiBase   *object,
                    const gchar *name,
                    gpointer     data)
{
  IdeGiBase **base = (IdeGiBase **)data;
  guint n_signals;

  n_signals = ide_gi_class_get_n_signals ((IdeGiClass *)object);
  for (guint i = 0; i < n_signals; i++)
    {
      g_autoptr(IdeGiSignal) signal = ide_gi_class_get_signal ((IdeGiClass *)object, i);
      const gchar *found_name = ide_gi_base_get_name ((IdeGiBase *)signal);

      if (dzl_str_equal0 (name, found_name))
        {
          *base = (IdeGiBase *)g_steal_pointer (&signal);
          return TRUE;
        }
    }

  return FALSE;
}

/* Currently used for property and signal in a class */
static IdeGiBase *
lookup_object_deep (IdeGiClass    *klass,
                    const gchar   *name,
                    IdeGiBlobType  type)
{
  IdeGiBase *base = NULL;

  if (type == IDE_GI_BLOB_TYPE_SIGNAL)
    ide_xml_utils_gi_class_walker ((IdeGiBase *)klass, name, signal_walker_func, &base);
  else if (type == IDE_GI_BLOB_TYPE_PROPERTY)
    ide_xml_utils_gi_class_walker ((IdeGiBase *)klass, name, property_walker_func, &base);

  return base;
}

static IdeGiBase *
get_gtype (IdeGiVersion   *version,
           IdeXmlAnalysis *analysis,
           const gchar    *name)
{
  g_autoptr(IdeGiRequire) version_req = NULL;
  g_autoptr(IdeGiRequire) req = NULL;
  IdeGiBase *base;

  g_assert (IDE_IS_GI_VERSION (version));
  g_assert (analysis != NULL);

  if (dzl_str_empty0 (name))
    return NULL;

  req = ide_gi_require_copy (ide_xml_analysis_get_require (analysis));
  version_req = ide_gi_version_get_highest_versions (version);
  ide_gi_require_merge (req, version_req, IDE_GI_REQUIRE_MERGE_STRATEGY_KEEP_SOURCE);

  base = ide_gi_version_lookup_gtype (version, req, name);

  return base;
}

static void
hover_cb (GObject      *object,
          GAsyncResult *result,
          gpointer      user_data)
{
  IdeXmlHoverProvider *self;
  IdeXmlService *service = (IdeXmlService *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(IdeXmlPosition) position = NULL;
  g_autoptr(GError) error = NULL;
  HoverState *state;
  IdeXmlSymbolNode *node;
  IdeXmlSymbolNode *child_node;
  IdeXmlAnalysis *analysis;
  IdeXmlDetail *detail;
  IdeXmlPositionKind kind;
  gboolean is_ui;

  g_assert (IDE_IS_XML_SERVICE (service));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  state = ide_task_get_task_data (task);

  if (!(position = ide_xml_service_get_position_from_cursor_finish (service, result, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (ide_task_return_error_if_cancelled (task))
    return;

  analysis = ide_xml_position_get_analysis (position);
  is_ui = ide_xml_analysis_get_is_ui (analysis);

  node = ide_xml_position_get_node (position);
  child_node = ide_xml_position_get_child_node (position);
  kind = ide_xml_position_get_kind (position);
  detail = ide_xml_position_get_detail (position);

  if (kind == IDE_XML_POSITION_KIND_IN_START_TAG &&
      detail->member == IDE_XML_DETAIL_MEMBER_ATTRIBUTE_VALUE &&
      is_ui)
    {
      IdeGiRepository *repository;
      g_autoptr(IdeGiVersion) version = NULL;

      g_assert (child_node != NULL);

      if ((repository = ide_gi_service_get_repository (state->gi_service)) &&
          (version = ide_gi_repository_get_current_version (repository)))
        {
          const gchar *element_name;
          g_autoptr(IdeGiBase) base = NULL;

          element_name = ide_xml_symbol_node_get_element_name (child_node);
          if (dzl_str_equal0 (element_name, "object") && dzl_str_equal0 (detail->name, "class"))
            {
              if (!dzl_str_empty0 (detail->value) &&
                  (base = get_gtype (version, analysis, detail->value)))
                add_proposal (self, state->context, base);
            }
          else if (dzl_str_equal0 (detail->name, "name"))
            {
              g_autoptr(IdeGiBase) obj = NULL;
              const gchar *gtype_name;
              IdeGiBlobType type;

              if (dzl_str_equal0 (element_name, "property"))
                type = IDE_GI_BLOB_TYPE_PROPERTY;
              else if (dzl_str_equal0 (element_name, "signal"))
                type = IDE_GI_BLOB_TYPE_SIGNAL;
              else
                goto finish;

              gtype_name = ide_xml_symbol_node_get_attribute_value (node, "class");
              if ((base = get_gtype (version, analysis, gtype_name)) &&
                  ide_gi_base_get_object_type (base) == IDE_GI_BLOB_TYPE_CLASS )
                {
                  obj = lookup_object_deep ((IdeGiClass *)base, detail->value, type);
                  add_proposal (self, state->context, (IdeGiBase *)obj);
                }
            }
        }
    }

finish:
  ide_task_return_boolean (task, TRUE);
}

static void
ide_xml_hover_provider_hover_async (IdeHoverProvider    *provider,
                                    IdeHoverContext     *context,
                                    const GtkTextIter   *iter,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  IdeXmlHoverProvider *self = (IdeXmlHoverProvider *)provider;
  g_autoptr(IdeTask) task = NULL;
  IdeBuffer *buffer;
  IdeContext *ide_context;
  IdeXmlService *xml_service;
  IdeGiService *gi_service;
  HoverState *state;

  g_assert (IDE_IS_XML_HOVER_PROVIDER (self));
  g_assert (IDE_IS_HOVER_CONTEXT (context));
  g_assert (iter != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_xml_hover_provider_hover_async);

  if (!(ide_context = ide_object_get_context (IDE_OBJECT (self))) ||
      !(xml_service = ide_context_get_service_typed (ide_context, IDE_TYPE_XML_SERVICE)))
    {
      ide_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED, "Xml service not available");
      return;
    }

  gi_service = ide_context_get_service_typed (ide_context, IDE_TYPE_GI_SERVICE);
  buffer = IDE_BUFFER (gtk_text_iter_get_buffer (iter));
  g_assert (IDE_IS_BUFFER (buffer));

  state = g_slice_new0 (HoverState);
  state->context = g_object_ref (context);
  state->ifile = g_object_ref (ide_buffer_get_file (IDE_BUFFER (buffer)));
  state->gi_service = (gi_service != NULL) ? g_object_ref (gi_service) : NULL;
  state->line = gtk_text_iter_get_line (iter) + 1;
  state->line_offset = gtk_text_iter_get_line_offset (iter) + 1;

  ide_task_set_task_data (task, state, hover_state_free);

  ide_xml_service_get_position_from_cursor_async (xml_service,
                                                  state->ifile,
                                                  IDE_BUFFER (buffer),
                                                  state->line,
                                                  state->line_offset,
                                                  cancellable,
                                                  hover_cb,
                                                  g_steal_pointer (&task));
}

static gboolean
ide_xml_hover_provider_hover_finish (IdeHoverProvider  *provider,
                                     GAsyncResult      *result,
                                     GError           **error)
{
  g_assert (IDE_IS_XML_HOVER_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
hover_provider_iface_init (IdeHoverProviderInterface *iface)
{
  iface->hover_async = ide_xml_hover_provider_hover_async;
  iface->hover_finish = ide_xml_hover_provider_hover_finish;
}

G_DEFINE_TYPE_WITH_CODE (IdeXmlHoverProvider,
                         ide_xml_hover_provider,
                         IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_HOVER_PROVIDER, hover_provider_iface_init))

static void
ide_xml_hover_provider_class_init (IdeXmlHoverProviderClass *klass)
{
}

static void
ide_xml_hover_provider_init (IdeXmlHoverProvider *self)
{
}
