/*
 * Copyright 2014 Hong Xu <hong AT topbug DOT net>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
*/

#include "global.h"

#include <ctype.h>
#include <string.h>
#include <pcre.h>

#include "utarray.h"
#include "misc.h"

#include "ec_glob.h"

typedef struct int_pair
{
    int     num1;
    int     num2;
} int_pair;
static const UT_icd ut_int_pair_icd = {sizeof(int_pair),NULL,NULL,NULL};

/* concatenate the string then move the pointer to the end */
#define STRING_CAT(p, string, end)  do {    \
    size_t string_len = strlen(string); \
    if (p + string_len >= end) \
        return -1; \
    strcat(p, string); \
    p += string_len; \
} while(0)

#define PATTERN_MAX  300
/*
 * Whether the string matches the given glob pattern
 */
EDITORCONFIG_LOCAL
int ec_glob(const char *pattern, const char *string)
{
    size_t                    i;
    int_pair *                p;
    char *                    c;
    char                      pcre_str[2 * PATTERN_MAX] = "^";
    char *                    p_pcre;
    char *                    pcre_str_end;
    int                       brace_level = 0;
    _Bool                     is_in_bracket = 0;
    const char *              error_msg;
    int                       erroffset;
    pcre *                    re;
    int                       rc;
    int *                     pcre_result;
    size_t                    pcre_result_len;
    char                      l_pattern[2 * PATTERN_MAX];
    _Bool                     are_brace_paired;
    UT_array *                nums;     /* number ranges */
    int                       ret = 0;

    if (pattern == NULL || string == NULL || (strlen (pattern) > PATTERN_MAX))
      return -1;

    strcpy(l_pattern, pattern);
    p_pcre = pcre_str + 1;
    pcre_str_end = pcre_str + 2 * PATTERN_MAX;

    {
        int     left_count = 0;
        int     right_count = 0;
        for (c = l_pattern; *c; ++ c)
        {
            if (*c == '\\' && *(c+1) != '\0')
            {
                ++ c;
                continue;
            }

            if (*c == '}')
                ++ right_count;
            if (*c == '{')
                ++ left_count;
        }

        are_brace_paired = (right_count == left_count);
    }

    /* used to search for {num1..num2} case */
    re = pcre_compile("^\\{[\\+\\-]?\\d+\\.\\.[\\+\\-]?\\d+\\}$", 0,
            &error_msg, &erroffset, NULL);
    if (!re)        /* failed to compile */
        return -1;

    utarray_new(nums, &ut_int_pair_icd);

    for (c = l_pattern; *c; ++ c)
    {
        switch (*c)
        {
        case '\\':      /* also skip the next one */
            if (*(c+1) != '\0')
            {
                *(p_pcre ++) = *(c++);
                *(p_pcre ++) = *c;
            }
            else
                STRING_CAT(p_pcre, "\\\\", pcre_str_end);

            break;
        case '?':
            *(p_pcre ++) = '.';
            break;
        case '*':
            if (*(c+1) == '*')      /* case of ** */
            {
                STRING_CAT(p_pcre, ".*", pcre_str_end);
                ++ c;
            }
            else                    /* case of * */
                STRING_CAT(p_pcre, "[^\\/]*", pcre_str_end);

            break;
        case '[':
            if (is_in_bracket)     /* inside brackets, we really mean bracket */
            {
                STRING_CAT(p_pcre, "\\[", pcre_str_end);
                break;
            }

            {
                /* check whether we have slash within the bracket */
                _Bool           has_slash = 0;
                char *          cc;
                for (cc = c; *cc && *cc != ']'; ++ cc)
                {
                    if (*cc == '\\' && *(cc+1) != '\0')
                    {
                        ++ cc;
                        continue;
                    }

                    if (*cc == '/')
                    {
                        has_slash = 1;
                        break;
                    }
                }

                /* if we have slash in the brackets, just do it literally */
                if (has_slash)
                {
                    char *           right_bracket = strchr(c, ']');

                    strcat(p_pcre, "\\");
                    strncat(p_pcre, c, right_bracket - c);
                    strcat(p_pcre, "\\]");
                    p_pcre += strlen(p_pcre);
                    c = right_bracket;
                    break;
                }
            }

            is_in_bracket = 1;
            if (*(c+1) == '!')     /* case of [!...] */
            {
                STRING_CAT(p_pcre, "[^", pcre_str_end);
                ++ c;
            }
            else
                *(p_pcre ++) = '[';

            break;

        case ']':
            is_in_bracket = 0;
            *(p_pcre ++) = *c;
            break;

        case '-':
            if (is_in_bracket)      /* in brackets, - indicates range */
                *(p_pcre ++) = *c;
            else
                STRING_CAT(p_pcre, "\\-", pcre_str_end);

            break;
        case '{':
            if (!are_brace_paired)
            {
                STRING_CAT(p_pcre, "\\{", pcre_str_end);
                break;
            }

            /* Check the case of {single}, where single can be empty */
            {
                char *                   cc;
                _Bool                    is_single = 1;

                for (cc = c + 1; *cc != '\0' && *cc != '}'; ++ cc)
                {
                    if (*cc == '\\' && *(cc+1) != '\0')
                    {
                        ++ cc;
                        continue;
                    }

                    if (*cc == ',')
                    {
                        is_single = 0;
                        break;
                    }
                }

                if (*cc == '\0')
                    is_single = 0;

                if (is_single)      /* escape the { and the corresponding } */
                {
                    const char *        double_dots;
                    int_pair            pair;
                    int                 pcre_res[3];

                    /* Check the case of {num1..num2} */
                    rc = pcre_exec(re, NULL, c, (int) (cc - c + 1), 0, 0,
                            pcre_res, 3);

                    if (rc < 0)    /* not {num1..num2} case */
                    {
                        STRING_CAT(p_pcre, "\\{", pcre_str_end);

                        memmove(cc+1, cc, strlen(cc) + 1);
                        *cc = '\\';

                        break;
                    }

                    /* Get the range */
                    double_dots = strstr(c, "..");
                    pair.num1 = atoi(c + 1);
                    pair.num2 = atoi(double_dots + 2);

                    utarray_push_back(nums, &pair);

                    STRING_CAT(p_pcre, "([\\+\\-]?\\d+)", pcre_str_end);
                    c = cc;

                    break;
                }
            }

            ++ brace_level;
            STRING_CAT(p_pcre, "(?:", pcre_str_end);
            break;

        case '}':
            if (!are_brace_paired)
            {
                STRING_CAT(p_pcre, "\\}", pcre_str_end);
                break;
            }

            -- brace_level;
            *(p_pcre ++) = ')';
            break;

        case ',':
            if (brace_level > 0)  /* , inside {...} */
                *(p_pcre ++) = '|';
            else
                STRING_CAT(p_pcre, "\\,", pcre_str_end);
            break;

        case '/':
            // /**/ case, match both single / and /anything/
            if (!strncmp(c, "/**/", 4))
            {
                STRING_CAT(p_pcre, "(\\/|\\/.*\\/)", pcre_str_end);
                c += 3;
            }
            else
                STRING_CAT(p_pcre, "\\/", pcre_str_end);

            break;

        default:
            if (!isalnum(*c))
                *(p_pcre ++) = '\\';

            *(p_pcre ++) = *c;
        }
    }

    *(p_pcre ++) = '$';

    pcre_free(re); /* ^\\d+\\.\\.\\d+$ */

    re = pcre_compile(pcre_str, 0, &error_msg, &erroffset, NULL);

    if (!re)        /* failed to compile */
    {
      utarray_free(nums);
      return -1;
    }

    pcre_result_len = 3 * (utarray_len(nums) + 1);
    pcre_result = (int *) calloc(pcre_result_len, sizeof(int_pair));
    rc = pcre_exec(re, NULL, string, (int) strlen(string), 0, 0,
            pcre_result, pcre_result_len);

    if (rc < 0)     /* failed to match */
    {
        if (rc == PCRE_ERROR_NOMATCH)
            ret = EC_GLOB_NOMATCH;
        else
            ret = rc;

        pcre_free(re);
        free(pcre_result);
        utarray_free(nums);

        return ret;
    }

    /* Whether the numbers are in the desired range? */
    for(p = (int_pair *) utarray_front(nums), i = 1; p;
            ++ i, p = (int_pair *) utarray_next(nums, p))
    {
        const char * substring_start = string + pcre_result[2 * i];
        size_t  substring_length = pcre_result[2 * i + 1] - pcre_result[2 * i];
        char *       num_string;
        int          num;

        /* we don't consider 0digits such as 010 as matched */
        if (*substring_start == '0')
            break;

        num_string = strndup(substring_start, substring_length);
        num = atoi(num_string);
        free(num_string);

        if (num < p->num1 || num > p->num2) /* not matched */
            break;
    }

    if (p != NULL)      /* numbers not matched */
        ret = EC_GLOB_NOMATCH;

    pcre_free(re);
    free(pcre_result);
    utarray_free(nums);

    return ret;
}
