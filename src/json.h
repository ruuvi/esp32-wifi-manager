/*
@file json.h
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

#ifndef JSON_H_INCLUDED
#define JSON_H_INCLUDED

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

__attribute__((format(printf, 4, 5))) //
bool
json_snprintf(int32_t *pIdx, char *buf, const size_t buf_size, char *fmt, ...);

/**
 * @brief Render the cstring provided to a JSON escaped version that can be printed.
 * @param p_out_buf_idx - a pointer to a variable that contains the current offset in the output buffer p_out_buf.
 * @param p_out_buf the output buffer to write to.
 * @param out_buf_size the length of the output buffer.
 * @param p_input_str the p_input_str buffer to be escaped.
 * @see cJSON equivalent static cJSON_bool print_string_ptr(const unsigned char * const p_input_str, printbuffer * const
 * output_buffer)
 */
bool
json_print_escaped_string(int32_t *p_out_buf_idx, char *p_out_buf, const size_t out_buf_size, const char *p_input_str);

#ifdef __cplusplus
}
#endif

#endif /* JSON_H_INCLUDED */
