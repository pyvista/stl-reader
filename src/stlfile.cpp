/*
The MIT License (MIT)

Copyright (c) 2016 Aki Nyrhinen

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

Also modified by Alex Kaszynski 2023-2024:
- Check is the file is ASCII and the elimination of stderr.
- Add nanobind interface

*/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/string.h>

#include "array_support.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hash96.h"
#include "stlfile.h"

#define STL_INVALID 0
#define STL_ASCII 1
#define STL_BINARY 2

namespace nb = nanobind;
using namespace nb::literals;

typedef int STL_STATUS;

static uint32_t get16(uint8_t *buf) {
  return (uint32_t)buf[0] + ((uint32_t)buf[1] << 8);
}

static uint32_t get32(uint8_t *buf) {
  return (uint32_t)buf[0] + ((uint32_t)buf[1] << 8) + ((uint32_t)buf[2] << 16) +
         ((uint32_t)buf[3] << 24);
}

static vertex_t vertex(uint32_t *verts, vertex_t nverts, vertex_t *vht,
                       vertex_t vhtcap, uint32_t *vert) {
  vertex_t *vip, vi;
  vertex_t hash;
  vertex_t i;

  hash = final96(vert[0], vert[1], vert[2]);
  for (i = 0; i < vhtcap; i++) {
    vip = vht + ((hash + i) & (vhtcap - 1));
    vi = *vip;
    if (vi == 0) {
      *vip = nverts + 1;
      return nverts;
    }
    vi--;
    if (cmp96(vert, verts + 3 * vi) == 0)
      return vi;
  }
  return ~(vertex_t)0;
}

STL_STATUS check_stl_format(FILE *fp) {
  if (fp == NULL) {
    printf("\n\tUnable to open the file");
    return STL_INVALID;
  }

  fseek(fp, 0, SEEK_END);
  size_t fileSize = ftell(fp);
  rewind(fp);

  if (fileSize < 15) {
    printf("\n\tThe STL file is not long enough (%zu bytes).\n", fileSize);
    return STL_INVALID;
  }

  char sixBytes[7];
  fread(sixBytes, 1, 6, fp);
  sixBytes[6] = '\0';

  if (strcmp(sixBytes, "solid ") == 0) {
    char line[100];
    fgets(line, 100, fp);
    if (strncmp(line, "facet ", 6) == 0) {
      return STL_ASCII;
    }
    rewind(fp);
  }

  if (fileSize < 84) {
    printf("\n\tThe STL file is not long enough (%zu bytes).\n", fileSize);
    return STL_INVALID;
  }

  fseek(fp, 80, SEEK_SET);
  uint32_t nTriangles;
  fread(&nTriangles, sizeof(nTriangles), 1, fp);
  if (fileSize != (84 + (nTriangles * 50))) {
    return STL_INVALID;
  }

  fseek(fp, 0, SEEK_SET);
  return STL_BINARY;
}

int loadstl(FILE *fp, char *comment, float **vertp, vertex_t *nvertp,
            vertex_t **trip, uint16_t **attrp, triangle_t *ntrip) {
  uint8_t buf[128];
  triangle_t i, ti;
  vertex_t *tris;
  triangle_t ntris;
  vertex_t *vht, vi, nverts, vhtcap;
  uint32_t *verts;
  uint16_t *attrs;

  STL_STATUS format_status = check_stl_format(fp);
  if (format_status == STL_INVALID) {
    fprintf(stderr, "loadstl: Invalid or unrecognized STL file format\n");
    return -2;
  }

  // the comment and triangle count
  if (fread(buf, 84, 1, fp) != 1) {
    fprintf(stderr, "loadstl: short read at header\n");
    return -1;
  }

  if (comment != NULL)
    memcpy(comment, buf, 80);

  ntris = get32(buf + 80);

  tris = (vertex_t *)malloc(ntris * 3 * sizeof tris[0]);
  attrs = (uint16_t *)malloc(ntris * sizeof attrs[0]);
  verts = (uint32_t *)malloc(3 * ntris * 3 * sizeof verts[0]);

  vhtcap = nextpow2(4 * ntris);
  vht = (vertex_t *)malloc(vhtcap * sizeof vht[0]);
  memset(vht, 0, vhtcap * sizeof vht[0]);

  /* fprintf(stderr, "loadstl: number of triangles: %u, vhtcap %d\n", ntris,
   * vhtcap); */

  nverts = 0;
  for (i = 0; i < ntris; i++) {
    if (fread(buf, 50, 1, fp) != 1) {
      fprintf(stderr, "loadstl: short read at triangle %d/%d\n", i, ntris);
      goto exit_fail;
    }
    // there's a normal vector at buf[0..11] which we are ignoring
    for (ti = 0; ti < 3; ti++) {
      uint32_t vert[3];
      vert[0] = get32(buf + 12 + 4 * 3 * ti);
      vert[1] = get32(buf + 12 + 4 * 3 * ti + 4);
      vert[2] = get32(buf + 12 + 4 * 3 * ti + 8);
      vi = vertex(verts, nverts, vht, vhtcap, vert);
      if (vi == ~(uint32_t)0) {
        fprintf(stderr, "loadstl: vertex hash full at triangle %d/%d\n", i,
                ntris);
        goto exit_fail;
      }
      if (vi == nverts) {
        copy96(verts + 3 * nverts, vert);
        nverts++;
      } else {
      }
      tris[3 * i + ti] = vi;
    }
    attrs[i] = get16(buf + 48);
  }

  free(vht);
  verts = (uint32_t *)realloc(verts, nverts * 3 * sizeof verts[0]);
  *vertp = (float *)verts;
  *nvertp = nverts;
  *trip = tris;
  *attrp = attrs;
  *ntrip = ntris;
  return 0;

exit_fail:
  free(vht);
  free(verts);
  free(tris);
  free(attrs);
  return -1;
}

nb::tuple GetStlData(const std::string &filename) {
  FILE *fp = fopen(filename.c_str(), "rb");
  if (!fp) {
    throw std::runtime_error("File not found: " + filename);
  }

  char comment[80];
  float *vertp;
  unsigned int nverts;
  unsigned int *trip;
  unsigned short *attrp;
  unsigned int ntrip;

  int result = loadstl(fp, comment, &vertp, &nverts, &trip, &attrp, &ntrip);
  fclose(fp);

  if (result == -2) {
    throw std::runtime_error("Invalid or unrecognized STL file format.");
  } else if (result != 0) {
    throw std::runtime_error("Failed to load STL file.");
  }

  NDArray<float, 2> vert_arr = WrapNDarray<float, 2>(vertp, {(int)nverts, 3});
  NDArray<unsigned int, 2> face_arr =
      WrapNDarray<unsigned int, 2>(trip, {(int)ntrip, 3});

  return nb::make_tuple(vert_arr, face_arr);
}

NB_MODULE(stl_reader, m) { m.def("get_stl_data", &GetStlData); }
