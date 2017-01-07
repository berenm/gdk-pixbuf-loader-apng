#include "io-apng-animation.h"

#include <errno.h>
#include <stdio.h>

static void gdk_pixbuf_apng_anim_finalize(GObject* object);
static gboolean
gdk_pixbuf_apng_anim_is_static_image(GdkPixbufAnimation* animation);
static GdkPixbuf*
            gdk_pixbuf_apng_anim_get_static_image(GdkPixbufAnimation* animation);
static void gdk_pixbuf_apng_anim_get_size(GdkPixbufAnimation* animation,
                                          int* width, int* height);
static GdkPixbufAnimationIter*
gdk_pixbuf_apng_anim_get_iter(GdkPixbufAnimation* animation,
                              const GTimeVal*     start_time);

G_DEFINE_TYPE(GdkPixbufApngAnim, gdk_pixbuf_apng_anim,
              GDK_TYPE_PIXBUF_ANIMATION);

static void gdk_pixbuf_apng_anim_init(GdkPixbufApngAnim* anim) {}
static void gdk_pixbuf_apng_anim_class_init(GdkPixbufApngAnimClass* klass) {
  GObjectClass*            object_class = G_OBJECT_CLASS(klass);
  GdkPixbufAnimationClass* anim_class   = GDK_PIXBUF_ANIMATION_CLASS(klass);

  object_class->finalize = gdk_pixbuf_apng_anim_finalize;

  anim_class->is_static_image  = gdk_pixbuf_apng_anim_is_static_image;
  anim_class->get_static_image = gdk_pixbuf_apng_anim_get_static_image;
  anim_class->get_size         = gdk_pixbuf_apng_anim_get_size;
  anim_class->get_iter         = gdk_pixbuf_apng_anim_get_iter;
}

static void gdk_pixbuf_apng_anim_finalize(GObject* object) {
  GdkPixbufApngAnim* anim = GDK_PIXBUF_APNG_ANIM(object);

  for (GList* l = anim->frames; l; l = l->next) {
    GdkPixbufApngFrame* frame = l->data;
    g_clear_object(&frame->pixbuf);
    g_clear_object(&frame->composited);
    g_clear_object(&frame->revert);
    g_free(frame);
  }
  g_list_free(anim->frames);

  G_OBJECT_CLASS(gdk_pixbuf_apng_anim_parent_class)->finalize(object);
}

static gboolean
gdk_pixbuf_apng_anim_is_static_image(GdkPixbufAnimation* animation) {
  // printf("%s:%d (%s)\n", __FILE__, __LINE__, __func__);
  GdkPixbufApngAnim* anim;

  anim = GDK_PIXBUF_APNG_ANIM(animation);

  return anim->actl.num_frames == 1;
}

static GdkPixbuf*
gdk_pixbuf_apng_anim_get_static_image(GdkPixbufAnimation* animation) {
  // printf("%s:%d (%s)\n", __FILE__, __LINE__, __func__);
  GdkPixbufApngAnim* anim;

  anim = GDK_PIXBUF_APNG_ANIM(animation);
  if (anim->frames == NULL)
    return NULL;

  return GDK_PIXBUF(((GdkPixbufApngFrame*)anim->frames->data)->pixbuf);
}

static void gdk_pixbuf_apng_anim_get_size(GdkPixbufAnimation* animation,
                                          int* width, int* height) {
  // printf("%s:%d (%s)\n", __FILE__, __LINE__, __func__);
  GdkPixbufApngAnim* anim;

  anim = GDK_PIXBUF_APNG_ANIM(animation);

  if (width)
    *width = anim->ihdr.width;
  if (height)
    *height = anim->ihdr.height;
}

static GdkPixbufAnimationIter*
gdk_pixbuf_apng_anim_get_iter(GdkPixbufAnimation* animation,
                              const GTimeVal*     start_time) {
  // printf("%s:%d (%s)\n", __FILE__, __LINE__, __func__);
  GdkPixbufApngAnimIter* iter;

  iter = g_object_new(GDK_TYPE_PIXBUF_APNG_ANIM_ITER, NULL);

  iter->anim = GDK_PIXBUF_APNG_ANIM(animation);
  g_object_ref(iter->anim);

  iter->current_frame = iter->anim->frames;
  iter->start_time    = *start_time;
  iter->current_time  = *start_time;

  return GDK_PIXBUF_ANIMATION_ITER(iter);
}

static void gdk_pixbuf_apng_anim_iter_finalize(GObject* object);
static int
gdk_pixbuf_apng_anim_iter_get_delay_time(GdkPixbufAnimationIter* iter);
static GdkPixbuf*
                gdk_pixbuf_apng_anim_iter_get_pixbuf(GdkPixbufAnimationIter* anim_iter);
static gboolean gdk_pixbuf_apng_anim_iter_on_currently_loading_frame(
    GdkPixbufAnimationIter* anim_iter);
static gboolean gdk_pixbuf_apng_anim_iter_advance(GdkPixbufAnimationIter* iter,
                                                  const GTimeVal* current_time);

G_DEFINE_TYPE(GdkPixbufApngAnimIter, gdk_pixbuf_apng_anim_iter,
              GDK_TYPE_PIXBUF_ANIMATION_ITER);

static void gdk_pixbuf_apng_anim_iter_init(GdkPixbufApngAnimIter* anim_iter) {
  // printf("%s:%d (%s)\n", __FILE__, __LINE__, __func__);
}

static void
gdk_pixbuf_apng_anim_iter_class_init(GdkPixbufApngAnimIterClass* klass) {
  // printf("%s:%d (%s)\n", __FILE__, __LINE__, __func__);

  GObjectClass*                object_class = G_OBJECT_CLASS(klass);
  GdkPixbufAnimationIterClass* anim_iter_class =
      GDK_PIXBUF_ANIMATION_ITER_CLASS(klass);

  object_class->finalize = gdk_pixbuf_apng_anim_iter_finalize;

  anim_iter_class->get_delay_time = gdk_pixbuf_apng_anim_iter_get_delay_time;
  anim_iter_class->get_pixbuf     = gdk_pixbuf_apng_anim_iter_get_pixbuf;
  anim_iter_class->on_currently_loading_frame =
      gdk_pixbuf_apng_anim_iter_on_currently_loading_frame;
  anim_iter_class->advance = gdk_pixbuf_apng_anim_iter_advance;
}

static void gdk_pixbuf_apng_anim_iter_finalize(GObject* object) {
  // printf("%s:%d (%s)\n", __FILE__, __LINE__, __func__);
  GdkPixbufApngAnimIter* iter = GDK_PIXBUF_APNG_ANIM_ITER(object);

  iter->current_frame = NULL;
  g_object_unref(iter->anim);

  G_OBJECT_CLASS(gdk_pixbuf_apng_anim_iter_parent_class)->finalize(object);
}

static int
gdk_pixbuf_apng_anim_iter_get_delay_time(GdkPixbufAnimationIter* anim_iter) {
  // printf("%s:%d (%s)\n", __FILE__, __LINE__, __func__);

  GdkPixbufApngAnimIter* iter;
  GdkPixbufApngFrame*    frame;
  gint64                 delay_us;

  iter = GDK_PIXBUF_APNG_ANIM_ITER(anim_iter);

  frame = iter->current_frame->data;
  if (frame->fctl.delay_den == 0)
    delay_us = frame->fctl.delay_num * 1000000 / 100;
  else
    delay_us = frame->fctl.delay_num * 1000000 / frame->fctl.delay_den;

  return delay_us / 1000;
}

static GdkPixbuf*
gdk_pixbuf_apng_anim_iter_get_pixbuf(GdkPixbufAnimationIter* anim_iter) {
  // printf("%s:%d (%s)\n", __FILE__, __LINE__, __func__);

  GdkPixbufApngAnimIter* iter;
  GdkPixbufApngFrame*    frame;

  iter = GDK_PIXBUF_APNG_ANIM_ITER(anim_iter);

  frame = iter->current_frame ? iter->current_frame->data
                              : g_list_last(iter->anim->frames)->data;
  if (frame == NULL)
    return NULL;

  gdk_pixbuf_apng_anim_frame_composite(iter->anim, frame);

  return frame->composited;
}

static gboolean gdk_pixbuf_apng_anim_iter_on_currently_loading_frame(
    GdkPixbufAnimationIter* anim_iter) {
  // printf("%s:%d (%s)\n", __FILE__, __LINE__, __func__);
  GdkPixbufApngAnimIter* iter;

  iter = GDK_PIXBUF_APNG_ANIM_ITER(anim_iter);

  return iter->current_frame == NULL || iter->current_frame->next == NULL;
}

static gboolean
gdk_pixbuf_apng_anim_iter_advance(GdkPixbufAnimationIter* anim_iter,
                                  const GTimeVal*         current_time) {
  // printf("%s:%d (%s)\n", __FILE__, __LINE__, __func__);
  GdkPixbufApngAnimIter* iter;
  GdkPixbufApngFrame*    frame;

  gint64 elapsed_us;
  gint64 delay_us;

  iter = GDK_PIXBUF_APNG_ANIM_ITER(anim_iter);

  iter->current_time = *current_time;

  elapsed_us =
      (iter->current_time.tv_sec - iter->start_time.tv_sec) * G_USEC_PER_SEC +
      iter->current_time.tv_usec - iter->start_time.tv_usec;
  if (elapsed_us < 0) {
    iter->start_time = iter->current_time;
    elapsed_us       = 0;
  }

  frame = iter->current_frame->data;
  if (frame->fctl.delay_den == 0)
    delay_us = frame->fctl.delay_num * 1000000 / 100;
  else
    delay_us = frame->fctl.delay_num * 1000000 / frame->fctl.delay_den;

  // printf("%ld %ld\n", delay_us, elapsed_us);

  if (elapsed_us >= delay_us) {
    iter->start_time    = iter->current_time;
    iter->current_frame = iter->current_frame->next;
    if (iter->current_frame == NULL)
      iter->current_frame = iter->anim->frames;
  }

  return TRUE;
}

void gdk_pixbuf_apng_anim_frame_composite(GdkPixbufApngAnim*  anim,
                                          GdkPixbufApngFrame* frame) {
  // printf("%s:%d (%s)\n", __FILE__, __LINE__, __func__);
  GList* link;
  GList* tmp;

  link = g_list_find(anim->frames, frame);

  if (frame->composited == NULL) {
    /* For now, to composite we start with the last
     * composited frame and composite everything up to
     * here.
     */

    /* Rewind to last composited frame. */
    tmp = link;
    while (tmp != NULL) {
      GdkPixbufApngFrame* f = tmp->data;
      if (f->composited != NULL)
        break;
      tmp = tmp->prev;
    }

    /* Go forward, compositing all frames up to the current frame */
    if (tmp == NULL)
      tmp = anim->frames;

    while (tmp != NULL) {
      GdkPixbufApngFrame* f = tmp->data;

      if (f->pixbuf == NULL)
        return;
      g_assert(f->fctl.x_offset >= 0);
      g_assert(f->fctl.y_offset >= 0);
      g_assert(f->fctl.width > 0);
      g_assert(f->fctl.height > 0);
      g_assert(f->fctl.x_offset + f->fctl.width <= anim->ihdr.width);
      g_assert(f->fctl.y_offset + f->fctl.height <= anim->ihdr.height);

      if (f->composited != NULL)
        goto next;

      if (tmp->prev == NULL) {
        /* First frame may be smaller than the whole image;
         * if so, we make the area outside it full alpha if the
         * image has alpha, and background color otherwise.
         * GIF spec doesn't actually say what to do about this.
         */
        f->composited = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8,
                                       anim->ihdr.width, anim->ihdr.height);
        if (f->composited == NULL)
          return;

        /* alpha gets dumped if f->composited has no alpha */
        gdk_pixbuf_fill(f->composited, 0);

        if (f->fctl.dispose_op == APNG_DISPOSE_OP_PREVIOUS)
          g_warning("First frame of APNG has bad dispose mode, APNG loader "
                    "should not have loaded this image");
      } else {
        GdkPixbufApngFrame* prev = tmp->prev->data;
        /* Init f->composited with what we should have after the previous
         * frame
         */

        f->composited    = prev->composited;
        prev->composited = NULL;
        if (f->composited == NULL)
          return;

        switch (prev->fctl.dispose_op) {
        case APNG_DISPOSE_OP_NONE:
          break;
        case APNG_DISPOSE_OP_BACKGROUND: {

          /* Clear area of previous frame to background */
          GdkPixbuf* area;
          area = gdk_pixbuf_new_subpixbuf(f->composited, prev->fctl.x_offset,
                                          prev->fctl.y_offset, prev->fctl.width,
                                          prev->fctl.height);
          if (area == NULL)
            return;

          gdk_pixbuf_fill(area, 0);
          g_object_unref(area);
          break;
        }
        case APNG_DISPOSE_OP_PREVIOUS:
          if (prev->revert != NULL) {
            /* Copy in the revert frame */
            gdk_pixbuf_copy_area(
                prev->revert, 0, 0, gdk_pixbuf_get_width(prev->revert),
                gdk_pixbuf_get_height(prev->revert), f->composited,
                prev->fctl.x_offset, prev->fctl.y_offset);
          }
          break;
        default:
          g_assert(FALSE);
          break;
        }

        if (f->revert == NULL &&
            f->fctl.dispose_op == APNG_DISPOSE_OP_PREVIOUS) {
          /* We need to save the contents before compositing */
          GdkPixbuf* area;
          area = gdk_pixbuf_new_subpixbuf(f->composited, f->fctl.x_offset,
                                          f->fctl.y_offset, f->fctl.width,
                                          f->fctl.height);
          if (area == NULL)
            return;

          f->revert = gdk_pixbuf_copy(area);
          g_object_unref(area);
          if (f->revert == NULL)
            return;
        }
      }

      g_assert(f->pixbuf != NULL);
      g_assert(f->composited != NULL);

      switch (f->fctl.blend_op) {
      case APNG_BLEND_OP_SOURCE:
        gdk_pixbuf_scale(f->pixbuf, f->composited, f->fctl.x_offset,
                         f->fctl.y_offset, f->fctl.width, f->fctl.height,
                         f->fctl.x_offset, f->fctl.y_offset, 1.0, 1.0,
                         GDK_INTERP_BILINEAR);
        break;
      case APNG_BLEND_OP_OVER:
        gdk_pixbuf_composite(f->pixbuf, f->composited, f->fctl.x_offset,
                             f->fctl.y_offset, f->fctl.width, f->fctl.height,
                             f->fctl.x_offset, f->fctl.y_offset, 1.0, 1.0,
                             GDK_INTERP_BILINEAR, 255);
        break;
      default:
        g_assert(FALSE);
        break;
      }

    next:
      if (tmp == link)
        break;

      tmp = tmp->next;
    }
  }
}