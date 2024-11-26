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
- Add nanobind interface.
- Incorporate an ASCII reader.

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

static uint32_t float_to_uint32(float value) {
  uint32_t result;
  std::memcpy(&result, &value, sizeof(float));
  return result;
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
    // fgets(line, 100, fp);
    // if (strncmp(line, "facet ", 6) == 0) {
    //   return STL_ASCII;
    // }
    return STL_ASCII;
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

// Fast string to float reader
static inline float fast_atof(const char *&p) {
  bool neg = false;
  if (*p == '-') {
    neg = true;
    ++p;
  } else if (*p == '+') {
    ++p;
  }

  double integer_part = 0.0;
  while (*p >= '0' && *p <= '9') {
    integer_part = integer_part * 10.0 + (*p - '0');
    ++p;
  }

  double fraction_part = 0.0;
  double fraction_scale = 1.0;
  if (*p == '.') {
    ++p;
    while (*p >= '0' && *p <= '9') {
      fraction_part = fraction_part * 10.0 + (*p - '0');
      fraction_scale *= 10.0;
      ++p;
    }
  }

  double exponent = 0.0;
  if (*p == 'e' || *p == 'E') {
    ++p;
    bool exp_neg = false;
    if (*p == '-') {
      exp_neg = true;
      ++p;
    } else if (*p == '+') {
      ++p;
    }
    while (*p >= '0' && *p <= '9') {
      exponent = exponent * 10.0 + (*p - '0');
      ++p;
    }
    if (exp_neg)
      exponent = -exponent;
  }

  double result = (integer_part + fraction_part / fraction_scale);
  if (exponent != 0.0) {
    result *= pow(10.0, exponent);
  }

  if (neg)
    result = -result;

  return static_cast<float>(result);
}

int loadstl_ascii(FILE *fp, char *comment, float **vertp, vertex_t *nvertp,
                  vertex_t **trip, uint16_t **attrp, triangle_t *ntrip) {
  // Read the entire file into memory
  fseek(fp, 0, SEEK_END);
  size_t fileSize = ftell(fp);
  rewind(fp);

  char *fileBuffer = (char *)malloc(fileSize + 1);
  if (!fileBuffer) {
    fprintf(stderr, "Memory allocation failed for file buffer\n");
    return -1;
  }
  if (fread(fileBuffer, 1, fileSize, fp) != fileSize) {
    fprintf(stderr, "Failed to read file into buffer\n");
    free(fileBuffer);
    return -1;
  }
  fileBuffer[fileSize] = '\0'; // Null-terminate the buffer

  const char *ptr = fileBuffer;
  const char *end = fileBuffer + fileSize;

  // Extract the comment from the first line
  if (sscanf(ptr, "solid %79[^\n]", comment) != 1) {
    strcpy(comment, "");
  }
  // Move ptr to the end of the line
  while (ptr < end && *ptr != '\n')
    ptr++;
  if (ptr < end)
    ptr++;

  // First, estimate the number of triangles
  triangle_t ntris_estimate = 0;
  const char *scan_ptr = ptr;
  while (scan_ptr < end) {
    // Skip whitespace
    while (scan_ptr < end && isspace(*scan_ptr))
      scan_ptr++;

    if (scan_ptr >= end)
      break;

    if (strncmp(scan_ptr, "facet", 5) == 0) {
      ntris_estimate++;
      // Move to the end of the line
      while (scan_ptr < end && *scan_ptr != '\n')
        scan_ptr++;
      if (scan_ptr < end)
        scan_ptr++;
      continue;
    }

    // Move to the next line
    while (scan_ptr < end && *scan_ptr != '\n')
      scan_ptr++;
    if (scan_ptr < end)
      scan_ptr++;
  }

  // Allocate initial memory for vertices and triangles
  triangle_t ntris = 0;
  vertex_t nverts = 0;
  size_t tris_cap = ntris_estimate > 0 ? ntris_estimate : 1024;
  vertex_t *tris = (vertex_t *)malloc(tris_cap * 3 * sizeof(vertex_t));
  size_t verts_cap = tris_cap * 3;
  uint32_t *verts = (uint32_t *)malloc(verts_cap * 3 * sizeof(uint32_t));

  // Set vhtcap based on estimated number of vertices
  vertex_t vhtcap = nextpow2(verts_cap * 2); // Must be a power of two
  vertex_t *vht = (vertex_t *)calloc(vhtcap, sizeof(vertex_t));
  if (!tris || !verts || !vht) {
    fprintf(stderr, "Memory allocation failed\n");
    free(fileBuffer);
    free(tris);
    free(verts);
    free(vht);
    return -1;
  }

  int v_idx = 0;
  uint32_t v[3][3];
  uint32_t vert[3];

  ptr = fileBuffer;
  // Skip the first line (solid ...)
  while (ptr < end && *ptr != '\n')
    ptr++;
  if (ptr < end)
    ptr++;

  while (ptr < end) {
    // Skip whitespace
    while (ptr < end && isspace(*ptr))
      ptr++;

    if (ptr >= end)
      break;

    if (strncmp(ptr, "facet", 5) == 0) {
      ptr += 5;
      // Skip to the end of the line
      while (ptr < end && *ptr != '\n')
        ptr++;
      if (ptr < end)
        ptr++;
      v_idx = 0;
      continue;
    }

    if (strncmp(ptr, "vertex", 6) == 0) {
      ptr += 6;
      // Skip whitespace
      while (ptr < end && isspace(*ptr))
        ptr++;
      // Parse three floats
      float x, y, z;

      x = fast_atof(ptr);
      while (ptr < end && isspace(*ptr))
        ptr++;

      y = fast_atof(ptr);
      while (ptr < end && isspace(*ptr))
        ptr++;

      z = fast_atof(ptr);
      while (ptr < end && *ptr != '\n')
        ptr++;
      if (ptr < end)
        ptr++;

      if (v_idx < 3) {
        vert[0] = float_to_uint32(x);
        vert[1] = float_to_uint32(y);
        vert[2] = float_to_uint32(z);
        copy96(v[v_idx], vert);
        v_idx++;
      } else {
        fprintf(stderr, "Warning: more than 3 vertices in a facet\n");
      }
      continue;
    }

    if (strncmp(ptr, "endfacet", 8) == 0) {
      ptr += 8;
      // Skip to the end of the line
      while (ptr < end && *ptr != '\n')
        ptr++;
      if (ptr < end)
        ptr++;
      if (v_idx == 3) {
        vertex_t vi[3];
        for (int i = 0; i < 3; i++) {
          vi[i] = vertex(verts, nverts, vht, vhtcap, v[i]);
          if (vi[i] == ~(vertex_t)0) {
            // Hash table is full, need to resize
            vertex_t old_vhtcap = vhtcap;
            vertex_t *old_vht = vht;
            vhtcap *= 2;
            vht = (vertex_t *)calloc(vhtcap, sizeof(vertex_t));
            if (!vht) {
              fprintf(stderr, "Memory allocation failed for vht\n");
              free(fileBuffer);
              free(tris);
              free(verts);
              free(old_vht);
              return -1;
            }
            // Rehash existing entries
            for (vertex_t idx = 0; idx < old_vhtcap; idx++) {
              if (old_vht[idx] != 0) {
                vertex_t vi_old = old_vht[idx] - 1;
                uint32_t *vert_old = verts + 3 * vi_old;
                vertex_t hash = final96(vert_old[0], vert_old[1], vert_old[2]);
                vertex_t j;
                for (j = 0; j < vhtcap; j++) {
                  vertex_t *vip_new = vht + ((hash + j) & (vhtcap - 1));
                  if (*vip_new == 0) {
                    *vip_new = vi_old + 1;
                    break;
                  }
                }
                if (j == vhtcap) {
                  fprintf(stderr, "Failed to rehash during vht resize\n");
                  free(fileBuffer);
                  free(tris);
                  free(verts);
                  free(old_vht);
                  free(vht);
                  return -1;
                }
              }
            }
            free(old_vht);
            // Now retry inserting the current vertex
            vi[i] = vertex(verts, nverts, vht, vhtcap, v[i]);
            if (vi[i] == ~(vertex_t)0) {
              fprintf(stderr, "Vertex hash table is full after resizing\n");
              free(fileBuffer);
              free(tris);
              free(verts);
              free(vht);
              return -1;
            }
          }
          if (vi[i] == nverts) {
            if (nverts >= verts_cap) {
              verts_cap *= 2;
              verts =
                  (uint32_t *)realloc(verts, verts_cap * 3 * sizeof(uint32_t));
              if (!verts) {
                fprintf(stderr, "Failed to reallocate verts array\n");
                free(fileBuffer);
                free(tris);
                free(vht);
                return -1;
              }
            }
            copy96(verts + 3 * nverts, v[i]);
            nverts++;
          }
        }

        if (ntris >= tris_cap) {
          tris_cap *= 2;
          tris = (vertex_t *)realloc(tris, tris_cap * 3 * sizeof(vertex_t));
          if (!tris) {
            fprintf(stderr, "Failed to reallocate tris array\n");
            free(fileBuffer);
            free(verts);
            free(vht);
            return -1;
          }
        }
        tris[3 * ntris + 0] = vi[0];
        tris[3 * ntris + 1] = vi[1];
        tris[3 * ntris + 2] = vi[2];
        ntris++;

        v_idx = 0;
      } else {
        fprintf(stderr, "Incomplete facet, expected 3 vertices\n");
      }
      continue;
    }

    // Skip unrecognized lines
    // Move to the end of the line
    while (ptr < end && *ptr != '\n')
      ptr++;
    if (ptr < end)
      ptr++;
  }

  free(fileBuffer);
  free(vht);
  verts = (uint32_t *)realloc(verts, nverts * 3 * sizeof(uint32_t));
  *vertp = (float *)verts;
  *nvertp = nverts;
  *trip = tris;
  *attrp = NULL; // ASCII STL does not have attributes
  *ntrip = ntris;
  return 0;
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

  if (format_status == STL_ASCII) {
    fseek(fp, 0, SEEK_SET);
    return loadstl_ascii(fp, comment, vertp, nvertp, trip, attrp, ntrip);
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
