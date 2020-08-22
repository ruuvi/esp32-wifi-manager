/*
@file json.c
@brief handles very basic JSON with a minimal footprint on the system

This code is a completely rewritten version of cJSON 1.4.7.
cJSON is licensed under the MIT license:
Copyright (c) 2009 Dave Gamble

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

@see https://github.com/DaveGamble/cJSON
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include "json.h"

__attribute__((format(printf, 4, 5))) //
bool
json_snprintf(int32_t *pIdx, char *buf, const size_t buf_size, char *fmt, ...)
{
    const int32_t idx = *pIdx;
    if (idx < 0)
    {
        return false;
    }
    va_list args;
    va_start(args, fmt);
    const int len = vsnprintf(&buf[idx], buf_size - idx, fmt, args);
    va_end(args);
    if (len < 0)
    {
        *pIdx = -1;
        return false;
    }
    *pIdx += len;
    if (*pIdx >= buf_size)
    {
        *pIdx = -1;
        return false;
    }
    return true;
}

bool
json_print_escaped_string(int32_t *p_out_buf_idx, char *p_out_buf, const size_t out_buf_size, const char *p_input_str)
{
    if (NULL == p_out_buf)
    {
        return false;
    }

    if (NULL == p_input_str)
    {
        if (!json_snprintf(p_out_buf_idx, p_out_buf, out_buf_size, "\"\""))
        {
            return false;
        }
        return true;
    }

    json_snprintf(p_out_buf_idx, p_out_buf, out_buf_size, "\"");
    for (const char *in_ptr = p_input_str; '\0' != *in_ptr; ++in_ptr)
    {
        const char in_chr = *in_ptr;
        switch (in_chr)
        {
            case '\\':
            case '\"':
                json_snprintf(p_out_buf_idx, p_out_buf, out_buf_size, "\\%c", in_chr);
                break;
            case '\b':
                json_snprintf(p_out_buf_idx, p_out_buf, out_buf_size, "\\b");
                break;
            case '\f':
                json_snprintf(p_out_buf_idx, p_out_buf, out_buf_size, "\\f");
                break;
            case '\n':
                json_snprintf(p_out_buf_idx, p_out_buf, out_buf_size, "\\n");
                break;
            case '\r':
                json_snprintf(p_out_buf_idx, p_out_buf, out_buf_size, "\\r");
                break;
            case '\t':
                json_snprintf(p_out_buf_idx, p_out_buf, out_buf_size, "\\t");
                break;
            default:
                if (in_chr >= '\x20')
                {
                    /* normal character, copy */
                    json_snprintf(p_out_buf_idx, p_out_buf, out_buf_size, "%c", in_chr);
                }
                else
                {
                    json_snprintf(p_out_buf_idx, p_out_buf, out_buf_size, "\\u%04x", in_chr);
                }
                break;
        }
    }
    if (!json_snprintf(p_out_buf_idx, p_out_buf, out_buf_size, "\""))
    {
        return false;
    }
    return true;
}
