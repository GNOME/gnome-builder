/* ide-marked-view.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-marked-view"

#include "config.h"

#include <glib/gi18n.h>

#ifdef HAVE_WEBKIT
# include <webkit2/webkit2.h>
#endif

#include <cmark.h>

#include "ide-marked-view.h"

G_DEFINE_AUTOPTR_CLEANUP_FUNC (cmark_node, cmark_node_free);
G_DEFINE_AUTOPTR_CLEANUP_FUNC (cmark_iter, cmark_iter_free);

/* Keeps track of a markdown list we are currently rendering in. */
struct list_context {
  cmark_list_type list_type;
  guint next_elem_number;
};

/**
 * node_is_leaf:
 * @node: (transfer none): The markdown node that will be checked
 *
 * Check whether the provided markdown node is a leaf node
 */
static gboolean
node_is_leaf(cmark_node *node)
{
  g_assert (node != NULL);

  return cmark_node_first_child(node) == NULL;
}

/**
 * render_node:
 * @out: (transfer none):        The #GString that the markdown is renderer into
 * @list_stack: (transfer none): A stack used to track all lists currently rendered into, must
 *                               be empty for the first #render_node call
 * @node: (transfer none):       The node that will be rendererd
 * @ev_type:                     The event that occurred when iterating to the provided none.
 *                               Either CMARK_EVENT_ENTER or CMARK_EVENT_EXIT
 *
 * Returns: FALSE if parsing failed somehow, otherwise TRUE
 *
 * Render a single markdown node
 */
static gboolean
render_node(GString          *out,
            GQueue           *list_stack,
            cmark_node       *node,
            cmark_event_type  ev_type)
{
  g_autofree char *literal_escaped = NULL;
  gboolean entering;

  g_assert (out != NULL);
  g_assert (list_stack != NULL);
  g_assert (node != NULL);

  entering = (ev_type == CMARK_EVENT_ENTER);

  switch (cmark_node_get_type (node))
    {
    case CMARK_NODE_NONE:
      return FALSE;

    case CMARK_NODE_DOCUMENT:
      break;

    /* Leaf nodes, these will never have an exit event. */
    case CMARK_NODE_THEMATIC_BREAK:
    case CMARK_NODE_LINEBREAK:
      g_string_append (out, "\n");
      break;

    case CMARK_NODE_SOFTBREAK:
      g_string_append (out, " ");
      break;

    case CMARK_NODE_CODE_BLOCK:
    case CMARK_NODE_CODE:
      literal_escaped = g_markup_escape_text (cmark_node_get_literal (node), -1);
      g_string_append (out, "<tt>");
      g_string_append (out, literal_escaped);
      g_string_append (out, "</tt>");
      break;

    case CMARK_NODE_TEXT:
      literal_escaped = g_markup_escape_text (cmark_node_get_literal (node), -1);
      g_string_append (out, literal_escaped);
      break;

    /* Normal nodes, these have exit events if they are not leaf nodes */
    case CMARK_NODE_EMPH:
      if (entering)
        {
          const char *literal = cmark_node_get_literal (node);

          g_string_append (out, "<i>");

          if (literal)
            {
              literal_escaped = g_markup_escape_text (literal, -1);
              g_string_append (out, literal_escaped);
            }
        }
      if (!entering || node_is_leaf (node))
        {
          g_string_append (out, "</i>");
        }
      break;

    case CMARK_NODE_STRONG:
      if (entering)
        {
          literal_escaped = g_markup_escape_text (cmark_node_get_literal (node), -1);
          g_string_append (out, "<b>");
          g_string_append (out, literal_escaped);
        }
      if (!entering || node_is_leaf (node))
        {
          g_string_append (out, "</b>");
        }
      break;

    case CMARK_NODE_LINK:
      if (entering)
        {
          g_string_append_printf (out,
                                  "<a href=\"%s\">",
                                  cmark_node_get_url (node)
                                 );
          g_string_append (out, cmark_node_get_title (node));
        }
      if (!entering || node_is_leaf (node))
          g_string_append (out, "</a>");
      break;

    case CMARK_NODE_HEADING:
      if (entering)
        {
          const gchar *level;

          switch (cmark_node_get_heading_level (node))
            {
            case 1:
              level = "14pt";
              break;

            case 2:
              level = "13pt";
              break;

            case 3:
              level = "12pt";
              break;

            case 4:
              level = "11pt";
              break;

            case 5:
              level = "10pt";
              break;

            case 6:
              level = "9pt";
              break;

            default:
              g_return_val_if_reached(FALSE);
            }

          g_string_append_printf (out, "<span size=\"%s\">", level);
        }
      if (!entering || node_is_leaf (node))
        {
          g_string_append (out, "</span>\n");
        }
      break;

    case CMARK_NODE_PARAGRAPH:
      if (!entering)
        {
          g_string_append (out, "\n");

          /* When not in a list, append another newline to create vertical space
           * between paragraphs.
           */
          if (g_queue_is_empty (list_stack))
            g_string_append (out, "\n");
        }
      break;

    case CMARK_NODE_LIST:
      if (entering)
        {
          g_autofree struct list_context *list = NULL;

          list = g_new0 (struct list_context, 1);
          list->list_type = cmark_node_get_list_type (node);
          list->next_elem_number = cmark_node_get_list_start (node);

          g_return_val_if_fail (list->list_type != CMARK_NO_LIST, FALSE);

          g_queue_push_tail (list_stack, g_steal_pointer (&list));
        }
      else
        {
          g_free (g_queue_pop_tail (list_stack));

          /* If this was the outermost list, add a newline to create vertical spacing. */
          if (g_queue_is_empty (list_stack))
            g_string_append (out, "\n");
        }
      break;

    case CMARK_NODE_ITEM:
      if (entering)
        {
          struct list_context *list;

          list = g_queue_peek_tail (list_stack);

          g_return_val_if_fail (list != NULL, FALSE);

          /* Indent sublists by four spaces per level */
          for (gint i = 0; i < g_queue_get_length (list_stack) - 1; i++)
            g_string_append (out, "    ");

          if (list->list_type == CMARK_ORDERED_LIST)
            {
              g_string_append_printf (out, "%u. ", list->next_elem_number);
              list->next_elem_number += 1;
            }
          else
            {
              g_string_append (out, "• ");
            }
        }
      break;

    /* Not properly implemented (yet), falls back to default implementation */
    case CMARK_NODE_BLOCK_QUOTE:
    case CMARK_NODE_HTML_BLOCK:
    case CMARK_NODE_CUSTOM_BLOCK:
    case CMARK_NODE_HTML_INLINE:
    case CMARK_NODE_CUSTOM_INLINE:
    case CMARK_NODE_IMAGE:
    default:
      if (entering)
        {
          const gchar *literal;
          literal = cmark_node_get_literal (node);

          if (literal != NULL)
            {
              literal_escaped = g_markup_escape_text (literal, -1);
              g_string_append (out, literal_escaped);
            }
        }
      break;
    }

  return TRUE;
}

/**
 * parse_markdown:
 * @markdown: (transfer none): The markdown that will be parsed to pango markup
 * @len: The length of the markdown in bytes, or -1 if the size is not known
 *
 * Parse the provided document and returns it converted to pango markup for use in a GtkLabel.
 * This will also render links as html <a> tags so GtkLabel can make them clickable.
 *
 * Returns: (transfer full) (nullable): The parsed document as pango markup, or %NULL on parsing errors
 */
static gchar *
parse_markdown (const gchar *markdown,
                gssize       len)
{
  g_autoptr(GString)     result = NULL;
  g_autoqueue(GQueue)    list_stack = NULL;
  g_autoptr(cmark_node)  root_node = NULL;
  cmark_node            *current_node;
  g_autoptr(cmark_iter)  iter = NULL;
  cmark_event_type       ev_type;

  IDE_ENTRY;

  g_assert (markdown != NULL);

  result = g_string_new (NULL);
  list_stack = g_queue_new();

  if (len < 0)
    len = strlen (markdown);

  root_node = cmark_parse_document (markdown, len, 0);

  iter = cmark_iter_new (root_node);

  while ((ev_type = cmark_iter_next (iter)) != CMARK_EVENT_DONE)
    {
      g_return_val_if_fail (ev_type == CMARK_EVENT_ENTER || ev_type == CMARK_EVENT_EXIT, NULL);

      current_node = cmark_iter_get_node (iter);
      g_return_val_if_fail (render_node (result, list_stack, current_node, ev_type), NULL);
    }

  IDE_RETURN (g_string_free (g_steal_pointer (&result), FALSE));
}

struct _IdeMarkedView
{
  GtkBin parent_instance;
};

G_DEFINE_FINAL_TYPE (IdeMarkedView, ide_marked_view, GTK_TYPE_BIN)

static void
ide_marked_view_class_init (IdeMarkedViewClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_css_name (widget_class, "markedview");
}

static void
ide_marked_view_init (IdeMarkedView *self)
{
}

GtkWidget *
ide_marked_view_new (IdeMarkedContent *content)
{
  const gchar *markup;
  gsize markup_len;
  GtkWidget *child = NULL;
  IdeMarkedView *self;
  IdeMarkedKind kind;

  g_return_val_if_fail (content != NULL, NULL);

  self = g_object_new (IDE_TYPE_MARKED_VIEW, NULL);
  kind = ide_marked_content_get_kind (content);
  markup = ide_marked_content_as_string (content, &markup_len);

  switch (kind)
    {
    default:
    case IDE_MARKED_KIND_PLAINTEXT:
    case IDE_MARKED_KIND_PANGO:
      {
        g_autofree char *markup_nul_terminated = g_strndup (markup, markup_len);
        child = g_object_new (GTK_TYPE_LABEL,
                              "max-width-chars", 80,
                              "selectable", TRUE,
                              "wrap", TRUE,
                              "xalign", 0.0f,
                              "visible", TRUE,
                              "use-markup", kind == IDE_MARKED_KIND_PANGO,
                              "label", markup_nul_terminated,
                              NULL);
        break;
      }
    case IDE_MARKED_KIND_HTML:
#ifdef HAVE_WEBKIT
      child = g_object_new (WEBKIT_TYPE_WEB_VIEW,
                            "visible", TRUE,
                            NULL);
      webkit_web_view_load_html (WEBKIT_WEB_VIEW (child), markup, NULL);
#else
      child = g_object_new (GTK_TYPE_LABEL,
                            "label", _("Cannot load HTML. Missing WebKit support."),
                            "visible", TRUE,
                            NULL);
#endif
      break;

    case IDE_MARKED_KIND_MARKDOWN:
      {
        g_autofree gchar *parsed = NULL;

        parsed = parse_markdown (markup, markup_len);

        if (parsed != NULL)
          child = g_object_new (GTK_TYPE_LABEL,
                                "max-width-chars", 80,
                                "selectable", TRUE,
                                "wrap", TRUE,
                                "xalign", 0.0f,
                                "visible", TRUE,
                                "use-markup", TRUE,
                                "label", parsed,
                                NULL);
      }
      break;
    }

  if (child != NULL)
    gtk_container_add (GTK_CONTAINER (self), child);

  return GTK_WIDGET (self);
}
