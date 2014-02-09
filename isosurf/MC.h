#ifndef TJF_MC_H
#define TJF_MC_H

#include <inttypes.h>
#include <stdio.h>

#ifdef __cplusplus
#	define CMARCH extern "C"
#else
#	define CMARCH /* no extern "C" needed */
#endif

/** @returns the number of vertices processed in this iteration. */
CMARCH size_t
marchlayeru16(const uint16_t* data, const size_t dims[3],
              uint64_t layer, float isovalue,
              FILE* vertices, FILE* faces,
              const uint64_t nvertices);
CMARCH size_t
marchlayer16(const int16_t* data, const size_t dims[3],
             uint64_t layer, float isovalue,
             FILE* vertices, FILE* faces,
             const uint64_t nvertices);
CMARCH size_t
marchlayeru8(const uint8_t* data, const size_t dims[3],
             uint64_t layer, float isovalue,
             FILE* vertices, FILE* faces,
             const uint64_t nvertices);
CMARCH size_t
marchlayer8(const int8_t* data, const size_t dims[3],
            uint64_t layer, float isovalue,
            FILE* vertices, FILE* faces,
            const uint64_t nvertices);

#endif /* TJF_MC_H */
/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2008 Scientific Computing and Imaging Institute,
   University of Utah.


   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/
