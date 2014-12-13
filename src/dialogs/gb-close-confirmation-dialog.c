/*
 * gb-close-confirmation-dialog.c
 * This file is part of gb
 *
 * Copyright (C) 2004-2005 GNOME Foundation
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gb-close-confirmation-dialog.h"
#include "gb-document-private.h"

#include <glib/gi18n.h>

/* Properties */
enum
{
	PROP_0,
	PROP_UNSAVED_DOCUMENTS
};

/* Mode */
enum
{
	SINGLE_DOC_MODE,
	MULTIPLE_DOCS_MODE
};

#define GET_MODE(priv) (((priv->unsaved_documents != NULL) && \
			 (priv->unsaved_documents->next == NULL)) ? \
			  SINGLE_DOC_MODE : MULTIPLE_DOCS_MODE)

#define GB_SAVE_DOCUMENT_KEY "gb-save-document"

struct _GbCloseConfirmationDialogPrivate
{
	GList       *unsaved_documents;
	GList       *selected_documents;
	GtkWidget   *list_box;
	gboolean     disable_save_to_disk;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbCloseConfirmationDialog, gb_close_confirmation_dialog, GTK_TYPE_DIALOG)

static void 	 set_unsaved_document 		(GbCloseConfirmationDialog *dlg,
						 const GList                  *list);

static GList *
get_selected_docs (GtkWidget *list_box)
{
	GList *rows;
	GList *l;
	GList *ret = NULL;

	rows = gtk_container_get_children (GTK_CONTAINER (list_box));
	for (l = rows; l != NULL; l = l->next)
	{
		GtkWidget *row = l->data;
		GtkWidget *check_button;

		check_button = gtk_bin_get_child (GTK_BIN (row));
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check_button)))
		{
			GbDocument *doc;

			doc = g_object_get_data (G_OBJECT (row), GB_SAVE_DOCUMENT_KEY);
			g_return_val_if_fail (doc != NULL, NULL);

			ret = g_list_prepend (ret, doc);
		}
	}

	g_list_free (rows);

	return g_list_reverse (ret);
}

/*  Since we connect in the costructor we are sure this handler will be called
 *  before the user ones
 */
static void
response_cb (GbCloseConfirmationDialog *dlg,
             gint                          response_id,
             gpointer                      data)
{
	GbCloseConfirmationDialogPrivate *priv;

	g_return_if_fail (GB_IS_CLOSE_CONFIRMATION_DIALOG (dlg));

	priv = dlg->priv;

	if (priv->selected_documents != NULL)
	{
		g_list_free (priv->selected_documents);
		priv->selected_documents = NULL;
	}

	if (response_id == GTK_RESPONSE_YES)
	{
		if (GET_MODE (priv) == SINGLE_DOC_MODE)
		{
			priv->selected_documents = g_list_copy (priv->unsaved_documents);
		}
		else
		{
			priv->selected_documents = get_selected_docs (priv->list_box);
		}
	}
}

static void
gb_close_confirmation_dialog_init (GbCloseConfirmationDialog *dlg)
{
	AtkObject *atk_obj;
	GtkWidget *action_area;

	dlg->priv = gb_close_confirmation_dialog_get_instance_private (dlg);

	gtk_box_set_spacing (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dlg))),
			     14);
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dlg), TRUE);

	gtk_window_set_title (GTK_WINDOW (dlg), "");

	gtk_window_set_modal (GTK_WINDOW (dlg), TRUE);
	gtk_window_set_destroy_with_parent (GTK_WINDOW (dlg), TRUE);

	atk_obj = gtk_widget_get_accessible (GTK_WIDGET (dlg));
	atk_object_set_role (atk_obj, ATK_ROLE_ALERT);
	atk_object_set_name (atk_obj, _("Question"));

	action_area = gtk_dialog_get_action_area (GTK_DIALOG (dlg));
	gtk_button_box_set_layout (GTK_BUTTON_BOX (action_area), GTK_BUTTONBOX_EXPAND);
	gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (dlg)),
	                                                           GTK_STYLE_CLASS_MESSAGE_DIALOG);

	g_signal_connect (dlg,
			  "response",
			  G_CALLBACK (response_cb),
			  NULL);
}

static void
gb_close_confirmation_dialog_finalize (GObject *object)
{
	GbCloseConfirmationDialogPrivate *priv;

	priv = GB_CLOSE_CONFIRMATION_DIALOG (object)->priv;

	g_list_free (priv->unsaved_documents);
	g_list_free (priv->selected_documents);

	/* Call the parent's destructor */
	G_OBJECT_CLASS (gb_close_confirmation_dialog_parent_class)->finalize (object);
}

static void
gb_close_confirmation_dialog_set_property (GObject      *object,
					      guint         prop_id,
					      const GValue *value,
					      GParamSpec   *pspec)
{
	GbCloseConfirmationDialog *dlg;

	dlg = GB_CLOSE_CONFIRMATION_DIALOG (object);

	switch (prop_id)
	{
		case PROP_UNSAVED_DOCUMENTS:
			set_unsaved_document (dlg, g_value_get_pointer (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gb_close_confirmation_dialog_get_property (GObject    *object,
					      guint       prop_id,
					      GValue     *value,
					      GParamSpec *pspec)
{
	GbCloseConfirmationDialogPrivate *priv;

	priv = GB_CLOSE_CONFIRMATION_DIALOG (object)->priv;

	switch (prop_id)
	{
		case PROP_UNSAVED_DOCUMENTS:
			g_value_set_pointer (value, priv->unsaved_documents);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gb_close_confirmation_dialog_class_init (GbCloseConfirmationDialogClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->set_property = gb_close_confirmation_dialog_set_property;
	gobject_class->get_property = gb_close_confirmation_dialog_get_property;
	gobject_class->finalize = gb_close_confirmation_dialog_finalize;

	g_object_class_install_property (gobject_class,
					 PROP_UNSAVED_DOCUMENTS,
					 g_param_spec_pointer ("unsaved_documents",
						 	       "Unsaved Documents",
							       "List of Unsaved Documents",
							       (G_PARAM_READWRITE |
							        G_PARAM_CONSTRUCT_ONLY)));
}

GList *
gb_close_confirmation_dialog_get_selected_documents (GbCloseConfirmationDialog *dlg)
{
	g_return_val_if_fail (GB_IS_CLOSE_CONFIRMATION_DIALOG (dlg), NULL);

	return g_list_copy (dlg->priv->selected_documents);
}

GtkWidget *
gb_close_confirmation_dialog_new (GtkWindow *parent,
                                  GList     *unsaved_documents)
{
	GtkWidget *dlg;
	gboolean use_header;

	g_return_val_if_fail (unsaved_documents != NULL, NULL);

	dlg = GTK_WIDGET (g_object_new (GB_TYPE_CLOSE_CONFIRMATION_DIALOG,
	                                "use-header-bar", FALSE,
	                                "unsaved_documents", unsaved_documents,
	                                NULL));

	/* As GtkMessageDialog we look at the setting to check
	 * whether to set a CSD header, but we actually force the
	 * buttons at the bottom
	 */
	g_object_get (gtk_settings_get_default (),
	              "gtk-dialogs-use-header", &use_header,
	              NULL);

	if (use_header)
	{
		GtkWidget *box;
		GtkWidget *label;

		box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_widget_show (box);
		gtk_widget_set_size_request (box, -1, 16);
		label = gtk_label_new ("");
		gtk_widget_set_margin_top (label, 6);
		gtk_widget_set_margin_bottom (label, 6);
		gtk_style_context_add_class (gtk_widget_get_style_context (label), "title");
		gtk_box_set_center_widget (GTK_BOX (box), label);
		gtk_window_set_titlebar (GTK_WINDOW (dlg), box);
	}

	if (parent != NULL)
	{
		gtk_window_set_transient_for (GTK_WINDOW (dlg), parent);
	}

	return dlg;
}

GtkWidget *
gb_close_confirmation_dialog_new_single (GtkWindow     *parent,
					    GbDocument *doc)
{
	GtkWidget *dlg;
	GList *unsaved_documents;
	g_return_val_if_fail (doc != NULL, NULL);

	unsaved_documents = g_list_prepend (NULL, doc);

	dlg = gb_close_confirmation_dialog_new (parent,
						   unsaved_documents);

	g_list_free (unsaved_documents);

	return dlg;
}

static void
add_buttons (GbCloseConfirmationDialog *dlg)
{
	gboolean save_as = FALSE;

	gtk_dialog_add_buttons (GTK_DIALOG (dlg),
	                        _("Close _without Saving"), GTK_RESPONSE_NO,
	                        _("_Cancel"), GTK_RESPONSE_CANCEL,
	                        NULL);


	if (GET_MODE (dlg->priv) == SINGLE_DOC_MODE)
	{
		GbDocument *doc;

		doc = GB_DOCUMENT (dlg->priv->unsaved_documents->data);

		if (gb_document_get_read_only (doc) ||
		    gb_document_is_untitled (doc))
		{
			save_as = TRUE;
		}
	}

	gtk_dialog_add_button (GTK_DIALOG (dlg),
			       save_as ? _("_Save As…") : _("_Save"),
			       GTK_RESPONSE_YES);
	gtk_dialog_set_default_response	(GTK_DIALOG (dlg),
					 GTK_RESPONSE_YES);
}

static gchar *
get_text_secondary_label (GbDocument *doc)
{
	glong  seconds;
	gchar *secondary_msg;

	seconds = MAX (1, _gb_document_get_seconds_since_last_save_or_load (doc));

	if (seconds < 55)
	{
		secondary_msg = g_strdup_printf (
					ngettext ("If you don't save, changes from the last %ld second "
					    	  "will be permanently lost.",
						  "If you don't save, changes from the last %ld seconds "
					    	  "will be permanently lost.",
						  seconds),
					seconds);
	}
	else if (seconds < 75) /* 55 <= seconds < 75 */
	{
		secondary_msg = g_strdup (_("If you don't save, changes from the last minute "
					    "will be permanently lost."));
	}
	else if (seconds < 110) /* 75 <= seconds < 110 */
	{
		secondary_msg = g_strdup_printf (
					ngettext ("If you don't save, changes from the last minute and %ld "
						  "second will be permanently lost.",
						  "If you don't save, changes from the last minute and %ld "
						  "seconds will be permanently lost.",
						  seconds - 60 ),
					seconds - 60);
	}
	else if (seconds < 3600)
	{
		secondary_msg = g_strdup_printf (
					ngettext ("If you don't save, changes from the last %ld minute "
					    	  "will be permanently lost.",
						  "If you don't save, changes from the last %ld minutes "
					    	  "will be permanently lost.",
						  seconds / 60),
					seconds / 60);
	}
	else if (seconds < 7200)
	{
		gint minutes;
		seconds -= 3600;

		minutes = seconds / 60;
		if (minutes < 5)
		{
			secondary_msg = g_strdup (_("If you don't save, changes from the last hour "
						    "will be permanently lost."));
		}
		else
		{
			secondary_msg = g_strdup_printf (
					ngettext ("If you don't save, changes from the last hour and %d "
						  "minute will be permanently lost.",
						  "If you don't save, changes from the last hour and %d "
						  "minutes will be permanently lost.",
						  minutes),
					minutes);
		}
	}
	else
	{
		gint hours;

		hours = seconds / 3600;

		secondary_msg = g_strdup_printf (
					ngettext ("If you don't save, changes from the last %d hour "
					    	  "will be permanently lost.",
						  "If you don't save, changes from the last %d hours "
					    	  "will be permanently lost.",
						  hours),
					hours);
	}

	return secondary_msg;
}

static void
build_single_doc_dialog (GbCloseConfirmationDialog *dlg)
{
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *primary_label;
	GtkWidget *secondary_label;
	GbDocument *doc;
	const gchar *doc_name;
	gchar *str;
	gchar *markup_str;

	gtk_window_set_resizable (GTK_WINDOW (dlg), FALSE);

	g_return_if_fail (dlg->priv->unsaved_documents->data != NULL);
	doc = GB_DOCUMENT (dlg->priv->unsaved_documents->data);

	add_buttons (dlg);

	/* Primary label */
	primary_label = gtk_label_new (NULL);
	gtk_label_set_line_wrap (GTK_LABEL (primary_label), TRUE);
	gtk_label_set_use_markup (GTK_LABEL (primary_label), TRUE);
	gtk_widget_set_halign (primary_label, GTK_ALIGN_CENTER);
	gtk_widget_set_valign (primary_label, GTK_ALIGN_START);
	gtk_label_set_selectable (GTK_LABEL (primary_label), TRUE);
	gtk_widget_set_can_focus (primary_label, FALSE);
	gtk_label_set_max_width_chars (GTK_LABEL (primary_label), 72);

	doc_name = gb_document_get_title (doc);

	if (dlg->priv->disable_save_to_disk)
	{
		str = g_markup_printf_escaped (_("Changes to document “%s” will be permanently lost."),
		                               doc_name);
	}
	else
	{
		str = g_markup_printf_escaped (_("Save changes to document “%s” before closing?"),
		                               doc_name);
	}

	markup_str = g_strconcat ("<span weight=\"bold\" size=\"larger\">", str, "</span>", NULL);
	g_free (str);

	gtk_label_set_markup (GTK_LABEL (primary_label), markup_str);
	g_free (markup_str);

	/* Secondary label */
	if (dlg->priv->disable_save_to_disk)
	{
		str = g_strdup (_("Saving has been disabled by the system administrator."));
	}
	else
	{
		str = get_text_secondary_label (doc);
	}

	secondary_label = gtk_label_new (str);
	g_free (str);
	gtk_label_set_line_wrap (GTK_LABEL (secondary_label), TRUE);
	gtk_widget_set_halign (secondary_label, GTK_ALIGN_CENTER);
	gtk_widget_set_valign (secondary_label, GTK_ALIGN_START);
	gtk_label_set_selectable (GTK_LABEL (secondary_label), TRUE);
	gtk_widget_set_can_focus (secondary_label, FALSE);
	gtk_label_set_max_width_chars (GTK_LABEL (secondary_label), 72);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 10);
	gtk_widget_set_margin_start (hbox, 30);
	gtk_widget_set_margin_end (hbox, 30);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), primary_label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), secondary_label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dlg))),
	                    hbox, FALSE, FALSE, 0);

	gtk_widget_show_all (hbox);
}

static GtkWidget *
create_list_box (GbCloseConfirmationDialogPrivate *priv)
{
	GtkWidget *list_box;
	GList *l;

	list_box = gtk_list_box_new ();

	for (l = priv->unsaved_documents; l != NULL; l = l->next)
	{
		GbDocument *doc = l->data;
		const gchar *name;
		GtkWidget *check_button;
		GtkWidget *row;

		name = gb_document_get_title (doc);
		check_button = gtk_check_button_new_with_label (name);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button), TRUE);
		gtk_widget_set_halign (check_button, GTK_ALIGN_START);

		row = gtk_list_box_row_new ();
		gtk_container_add (GTK_CONTAINER (row), check_button);
		gtk_widget_show_all (row);

		g_object_set_data_full (G_OBJECT (row),
		                        GB_SAVE_DOCUMENT_KEY,
		                        g_object_ref (doc),
		                        (GDestroyNotify) g_object_unref);

		gtk_list_box_insert (GTK_LIST_BOX (list_box), row, -1);
	}

	return list_box;
}

static void
build_multiple_docs_dialog (GbCloseConfirmationDialog *dlg)
{
	GbCloseConfirmationDialogPrivate *priv;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *primary_label;
	GtkWidget *vbox2;
	GtkWidget *select_label;
	GtkWidget *scrolledwindow;
	GtkWidget *secondary_label;
	gchar     *str;
	gchar     *markup_str;

	priv = dlg->priv;

	add_buttons (dlg);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 10);
	gtk_widget_set_margin_start (hbox, 30);
	gtk_widget_set_margin_end (hbox, 30);

	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dlg))),
			    hbox, TRUE, TRUE, 0);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

	/* Primary label */
	primary_label = gtk_label_new (NULL);
	gtk_label_set_line_wrap (GTK_LABEL (primary_label), TRUE);
	gtk_label_set_use_markup (GTK_LABEL (primary_label), TRUE);
	gtk_widget_set_halign (primary_label, GTK_ALIGN_CENTER);
	gtk_widget_set_valign (primary_label, GTK_ALIGN_START);
	gtk_label_set_selectable (GTK_LABEL (primary_label), TRUE);
	gtk_widget_set_can_focus (primary_label, FALSE);
	gtk_label_set_max_width_chars (GTK_LABEL (primary_label), 72);

	if (priv->disable_save_to_disk)
	{
		str = g_strdup_printf (
				ngettext ("Changes to %d document will be permanently lost.",
					  "Changes to %d documents will be permanently lost.",
					  g_list_length (priv->unsaved_documents)),
				g_list_length (priv->unsaved_documents));
	}
	else
	{
		str = g_strdup_printf (
				ngettext ("There is %d document with unsaved changes. "
					  "Save changes before closing?",
					  "There are %d documents with unsaved changes. "
					  "Save changes before closing?",
					  g_list_length (priv->unsaved_documents)),
				g_list_length (priv->unsaved_documents));
	}

	markup_str = g_strconcat ("<span weight=\"bold\" size=\"larger\">", str, "</span>", NULL);
	g_free (str);

	gtk_label_set_markup (GTK_LABEL (primary_label), markup_str);
	g_free (markup_str);
	gtk_box_pack_start (GTK_BOX (vbox), primary_label, FALSE, FALSE, 0);

	vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
	gtk_box_pack_start (GTK_BOX (vbox), vbox2, TRUE, TRUE, 0);

	if (priv->disable_save_to_disk)
	{
		select_label = gtk_label_new_with_mnemonic (_("Docum_ents with unsaved changes:"));
	}
	else
	{
		select_label = gtk_label_new_with_mnemonic (_("S_elect the documents you want to save:"));
	}

	gtk_box_pack_start (GTK_BOX (vbox2), select_label, FALSE, FALSE, 0);
	gtk_label_set_line_wrap (GTK_LABEL (select_label), TRUE);
	gtk_label_set_max_width_chars (GTK_LABEL (select_label), 72);
	gtk_widget_set_halign (select_label, GTK_ALIGN_START);

	scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_box_pack_start (GTK_BOX (vbox2), scrolledwindow, TRUE, TRUE, 0);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow),
					     GTK_SHADOW_IN);
	gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scrolledwindow), 60);

	priv->list_box = create_list_box (priv);
	gtk_container_add (GTK_CONTAINER (scrolledwindow), priv->list_box);

	/* Secondary label */
	if (priv->disable_save_to_disk)
	{
		secondary_label = gtk_label_new (_("Saving has been disabled by the system administrator."));
	}
	else
	{
		secondary_label = gtk_label_new (_("If you don't save, "
						   "all your changes will be permanently lost."));
	}

	gtk_box_pack_start (GTK_BOX (vbox2), secondary_label, FALSE, FALSE, 0);
	gtk_label_set_line_wrap (GTK_LABEL (secondary_label), TRUE);
	gtk_widget_set_halign (secondary_label, GTK_ALIGN_CENTER);
	gtk_widget_set_valign (secondary_label, GTK_ALIGN_START);
	gtk_label_set_selectable (GTK_LABEL (secondary_label), TRUE);
	gtk_label_set_max_width_chars (GTK_LABEL (secondary_label), 72);

	gtk_label_set_mnemonic_widget (GTK_LABEL (select_label), priv->list_box);

	gtk_widget_show_all (hbox);
}

static void
set_unsaved_document (GbCloseConfirmationDialog *dlg,
		      const GList                  *list)
{
	GbCloseConfirmationDialogPrivate *priv;

	g_return_if_fail (list != NULL);

	priv = dlg->priv;
	g_return_if_fail (priv->unsaved_documents == NULL);

	priv->unsaved_documents = g_list_copy ((GList *)list);

	if (GET_MODE (priv) == SINGLE_DOC_MODE)
	{
		build_single_doc_dialog (dlg);
	}
	else
	{
		build_multiple_docs_dialog (dlg);
	}
}

const GList *
gb_close_confirmation_dialog_get_unsaved_documents (GbCloseConfirmationDialog *dlg)
{
	g_return_val_if_fail (GB_IS_CLOSE_CONFIRMATION_DIALOG (dlg), NULL);

	return dlg->priv->unsaved_documents;
}

/* ex:set ts=8 noet: */
