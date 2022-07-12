/* ide-webkit-util.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

/* Much of the code within this file is derived from numerous files
 * within the Epiphany web browser. The original copyright is provided
 * below.
 */
/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
 *  Copyright © 2002 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005 Christian Persch
 *  Copyright © 2004 Crispin Flowerday
 *  Copyright © 2004 Adam Hooper
 *  Copyright © 2008, 2009 Gustavo Noronha Silva
 *  Copyright © 2009, 2010, 2014 Igalia S.L.
 *
 *  This file is part of Epiphany.
 *
 *  Epiphany is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Epiphany is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Epiphany.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "ide-webkit-util"

#include "config.h"

#include <libsoup/soup.h>

#include "ide-webkit-util.h"

#define DOMAIN_REGEX "^localhost(\\.[^[:space:]]+)?(:\\d+)?(:[0-9]+)?(/.*)?$|" \
                     "^[^\\.[:space:]]+\\.[^\\.[:space:]]+.*$|"

static char *
string_find_and_replace (const char *haystack,
                         const char *to_find,
                         const char *to_repl)
{
  GString *str;

  g_assert (haystack);
  g_assert (to_find);
  g_assert (to_repl);

  str = g_string_new (haystack);
  g_string_replace (str, to_find, to_repl, 0);
  return g_string_free (str, FALSE);
}

static char *
string_get_host_name (const char *url)
{
  g_autoptr(GUri) uri = NULL;

  if (url == NULL ||
      g_str_has_prefix (url, "file://") ||
      g_str_has_prefix (url, "about:"))
    return NULL;

  uri = g_uri_parse (url, G_URI_FLAGS_NONE, NULL);
  /* If uri is NULL it's very possible that we just got
   * something without a scheme, let's try to prepend
   * 'http://' */
  if (uri == NULL) {
    char *effective_url = g_strconcat ("http://", url, NULL);
    uri = g_uri_parse (effective_url, G_URI_FLAGS_NONE, NULL);
    g_free (effective_url);
  }

  if (uri == NULL)
    return NULL;

  return g_strdup (g_uri_get_host (uri));
}

static gboolean
address_has_web_scheme (const char *address)
{
  gboolean has_web_scheme;
  int colonpos;

  if (address == NULL)
    return FALSE;

  colonpos = (int)((strstr (address, ":")) - address);

  if (colonpos < 0)
    return FALSE;

  has_web_scheme = !(g_ascii_strncasecmp (address, "http", colonpos) &&
                     g_ascii_strncasecmp (address, "https", colonpos) &&
                     g_ascii_strncasecmp (address, "file", colonpos) &&
                     g_ascii_strncasecmp (address, "javascript", colonpos) &&
                     g_ascii_strncasecmp (address, "data", colonpos) &&
                     g_ascii_strncasecmp (address, "blob", colonpos) &&
                     g_ascii_strncasecmp (address, "about", colonpos) &&
                     g_ascii_strncasecmp (address, "gopher", colonpos) &&
                     g_ascii_strncasecmp (address, "inspector", colonpos) &&
                     g_ascii_strncasecmp (address, "webkit", colonpos));

  return has_web_scheme;
}

static gboolean
address_is_existing_absolute_filename (const char *address)
{
  g_autofree char *real_address = NULL;

  if (strchr (address, '#') == NULL) {
    real_address = g_strdup (address);
  } else {
    gint pos;

    pos = g_strstr_len (address, -1, "#") - address;
    real_address = g_strndup (address, pos);
  }

  return g_path_is_absolute (real_address) &&
         g_file_test (real_address, G_FILE_TEST_EXISTS);
}

static gboolean
is_host_with_port (const char *address)
{
  g_auto (GStrv) split = NULL;
  gint64 port = 0;

  if (strchr (address, ' '))
    return FALSE;

  split = g_strsplit (address, ":", -1);
  if (g_strv_length (split) == 2)
    port = g_ascii_strtoll (split[1], NULL, 10);

  return port != 0;
}

static char *
ensure_host_name_is_lowercase (const char *address)
{
  g_autofree gchar *host = string_get_host_name (address);
  g_autofree gchar *lowercase_host = NULL;

  if (host == NULL)
    return g_strdup (address);

  lowercase_host = g_utf8_strdown (host, -1);

  if (strcmp (host, lowercase_host) != 0)
    return string_find_and_replace (address, host, lowercase_host);
  else
    return g_strdup (address);
}


/* Does various normalization rules to make sure @input_address ends up
 * with a URI scheme (e.g. absolute filenames or "localhost"), changes
 * the URI scheme to something more appropriate when needed and lowercases
 * the hostname.
 */
char *
ide_webkit_util_normalize_address (const char *input_address)
{
  char *effective_address = NULL;
  g_autofree gchar *address = NULL;

  g_return_val_if_fail (input_address != NULL, NULL);

  address = ensure_host_name_is_lowercase (input_address);

  if (address_is_existing_absolute_filename (address))
    return g_strconcat ("file://", address, NULL);

  if (strcmp (address, "about:gpu") == 0)
    return g_strdup ("webkit://gpu");

  if (!address_has_web_scheme (address)) {
    const char *scheme;

    scheme = g_uri_peek_scheme (address);

    /* Auto-prepend http:// to anything that is not
     * one according to GLib, because it probably will be
     * something like "google.com". Special case localhost(:port)
     * and IP(:port), because GUri, correctly, thinks it is a
     * URI with scheme being localhost/IP and, optionally, path
     * being the port. Ideally we should check if we have a
     * handler for the scheme, and since we'll fail for localhost
     * and IP, we'd fallback to loading it as a domain. */
    if (!scheme ||
        !g_strcmp0 (scheme, "localhost") ||
        g_hostname_is_ip_address (scheme) ||
        is_host_with_port (address))
      effective_address = g_strconcat ("http://", address, NULL);
  }

  return effective_address ? effective_address : g_strdup (address);
}

static char *
hostname_to_tld (const char *hostname)
{
  g_auto(GStrv) parts = NULL;
  guint length;

  parts = g_strsplit (hostname, ".", 0);
  length = g_strv_length (parts);

  if (length >= 1)
    return g_strdup (parts[length - 1]);

  return g_strdup ("");
}

IdeWebkitSecurityLevel
ide_webkit_util_get_security_level (WebKitWebView *web_view)
{
  IdeWebkitSecurityLevel security_level;
  GTlsCertificateFlags tls_errors = 0;
  WebKitSecurityManager *security_manager;
  WebKitWebContext *web_context;
  GTlsCertificate *certificate = NULL;
  g_autoptr(GUri) guri = NULL;
  g_autofree char *tld = NULL;
  const char *uri;

  g_return_val_if_fail (WEBKIT_IS_WEB_VIEW (web_view), 0);

  uri = webkit_web_view_get_uri (web_view);
  web_context = webkit_web_view_get_context (web_view);
  security_manager = webkit_web_context_get_security_manager (web_context);
  guri = g_uri_parse (uri, G_URI_FLAGS_NONE, NULL);

  if (guri && g_uri_get_host (guri))
    tld = hostname_to_tld (g_uri_get_host (guri));

  if (!guri ||
      g_strcmp0 (tld, "127.0.0.1") == 0 ||
      g_strcmp0 (tld, "::1") == 0 ||
      g_strcmp0 (tld, "localhost") == 0 || /* We trust localhost to be local since glib!616. */
      webkit_security_manager_uri_scheme_is_local (security_manager, g_uri_get_scheme (guri)) ||
      webkit_security_manager_uri_scheme_is_empty_document (security_manager, g_uri_get_scheme (guri)))
    security_level = IDE_WEBKIT_SECURITY_LEVEL_LOCAL_PAGE;
  else if (webkit_web_view_get_tls_info (web_view, &certificate, &tls_errors))
    security_level = tls_errors == 0 ?
                     IDE_WEBKIT_SECURITY_LEVEL_STRONG_SECURITY : IDE_WEBKIT_SECURITY_LEVEL_UNACCEPTABLE_CERTIFICATE;
  else if (webkit_web_view_is_loading (web_view))
    security_level = IDE_WEBKIT_SECURITY_LEVEL_TO_BE_DETERMINED;
  else
    security_level = IDE_WEBKIT_SECURITY_LEVEL_NONE;

  return security_level;
}
