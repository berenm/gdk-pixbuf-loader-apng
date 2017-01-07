#ifndef IO_APNG_ANIMATION_H
#define IO_APNG_ANIMATION_H

#include "io-apng.h"

typedef enum {
  APNG_DISPOSE_OP_NONE       = 0,
  APNG_DISPOSE_OP_BACKGROUND = 1,
  APNG_DISPOSE_OP_PREVIOUS   = 2
} GdkPixbufApngDisposeOp;

typedef enum {
  APNG_BLEND_OP_SOURCE = 0,
  APNG_BLEND_OP_OVER   = 1
} GdkPixbufApngBlendOp;

#define GDK_TYPE_PIXBUF_APNG_ANIM (gdk_pixbuf_apng_anim_get_type())
#define GDK_PIXBUF_APNG_ANIM(object)                                           \
  (G_TYPE_CHECK_INSTANCE_CAST((object), GDK_TYPE_PIXBUF_APNG_ANIM,             \
                              GdkPixbufApngAnim))
#define GDK_IS_PIXBUF_APNG_ANIM(object)                                        \
  (G_TYPE_CHECK_INSTANCE_TYPE((object), GDK_TYPE_PIXBUF_APNG_ANIM))

#define GDK_PIXBUF_APNG_ANIM_CLASS(klass)                                      \
  (G_TYPE_CHECK_CLASS_CAST((klass), GDK_TYPE_PIXBUF_APNG_ANIM,                 \
                           GdkPixbufApngAnimClass))
#define GDK_IS_PIXBUF_APNG_ANIM_CLASS(klass)                                   \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GDK_TYPE_PIXBUF_APNG_ANIM))
#define GDK_PIXBUF_APNG_ANIM_GET_CLASS(obj)                                    \
  (G_TYPE_INSTANCE_GET_CLASS((obj), GDK_TYPE_PIXBUF_APNG_ANIM,                 \
                             GdkPixbufApngAnimClass))

struct _GdkPixbufApngAnim {
  GdkPixbufAnimation parent_instance;

  ApngChunk_IHDR ihdr;
  ApngChunk_acTL actl;

  gsize  n_frames;
  GList* frames;
};

struct _GdkPixbufApngAnimClass {
  GdkPixbufAnimationClass parent_class;
};

GType gdk_pixbuf_apng_anim_get_type(void) G_GNUC_CONST;

typedef struct _GdkPixbufApngAnimIter      GdkPixbufApngAnimIter;
typedef struct _GdkPixbufApngAnimIterClass GdkPixbufApngAnimIterClass;

#define GDK_TYPE_PIXBUF_APNG_ANIM_ITER (gdk_pixbuf_apng_anim_iter_get_type())
#define GDK_PIXBUF_APNG_ANIM_ITER(object)                                      \
  (G_TYPE_CHECK_INSTANCE_CAST((object), GDK_TYPE_PIXBUF_APNG_ANIM_ITER,        \
                              GdkPixbufApngAnimIter))
#define GDK_IS_PIXBUF_APNG_ANIM_ITER(object)                                   \
  (G_TYPE_CHECK_INSTANCE_TYPE((object), GDK_TYPE_PIXBUF_APNG_ANIM_ITER))

#define GDK_PIXBUF_APNG_ANIM_ITER_CLASS(klass)                                 \
  (G_TYPE_CHECK_CLASS_CAST((klass), GDK_TYPE_PIXBUF_APNG_ANIM_ITER,            \
                           GdkPixbufApngAnimIterClass))
#define GDK_IS_PIXBUF_APNG_ANIM_ITER_CLASS(klass)                              \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GDK_TYPE_PIXBUF_APNG_ANIM_ITER))
#define GDK_PIXBUF_APNG_ANIM_ITER_GET_CLASS(obj)                               \
  (G_TYPE_INSTANCE_GET_CLASS((obj), GDK_TYPE_PIXBUF_APNG_ANIM_ITER,            \
                             GdkPixbufApngAnimIterClass))

struct _GdkPixbufApngAnimIter {
  GdkPixbufAnimationIter parent_instance;
  GdkPixbufApngAnim*     anim;

  GTimeVal start_time;
  GTimeVal current_time;
  GList*   current_frame;
};

struct _GdkPixbufApngAnimIterClass {
  GdkPixbufAnimationIterClass parent_class;
};

GType gdk_pixbuf_apng_anim_iter_get_type(void) G_GNUC_CONST;

struct _GdkPixbufApngFrame {
  ApngChunk_fcTL fctl;

  guchar* buf;
  gsize   off;
  gsize   size;

  GdkPixbuf* pixbuf;
  GdkPixbuf* composited;
  GdkPixbuf* revert;
};

void gdk_pixbuf_apng_anim_frame_composite(GdkPixbufApngAnim*  animation,
                                          GdkPixbufApngFrame* frame);

#endif // IO_APNG_ANIMATION_H
