/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#define G_LOG_DOMAIN "markdown"

#include <string.h>
#include <glib.h>

#include "gs-markdown.h"

#if 0
# define DEBUG g_debug
#else
# define DEBUG(...)
#endif

/*******************************************************************************
 *
 * This is a simple Markdown parser.
 * It can output to Pango, HTML or plain text. The following limitations are
 * already known, and properly deliberate:
 *
 * - No code section support
 * - No ordered list support
 * - No blockquote section support
 * - No image support
 * - No links or email support
 * - No backslash escapes support
 * - No HTML escaping support
 * - Auto-escapes certain word patterns, like http://
 *
 * It does support the rest of the standard pretty well, although it's not
 * been run against any conformance tests. The parsing is single pass, with
 * a simple enumerated intepretor mode and a single line back-memory.
 *
 ******************************************************************************/

typedef enum {
	GS_MARKDOWN_MODE_BLANK,
	GS_MARKDOWN_MODE_RULE,
	GS_MARKDOWN_MODE_BULLETT,
	GS_MARKDOWN_MODE_PARA,
	GS_MARKDOWN_MODE_CODE,
	GS_MARKDOWN_MODE_H1,
	GS_MARKDOWN_MODE_H2,
	GS_MARKDOWN_MODE_H3,
	GS_MARKDOWN_MODE_UNKNOWN
} GsMarkdownMode;

typedef struct {
	const gchar *em_start;
	const gchar *em_end;
	const gchar *strong_start;
	const gchar *strong_end;
	const gchar *code_start;
	const gchar *code_end;
	const gchar *codeblock_start;
	const gchar *codeblock_end;
	const gchar *para_start;
	const gchar *para_end;
	const gchar *h1_start;
	const gchar *h1_end;
	const gchar *h2_start;
	const gchar *h2_end;
	const gchar *h3_start;
	const gchar *h3_end;
	const gchar *bullet_start;
	const gchar *bullet_end;
	const gchar *rule;
} GsMarkdownTags;

typedef struct {
	GsMarkdownMode		 mode;
	GsMarkdownTags		 tags;
	GsMarkdownOutputKind	 output;
	gint			 max_lines;
	gint			 line_count;
	gboolean		 smart_quoting;
	gboolean		 escape;
	gboolean		 autocode;
	gboolean		 autolinkify;
	GString			*pending;
	GString			*processed;
} GsMarkdownPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GsMarkdown, gs_markdown, G_TYPE_OBJECT)

/**
 * gs_markdown_to_text_line_is_rule:
 *
 * Horizontal rules are created by placing three or more hyphens, asterisks,
 * or underscores on a line by themselves.
 * You may use spaces between the hyphens or asterisks.
 **/
static gboolean
gs_markdown_to_text_line_is_rule (const gchar *line)
{
	guint i;
	guint len;
	guint count = 0;
	gchar *copy = NULL;
	gboolean ret = FALSE;

	len = strlen (line);
	if (len == 0)
		goto out;

	/* replace non-rule chars with ~ */
	copy = g_strdup (line);
	g_strcanon (copy, "-*_ ", '~');
	for (i = 0; i < len; i++) {
		if (copy[i] == '~')
			goto out;
		if (copy[i] != ' ')
			count++;
	}

	/* if we matched, return true */
	if (count >= 3)
		ret = TRUE;
out:
	g_free (copy);
	return ret;
}

/**
 * gs_markdown_to_text_line_is_bullet:
 **/
static gboolean
gs_markdown_to_text_line_is_bullet (const gchar *line)
{
	return (g_str_has_prefix (line, "- ") ||
		g_str_has_prefix (line, "* ") ||
		g_str_has_prefix (line, "+ ") ||
		g_str_has_prefix (line, " - ") ||
		g_str_has_prefix (line, " * ") ||
		g_str_has_prefix (line, " + "));
}

static gboolean
gs_markdown_to_text_line_is_code (const gchar *line)
{
	return g_str_has_prefix (line, "```");
}

/**
 * gs_markdown_to_text_line_is_header1:
 **/
static gboolean
gs_markdown_to_text_line_is_header1 (const gchar *line)
{
	return g_str_has_prefix (line, "# ");
}

/**
 * gs_markdown_to_text_line_is_header2:
 **/
static gboolean
gs_markdown_to_text_line_is_header2 (const gchar *line)
{
	return g_str_has_prefix (line, "## ");
}

/**
 * gs_markdown_to_text_line_is_header3:
 **/
static gboolean
gs_markdown_to_text_line_is_header3 (const gchar *line)
{
	return g_str_has_prefix (line, "### ");
}

/**
 * gs_markdown_to_text_line_is_header1_type2:
 **/
static gboolean
gs_markdown_to_text_line_is_header1_type2 (const gchar *line)
{
	return g_str_has_prefix (line, "===");
}

/**
 * gs_markdown_to_text_line_is_header2_type2:
 **/
static gboolean
gs_markdown_to_text_line_is_header2_type2 (const gchar *line)
{
	return g_str_has_prefix (line, "---");
}

#if 0
/**
 * gs_markdown_to_text_line_is_code:
 **/
static gboolean
gs_markdown_to_text_line_is_code (const gchar *line)
{
	return (g_str_has_prefix (line, "    ") ||
		g_str_has_prefix (line, "\t"));
}

/**
 * gs_markdown_to_text_line_is_blockquote:
 **/
static gboolean
gs_markdown_to_text_line_is_blockquote (const gchar *line)
{
	return (g_str_has_prefix (line, "> "));
}
#endif

/**
 * gs_markdown_to_text_line_is_blank:
 **/
static gboolean
gs_markdown_to_text_line_is_blank (const gchar *line)
{
	guint i;
	guint len;
	gboolean ret = FALSE;

	/* a line with no characters is blank by definition */
	len = strlen (line);
	if (len == 0) {
		ret = TRUE;
		goto out;
	}

	/* find if there are only space chars */
	for (i = 0; i < len; i++) {
		if (line[i] != ' ' && line[i] != '\t')
			goto out;
	}

	/* if we matched, return true */
	ret = TRUE;
out:
	return ret;
}

/**
 * gs_markdown_replace:
 **/
static gchar *
gs_markdown_replace (const gchar *haystack,
		     const gchar *needle,
		     const gchar *replace)
{
	gchar *new;
	gchar **split;

	split = g_strsplit (haystack, needle, -1);
	new = g_strjoinv (replace, split);
	g_strfreev (split);

	return new;
}

/**
 * gs_markdown_strstr_spaces:
 **/
static gchar *
gs_markdown_strstr_spaces (const gchar *haystack, const gchar *needle)
{
	gchar *found;
	const gchar *haystack_new = haystack;

retry:
	/* don't find if surrounded by spaces */
	found = strstr (haystack_new, needle);
	if (found == NULL)
		return NULL;

	/* start of the string, always valid */
	if (found == haystack)
		return found;

	/* end of the string, always valid */
	if (*(found-1) == ' ' && *(found+1) == ' ') {
		haystack_new = found+1;
		goto retry;
	}
	return found;
}

/**
 * gs_markdown_to_text_line_formatter:
 **/
static gchar *
gs_markdown_to_text_line_formatter (const gchar *line,
				    const gchar *formatter,
				    const gchar *left,
				    const gchar *right)
{
	guint len;
	gchar *str1;
	gchar *str2;
	gchar *start = NULL;
	gchar *middle = NULL;
	gchar *end = NULL;
	gchar *copy = NULL;
	gchar *data = NULL;
	gchar *temp;

	/* needed to know for shifts */
	len = strlen (formatter);
	if (len == 0)
		goto out;

	/* find sections */
	copy = g_strdup (line);
	str1 = gs_markdown_strstr_spaces (copy, formatter);
	if (str1 != NULL) {
		*str1 = '\0';
		str2 = gs_markdown_strstr_spaces (str1+len, formatter);
		if (str2 != NULL) {
			*str2 = '\0';
			middle = str1 + len;
			start = copy;
			end = str2 + len;
		}
	}

	/* if we found, replace and keep looking for the same string */
	if (start != NULL && middle != NULL && end != NULL) {
		temp = g_strdup_printf ("%s%s%s%s%s", start, left, middle, right, end);
		/* recursive */
		data = gs_markdown_to_text_line_formatter (temp, formatter, left, right);
		g_free (temp);
	} else {
		/* not found, keep return as-is */
		data = g_strdup (line);
	}
out:
	g_free (copy);
	return data;
}

/**
 * gs_markdown_to_text_line_format_sections:
 **/
static gchar *
gs_markdown_to_text_line_format_sections (GsMarkdown *self, const gchar *line)
{
	GsMarkdownPrivate *priv = gs_markdown_get_instance_private (self);
	gchar *data = g_strdup (line);
	gchar *temp;

	/* bold1 */
	temp = data;
	data = gs_markdown_to_text_line_formatter (temp, "**",
						   priv->tags.strong_start,
						   priv->tags.strong_end);
	g_free (temp);

	/* bold2 */
	temp = data;
	data = gs_markdown_to_text_line_formatter (temp, "__",
						   priv->tags.strong_start,
						   priv->tags.strong_end);
	g_free (temp);

	/* italic1 */
	temp = data;
	data = gs_markdown_to_text_line_formatter (temp, "*",
						   priv->tags.em_start,
						   priv->tags.em_end);
	g_free (temp);

	/* italic2 */
	temp = data;
	data = gs_markdown_to_text_line_formatter (temp, "_",
						   priv->tags.em_start,
						   priv->tags.em_end);
	g_free (temp);

	/* em-dash */
	temp = data;
	data = gs_markdown_replace (temp, " -- ", " — ");
	g_free (temp);

	/* smart quoting */
	if (priv->smart_quoting) {
		temp = data;
		data = gs_markdown_to_text_line_formatter (temp, "\"", "“", "”");
		g_free (temp);

		temp = data;
		data = gs_markdown_to_text_line_formatter (temp, "'", "‘", "’");
		g_free (temp);
	}

	return data;
}

/**
 * gs_markdown_to_text_line_format:
 **/
static gchar *
gs_markdown_to_text_line_format (GsMarkdown *self, const gchar *line)
{
	GString *string;
	GsMarkdownPrivate *priv = gs_markdown_get_instance_private (self);
	gboolean mode = FALSE;
	gchar **codes = NULL;
	gchar *text;
	guint i;

	/* optimise the trivial case where we don't have any code tags */
	text = strstr (line, "`");
	if (text == NULL) {
		text = gs_markdown_to_text_line_format_sections (self, line);
		goto out;
	}

	/* we want to parse the code sections without formatting */
	codes = g_strsplit (line, "`", -1);
	string = g_string_new ("");
	for (i = 0; codes[i] != NULL; i++) {
		if (!mode) {
			text = gs_markdown_to_text_line_format_sections (self, codes[i]);
			g_string_append (string, text);
			g_free (text);
			mode = TRUE;
		} else {
			/* just append without formatting */
			g_string_append (string, priv->tags.code_start);
			g_string_append (string, codes[i]);
			g_string_append (string, priv->tags.code_end);
			mode = FALSE;
		}
	}
	text = g_string_free (string, FALSE);
out:
	g_strfreev (codes);
	return text;
}

/**
 * gs_markdown_add_pending:
 **/
static gboolean
gs_markdown_add_pending (GsMarkdown *self, const gchar *line)
{
	gchar *copy;
	GsMarkdownPrivate *priv = gs_markdown_get_instance_private (self);

	/* would put us over the limit */
	if (priv->max_lines > 0 && priv->line_count >= priv->max_lines)
		return FALSE;

	copy = g_strdup (line);

	/* strip leading and trailing spaces */
	g_strstrip (copy);

	/* append */
	g_string_append_printf (priv->pending, "%s ", copy);

	g_free (copy);
	return TRUE;
}

/**
 * gs_markdown_add_pending_header:
 **/
static gboolean
gs_markdown_add_pending_header (GsMarkdown *self, const gchar *line)
{
	gchar *copy;
	gboolean ret;

	/* strip trailing # */
	copy = g_strdup (line);
	g_strdelimit (copy, "#", ' ');
	ret = gs_markdown_add_pending (self, copy);
	g_free (copy);
	return ret;
}

/**
 * gs_markdown_count_chars_in_word:
 **/
static guint
gs_markdown_count_chars_in_word (const gchar *text, gchar find)
{
	guint i;
	guint len;
	guint count = 0;

	/* get length */
	len = strlen (text);
	if (len == 0)
		goto out;

	/* find matching chars */
	for (i = 0; i < len; i++) {
		if (text[i] == find)
			count++;
	}
out:
	return count;
}

/**
 * gs_markdown_word_is_code:
 **/
static gboolean
gs_markdown_word_is_code (const gchar *text)
{
	/* already code */
	if (g_str_has_prefix (text, "`"))
		return FALSE;
	if (g_str_has_suffix (text, "`"))
		return FALSE;

	/* paths */
	if (g_str_has_prefix (text, "/"))
		return TRUE;

	/* bugzillas */
	if (g_str_has_prefix (text, "#"))
		return TRUE;

	/* patch files */
	if (g_strrstr (text, ".patch") != NULL)
		return TRUE;
	if (g_strrstr (text, ".diff") != NULL)
		return TRUE;

	/* function names */
	if (g_strrstr (text, "()") != NULL)
		return TRUE;

	/* email addresses */
	if (g_strrstr (text, "@") != NULL)
		return TRUE;

	/* compiler defines */
	if (text[0] != '_' &&
	    gs_markdown_count_chars_in_word (text, '_') > 1)
		return TRUE;

	/* nothing special */
	return FALSE;
}

/**
 * gs_markdown_word_auto_format_code:
 **/
static gchar *
gs_markdown_word_auto_format_code (const gchar *text)
{
	guint i;
	gchar *temp;
	gchar **words;
	gboolean ret = FALSE;

	/* split sentence up with space */
	words = g_strsplit (text, " ", -1);

	/* search each word */
	for (i = 0; words[i] != NULL; i++) {
		if (gs_markdown_word_is_code (words[i])) {
			temp = g_strdup_printf ("`%s`", words[i]);
			g_free (words[i]);
			words[i] = temp;
			ret = TRUE;
		}
	}

	/* no replacements, so just return a copy */
	if (!ret) {
		temp = g_strdup (text);
		goto out;
	}

	/* join the array back into a string */
	temp = g_strjoinv (" ", words);
out:
	g_strfreev (words);
	return temp;
}

/**
 * gs_markdown_word_is_url:
 **/
static gboolean
gs_markdown_word_is_url (const gchar *text)
{
	if (g_str_has_prefix (text, "http://"))
		return TRUE;
	if (g_str_has_prefix (text, "https://"))
		return TRUE;
	if (g_str_has_prefix (text, "ftp://"))
		return TRUE;
	return FALSE;
}

/**
 * gs_markdown_word_auto_format_urls:
 **/
static gchar *
gs_markdown_word_auto_format_urls (const gchar *text)
{
	guint i;
	gchar *temp;
	gchar **words;
	gboolean ret = FALSE;

	/* split sentence up with space */
	words = g_strsplit (text, " ", -1);

	/* search each word */
	for (i = 0; words[i] != NULL; i++) {
		if (gs_markdown_word_is_url (words[i])) {
			temp = g_strdup_printf ("<a href=\"%s\">%s</a>",
						words[i], words[i]);
			g_free (words[i]);
			words[i] = temp;
			ret = TRUE;
		}
	}

	/* no replacements, so just return a copy */
	if (!ret) {
		temp = g_strdup (text);
		goto out;
	}

	/* join the array back into a string */
	temp = g_strjoinv (" ", words);
out:
	g_strfreev (words);
	return temp;
}

/**
 * gs_markdown_flush_pending:
 **/
static void
gs_markdown_flush_pending (GsMarkdown *self)
{
	gchar *copy;
	gchar *temp;
	GsMarkdownPrivate *priv = gs_markdown_get_instance_private (self);

	/* no data yet */
	if (priv->mode == GS_MARKDOWN_MODE_UNKNOWN)
		return;

	/* remove trailing spaces */
	while (g_str_has_suffix (priv->pending->str, " "))
		g_string_set_size (priv->pending, priv->pending->len - 1);

	/* pango requires escaping */
	copy = g_strdup (priv->pending->str);
	if (!priv->escape && priv->output == GS_MARKDOWN_OUTPUT_PANGO) {
		g_strdelimit (copy, "<", '(');
		g_strdelimit (copy, ">", ')');
		g_strdelimit (copy, "&", '+');
	}

	/* check words for code */
	if (priv->autocode &&
	    (priv->mode == GS_MARKDOWN_MODE_PARA ||
	     priv->mode == GS_MARKDOWN_MODE_BULLETT)) {
		temp = gs_markdown_word_auto_format_code (copy);
		g_free (copy);
		copy = temp;
	}

	/* escape */
	if (priv->escape) {
		temp = g_markup_escape_text (copy, -1);
		g_free (copy);
		copy = temp;
	}

	/* check words for URLS */
	if (priv->autolinkify &&
	    priv->output == GS_MARKDOWN_OUTPUT_PANGO &&
	    (priv->mode == GS_MARKDOWN_MODE_PARA ||
	     priv->mode == GS_MARKDOWN_MODE_BULLETT)) {
		temp = gs_markdown_word_auto_format_urls (copy);
		g_free (copy);
		copy = temp;
	}

	/* do formatting */
	temp = gs_markdown_to_text_line_format (self, copy);
	if (priv->mode == GS_MARKDOWN_MODE_BULLETT) {
		g_string_append_printf (priv->processed, "%s%s%s\n",
					priv->tags.bullet_start,
					temp,
					priv->tags.bullet_end);
		priv->line_count++;
	} else if (priv->mode == GS_MARKDOWN_MODE_H1) {
		g_string_append_printf (priv->processed, "%s%s%s\n",
					priv->tags.h1_start,
					temp,
					priv->tags.h1_end);
	} else if (priv->mode == GS_MARKDOWN_MODE_H2) {
		g_string_append_printf (priv->processed, "%s%s%s\n",
					priv->tags.h2_start,
					temp,
					priv->tags.h2_end);
	} else if (priv->mode == GS_MARKDOWN_MODE_H3) {
		g_string_append_printf (priv->processed, "%s%s%s\n",
					priv->tags.h3_start,
					temp,
					priv->tags.h3_end);
	} else if (priv->mode == GS_MARKDOWN_MODE_PARA ||
		   priv->mode == GS_MARKDOWN_MODE_RULE) {
		g_string_append_printf (priv->processed,
                              "%s%s%s\n",
                              priv->tags.para_start,
                              temp,
                              priv->tags.para_end);
		priv->line_count++;
	} else if (priv->mode == GS_MARKDOWN_MODE_CODE) {
		g_string_append_printf (priv->processed, "%s%s%s\n",
					priv->tags.codeblock_start,
					temp,
					priv->tags.codeblock_end);
	}

	DEBUG ("adding '%s'", temp);

	/* clear */
	g_string_truncate (priv->pending, 0);
	g_free (copy);
	g_free (temp);
}

/**
 * gs_markdown_to_text_line_process:
 **/
static gboolean
gs_markdown_to_text_line_process (GsMarkdown *self, const gchar *line)
{
	GsMarkdownPrivate *priv = gs_markdown_get_instance_private (self);
	gboolean ret;

	/* inside code */
	if (priv->mode == GS_MARKDOWN_MODE_CODE) {
		DEBUG ("code: '%s'", line);
		ret = gs_markdown_to_text_line_is_code (line);
		if (ret) {
			gs_markdown_flush_pending (self);
			priv->mode = GS_MARKDOWN_MODE_UNKNOWN;
			goto out;
		}
		if (priv->pending->len) {
			g_string_append_c (priv->pending, '\n');
		}
		g_string_append (priv->pending, line);
		ret = TRUE;
		goto out;
	}

	/* code */
	ret = gs_markdown_to_text_line_is_code (line);
	if (ret) {
		DEBUG ("code: '%s'", line);
		gs_markdown_flush_pending (self);
		priv->mode = GS_MARKDOWN_MODE_CODE;
		goto out;
	}

	/* blank */
	ret = gs_markdown_to_text_line_is_blank (line);
	if (ret) {
		DEBUG ("blank: '%s'", line);
		gs_markdown_flush_pending (self);
		/* a new line after a list is the end of list, not a gap */
		if (priv->mode != GS_MARKDOWN_MODE_BULLETT)
			ret = gs_markdown_add_pending (self, "\n");
		priv->mode = GS_MARKDOWN_MODE_BLANK;
		goto out;
	}

	/* header1_type2 */
	ret = gs_markdown_to_text_line_is_header1_type2 (line);
	if (ret) {
		DEBUG ("header1_type2: '%s'", line);
		if (priv->mode == GS_MARKDOWN_MODE_PARA)
			priv->mode = GS_MARKDOWN_MODE_H1;
		goto out;
	}

	/* header2_type2 */
	ret = gs_markdown_to_text_line_is_header2_type2 (line);
	if (ret) {
		DEBUG ("header2_type2: '%s'", line);
		if (priv->mode == GS_MARKDOWN_MODE_PARA)
			priv->mode = GS_MARKDOWN_MODE_H2;
		goto out;
	}

	/* rule */
	ret = gs_markdown_to_text_line_is_rule (line);
	if (ret) {
		DEBUG ("rule: '%s'", line);
		gs_markdown_flush_pending (self);
		priv->mode = GS_MARKDOWN_MODE_RULE;
		ret = gs_markdown_add_pending (self, priv->tags.rule);
		goto out;
	}

	/* bullet */
	ret = gs_markdown_to_text_line_is_bullet (line);
	if (ret) {
		DEBUG ("bullet: '%s'", line);
		gs_markdown_flush_pending (self);
		priv->mode = GS_MARKDOWN_MODE_BULLETT;
		ret = gs_markdown_add_pending (self, &line[2]);
		goto out;
	}

	/* header1 */
	ret = gs_markdown_to_text_line_is_header1 (line);
	if (ret) {
		DEBUG ("header1: '%s'", line);
		gs_markdown_flush_pending (self);
		priv->mode = GS_MARKDOWN_MODE_H1;
		ret = gs_markdown_add_pending_header (self, &line[2]);
		goto out;
	}

	/* header2 */
	ret = gs_markdown_to_text_line_is_header2 (line);
	if (ret) {
		DEBUG ("header2: '%s'", line);
		gs_markdown_flush_pending (self);
		priv->mode = GS_MARKDOWN_MODE_H2;
		ret = gs_markdown_add_pending_header (self, &line[3]);
		goto out;
	}

	/* header3 */
	ret = gs_markdown_to_text_line_is_header3 (line);
	if (ret) {
		DEBUG ("header3: '%s'", line);
		gs_markdown_flush_pending (self);
		priv->mode = GS_MARKDOWN_MODE_H3;
		ret = gs_markdown_add_pending_header (self, &line[4]);
		goto out;
	}

	/* paragraph */
	if (priv->mode == GS_MARKDOWN_MODE_BLANK ||
	    priv->mode == GS_MARKDOWN_MODE_UNKNOWN) {
		gs_markdown_flush_pending (self);
		priv->mode = GS_MARKDOWN_MODE_PARA;
	}

	/* add to pending */
	DEBUG ("continue: '%s'", line);
	ret = gs_markdown_add_pending (self, line);
out:
	/* if we failed to add, we don't know the mode */
	if (!ret)
		priv->mode = GS_MARKDOWN_MODE_UNKNOWN;
	return ret;
}

/**
 * gs_markdown_set_output_kind:
 **/
static void
gs_markdown_set_output_kind (GsMarkdown *self, GsMarkdownOutputKind output)
{
	GsMarkdownPrivate *priv = gs_markdown_get_instance_private (self);

	g_return_if_fail (GS_IS_MARKDOWN (self));

	priv->output = output;
	switch (output) {
	case GS_MARKDOWN_OUTPUT_PANGO:
		/* PangoMarkup */
		priv->tags.em_start = "<i>";
		priv->tags.em_end = "</i>";
		priv->tags.strong_start = "<b>";
		priv->tags.strong_end = "</b>";
		priv->tags.code_start = "<tt>";
		priv->tags.code_end = "</tt>";
		priv->tags.codeblock_start = "<tt>";
		priv->tags.codeblock_end = "</tt>";
		priv->tags.para_start = "";
		priv->tags.para_end = "";
		priv->tags.h1_start = "<big>";
		priv->tags.h1_end = "</big>";
		priv->tags.h2_start = "<b>";
		priv->tags.h2_end = "</b>";
		priv->tags.h3_start = "<smallcaps><b>";
		priv->tags.h3_end = "</b></smallcaps>";
		priv->tags.bullet_start = "• ";
		priv->tags.bullet_end = "";
		priv->tags.rule = "⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯\n";
		priv->escape = TRUE;
		priv->autolinkify = TRUE;
		break;
	case GS_MARKDOWN_OUTPUT_HTML:
		/* XHTML */
		priv->tags.em_start = "<em>";
		priv->tags.em_end = "<em>";
		priv->tags.strong_start = "<strong>";
		priv->tags.strong_end = "</strong>";
		priv->tags.code_start = "<code>";
		priv->tags.code_end = "</code>";
		priv->tags.codeblock_start = "<pre>";
		priv->tags.codeblock_end = "</pre>";
		priv->tags.para_start = "<p>";
		priv->tags.para_end = "</p>";
		priv->tags.h1_start = "<h1>";
		priv->tags.h1_end = "</h1>";
		priv->tags.h2_start = "<h2>";
		priv->tags.h2_end = "</h2>";
		priv->tags.h3_start = "<h3>";
		priv->tags.h3_end = "</h3>";
		priv->tags.bullet_start = "<li>";
		priv->tags.bullet_end = "</li>";
		priv->tags.rule = "<hr>";
		priv->escape = TRUE;
		priv->autolinkify = TRUE;
		break;
	case GS_MARKDOWN_OUTPUT_TEXT:
		/* plain text */
		priv->tags.em_start = "";
		priv->tags.em_end = "";
		priv->tags.strong_start = "";
		priv->tags.strong_end = "";
		priv->tags.code_start = "";
		priv->tags.code_end = "";
		priv->tags.codeblock_start = "";
		priv->tags.codeblock_end = "";
		priv->tags.h1_start = "[";
		priv->tags.h1_end = "]";
		priv->tags.h2_start = "-";
		priv->tags.h2_end = "-";
		priv->tags.h3_start = "~";
		priv->tags.h3_end = "~";
		priv->tags.bullet_start = "* ";
		priv->tags.bullet_end = "";
		priv->tags.rule = " ----- \n";
		priv->escape = FALSE;
		priv->autolinkify = FALSE;
		break;
	case GS_MARKDOWN_OUTPUT_LAST:
	default:
		g_warning ("unknown output enum");
		break;
	}
}

/**
 * gs_markdown_set_max_lines:
 **/
void
gs_markdown_set_max_lines (GsMarkdown *self, gint max_lines)
{
	GsMarkdownPrivate *priv = gs_markdown_get_instance_private (self);
	g_return_if_fail (GS_IS_MARKDOWN (self));
	priv->max_lines = max_lines;
}

/**
 * gs_markdown_set_smart_quoting:
 **/
void
gs_markdown_set_smart_quoting (GsMarkdown *self, gboolean smart_quoting)
{
	GsMarkdownPrivate *priv = gs_markdown_get_instance_private (self);
	g_return_if_fail (GS_IS_MARKDOWN (self));
	priv->smart_quoting = smart_quoting;
}

/**
 * gs_markdown_set_escape:
 **/
void
gs_markdown_set_escape (GsMarkdown *self, gboolean escape)
{
	GsMarkdownPrivate *priv = gs_markdown_get_instance_private (self);
	g_return_if_fail (GS_IS_MARKDOWN (self));
	priv->escape = escape;
}

/**
 * gs_markdown_set_autocode:
 **/
void
gs_markdown_set_autocode (GsMarkdown *self, gboolean autocode)
{
	GsMarkdownPrivate *priv = gs_markdown_get_instance_private (self);
	g_return_if_fail (GS_IS_MARKDOWN (self));
	priv->autocode = autocode;
}

/**
 * gs_markdown_set_autolinkify:
 **/
void
gs_markdown_set_autolinkify (GsMarkdown *self, gboolean autolinkify)
{
	GsMarkdownPrivate *priv = gs_markdown_get_instance_private (self);
	g_return_if_fail (GS_IS_MARKDOWN (self));
	priv->autolinkify = autolinkify;
}

/**
 * gs_markdown_parse:
 **/
gchar *
gs_markdown_parse (GsMarkdown *self, const gchar *markdown)
{
	GsMarkdownPrivate *priv = gs_markdown_get_instance_private (self);
	gboolean ret;
	gchar **lines;
	gchar *temp;
	guint i;
	guint len;

	g_return_val_if_fail (GS_IS_MARKDOWN (self), NULL);

	DEBUG ("input='%s'", markdown);

	/* process */
	priv->mode = GS_MARKDOWN_MODE_UNKNOWN;
	priv->line_count = 0;
	g_string_truncate (priv->pending, 0);
	g_string_truncate (priv->processed, 0);
	lines = g_strsplit (markdown, "\n", -1);
	len = g_strv_length (lines);

	/* process each line */
	for (i = 0; i < len; i++) {
		ret = gs_markdown_to_text_line_process (self, lines[i]);
		if (!ret)
			break;
	}
	g_strfreev (lines);
	gs_markdown_flush_pending (self);

	/* remove trailing \n */
	while (g_str_has_suffix (priv->processed->str, "\n"))
		g_string_set_size (priv->processed, priv->processed->len - 1);

	/* get a copy */
	temp = g_strdup (priv->processed->str);
	g_string_truncate (priv->pending, 0);
	g_string_truncate (priv->processed, 0);

	DEBUG ("output='%s'", temp);

	return temp;
}

static void
gs_markdown_finalize (GObject *object)
{
	GsMarkdown *self;
	GsMarkdownPrivate *priv;

	g_return_if_fail (GS_IS_MARKDOWN (object));

	self = GS_MARKDOWN (object);
	priv = gs_markdown_get_instance_private (self);

	g_string_free (priv->pending, TRUE);
	g_string_free (priv->processed, TRUE);

	G_OBJECT_CLASS (gs_markdown_parent_class)->finalize (object);
}

static void
gs_markdown_class_init (GsMarkdownClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gs_markdown_finalize;
}

static void
gs_markdown_init (GsMarkdown *self)
{
	GsMarkdownPrivate *priv = gs_markdown_get_instance_private (self);
	priv->mode = GS_MARKDOWN_MODE_UNKNOWN;
	priv->pending = g_string_new ("");
	priv->processed = g_string_new ("");
	priv->max_lines = -1;
	priv->smart_quoting = FALSE;
	priv->escape = FALSE;
	priv->autocode = FALSE;
}

GsMarkdown *
gs_markdown_new (GsMarkdownOutputKind output)
{
	GsMarkdown *self;
	self = g_object_new (GS_TYPE_MARKDOWN, NULL);
	gs_markdown_set_output_kind (self, output);
	return GS_MARKDOWN (self);
}
