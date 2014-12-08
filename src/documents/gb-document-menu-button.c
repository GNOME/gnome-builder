/* gb-document-menu-button.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "document-menu-button"

#include <glib/gi18n.h>

#include "gb-document-menu-button.h"
#include "gb-glib.h"
#include "gb-log.h"
#include "gb-string.h"

struct _GbDocumentMenuButtonPrivate
{
  /* Objects owned by menu button */
  GbDocumentManager *document_manager;
  GHashTable        *focus_time;

  /* Weak references */
  GbDocument        *selected_document;

  /* Unowned references */
  GBinding          *title_binding;
  GBinding          *modified_binding;

  /* Widgets owned by Template */
  GtkLabel          *label;
  GtkLabel          *modified_label;
  GtkListBox        *list_box;
  GtkPopover        *popover;
  GtkScrolledWindow *scrolled_window;
  GtkSearchEntry    *search_entry;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbDocumentMenuButton,
                            gb_document_menu_button,
                            GTK_TYPE_MENU_BUTTON)

enum {
  PROP_0,
  PROP_DOCUMENT_MANAGER,
  LAST_PROP
};

enum {
  DOCUMENT_SELECTED,
  LAST_SIGNAL
};

static GParamSpec *gParamSpecs [LAST_PROP];
static guint       gSignals [LAST_SIGNAL];

GtkWidget *
gb_document_menu_button_new (void)
{
  return g_object_new (GB_TYPE_DOCUMENT_MENU_BUTTON, NULL);
}

GbDocumentManager *
gb_document_menu_button_get_document_manager (GbDocumentMenuButton *button)
{
  g_return_val_if_fail (GB_IS_DOCUMENT_MENU_BUTTON (button), NULL);

  return button->priv->document_manager;
}

static GtkListBoxRow *
gb_document_menu_button_create_row (GbDocumentMenuButton *button,
                                    GbDocument           *document)
{
  GtkListBoxRow *row;
  GtkLabel *label;
  GtkLabel *modified;
  GtkBox *box;

  g_return_val_if_fail (GB_IS_DOCUMENT_MENU_BUTTON (button), NULL);
  g_return_val_if_fail (GB_IS_DOCUMENT (document), NULL);

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "visible", TRUE,
                      NULL);
  g_object_set_data (G_OBJECT (row), "GB_DOCUMENT", document);

  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_HORIZONTAL,
                      "visible", TRUE,
                      NULL);
  gtk_container_add (GTK_CONTAINER (row), GTK_WIDGET (box));

  label = g_object_new (GTK_TYPE_LABEL,
                        "margin-top", 3,
                        "margin-bottom", 3,
                        "margin-start", 6,
                        "margin-end", 3,
                        "hexpand", FALSE,
                        "visible", TRUE,
                        "valign", GTK_ALIGN_BASELINE,
                        "xalign", 0.0f,
                        NULL);
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (label));

  modified = g_object_new (GTK_TYPE_LABEL,
                           "label", "•",
                           "visible", FALSE,
                           "valign", GTK_ALIGN_BASELINE,
                           "xalign", 0.0f,
                           NULL);
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (modified));

  g_object_bind_property (document, "title", label, "label",
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (document, "modified", modified, "visible",
                          G_BINDING_SYNC_CREATE);

  return row;
}

static void
gb_document_menu_button_update_sensitive (GbDocumentMenuButton *button)
{
  gboolean sensitive = FALSE;
  guint count;

  g_return_if_fail (GB_IS_DOCUMENT_MENU_BUTTON (button));

  if (button->priv->document_manager)
    {
      count = gb_document_manager_get_count (button->priv->document_manager);
      sensitive = (count > 0);
    }

  gtk_widget_set_sensitive (GTK_WIDGET (button), sensitive);
}

static void
gb_document_menu_button_add_document (GbDocumentMenuButton *button,
                                      GbDocument           *document,
                                      GbDocumentManager    *document_manager)
{
  GtkListBoxRow *row;

  ENTRY;

  g_return_if_fail (GB_IS_DOCUMENT_MENU_BUTTON (button));
  g_return_if_fail (GB_IS_DOCUMENT (document));
  g_return_if_fail (GB_IS_DOCUMENT_MANAGER (document_manager));

  row = gb_document_menu_button_create_row (button, document);

  gtk_list_box_insert (button->priv->list_box, GTK_WIDGET (row), -1);

  gb_document_menu_button_update_sensitive (button);

  EXIT;
}

static void
gb_document_menu_button_remove_document (GbDocumentMenuButton *button,
                                         GbDocument           *document,
                                         GbDocumentManager    *document_manager)
{
  GQuark qname;
  GList *list;
  GList *iter;

  g_return_if_fail (GB_IS_DOCUMENT_MENU_BUTTON (button));
  g_return_if_fail (GB_IS_DOCUMENT (document));
  g_return_if_fail (GB_IS_DOCUMENT_MANAGER (document_manager));

  g_hash_table_remove (button->priv->focus_time, document);

  list = gtk_container_get_children (GTK_CONTAINER (button->priv->list_box));
  qname = g_quark_from_static_string ("GB_DOCUMENT");

  for (iter = list; iter; iter = iter->next)
    {
      GtkListBoxRow *row = iter->data;
      GbDocument *item;

      if (!GTK_IS_LIST_BOX_ROW (iter->data))
        continue;

      item = g_object_get_qdata (G_OBJECT (row), qname);

      if (item == document)
        {
          gtk_container_remove (GTK_CONTAINER (button->priv->list_box),
                                GTK_WIDGET (row));
          break;
        }
    }

  g_list_free (list);

  gb_document_menu_button_update_sensitive (button);
}

static void
gb_document_menu_button_connect (GbDocumentMenuButton *button,
                                 GbDocumentManager    *document_manager)
{
  GList *documents;
  GList *iter;

  ENTRY;

  g_return_if_fail (GB_IS_DOCUMENT_MENU_BUTTON (button));
  g_return_if_fail (GB_IS_DOCUMENT_MANAGER (document_manager));

  g_signal_connect_object (document_manager,
                           "document-added",
                           G_CALLBACK (gb_document_menu_button_add_document),
                           button,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (document_manager,
                           "document-removed",
                           G_CALLBACK (gb_document_menu_button_remove_document),
                           button,
                           G_CONNECT_SWAPPED);

  documents = gb_document_manager_get_documents (document_manager);

  for (iter = documents; iter; iter = iter->next)
    {
      GbDocument *document;

      document = GB_DOCUMENT (iter->data);
      gb_document_menu_button_add_document (button, document, document_manager);
    }

  g_list_free (documents);

  EXIT;
}

static void
gb_document_menu_button_disconnect (GbDocumentMenuButton *button,
                                    GbDocumentManager    *document_manager)
{
  g_return_if_fail (GB_IS_DOCUMENT_MENU_BUTTON (button));
  g_return_if_fail (GB_IS_DOCUMENT_MANAGER (document_manager));

  g_signal_handlers_disconnect_by_func (document_manager,
                                        gb_document_menu_button_add_document,
                                        button);

  g_signal_handlers_disconnect_by_func (document_manager,
                                        gb_document_menu_button_remove_document,
                                        button);
}

void
gb_document_menu_button_set_document_manager (GbDocumentMenuButton *button,
                                              GbDocumentManager    *document_manager)
{
  ENTRY;

  g_return_if_fail (GB_IS_DOCUMENT_MENU_BUTTON (button));
  g_return_if_fail (!document_manager || GB_IS_DOCUMENT_MANAGER (document_manager));

  if (document_manager != button->priv->document_manager)
    {
      if (button->priv->document_manager)
        {
          gb_document_menu_button_disconnect (button, document_manager);
          g_clear_object (&button->priv->document_manager);
        }

      if (document_manager)
        {
          button->priv->document_manager = g_object_ref (document_manager);
          gb_document_menu_button_connect (button, document_manager);
        }

      g_object_notify_by_pspec (G_OBJECT (button),
                                gParamSpecs [PROP_DOCUMENT_MANAGER]);
    }

  EXIT;
}

void
gb_document_menu_button_select_document (GbDocumentMenuButton *button,
                                         GbDocument           *document)
{
  GbDocumentMenuButtonPrivate *priv;
  gsize value;

  g_return_if_fail (GB_IS_DOCUMENT_MENU_BUTTON (button));
  g_return_if_fail (!document || GB_IS_DOCUMENT (document));

  priv = button->priv;

  if (priv->title_binding)
    {
      g_binding_unbind (priv->title_binding);
      if (priv->title_binding)
        gb_clear_weak_pointer (&priv->title_binding);
    }

  if (priv->modified_binding)
    {
      g_binding_unbind (priv->modified_binding);
      if (priv->modified_binding)
        gb_clear_weak_pointer (&priv->modified_binding);
    }

  gb_clear_weak_pointer (&priv->selected_document);
  gb_set_weak_pointer (document, &priv->selected_document);

  if (document)
    {
      priv->title_binding =
        g_object_bind_property (document, "title", priv->label, "label",
                                G_BINDING_SYNC_CREATE);
      gb_set_weak_pointer (priv->title_binding, &priv->title_binding);

      priv->modified_binding =
        g_object_bind_property (document, "modified",
                                priv->modified_label, "visible",
                                G_BINDING_SYNC_CREATE);
      gb_set_weak_pointer (priv->title_binding, &priv->modified_binding);

      value = g_get_monotonic_time () / (G_USEC_PER_SEC / 10);
      g_hash_table_replace (priv->focus_time, document,
                            GSIZE_TO_POINTER (value));

      g_signal_emit (button, gSignals [DOCUMENT_SELECTED], 0, document);
    }
  else
    {
      gtk_label_set_label (priv->label, NULL);
      gtk_widget_set_visible (GTK_WIDGET (priv->modified_label), FALSE);
    }

  gb_document_menu_button_update_sensitive (button);

  gtk_list_box_invalidate_sort (priv->list_box);
}

static gboolean
gb_document_menu_button_do_hide (gpointer data)
{
  GbDocumentMenuButton *button = data;

  g_return_val_if_fail (GB_IS_DOCUMENT_MENU_BUTTON (button), FALSE);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), FALSE);
  g_object_unref (button);

  return G_SOURCE_REMOVE;
}

static void
gb_document_menu_button_delayed_hide (GbDocumentMenuButton *button)
{
  g_return_if_fail (GB_IS_DOCUMENT_MENU_BUTTON (button));

  g_timeout_add (200,
                 gb_document_menu_button_do_hide,
                 g_object_ref (button));
}

static void
gb_document_menu_button_row_activated (GbDocumentMenuButton *button,
                                       GtkListBoxRow        *row,
                                       GtkListBox           *list_box)
{
  GbDocument *document;

  g_return_if_fail (GB_IS_DOCUMENT_MENU_BUTTON (button));
  g_return_if_fail (GTK_IS_LIST_BOX_ROW (row));
  g_return_if_fail (GTK_IS_LIST_BOX (list_box));

  document = g_object_get_data (G_OBJECT (row), "GB_DOCUMENT");

  if (GB_IS_DOCUMENT (document))
    gb_document_menu_button_select_document (button, document);

  gb_document_menu_button_delayed_hide (button);
}

static void
gb_document_menu_button_header_func (GtkListBoxRow *row,
                                     GtkListBoxRow *before,
                                     gpointer       user_data)
{
  g_return_if_fail (GTK_IS_LIST_BOX_ROW (row));

  if (before)
    {
      GtkWidget *widget;

      widget = g_object_new (GTK_TYPE_SEPARATOR,
                             "orientation", GTK_ORIENTATION_HORIZONTAL,
                             "visible", TRUE,
                             NULL);
      gtk_list_box_row_set_header (row, widget);
    }
}

static gint
gb_document_menu_button_sort_func (GtkListBoxRow *row1,
                                   GtkListBoxRow *row2,
                                   gpointer       user_data)
{

  GbDocumentMenuButton *button = user_data;
  GbDocument *doc1 = g_object_get_data (G_OBJECT (row1), "GB_DOCUMENT");
  GbDocument *doc2 = g_object_get_data (G_OBJECT (row2), "GB_DOCUMENT");
  gpointer value1 = g_hash_table_lookup (button->priv->focus_time, doc1);
  gpointer value2 = g_hash_table_lookup (button->priv->focus_time, doc2);

  if (!value1 && !value2)
    return g_strcmp0 (gb_document_get_title (doc1),
                      gb_document_get_title (doc2));

  if (!value1)
    return 1;

  if (!value2)
    return -1;

  return GPOINTER_TO_INT (value2 - value1);
}

static gboolean
gb_document_menu_button_filter_func (GtkListBoxRow *row,
                                     gpointer       user_data)
{
  GbDocumentMenuButton *button = user_data;
  GbDocument *document;
  const gchar *title;
  const gchar *str;

  g_return_val_if_fail (GB_IS_DOCUMENT_MENU_BUTTON (button), FALSE);

  str = gtk_entry_get_text (GTK_ENTRY (button->priv->search_entry));
  if (gb_str_empty0 (str))
    return TRUE;

  document = g_object_get_data (G_OBJECT (row), "GB_DOCUMENT");
  title = gb_document_get_title (document);

  /*
   * TODO: Replace this with a proper fuzzy search with scoring and
   *       highlighting. Score should include the distance between
   *       matched characters.
   */
  for (; title && *str; str = g_utf8_next_char (str))
    {
      gunichar c = g_utf8_get_char (str);

      if (g_unichar_isspace (c))
        continue;

      title = g_utf8_strchr (title, -1, c);

      if (title)
        title = g_utf8_next_char (title);
    }

  return title && ((str == NULL) || (*str == '\0'));
}

static void
gb_document_menu_button_search_changed (GbDocumentMenuButton *button,
                                        GtkEditable          *editable)
{
  GtkListBoxRow *row;

  g_return_if_fail (GB_IS_DOCUMENT_MENU_BUTTON (button));

  gtk_list_box_invalidate_filter (button->priv->list_box);

  /*
   * WORKAROUND:
   *
   * Using a y of 1 since 0 returns NULL. This is a bug in GtkListBoxRow
   * and has been filed upstream.
   */
  row = gtk_list_box_get_row_at_y (button->priv->list_box, 1);
  if (row)
    gtk_list_box_select_row (button->priv->list_box, row);
}

static void
gb_document_menu_button_search_activate (GbDocumentMenuButton *button,
                                         GtkEditable          *editable)
{
  GtkListBoxRow *row;

  ENTRY;

  g_return_if_fail (GB_IS_DOCUMENT_MENU_BUTTON (button));

  row = gtk_list_box_get_row_at_y (button->priv->list_box, 1);

  if (row)
    {
      GbDocument *document;

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), FALSE);

      document = g_object_get_data (G_OBJECT (row), "GB_DOCUMENT");
      gb_document_menu_button_select_document (button, document);
    }

  EXIT;
}

static void
gb_document_menu_button_clicked (GtkButton *button)
{
  GbDocumentMenuButton *self = (GbDocumentMenuButton *)button;

  g_return_if_fail (GB_IS_DOCUMENT_MENU_BUTTON (button));

  gtk_entry_set_text (GTK_ENTRY (self->priv->search_entry), "");

  GTK_BUTTON_CLASS (gb_document_menu_button_parent_class)->clicked (button);
}

void
gb_document_menu_button_focus_search (GbDocumentMenuButton *button)
{
  g_return_if_fail (GB_IS_DOCUMENT_MENU_BUTTON (button));

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
  gtk_widget_grab_focus (GTK_WIDGET (button->priv->search_entry));
}

static void
gb_document_menu_button_constructed (GObject *object)
{
  GbDocumentMenuButton *button = (GbDocumentMenuButton *)object;

  G_OBJECT_CLASS (gb_document_menu_button_parent_class)->constructed (object);

  g_signal_connect_object (button->priv->list_box,
                           "row-activated",
                           G_CALLBACK (gb_document_menu_button_row_activated),
                           button,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (button->priv->search_entry,
                           "activate",
                           G_CALLBACK (gb_document_menu_button_search_activate),
                           button,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (button->priv->search_entry,
                           "changed",
                           G_CALLBACK (gb_document_menu_button_search_changed),
                           button,
                           G_CONNECT_SWAPPED);

  gtk_list_box_set_header_func (button->priv->list_box,
                                gb_document_menu_button_header_func,
                                button,
                                NULL);

  gtk_list_box_set_sort_func (button->priv->list_box,
                              gb_document_menu_button_sort_func,
                              button,
                              NULL);

  gtk_list_box_set_filter_func (button->priv->list_box,
                                gb_document_menu_button_filter_func,
                                button,
                                NULL);
}

static void
gb_document_menu_button_finalize (GObject *object)
{
  GbDocumentMenuButtonPrivate *priv = GB_DOCUMENT_MENU_BUTTON (object)->priv;

  g_clear_object (&priv->document_manager);
  g_clear_pointer (&priv->focus_time, g_hash_table_unref);

  G_OBJECT_CLASS (gb_document_menu_button_parent_class)->finalize (object);
}

static void
gb_document_menu_button_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GbDocumentMenuButton *self = GB_DOCUMENT_MENU_BUTTON (object);

  switch (prop_id)
    {
    case PROP_DOCUMENT_MANAGER:
      g_value_set_object (value,
                          gb_document_menu_button_get_document_manager (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_document_menu_button_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GbDocumentMenuButton *self = GB_DOCUMENT_MENU_BUTTON (object);

  switch (prop_id)
    {
    case PROP_DOCUMENT_MANAGER:
      gb_document_menu_button_set_document_manager (self,
                                                    g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_document_menu_button_class_init (GbDocumentMenuButtonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkButtonClass *button_class = GTK_BUTTON_CLASS (klass);

  object_class->constructed = gb_document_menu_button_constructed;
  object_class->finalize = gb_document_menu_button_finalize;
  object_class->get_property = gb_document_menu_button_get_property;
  object_class->set_property = gb_document_menu_button_set_property;

  button_class->clicked = gb_document_menu_button_clicked;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/builder/ui/gb-document-menu-button.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GbDocumentMenuButton, label);
  gtk_widget_class_bind_template_child_private (widget_class, GbDocumentMenuButton, list_box);
  gtk_widget_class_bind_template_child_private (widget_class, GbDocumentMenuButton, modified_label);
  gtk_widget_class_bind_template_child_private (widget_class, GbDocumentMenuButton, popover);
  gtk_widget_class_bind_template_child_private (widget_class, GbDocumentMenuButton, scrolled_window);
  gtk_widget_class_bind_template_child_private (widget_class, GbDocumentMenuButton, search_entry);

  gParamSpecs [PROP_DOCUMENT_MANAGER] =
    g_param_spec_object ("document-manager",
                         _("Document Manager"),
                         _("The document manager for the button."),
                         GB_TYPE_DOCUMENT_MANAGER,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_DOCUMENT_MANAGER,
                                   gParamSpecs [PROP_DOCUMENT_MANAGER]);

  gSignals [DOCUMENT_SELECTED] =
    g_signal_new ("document-selected",
                  GB_TYPE_DOCUMENT_MENU_BUTTON,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GbDocumentMenuButtonClass,
                                   document_selected),
                  NULL,
                  NULL,
                  NULL,
                  G_TYPE_NONE,
                  1,
                  GB_TYPE_DOCUMENT);
}

static void
gb_document_menu_button_init (GbDocumentMenuButton *self)
{
  self->priv = gb_document_menu_button_get_instance_private (self);

  self->priv->focus_time = g_hash_table_new (g_direct_hash, g_direct_equal);

  gtk_widget_init_template (GTK_WIDGET (self));

  gb_document_menu_button_update_sensitive (self);
}
