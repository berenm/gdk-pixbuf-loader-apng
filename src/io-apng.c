#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "io-apng-animation.h"

static gpointer
gdk_pixbuf__apng_image_begin_load(GdkPixbufModuleSizeFunc     size_func,
                                  GdkPixbufModulePreparedFunc prepare_func,
                                  GdkPixbufModuleUpdatedFunc  update_func,
                                  gpointer user_data, GError** error) {
  ApngContext* ctx;

  ctx = g_new0(ApngContext, 1);
  if (ctx == NULL) {
    g_set_error_literal(error, GDK_PIXBUF_ERROR,
                        GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                        "Not enough memory to load APNG file");
    goto error;
  }

  ctx->anim = g_object_new(GDK_TYPE_PIXBUF_APNG_ANIM, NULL);
  if (ctx->anim == NULL) {
    g_set_error_literal(error, GDK_PIXBUF_ERROR,
                        GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                        "Not enough memory to load APNG file");
    goto error;
  }

  ctx->size_func    = size_func;
  ctx->prepare_func = prepare_func;
  ctx->update_func  = update_func;
  ctx->user_data    = user_data;

  return (gpointer)ctx;

error:
  g_clear_object(&ctx->anim);
  g_free(ctx->frame);
  g_free(ctx->buf);
  g_free(ctx);
  return NULL;
}

static gboolean gdk_pixbuf__apng_image_stop_load(gpointer context,
                                                 GError** error) {
  ApngContext* ctx    = context;
  gboolean     retval = TRUE;

  if (ctx->anim->frames == NULL ||
      ctx->anim->n_frames < ctx->anim->actl.num_frames) {
    g_set_error_literal(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                        "APNG image was truncated or incomplete.");

    retval = FALSE;
  }

  g_clear_object(&ctx->anim);
  g_free(ctx->frame);
  g_free(ctx->buf);
  g_free(ctx);

  return retval;
}

static gboolean apng_decompress_3(ApngContext* ctx, GdkPixbufApngFrame* frame,
                                  const guchar* buf, guint size, int* zerr) {
  uLongf dsize;

  *zerr = Z_OK;
  if (frame->buf == NULL) {
    frame->buf  = gdk_pixbuf_get_pixels(frame->pixbuf);
    frame->off  = 0;
    frame->size = gdk_pixbuf_get_byte_length(frame->pixbuf);
  }

  dsize = frame->size - frame->off;
  *zerr = uncompress(frame->buf + frame->off, &dsize, buf, size);
  if (*zerr != Z_OK)
    return FALSE;
  frame->off += dsize;

  gsize const height = frame->fctl.height;
  gsize const width  = frame->fctl.width;

  if (frame->off == height * (width + 1)) {
    guint8*  index = (guint8*)frame->buf;
    guint32* pixel = (guint32*)index;

    for (gssize y = height - 1; y >= 0; --y) {
      guint8 filter_type = index[y * (width + 1)];
      g_assert(filter_type == 0);

      for (gssize x = width - 1; x >= 0; --x)
        pixel[y * width + x] = ctx->plte.rgba[index[y * (width + 1) + x + 1]];
    }

    frame->off = frame->size;
  }

  return TRUE;
}

static gboolean apng_decompress_6(ApngContext* ctx, GdkPixbufApngFrame* frame,
                                  const guchar* buf, guint size, int* zerr) {
  uLongf dsize;

  gsize const height = frame->fctl.height;
  gsize const width  = frame->fctl.width;

  *zerr = Z_OK;
  if (frame->buf == NULL) {
    frame->size = height * (width * 4 + 1);
    frame->buf  = g_malloc(frame->size);
    frame->off  = 0;
  }

  dsize = frame->size - frame->off;
  *zerr = uncompress(frame->buf + frame->off, &dsize, buf, size);
  if (*zerr != Z_OK)
    return FALSE;
  frame->off += dsize;

  if (frame->off == frame->size) {
    guint8* buffer = (guint8*)frame->buf;
    guint8* pixel  = (guint8*)gdk_pixbuf_get_pixels(frame->pixbuf);

    for (gsize y = 0; y < height; ++y) {
      guint8 filter_type = buffer[y * (width * 4 + 1)];
      memcpy(&pixel[y * width * 4], &buffer[y * (width * 4 + 1) + 1],
             width * 4);

      gsize dx = ctx->anim->ihdr.bit_depth >= 8
                     ? 4 * ctx->anim->ihdr.bit_depth / 8
                     : 1;
      gsize dy = width * dx;
      if (filter_type == 0) {
      } else if (filter_type == 1) {
        for (gsize x = 0; x < dy; ++x) {
          guint8 a = x > 3 ? pixel[y * dy + x - dx] : 0;
          pixel[y * dy + x] += a;
        }
      } else if (filter_type == 2) {
        for (gsize x = 0; x < dy; ++x) {
          guint8 b = y > 0 ? pixel[y * dy + x - dy] : 0;
          pixel[y * dy + x] += b;
        }
      } else if (filter_type == 3) {
        for (gsize x = 0; x < dy; ++x) {
          guint8 a = x > 3 ? pixel[y * dy + x - dx] : 0;
          guint8 b = y > 0 ? pixel[y * dy + x - dy] : 0;
          pixel[y * dy + x] += (a + b) / 2;
        }
      } else if (filter_type == 4) {
        for (gsize x = 0; x < dy; ++x) {
          guint8 a  = x > 3 ? pixel[y * dy + x - dx] : 0;
          guint8 b  = y > 0 ? pixel[y * dy + x - dy] : 0;
          guint8 c  = y > 0 && x > 3 ? pixel[y * dy + x - dx - dy] : 0;
          gint   p  = a + b - c;
          guint  pa = abs(p - a);
          guint  pb = abs(p - b);
          guint  pc = abs(p - c);
          if (pa <= pb && pa <= pc)
            pixel[y * dy + x] += a;
          else if (pb <= pc)
            pixel[y * dy + x] += b;
          else
            pixel[y * dy + x] += c;
        }
      } else {
        g_assert(FALSE);
      }
    }

    g_free(frame->buf);
    frame->buf = NULL;
  }

  return TRUE;
}

static gboolean gdk_pixbuf__apng_image_load_increment(gpointer      context,
                                                      const guchar* buf,
                                                      guint         size,
                                                      GError**      error) {
  // printf("%s:%d (%s)\n", __FILE__, __LINE__, __func__);
  int          zerr = Z_OK;
  ApngContext* ctx  = context;

  ctx->buf = g_realloc(ctx->buf, ctx->size + size);
  memcpy(ctx->buf + ctx->size, buf, size);
  ctx->size += size;

  while (ctx->size >= 8) {
    gsize offset = 0;

    if (ctx->off == 0) {
      guint64 apng_header;
      memcpy(&apng_header, ctx->buf + offset, sizeof(apng_header));
      offset += sizeof(apng_header);

      g_assert(apng_header == APNG_HEADER_MAGIC);
    } else {
      g_assert(ctx->off >= 8);

      guint32 chunk_size;
      memcpy(&chunk_size, ctx->buf + offset, 4);
      offset += 4;

      chunk_size = GUINT32_FROM_BE(chunk_size);
      if (ctx->size < chunk_size + 12)
        return TRUE;

      char chunk_type[4];
      memcpy(&chunk_type[0], ctx->buf + offset, 4);
      offset += 4;

      if (strncmp(chunk_type, "IHDR", 4) == 0) {
        g_assert(chunk_size == 13);
        g_assert(sizeof(ctx->anim->ihdr) == 13);

        memcpy(&ctx->anim->ihdr, ctx->buf + offset, sizeof(ctx->anim->ihdr));
        offset += sizeof(ctx->anim->ihdr);
        ctx->anim->ihdr.width  = GUINT32_FROM_BE(ctx->anim->ihdr.width);
        ctx->anim->ihdr.height = GUINT32_FROM_BE(ctx->anim->ihdr.height);

        // printf(
        //   "IHDR\n"
        //   "  width: %d\n"
        //   "  height: %d\n"
        //   "  bit_depth: %d\n"
        //   "  colour_type: %d\n"
        //   "  compression_method: %d\n"
        //   "  filter_method: %d\n"
        //   "  interlace_method: %d\n",
        //   ctx->anim->ihdr.width, ctx->anim->ihdr.height,
        //   ctx->anim->ihdr.bit_depth, ctx->anim->ihdr.colour_type,
        //   ctx->anim->ihdr.compression_method, ctx->anim->ihdr.filter_method,
        //   ctx->anim->ihdr.interlace_method);
        g_assert(ctx->anim->ihdr.bit_depth == 8);
        // g_assert(ctx->anim->ihdr.colour_type == 3);
        g_assert(ctx->anim->ihdr.compression_method == 0);
        g_assert(ctx->anim->ihdr.filter_method == 0);
        g_assert(ctx->anim->ihdr.interlace_method == 0);

        if (ctx->size_func)
          (*ctx->size_func)(&ctx->anim->ihdr.width, &ctx->anim->ihdr.height,
                            ctx->user_data);

      } else if (strncmp(chunk_type, "acTL", 4) == 0) {
        g_assert(chunk_size == 8);
        g_assert(sizeof(ctx->anim->actl) == 8);

        g_assert(ctx->frame == NULL);
        g_assert(ctx->anim->frames == NULL);

        memcpy(&ctx->anim->actl, ctx->buf + offset, sizeof(ctx->anim->actl));
        offset += sizeof(ctx->anim->actl);
        ctx->anim->actl.num_frames =
            GUINT32_FROM_BE(ctx->anim->actl.num_frames);
        ctx->anim->actl.num_plays = GUINT32_FROM_BE(ctx->anim->actl.num_plays);

        // printf(
        //   "acTL\n"
        //   "  num_frames: %d\n"
        //   "  num_plays: %d\n",
        //   ctx->anim->actl.num_frames, ctx->anim->actl.num_plays);

      } else if (strncmp(chunk_type, "PLTE", 4) == 0) {
        g_assert(chunk_size % 3 == 0);
        g_assert(chunk_size / 3 <= 256);
        g_assert(chunk_size / 3 > 1);
        g_assert(ctx->anim->ihdr.colour_type == 3);

        ctx->plte.size = chunk_size / 3;
        for (gsize i = 0; i < ctx->plte.size; ++i) {
          guint8 r = ctx->buf[offset + 0];
          guint8 g = ctx->buf[offset + 1];
          guint8 b = ctx->buf[offset + 2];
          guint8 a = 0xff;

          ctx->plte.rgba[i] = (r << 0) | (g << 8) | (b << 16) | (a << 24);
          offset += 3;
        }

        // printf("PLTE\n");
        // for (gsize i = 0; i < ctx->plte.size; ++i)
        //   printf("  %08x\n", ctx->plte.rgba[i]);

      } else if (strncmp(chunk_type, "tRNS", 4) == 0) {
        g_assert(ctx->anim->ihdr.colour_type == 0 ||
                 ctx->anim->ihdr.colour_type == 2 ||
                 ctx->anim->ihdr.colour_type == 3);

        if (ctx->anim->ihdr.colour_type == 0)
          g_assert(chunk_size == 2);
        if (ctx->anim->ihdr.colour_type == 2)
          g_assert(chunk_size == 6);
        if (ctx->anim->ihdr.colour_type == 3) {
          g_assert(ctx->plte.size > 0);
          g_assert(chunk_size <= ctx->plte.size);

          for (gsize i = 0; i < chunk_size; ++i) {
            ctx->plte.rgba[i] &= ~0xff000000;
            ctx->plte.rgba[i] |= (ctx->buf[offset++] << 24);
          }

          // printf("tRNS\n");
          // for (gsize i = 0; i < chunk_size; ++i)
          //   printf("  %08x\n", ctx->plte.rgba[i]);
        }

      } else if (strncmp(chunk_type, "fcTL", 4) == 0) {
        g_assert(chunk_size == 26);
        g_assert(sizeof(ctx->frame->fctl) == 26);

        g_assert(ctx->frame == NULL);

        ctx->frame = g_new0(GdkPixbufApngFrame, 1);
        if (ctx->frame == NULL) {
          g_set_error_literal(error, GDK_PIXBUF_ERROR,
                              GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                              "Not enough memory to load a frame in APNG file");
          goto error;
        }

        memcpy(&ctx->frame->fctl, ctx->buf + offset, sizeof(ctx->frame->fctl));
        offset += sizeof(ctx->frame->fctl);
        ctx->frame->fctl.sequence_number =
            GUINT32_FROM_BE(ctx->frame->fctl.sequence_number);
        ctx->frame->fctl.width    = GUINT32_FROM_BE(ctx->frame->fctl.width);
        ctx->frame->fctl.height   = GUINT32_FROM_BE(ctx->frame->fctl.height);
        ctx->frame->fctl.x_offset = GUINT32_FROM_BE(ctx->frame->fctl.x_offset);
        ctx->frame->fctl.y_offset = GUINT32_FROM_BE(ctx->frame->fctl.y_offset);
        ctx->frame->fctl.delay_num =
            GUINT16_FROM_BE(ctx->frame->fctl.delay_num);
        ctx->frame->fctl.delay_den =
            GUINT16_FROM_BE(ctx->frame->fctl.delay_den);

        // printf(
        //   "fcTL\n"
        //   "  sequence_number: %d\n"
        //   "  width: %d\n"
        //   "  height: %d\n"
        //   "  x_offset: %d\n"
        //   "  y_offset: %d\n"
        //   "  delay_num: %d\n"
        //   "  delay_den: %d\n"
        //   "  dispose_op: %d\n"
        //   "  blend_op: %d\n",
        //   ctx->frame->fctl.sequence_number,
        //   ctx->frame->fctl.width,
        //   ctx->frame->fctl.height,
        //   ctx->frame->fctl.x_offset,
        //   ctx->frame->fctl.y_offset,
        //   ctx->frame->fctl.delay_num,
        //   ctx->frame->fctl.delay_den,
        //   ctx->frame->fctl.dispose_op,
        //   ctx->frame->fctl.blend_op);

        // g_assert(ctx->frame->sequence_number == ctx->anim->n_frames);
        ctx->frame->pixbuf =
            gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, ctx->frame->fctl.width,
                           ctx->frame->fctl.height);
        if (ctx->frame->pixbuf == NULL) {
          g_set_error_literal(error, GDK_PIXBUF_ERROR,
                              GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                              "Not enough memory to load a frame in APNG file");
          goto error;
        }

      } else if (strncmp(chunk_type, "IDAT", 4) == 0) {
        g_assert(ctx->frame != NULL);
        g_assert(ctx->anim->frames == NULL);
        if (ctx->anim->ihdr.colour_type == 3)
          g_assert(ctx->plte.size > 0);

        g_assert(ctx->anim->ihdr.colour_type == 3 ||
                 ctx->anim->ihdr.colour_type == 6);
        if (ctx->anim->ihdr.colour_type == 3) {
          if (apng_decompress_3(ctx, ctx->frame, ctx->buf + offset, chunk_size,
                                &zerr) == TRUE)
            offset += chunk_size;
          else if (zerr != Z_OK)
            goto zerror;
        } else if (ctx->anim->ihdr.colour_type == 6) {
          if (apng_decompress_6(ctx, ctx->frame, ctx->buf + offset, chunk_size,
                                &zerr) == TRUE)
            offset += chunk_size;
          else if (zerr != Z_OK)
            goto zerror;
        }

        if (ctx->frame->off == ctx->frame->size) {
          ctx->anim->n_frames++;
          ctx->anim->frames = g_list_append(ctx->anim->frames, ctx->frame);

          if (ctx->prepare_func)
            (*ctx->prepare_func)(ctx->frame->pixbuf,
                                 GDK_PIXBUF_ANIMATION(ctx->anim),
                                 ctx->user_data);

          // if (ctx->update_func != NULL)
          //   (ctx->update_func)(ctx->frame->pixbuf, ctx->frame->x_offset,
          //                      ctx->frame->y_offset, ctx->frame->width,
          //                      ctx->frame->height, ctx->user_data);

          gdk_pixbuf_apng_anim_frame_composite(ctx->anim, ctx->frame);

          if (ctx->frame->composited == NULL) {
            ctx->frame = NULL;
            g_set_error_literal(
                error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                "Not enough memory to composite a frame in APNG file");
            goto error;
          }
          ctx->frame = NULL;
        }

      } else if (strncmp(chunk_type, "fdAT", 4) == 0) {
        g_assert(ctx->frame != NULL);
        g_assert(ctx->anim->frames != NULL);

        if (ctx->anim->ihdr.colour_type == 3)
          g_assert(ctx->plte.size > 0);

        guint32 sequence_number;
        memcpy(&sequence_number, ctx->buf + offset, sizeof(sequence_number));
        offset += sizeof(sequence_number);
        sequence_number = GUINT32_FROM_BE(sequence_number);
        chunk_size -= sizeof(sequence_number);

        g_assert(ctx->anim->ihdr.colour_type == 3 ||
                 ctx->anim->ihdr.colour_type == 6);
        if (ctx->anim->ihdr.colour_type == 3) {
          if (apng_decompress_3(ctx, ctx->frame, ctx->buf + offset, chunk_size,
                                &zerr) == TRUE)
            offset += chunk_size;
          else if (zerr != Z_OK)
            goto zerror;
        } else if (ctx->anim->ihdr.colour_type == 6) {
          if (apng_decompress_6(ctx, ctx->frame, ctx->buf + offset, chunk_size,
                                &zerr) == TRUE)
            offset += chunk_size;
          else if (zerr != Z_OK)
            goto zerror;
        }

        if (ctx->frame->off == ctx->frame->size) {
          ctx->anim->n_frames++;
          ctx->anim->frames = g_list_append(ctx->anim->frames, ctx->frame);

          // if (ctx->update_func != NULL)
          //   (ctx->update_func)(ctx->frame->pixbuf, ctx->frame->x_offset,
          //                      ctx->frame->y_offset, ctx->frame->width,
          //                      ctx->frame->height, ctx->user_data);

          gdk_pixbuf_apng_anim_frame_composite(ctx->anim, ctx->frame);

          if (ctx->frame->composited == NULL) {
            ctx->frame = NULL;
            g_set_error_literal(
                error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                "Not enough memory to composite a frame in APNG file");
            goto error;
          }
          ctx->frame = NULL;
        }

      } else {
        offset += chunk_size;

        // printf("%c%c%c%c\n", chunk_type[0], chunk_type[1], chunk_type[2],
        // chunk_type[3]);
      }

      guint32 chunk_crc;
      memcpy(&chunk_crc, ctx->buf + offset, 4);
      offset += 4;
    }

    memmove(ctx->buf, ctx->buf + offset, ctx->size - offset);
    ctx->size -= offset;
    ctx->off += offset;
  }

  return TRUE;

zerror:
  if (zerr == Z_MEM_ERROR)
    g_set_error_literal(error, GDK_PIXBUF_ERROR,
                        GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                        "Not enough memory to decompress a frame in APNG file");
  else if (zerr == Z_BUF_ERROR)
    g_set_error_literal(error, GDK_PIXBUF_ERROR,
                        GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                        "Output buffer too small while decompressing a "
                        "frame in APNG file");
  else if (zerr == Z_DATA_ERROR)
    g_set_error_literal(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                        "Error while decompressing a frame in APNG file");
  else
    g_set_error_literal(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                        "Unknown error while decompressing a frame in APNG "
                        "file");

error:
  if (ctx->frame != NULL) {
    g_clear_object(&ctx->frame->pixbuf);
    g_clear_object(&ctx->frame->composited);
    g_clear_object(&ctx->frame->revert);
  }
  g_free(ctx->frame);
  ctx->frame = NULL;

  return FALSE;
}

static GdkPixbufAnimation*
gdk_pixbuf__apng_image_load_animation(FILE* file, GError** error) {
  // printf("%s:%d (%s)\n", __FILE__, __LINE__, __func__);
  return g_object_new(GDK_TYPE_PIXBUF_APNG_ANIM, NULL);
}

#ifndef INCLUDE_apng
#define MODULE_ENTRY(function) G_MODULE_EXPORT void function
#else
#define MODULE_ENTRY(function) void _gdk_pixbuf__apng_##function
#endif

MODULE_ENTRY(fill_vtable)(GdkPixbufModule* module) {
  module->begin_load     = gdk_pixbuf__apng_image_begin_load;
  module->stop_load      = gdk_pixbuf__apng_image_stop_load;
  module->load_increment = gdk_pixbuf__apng_image_load_increment;
  module->load_animation = gdk_pixbuf__apng_image_load_animation;
}

MODULE_ENTRY(fill_info)(GdkPixbufFormat* info) {
  static const GdkPixbufModulePattern signature[] = {
      {"\x89PNG\r\n\x1a\n...\rIHDR..??...???...????...?acTL",
       "        zzz     zzxxzzzxxxzzzxxxxxxxx    ", 100},
      {NULL, NULL, 0}};
  static const gchar* mime_types[] = {"image/apng", "video/apng", NULL};
  static const gchar* extensions[] = {"png", "apng", NULL};

  info->name        = "apng";
  info->signature   = (GdkPixbufModulePattern*)signature;
  info->description = "APNG image format";
  info->mime_types  = (gchar**)mime_types;
  info->extensions  = (gchar**)extensions;
  info->flags       = GDK_PIXBUF_FORMAT_THREADSAFE;
  info->license     = "UNLICENSE";
}
