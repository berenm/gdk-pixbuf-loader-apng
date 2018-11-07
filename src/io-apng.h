#ifndef IO_APNG_H
#define IO_APNG_H

#define GDK_PIXBUF_ENABLE_BACKEND
#include <gdk-pixbuf/gdk-pixbuf.h>
#undef GDK_PIXBUF_ENABLE_BACKEND

typedef struct __attribute__((packed)) {
  guint32 width;
  guint32 height;
  guint8  bit_depth;
  guint8  colour_type;
  guint8  compression_method;
  guint8  filter_method;
  guint8  interlace_method;
} ApngChunk_IHDR;

typedef struct __attribute__((packed)) {
  guint32 num_frames;
  guint32 num_plays;
} ApngChunk_acTL;

typedef struct __attribute__((packed)) {
  guint32 sequence_number;
  guint32 width;
  guint32 height;
  guint32 x_offset;
  guint32 y_offset;
  guint16 delay_num;
  guint16 delay_den;
  guint8  dispose_op;
  guint8  blend_op;
} ApngChunk_fcTL;

typedef struct {
  gsize   size;
  guint32 rgba[256];
} ApngChunk_PLTE;

typedef struct _GdkPixbufApngAnim      GdkPixbufApngAnim;
typedef struct _GdkPixbufApngAnimClass GdkPixbufApngAnimClass;
typedef struct _GdkPixbufApngFrame     GdkPixbufApngFrame;

typedef struct {
  GdkPixbufApngAnim*  anim;
  GdkPixbufApngFrame* frame;

  GdkPixbufModuleSizeFunc     size_func;
  GdkPixbufModulePreparedFunc prepare_func;
  GdkPixbufModuleUpdatedFunc  update_func;
  gpointer                    user_data;

  guchar* buf;
  gsize   off;
  gsize   size;

  ApngChunk_PLTE plte;
} ApngContext;

#endif // IO_APNG_H
