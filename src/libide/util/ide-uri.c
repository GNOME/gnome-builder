/* GLIB - Library of useful routines for C programming
 * Copyright 2010-2015 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>

#include <stdlib.h>
#include <string.h>

#include "util/ide-uri.h"

#if 0
# define GOTO(l) do { g_print ("%s():%d\n", G_STRFUNC, __LINE__); goto l; } while (0)
#else
# define GOTO(l) goto l
#endif

/**
 * SECTION:ideuri
 * @short_description: URI-handling utilities
 * @include: glib.h
 *
 * FIXME
 */

/**
 * IdeUri:
 * @scheme: the URI scheme
 * @user: the user or userinfo component, or %NULL
 * @password: the password component, or %NULL
 * @auth_params: the userinfo "params" component, or %NULL
 * @host: the host component, or %NULL
 * @port: the port, or %0 if not specified
 * @path: the path component
 * @query: the query component, or %NULL
 * @fragment: the fragment, or %NULL
 * @uri_string: the string the #IdeUri corresponds to, or %NULL
 *
 * A parsed URI. The exact manner in which a URI string is broken down
 * into a #IdeUri depends on the #IdeUriParseFlags that were used when
 * creating it.
 *
 * @scheme is always set, and always lowercase, even if @uri_string
 * contains uppercase letters in the scheme.
 *
 * @host will be set if @uri_string has an "authority" component (that
 * is, if the scheme is followed by "://" rather than just ":"). If
 * the URI was not parsed with %IDE_URI_PARSE_NON_DNS, @host will be
 * assumed to be an internet hostname (or IP address) and will be
 * decoded accordingly.
 *
 * The generic URI syntax allows a "userinfo" component before the
 * hostname. Some URI schemes further break the userinfo down into a
 * username, a password (separated from the username by a colon),
 * and/or additional parameters (separated by a semicolon). If you
 * parse the URI with %IDE_URI_PARSE_PASSWORD and/or
 * %IDE_URI_PARSE_AUTH_PARAMS, then the @password and @auth_params
 * fields will be filled in (assuming they were present). Otherwise,
 * the entire userinfo component will be put into the @user field.
 *
 * By default, @path, @query, and @fragment are stored undecoded,
 * because with some schemes (such as "http"), it is possible that the
 * encoded and unencoded forms of a character (eg,
 * "<literal>/</literal>" and "<literal>%<!-- -->2F</literal>") may
 * have different meanings. On the other hand, with schemes that do
 * not use URIs as protocol elements (such as "ftp"), that sort of
 * confusion is not possible, and it is always safe (and useful) to
 * decode the URI fully. You can parse the URI with
 * %IDE_URI_PARSE_DECODED if you want @path, @query, and @fragment to be
 * decoded.
 *
 * Note however that all of the (string) fields in a #IdeUri are
 * guaranteed to be valid UTF-8 strings, so if @uri_string contained
 * encoded non-UTF-8 data, it will normally be left %<!-- -->-encoded
 * in the corresponding #IdeUri fields, even if the #IdeUriParseFlags
 * would otherwise call for decoding it. You can use the flag
 * %IDE_URI_PARSE_UTF8_ONLY to cause this case to be an error instead.
 */

/**
 * IdeUriParseFlags:
 * @IDE_URI_PARSE_STRICT: Parse the URI strictly according to the RFC
 *     3986 grammar.
 * @IDE_URI_PARSE_HTML5: Parse the URI according to the HTML5 web
 *     address parsing rules.
 * @IDE_URI_PARSE_NO_IRI: Disallow Internationalized URIs; return an
 *     error if the URI contains non-ASCII characters
 * @IDE_URI_PARSE_PASSWORD: Split the userinfo into user and password,
 *     separated by ':'.
 * @IDE_URI_PARSE_AUTH_PARAMS: Split the userinfo into user/password and
 *     parameters, separated by ';'.
 * @IDE_URI_PARSE_NON_DNS: Do not parse the host as a DNS host/IP address.
 *     (Eg, for smb URIs with NetBIOS hostnames).
 * @IDE_URI_PARSE_DECODED: Decode even reserved %<!-- -->encoded
 *     characters in the URI (unless this would result in non-UTF8
 *     strings). Using this flag means that you cannot reliably
 *     convert the parsed URI back to string form with
 *     ide_uri_to_string().
 * @IDE_URI_PARSE_UTF8_ONLY: Return an error if non-UTF8 characters are
 *     encountered in the URI.
 *
 * Flags that control how a URI string is parsed (or re-parsed).
 */

struct _IdeUri
{
  volatile gint ref_count;

  gchar   *scheme;

  gchar   *user;
  gchar   *password;
  gchar   *auth_params;

  gchar   *host;
  gushort  port;

  gchar   *path;
  gchar   *query;
  gchar   *fragment;
};

G_DEFINE_BOXED_TYPE (IdeUri, ide_uri, ide_uri_ref, ide_uri_unref)

#define XDIGIT(c) ((c) <= '9' ? (c) - '0' : ((c) & 0x4F) - 'A' + 10)
#define HEXCHAR(s) ((XDIGIT (s[1]) << 4) + XDIGIT (s[2]))

static gboolean
ide_uri_char_is_unreserved (gchar ch)
{
  if (g_ascii_isalnum (ch))
    return TRUE;
  return ch == '-' || ch == '.' || ch == '_' || ch == '~';
}

static gchar *
uri_decoder (const gchar       *part,
             gboolean           just_normalize,
             IdeUriParseFlags   flags,
             IdeUriError        parse_error,
             GError           **error)
{
  gchar *decoded;
  guchar *s, *d, c;
  const gchar *invalid;

#if 0
  if (flags & IDE_URI_PARSE_DECODED)
    just_normalize = FALSE;
#endif

  decoded = g_malloc (strlen (part) + 1);
  for (s = (guchar *)part, d = (guchar *)decoded; *s; s++)
    {
      if (*s == '%')
        {
          if (!g_ascii_isxdigit (s[1]) ||
              !g_ascii_isxdigit (s[2]))
            {
              /* % followed by non-hex; this is an error */
              if (flags & IDE_URI_PARSE_STRICT)
                {
                  g_set_error_literal (error, IDE_URI_ERROR, parse_error,
                                       /* xgettext: no-c-format */
                                       _("Invalid %-encoding in URI"));
                  g_free (decoded);
                  return FALSE;
                }

              /* In non-strict mode, just let it through; we *don't*
               * fix it to "%25", since that might change the way that
               * the URI's owner would interpret it.
               */
              *d++ = *s;
              continue;
            }

          c = HEXCHAR (s);
          if (just_normalize && !ide_uri_char_is_unreserved (c))
            {
              /* Leave the % sequence there. */
              *d++ = *s;
            }
          else
            {
              *d++ = c;
              s += 2;
            }
        }
      else
        *d++ = *s;
    }
  *d = '\0';

  if (!g_utf8_validate (decoded, (gchar *)d - decoded, &invalid))
    {
      GString *tmp;
      const gchar *p = decoded;

      if (flags & IDE_URI_PARSE_UTF8_ONLY)
        {
          g_set_error_literal (error, IDE_URI_ERROR, parse_error,
                               _("Non-UTF-8 characters in URI"));
          g_free (decoded);
          return FALSE;
        }

      tmp = g_string_new (NULL);

      do
        {
          g_string_append_len (tmp, p, invalid - p);
          g_string_append_printf (tmp, "%%%02d", *(const guchar *)invalid);
          p = invalid + 1;
        }
      while (!g_utf8_validate (p, (const gchar *)d - p, &invalid));

      g_string_append (tmp, p);

      g_free (decoded);
      decoded = g_string_free (tmp, FALSE);
    }

  return decoded;
}

static gchar *
uri_decode (const gchar       *part,
            IdeUriParseFlags   flags,
            IdeUriError        parse_error,
            GError           **error)
{
  return uri_decoder (part, FALSE, flags, parse_error, error);
}

static gchar *
uri_normalize (const gchar       *part,
               IdeUriParseFlags   flags,
               IdeUriError        parse_error,
               GError           **error)
{
  return uri_decoder (part, TRUE, flags, parse_error, error);
}

/* Does the "Remove Dot Segments" algorithm from section 5.2.4 of RFC
 * 3986. @path is assumed to start with '/', and is modified in place.
 */
static void
remove_dot_segments (gchar *path)
{
  gchar *p, *q;

  /* Remove "./" where "." is a complete segment. */
  for (p = path + 1; *p; )
    {
      if (*(p - 1) == '/' &&
          *p == '.' && *(p + 1) == '/')
        memmove (p, p + 2, strlen (p + 2) + 1);
      else
        p++;
    }
  /* Remove "." at end. */
  if (p > path + 2 &&
      *(p - 1) == '.' && *(p - 2) == '/')
    *(p - 1) = '\0';

  /* Remove "<segment>/../" where <segment> != ".." */
  for (p = path + 1; *p; )
    {
      if (!strncmp (p, "../", 3))
        {
          p += 3;
          continue;
        }
      q = strchr (p + 1, '/');
      if (!q)
        break;
      if (strncmp (q, "/../", 4) != 0)
        {
          p = q + 1;
          continue;
        }
      memmove (p, q + 4, strlen (q + 4) + 1);
      p = path + 1;
    }
  /* Remove "<segment>/.." at end where <segment> != ".." */
  q = strrchr (path, '/');
  if (q && !strcmp (q, "/.."))
    {
      p = q - 1;
      while (p > path && *p != '/')
        p--;
      if (strncmp (p, "/../", 4) != 0)
        *(p + 1) = 0;
    }

  /* Remove extraneous initial "/.."s */
  while (!strncmp (path, "/../", 4))
    memmove (path, path + 3, strlen (path) - 2);
  if (!strcmp (path, "/.."))
    path[1] = '\0';
}

static char *
uri_cleanup (const char *uri_string)
{
  GString *copy;
  const char *end;

  /* Skip leading whitespace */
  while (g_ascii_isspace (*uri_string))
    uri_string++;

  /* Ignore trailing whitespace */
  end = uri_string + strlen (uri_string);
  while (end > uri_string && g_ascii_isspace (*(end - 1)))
    end--;

  /* Copy the rest, encoding unencoded spaces and stripping other whitespace */
  copy = g_string_sized_new (end - uri_string);
  while (uri_string < end)
    {
      if (*uri_string == ' ')
        g_string_append (copy, "%20");
      else if (g_ascii_isspace (*uri_string))
        ;
      else
        g_string_append_c (copy, *uri_string);
      uri_string++;
    }

  return g_string_free (copy, FALSE);
}

static gboolean
parse_host (const gchar       *raw_host,
            IdeUriParseFlags   flags,
            gchar            **host,
            GError           **error)
{
  gchar *decoded, *addr;

  if (*raw_host == '[')
    {
      int len = strlen (raw_host);

      if (raw_host[len - 1] != ']')
        {
          g_set_error (error, IDE_URI_ERROR, IDE_URI_ERROR_BAD_HOST,
                       _("Invalid IP literal “%s” in URI"),
                       raw_host);
          return FALSE;
        }

      addr = g_strndup (raw_host + 1, len - 2);
      /* addr must be an IPv6 address */
      if (!g_hostname_is_ip_address (addr) || !strchr (addr, ':'))
        {
          g_free (addr);
          g_set_error (error, IDE_URI_ERROR, IDE_URI_ERROR_BAD_HOST,
                       _("Invalid IP literal “%s” in URI"),
                       raw_host);
          return FALSE;
        }

      *host = addr;
      return TRUE;
    }

  if (g_hostname_is_ip_address (raw_host))
    {
      *host = g_strdup (raw_host);
      return TRUE;
    }

  decoded = uri_decode (raw_host,
                        (flags & IDE_URI_PARSE_NON_DNS) ? flags : IDE_URI_PARSE_STRICT,
                        IDE_URI_ERROR_BAD_HOST, error);
  if (!decoded)
    return FALSE;

  if (flags & IDE_URI_PARSE_NON_DNS)
    {
      *host = decoded;
      return TRUE;
    }

  /* You're not allowed to %-encode an IP address, so if it wasn't
   * one before, it better not be one now.
   */
  if (g_hostname_is_ip_address (decoded))
    {
      g_free (decoded);
      g_set_error (error, IDE_URI_ERROR, IDE_URI_ERROR_BAD_HOST,
                   _("Invalid encoded IP literal “%s” in URI"),
                   raw_host);
      return FALSE;
    }

  if (strchr (decoded, '%') || !g_utf8_validate (decoded, -1, NULL))
    {
      g_free (decoded);
      g_set_error (error, IDE_URI_ERROR, IDE_URI_ERROR_BAD_HOST,
                   _("Invalid non-ASCII hostname “%s” in URI"),
                   raw_host);
      return FALSE;
    }

  if (!g_hostname_is_non_ascii (decoded))
    {
      *host = decoded;
      return TRUE;
    }

  if (flags & IDE_URI_PARSE_NO_IRI)
    {
      g_set_error (error, IDE_URI_ERROR, IDE_URI_ERROR_BAD_HOST,
                   _("Non-ASCII hostname “%s” forbidden in this URI"),
                   decoded);
      g_free (decoded);
      return FALSE;
    }

  *host = g_hostname_to_ascii (decoded);
  g_free (decoded);
  return TRUE;
}

static gboolean
parse_port (const gchar  *raw_port,
            gushort      *port,
            GError      **error)
{
  gchar *end;
  int parsed_port;

  parsed_port = strtoul (raw_port, &end, 10);
  if (*end)
    {
      g_set_error (error, IDE_URI_ERROR, IDE_URI_ERROR_BAD_PORT,
                   _("Could not parse port “%s” in URI"),
                   raw_port);
      return FALSE;
    }
  else if (parsed_port > 65535)
    {
      g_set_error (error, IDE_URI_ERROR, IDE_URI_ERROR_BAD_PORT,
                   _("Port “%s” in URI is out of range"),
                   raw_port);
      return FALSE;
    }

  *port = parsed_port;
  return TRUE;
}

static gboolean
parse_userinfo (const gchar       *raw_userinfo,
                IdeUriParseFlags   flags,
                gchar            **user,
                gchar            **password,
                gchar            **auth_params,
                GError           **error)
{
  IdeUriParseFlags userflags = flags & (IDE_URI_PARSE_PASSWORD | IDE_URI_PARSE_AUTH_PARAMS);
  const gchar *start, *end;
  gchar *raw_user, *raw_password, *raw_params;

  start = raw_userinfo;
  if (userflags == (IDE_URI_PARSE_PASSWORD | IDE_URI_PARSE_AUTH_PARAMS))
    end = start + strcspn (start, ":;");
  else if (userflags == IDE_URI_PARSE_PASSWORD)
    end = start + strcspn (start, ":");
  else if (userflags == IDE_URI_PARSE_AUTH_PARAMS)
    end = start + strcspn (start, ";");
  else
    end = start + strlen (start);
  raw_user = g_strndup (start, end - start);

  *user = uri_decode (raw_user, flags, IDE_URI_ERROR_BAD_USER, error);
  g_free (raw_user);
  if (!*user)
    return FALSE;

  if (*end == ':')
    {
      start = end + 1;
      if (userflags & IDE_URI_PARSE_AUTH_PARAMS)
        end = start + strcspn (start, ";");
      else
        end = start + strlen (start);
      raw_password = g_strndup (start, end - start);

      *password = uri_decode (raw_password, flags, IDE_URI_ERROR_BAD_PASSWORD, error);
      g_free (raw_password);
      if (!*password)
        {
          g_free (*user);
          *user = NULL;
          return FALSE;
        }
    }
  else
    *password = NULL;

  if (*end == ';')
    {
      start = end + 1;
      end = start + strlen (start);
      raw_params = g_strndup (start, end - start);

      *auth_params = uri_decode (raw_params, flags, IDE_URI_ERROR_BAD_AUTH_PARAMS, error);
      g_free (raw_params);
      if (!*auth_params)
        {
          g_free (*user);
          *user = NULL;
          g_free (*password);
          *password = NULL;
          return FALSE;
        }
    }
  else
    *auth_params = NULL;

  return TRUE;
}

/**
 * ide_uri_new:
 * @uri_string: a string representing an absolute URI
 * @flags: flags describing how to parse @uri_string
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Parses @uri_string according to @flags. If the result is not a
 * valid absolute URI, it will be discarded, and an error returned.
 *
 * Return value: a new #IdeUri.
 */
IdeUri *
ide_uri_new (const gchar       *uri_string,
             IdeUriParseFlags   flags,
             GError           **error)
{
  return ide_uri_new_relative (NULL, uri_string, flags, error);
}

/**
 * ide_uri_new_relative:
 * @base_uri: (allow-none): a base URI
 * @uri_string: a string representing a relative or absolute URI
 * @flags: flags describing how to parse @uri_string
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Parses @uri_string according to @flags and, if it is a relative
 * URI, merges it with @base_uri. If the result is not a valid
 * absolute URI, it will be discarded, and an error returned.
 *
 * Return value: a new #IdeUri.
 */
IdeUri *
ide_uri_new_relative (IdeUri            *base_uri,
                      const gchar       *uri_string,
                      IdeUriParseFlags   flags,
                      GError           **error)
{
  g_autofree gchar *cleaned_uri_string = NULL;
  IdeUri *raw = NULL, *uri = NULL;
  gchar *raw_port = NULL;

  if (base_uri && !base_uri->scheme)
    {
      g_set_error_literal (error, IDE_URI_ERROR, IDE_URI_ERROR_MISC,
                           _("Base URI is not absolute"));
      return NULL;
    }

  uri = g_slice_new0 (IdeUri);
  uri->ref_count = 1;

  if (!(flags & IDE_URI_PARSE_STRICT) && strpbrk (uri_string, " \t\n\r"))
    {
      cleaned_uri_string = uri_cleanup (uri_string);
      uri_string = cleaned_uri_string;
    }

  /* We use another IdeUri to store the raw data in, for convenience */
  raw = g_slice_new0 (IdeUri);
  raw->ref_count = 1;
  ide_uri_split (uri_string, (flags & IDE_URI_PARSE_STRICT) != 0,
               &raw->scheme, &raw->user, &raw->host, &raw_port,
               &raw->path, &raw->query, &raw->fragment);

  if (raw->scheme)
    uri->scheme = g_ascii_strdown (raw->scheme, -1);
  else if (!base_uri)
    {
      g_set_error_literal (error, IDE_URI_ERROR, IDE_URI_ERROR_MISC,
                           _("URI is not absolute, and no base URI was provided"));
      GOTO (fail);
    }

  if (raw->user)
    {
      if (!parse_userinfo (raw->user, flags,
                           &uri->user, &uri->password, &uri->auth_params,
                           error))
        GOTO (fail);
    }

  if (raw->host)
    {
      if (!parse_host (raw->host, flags, &uri->host, error))
        GOTO (fail);
    }

  if (raw_port)
    {
      if (!parse_port (raw_port, &uri->port, error))
        GOTO (fail);
    }

  uri->path = uri_normalize (raw->path, flags, IDE_URI_ERROR_BAD_PATH, error);
  if (!uri->path)
    GOTO (fail);

  if (raw->query)
    {
      uri->query = uri_normalize (raw->query, flags, IDE_URI_ERROR_BAD_QUERY, error);
      if (!uri->query)
        GOTO (fail);
    }

  if (raw->fragment)
    {
      uri->fragment = uri_normalize (raw->fragment, flags, IDE_URI_ERROR_BAD_FRAGMENT, error);
      if (!uri->fragment)
        GOTO (fail);
    }

  if (!uri->scheme && !base_uri)
    {
      g_set_error (error, IDE_URI_ERROR, IDE_URI_ERROR_MISC,
                   _("Could not parse “%s” as absolute URI"),
                   uri_string);
      GOTO (fail);
    }

  if (base_uri)
    {
      /* This is section 5.2.2 of RFC 3986, except that we're doing
       * it in place in @uri rather than copying from R to T.
       */
      if (uri->scheme)
        remove_dot_segments (uri->path);
      else
        {
          if (uri->host)
            remove_dot_segments (uri->path);
          else
            {
              if (!*uri->path)
                {
                  g_free (uri->path);
                  uri->path = g_strdup (base_uri->path);
                  g_free (raw->path);
                  raw->path = NULL;
                  if (!uri->query)
                    uri->query = g_strdup (base_uri->query);
                }
              else
                {
                  if (*uri->path != '/')
                    remove_dot_segments (uri->path);
                  else
                    {
                      gchar *newpath, *last;

                      last = strrchr (base_uri->path, '/');
                      if (last)
                        {
                          newpath = g_strdup_printf ("%.*s/%s",
                                                     (int)(last - base_uri->path),
                                                     base_uri->path,
                                                     uri->path);
                        }
                      else
                        newpath = g_strdup_printf ("/%s", uri->path);

                      g_free (uri->path);
                      uri->path = newpath;
                      g_free (raw->path);
                      raw->path = NULL;

                      remove_dot_segments (uri->path);
                    }
                }

              uri->user = g_strdup (base_uri->user);
              uri->password = g_strdup (base_uri->password);
              uri->auth_params = g_strdup (base_uri->auth_params);
              uri->host = g_strdup (base_uri->host);
              uri->port = base_uri->port;
            }
        }
    }

  ide_uri_unref (raw);
  g_free (raw_port);
  return uri;

 fail:
  ide_uri_unref (raw);
  g_free (raw_port);
  ide_uri_unref (uri);
  return NULL;
}

/**
 * ide_uri_to_string:
 * @uri: a #IdeUri
 * @flags: flags describing how to convert @uri
 *
 * Returns a string representing @uri.
 *
 * Return value: a string representing @uri, which the caller must free.
 */
gchar *
ide_uri_to_string (IdeUri              *uri,
                   IdeUriToStringFlags  flags)
{
  GString *str;

  g_return_val_if_fail (uri != NULL, NULL);

  if (g_strcmp0 (uri->scheme, "file") == 0)
    {
      if (uri->fragment && !(flags & IDE_URI_HIDE_FRAGMENT))
        return g_strdup_printf ("file://%s#%s", uri->path, uri->fragment);
      else
        return g_strdup_printf ("file://%s", uri->path);
    }

  str = g_string_new (uri->scheme);
  g_string_append_c (str, ':');

  if (uri->host)
    {
      g_string_append (str, "//");

      if (uri->user)
        {
          g_string_append (str, uri->user);

          if (!(flags & IDE_URI_HIDE_AUTH_PARAMS))
            {
              if (uri->auth_params)
                {
                  g_string_append_c (str, ':');
                  g_string_append (str, uri->auth_params);
                }
              else if (uri->password)
                {
                  g_string_append_c (str, ':');
                  g_string_append (str, uri->password);
                }
            }

          g_string_append_c (str, '@');
        }

      if (uri->host)
       g_string_append (str, uri->host);

      if (uri->port)
        g_string_append_printf (str, ":%d", uri->port);
    }

  if (uri->path)
    g_string_append (str, uri->path);

  if (uri->query)
    {
      g_string_append_c (str, '?');
      g_string_append (str, uri->query);
    }
  if (uri->fragment && !(flags & IDE_URI_HIDE_FRAGMENT))
    {
      g_string_append_c (str, '#');
      g_string_append (str, uri->fragment);
    }

  return g_string_free (str, FALSE);
}

/**
 * ide_uri_copy:
 * @uri: a #IdeUri
 *
 * Copies @uri
 *
 * Return value: a copy of @uri
 */
IdeUri *
ide_uri_copy (IdeUri *uri)
{
  IdeUri *dup;

  g_return_val_if_fail (uri != NULL, NULL);

  dup = g_slice_new0 (IdeUri);
  dup->ref_count   = 1;
  dup->scheme      = g_strdup (uri->scheme);
  dup->user        = g_strdup (uri->user);
  dup->password    = g_strdup (uri->password);
  dup->auth_params = g_strdup (uri->auth_params);
  dup->host        = g_strdup (uri->host);
  dup->port        = uri->port;
  dup->path        = g_strdup (uri->path);
  dup->query       = g_strdup (uri->query);
  dup->fragment    = g_strdup (uri->fragment);

  return dup;
}

/**
 * ide_uri_ref:
 * @uri: An #IdeUri
 *
 * Increments the reference count of @uri by one.
 *
 * Returns: (transfer full): uri
 */
IdeUri *
ide_uri_ref (IdeUri *uri)
{
  g_return_val_if_fail (uri != NULL, NULL);
  g_return_val_if_fail (uri->ref_count > 0, NULL);

  g_atomic_int_inc (&uri->ref_count);

  return uri;
}

/**
 * ide_uri_unref:
 * @uri: a #IdeUri
 *
 * Decrements the reference count of @uri by 1. If the reference count
 * reaches zero, the structure will be freed.
 */
void
ide_uri_unref (IdeUri *uri)
{
  g_return_if_fail (uri != NULL);
  g_return_if_fail (uri->ref_count > 0);

  if (g_atomic_int_dec_and_test (&uri->ref_count))
    {
      g_free (uri->scheme);
      g_free (uri->user);
      g_free (uri->password);
      g_free (uri->auth_params);
      g_free (uri->host);
      g_free (uri->path);
      g_free (uri->query);
      g_free (uri->fragment);

      g_slice_free (IdeUri, uri);
    }
}

/**
 * ide_uri_split:
 * @uri_string: a string containing a relative or absolute URI
 * @strict: whether to parse @uri_string strictly
 * @scheme: (out) (nullable): on return, contains the scheme, or %NULL
 * @userinfo: (out) (nullable): on return, contains the userinfo, or %NULL
 * @host: (out) (nullable): on return, contains the host, or %NULL
 * @port: (out) (nullable): on return, contains the port, or %NULL
 * @path: (out) (nullable): on return, contains the path, or %NULL
 * @query: (out) (nullable): on return, contains the query, or %NULL
 * @fragment: (out) (nullable): on return, contains the fragment, or %NULL
 *
 * Parses @uri_string more-or-less according to the generic grammar of
 * RFC 3986 ("more" if @strict is %TRUE, "less" if %FALSE), and
 * outputs the pieces into the provided variables. This is a low-level
 * method that does not do any pre- or post-processing of @uri_string,
 * and is "garbage in, garbage out"; it just splits @uri_string into
 * pieces at the appropriate punctuation characters (consuming
 * delimiters as appropriate), and returns the pieces. Components that
 * are not present in @uri_string will be set to %NULL (but note that
 * the path is always present, though it may be an empty string).
 */
void
ide_uri_split (const gchar  *uri_string,
               gboolean      strict,
               gchar       **scheme,
               gchar       **userinfo,
               gchar       **host,
               gchar       **port,
               gchar       **path,
               gchar       **query,
               gchar       **fragment)
{
  const gchar *end, *colon, *at, *path_start, *semi, *question;
  const gchar *p, *bracket, *hostend;

  if (scheme)
    *scheme = NULL;
  if (userinfo)
    *userinfo = NULL;
  if (host)
    *host = NULL;
  if (port)
    *port = NULL;
  if (path)
    *path = NULL;
  if (query)
    *query = NULL;
  if (fragment)
    *fragment = NULL;

  /* Find scheme: initial [a-z+.-]* substring until ":" */
  p = uri_string;
  while (*p && (g_ascii_isalnum (*p) ||
                *p == '.' || *p == '+' || *p == '-'))
    p++;

  if (p > uri_string && *p == ':')
    {
      if (scheme)
        *scheme = g_strndup (uri_string, p - uri_string);
      p++;
    }
  else
    p = uri_string;

  /* Check for authority */
  if (strncmp (p, "//", 2) == 0)
    {
      p += 2;

      path_start = p + strcspn (p, "/?#");
      at = memchr (p, '@', path_start - p);
      if (at)
        {
          if (!strict)
            {
              gchar *next_at;

              /* Any "@"s in the userinfo must be %-encoded, but
               * people get this wrong sometimes. Since "@"s in the
               * hostname are unlikely (and also wrong anyway), assume
               * that if there are extra "@"s, they belong in the
               * userinfo.
               */
              do
                {
                  next_at = memchr (at + 1, '@', path_start - (at + 1));
                  if (next_at)
                    at = next_at;
                }
              while (next_at);
            }

          if (userinfo)
            *userinfo = g_strndup (p, at - p);
          p = at + 1;
        }

      if (!strict)
        {
          semi = strchr (p, ';');
          if (semi && semi < path_start)
            {
              /* Technically, semicolons are allowed in the "host"
               * production, but no one ever does this, and some
               * schemes mistakenly use semicolon as a delimiter
               * marking the start of the path. We have to check this
               * after checking for userinfo though, because a
               * semicolon before the "@" must be part of the
               * userinfo.
               */
              path_start = semi;
            }
        }

      /* Find host and port. The host may be a bracket-delimited IPv6
       * address, in which case the colon delimiting the port must come
       * after the close bracket.
       */
      if (*p == '[')
        {
          bracket = memchr (p, ']', path_start - p);
          if (bracket && *(bracket + 1) == ':')
            colon = bracket + 1;
          else
            colon = NULL;
        }
      else
        colon = memchr (p, ':', path_start - p);

      if (host)
        {
          hostend = colon ? colon : path_start;
          *host = g_strndup (p, hostend - p);
        }

      if (colon && colon != path_start - 1 && port)
        *port = g_strndup (colon + 1, path_start - (colon + 1));

      p = path_start;
    }

  /* Find fragment. */
  end = p + strcspn (p, "#");
  if (*end == '#')
    {
      if (fragment)
        *fragment = g_strdup (end + 1);
    }

  /* Find query */
  question = memchr (p, '?', end - p);
  if (question)
    {
      if (query)
        *query = g_strndup (question + 1, end - (question + 1));
      end = question;
    }

  if (path)
    *path = g_strndup (p, end - p);
}

/* This is just a copy of g_str_hash() with g_ascii_toupper()s added */
static guint
str_ascii_case_hash (gconstpointer key)
{
  const char *p = key;
  guint h = g_ascii_toupper(*p);

  if (h)
    {
      for (p += 1; *p != '\0'; p++)
        h = (h << 5) - h + g_ascii_toupper(*p);
    }

  return h;
}

static gboolean
str_ascii_case_equal (gconstpointer v1,
                      gconstpointer v2)
{
  const char *string1 = v1;
  const char *string2 = v2;

  return g_ascii_strcasecmp (string1, string2) == 0;
}

/**
 * ide_uri_parse_params:
 * @params: a string containing "attribute=value" parameters
 * @length: the length of @params, or -1 if it is NUL-terminated
 * @separator: the separator character between parameters.
 *   (usually ';', but sometimes '&')
 * @case_insensitive: whether to match parameter names case-insensitively
 *
 * Many URI schemes include one or more attribute/value pairs
 * as part of the URI value. This method can be used to parse them
 * into a hash table.
 *
 * The @params string is assumed to still be %<!-- -->-encoded, but
 * the returned values will be fully decoded. (Thus it is possible
 * that the returned values may contain '=' or @separator, if the
 * value was encoded in the input.) Invalid %<!-- -->-encoding is
 * treated as with the non-%IDE_URI_PARSE_STRICT rules for ide_uri_new().
 * (However, if @params is the path or query string from a #IdeUri that
 * was parsed with %IDE_URI_PARSE_STRICT, then you already know that it
 * does not contain any invalid encoding.)
 *
 * Return value: (element-type utf8 utf8) (transfer container): a hash table
 * of attribute/value pairs. Both names and values will be fully-decoded. If
 * @params cannot be parsed (eg, it contains two @separator characters in a
 * row), then %NULL is returned.
 */
GHashTable *
ide_uri_parse_params (const gchar *params,
                      gssize       length,
                      gchar        separator,
                      gboolean     case_insensitive)
{
  GHashTable *hash;
  const char *end, *attr, *attr_end, *value, *value_end;
  char *copy, *decoded_attr, *decoded_value;

  if (case_insensitive)
    {
      hash = g_hash_table_new_full (str_ascii_case_hash,
                                    str_ascii_case_equal,
                                    g_free, g_free);
    }
  else
    {
      hash = g_hash_table_new_full (g_str_hash, g_str_equal,
                                    g_free, g_free);
    }

  if (length == -1)
    end = params + strlen (params);
  else
    end = params + length;

  attr = params;
  while (attr < end)
    {
      value_end = memchr (attr, separator, end - attr);
      if (!value_end)
        value_end = end;

      attr_end = memchr (attr, '=', value_end - attr);
      if (!attr_end)
        {
          g_hash_table_destroy (hash);
          return NULL;
        }
      copy = g_strndup (attr, attr_end - attr);
      decoded_attr = uri_decode (copy, 0, IDE_URI_ERROR_MISC, NULL);
      g_free (copy);
      if (!decoded_attr)
        {
          g_hash_table_destroy (hash);
          return NULL;
        }

      value = attr_end + 1;
      copy = g_strndup (value, value_end - value);
      decoded_value = uri_decode (copy, 0, IDE_URI_ERROR_MISC, NULL);
      g_free (copy);
      if (!decoded_value)
        {
          g_free (decoded_attr);
          g_hash_table_destroy (hash);
          return NULL;
        }

      g_hash_table_insert (hash, decoded_attr, decoded_value);
      attr = value_end + 1;
    }

  return hash;
}

/**
 * ide_uri_parse_host:
 * @uri_string: a string containing a network URI
 * @flags: flags for parsing @uri_string
 * @scheme: (out): on return, will contain @uri_string's URI scheme
 * @host: (out): on return, will contain @uri_string's decoded hostname
 * @port: (out): on return, will contain @uri_string's port, or %0
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Utility function for parsing "network" URIs. This extracts just the
 * scheme, host, and port from @uri_string. All three out parameters
 * are mandatory.
 *
 * Return value: %TRUE on success, %FALSE on failure.
 */
gboolean
ide_uri_parse_host (const gchar       *uri_string,
                    IdeUriParseFlags   flags,
                    gchar            **scheme,
                    gchar            **host,
                    gushort           *port,
                    GError           **error)
{
  gchar *raw_scheme, *raw_host, *raw_port;

  ide_uri_split (uri_string, flags & IDE_URI_PARSE_STRICT,
               &raw_scheme, NULL, &raw_host, &raw_port,
               NULL, NULL, NULL);
  if (!raw_host)
    {
      g_set_error (error, IDE_URI_ERROR, IDE_URI_ERROR_BAD_HOST,
                   _("URI “%s” has no host component"),
                   uri_string);
      GOTO (fail);
    }

  if (raw_port)
    {
      if (!parse_port (raw_port, port, error))
        GOTO (fail);
    }
  else
    *port = 0;

  if (!parse_host (raw_host, flags, host, error))
    GOTO (fail);

  *scheme = raw_scheme;
  g_free (raw_host);
  g_free (raw_port);
  return TRUE;

 fail:
  g_free (raw_scheme);
  g_free (raw_host);
  g_free (raw_port);
  return FALSE;
}

/**
 * ide_uri_get_scheme:
 * @uri: a #IdeUri
 *
 * Gets @uri's scheme.
 *
 * Return value: @uri's scheme.
 */
const gchar *
ide_uri_get_scheme (IdeUri *uri)
{
  return uri->scheme;
}

/**
 * ide_uri_set_scheme:
 * @uri: a #IdeUri
 * @scheme: the URI scheme
 *
 * Sets @uri's scheme to @scheme.
 */
void
ide_uri_set_scheme (IdeUri      *uri,
                    const gchar *scheme)
{
  g_free (uri->scheme);
  uri->scheme = g_strdup (scheme);
}

/**
 * ide_uri_get_user:
 * @uri: a #IdeUri
 *
 * Gets @uri's user. If @uri was parsed with %IDE_URI_PARSE_PASSWORD or
 * %IDE_URI_PARSE_AUTH_PARAMS, this is the string that appears before the
 * password and parameters in the userinfo. If not, then the entire
 * userinfo is considered the user.
 *
 * Return value: @uri's user.
 */
const gchar *
ide_uri_get_user (IdeUri *uri)
{
  return uri->user;
}

/**
 * ide_uri_set_user:
 * @uri: a #IdeUri
 * @user: the username, or %NULL
 *
 * Sets @uri's user to @user. See ide_uri_get_user() for a description
 * of how this interacts with various parsing flags.
 */
void
ide_uri_set_user (IdeUri      *uri,
                  const gchar *user)
{
  g_free (uri->user);
  uri->user = g_strdup (user);
}

/**
 * ide_uri_get_password:
 * @uri: a #IdeUri
 *
 * Gets @uri's password. If @uri was not parsed with
 * %IDE_URI_PARSE_PASSWORD, this will always be %NULL.
 *
 * Return value: @uri's password.
 */
const gchar *
ide_uri_get_password (IdeUri *uri)
{
  return uri->password;
}

/**
 * ide_uri_set_password:
 * @uri: a #IdeUri
 * @password: the password, or %NULL
 *
 * Sets @uri's password to @password.
 */
void
ide_uri_set_password (IdeUri      *uri,
                      const gchar *password)
{
  g_free (uri->password);
  uri->password = g_strdup (password);
}

/**
 * ide_uri_get_auth_params:
 * @uri: a #IdeUri
 *
 * Gets @uri's authentication parameters. Depending on the URI scheme,
 * ide_uri_parse_params() may be useful for further parsing this
 * information.
 *
 * Return value: @uri's authentication parameters.
 */
const gchar *
ide_uri_get_auth_params (IdeUri *uri)
{
  return uri->auth_params;
}

/**
 * ide_uri_set_auth_params:
 * @uri: a #IdeUri
 * @auth_params: the authentication parameters, or %NULL
 *
 * Sets @uri's authentication parameters to @auth_params.
 */
void
ide_uri_set_auth_params (IdeUri      *uri,
                         const gchar *auth_params)
{
  g_free (uri->auth_params);
  uri->auth_params = g_strdup (auth_params);
}

/**
 * ide_uri_get_host:
 * @uri: a #IdeUri
 *
 * Gets @uri's host. If @uri contained an IPv6 address literal, this
 * value will not include the brackets that are required by the URI
 * syntax.
 *
 * Return value: @uri's host.
 */
const gchar *
ide_uri_get_host (IdeUri *uri)
{
  return uri->host;
}

/**
 * ide_uri_set_host:
 * @uri: a #IdeUri
 * @host: the hostname or IP address, or %NULL
 *
 * Sets @uri's host to @host.
 *
 * If @host is an IPv6 IP address, it should not include the brackets
 * required by the URI syntax; they will be added automatically when
 * converting @uri to a string.
 */
void
ide_uri_set_host (IdeUri      *uri,
                  const gchar *host)
{
  g_free (uri->host);
  uri->host = g_strdup (host);
}

/**
 * ide_uri_get_port:
 * @uri: a #IdeUri
 *
 * Gets @uri's port.
 *
 * Return value: @uri's port, or %0 if it was unset
 */
gushort
ide_uri_get_port (IdeUri *uri)
{
  return uri->port;
}

/**
 * ide_uri_set_port:
 * @uri: a #IdeUri
 * @port: the port, or %0
 *
 * Sets @uri's port to @port. If @port is 0, it will not be output
 * when calling ide_uri_to_string().
 */
void
ide_uri_set_port (IdeUri  *uri,
                  gushort  port)
{
  uri->port = port;
}

/**
 * ide_uri_get_path:
 * @uri: a #IdeUri
 *
 * Gets @uri's path, which may contain %<!-- -->-encoding, depending
 * on the flags with which @uri was parsed.
 *
 * Return value: @uri's path.
 */
const gchar *
ide_uri_get_path (IdeUri *uri)
{
  return uri->path;
}

/**
 * ide_uri_set_path:
 * @uri: a #IdeUri
 * @path: the (%<!-- -->-encoded) path
 *
 * Sets @uri's path to @path, which is assumed to have been
 * appropriately %<!-- -->-encoded. In particular, this means that if
 * you want to include a literal percent sign the path, you must write
 * it as "%<!-- -->25". That being said, if @path contains an
 * unencoded '?' or '#' character, it will get encoded, since
 * otherwise converting @uri to a string and then back to a #IdeUri
 * again would give a different result.
 */
void
ide_uri_set_path (IdeUri      *uri,
                  const gchar *path)
{
  g_free (uri->path);
  uri->path = g_strdup (path);
}

/**
 * ide_uri_get_query:
 * @uri: a #IdeUri
 *
 * Gets @uri's query, which may contain %<!-- -->-encoding, depending
 * on the flags with which @uri was parsed.
 *
 * For queries consisting of a series of "name=value" parameters,
 * ide_uri_parse_params() may be useful.
 *
 * Return value: @uri's query.
 */
const gchar *
ide_uri_get_query (IdeUri *uri)
{
  return uri->query;
}

/**
 * ide_uri_set_query:
 * @uri: a #IdeUri
 * @query: the (%<!-- -->-encoded) query
 *
 * Sets @uri's query to @query, which is assumed to have been
 * %<!-- -->-encoded by the caller. See ide_uri_set_path() for more
 * details.
 */
void
ide_uri_set_query (IdeUri      *uri,
                   const gchar *query)
{
  g_free (uri->query);
  uri->query = g_strdup (query);
}

/**
 * ide_uri_get_fragment:
 * @uri: a #IdeUri
 *
 * Gets @uri's fragment, which may contain %<!-- -->-encoding,
 * depending on the flags with which @uri was parsed.
 *
 * Return value: @uri's fragment.
 */
const gchar *
ide_uri_get_fragment (IdeUri *uri)
{
  return uri->fragment;
}

/**
 * ide_uri_set_fragment:
 * @uri: a #IdeUri
 * @fragment: the (%<!-- -->-encoded) fragment
 *
 * Sets @uri's fragment to @fragment, which is assumed to have been
 * %<!-- -->-encoded by the caller. See ide_uri_set_path() for more
 * details.
 */
void
ide_uri_set_fragment (IdeUri      *uri,
                      const gchar *fragment)
{
  g_free (uri->fragment);
  uri->fragment = g_strdup (fragment);
}

GQuark
ide_uri_error_quark (void)
{
  return g_quark_from_static_string ("ide-uri-error-quark");
}

/**
 * ide_uri_new_from_file:
 * @file: a #GFile.
 *
 * Creates a new #IdeUri from the uri provided by @file.
 *
 * Returns: (transfer full): A newly allcoated #IdeUri.
 */
IdeUri *
ide_uri_new_from_file (GFile *file)
{
  IdeUri *uri;
  gchar *uristr;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  uristr = g_file_get_uri (file);
  uri = ide_uri_new (uristr, 0, NULL);
  g_free (uristr);

  return uri;
}

gboolean
ide_uri_is_file (IdeUri *uri,
                 GFile  *file)
{
  gchar *file_uri;
  gchar *str;
  gboolean ret;

  g_return_val_if_fail (uri != NULL, FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  if (uri->host && uri->host [0])
    return FALSE;

  file_uri = g_file_get_uri (file);
  str = g_strdup_printf ("%s://%s", uri->scheme ?: "", uri->path ?: "");

  ret = (g_strcmp0 (file_uri, str) == 0);

  g_free (file_uri);
  g_free (str);

  return ret;
}

/**
 * ide_uri_to_file:
 * @uri: An #IdeUri
 *
 * Creates a #GFile that represents the resource @uri.
 *
 * Returns: (transfer full) (nullable): a #GFile or %NULL upon failure.
 */
GFile *
ide_uri_to_file (IdeUri *uri)
{
  GFile *ret;
  gchar *str;

  g_return_val_if_fail (uri != NULL, NULL);

  str = ide_uri_to_string (uri, IDE_URI_HIDE_FRAGMENT);
  ret = g_file_new_for_uri (str);
  g_free (str);

  return ret;
}
