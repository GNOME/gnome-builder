/* gb-git-search-result.h
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

#ifndef GB_GIT_SEARCH_RESULT_H
#define GB_GIT_SEARCH_RESULT_H

#include "gb-search-result.h"

G_BEGIN_DECLS

#define GB_TYPE_GIT_SEARCH_RESULT            (gb_git_search_result_get_type())
#define GB_GIT_SEARCH_RESULT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_GIT_SEARCH_RESULT, GbGitSearchResult))
#define GB_GIT_SEARCH_RESULT_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_GIT_SEARCH_RESULT, GbGitSearchResult const))
#define GB_GIT_SEARCH_RESULT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_GIT_SEARCH_RESULT, GbGitSearchResultClass))
#define GB_IS_GIT_SEARCH_RESULT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_GIT_SEARCH_RESULT))
#define GB_IS_GIT_SEARCH_RESULT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_GIT_SEARCH_RESULT))
#define GB_GIT_SEARCH_RESULT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_GIT_SEARCH_RESULT, GbGitSearchResultClass))

typedef struct _GbGitSearchResult        GbGitSearchResult;
typedef struct _GbGitSearchResultClass   GbGitSearchResultClass;
typedef struct _GbGitSearchResultPrivate GbGitSearchResultPrivate;

struct _GbGitSearchResult
{
  GbSearchResult parent;

  /*< private >*/
  GbGitSearchResultPrivate *priv;
};

struct _GbGitSearchResultClass
{
  GbSearchResultClass parent;
};

GType      gb_git_search_result_get_type (void);
GtkWidget *gb_git_search_result_new      (const gchar *path);

G_END_DECLS

#endif /* GB_GIT_SEARCH_RESULT_H */
