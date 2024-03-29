#include <arpa/inet.h>
#include <fstream>
#include <gio/gio.h>
#include <glib.h>
#include <gst/gst.h>
#include <gst/video/gstvideosink.h>
#include <gst/video/video.h>
#include <map>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <vector>

#include "player.h"

static GAsyncQueue *networkQueue;

G_BEGIN_DECLS

#define GST_TYPE_ARTNETVIDEOSINK (gst_artnetvideosink_get_type())

#define GST_ARTNETVIDEOSINK(obj)                                               \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_ARTNETVIDEOSINK,                 \
                              GstArtnetVideoSink))
#define GST_ARTNETVIDEOSINK_CLASS(klass)                                       \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_ARTNETVIDEOSINK,                  \
                           GstArtnetVideoSinkClass))
#define GST_IS_ARTNETVIDEOSINK(obj)                                            \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_ARTNETVIDEOSINK))
#define GST_IS_ARTNETVIDEOSINK_CLASS(obj)                                      \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_ARTNETVIDEOSINK))

typedef struct _FramePacket {
  int frame;
  GstBuffer *buffer;
} FramePacket;

typedef struct _GstArtnetVideoSink {
  GstVideoSink basesink;
  gboolean silent;
  GstBuffer *pixels;
} GstArtnetVideoSink;

typedef struct _GstArtnetVideoSinkClass {
  // GstBaseSinkClass parent_class;
  GstVideoSinkClass parent_class;
} GstArtnetVideoSinkClass;

GType gst_artnetvideosink_get_type(void);

G_END_DECLS

GST_DEBUG_CATEGORY_STATIC(gst_artnetvideosink_debug);
#define GST_CAT_DEFAULT gst_artnetvideosink_debug

#define DEFAULT_SYNC true
#define DEFAULT_DROP_OUT_OF_SEGMENT TRUE
#define DEFAULT_SILENT false

enum { LAST_SIGNAL };

enum {
  PROP_0,
  PROP_SILENT,
};

static GstStaticPadTemplate sinkpadtemplate =
    GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                            GST_STATIC_CAPS("video/x-raw, "
                                            "format = (string) { RGB }, "
                                            "width = (int) [ 1, 32767 ], "
                                            "height = (int) [ 1, 32767 ]; "));

#define gst_artnetvideosink_parent_class parent_class

/* class initialization */

G_DEFINE_TYPE(GstArtnetVideoSink, gst_artnetvideosink, GST_TYPE_VIDEO_SINK);

static void gst_artnetvideosink_set_property(GObject *object, guint property_id,
                                             const GValue *value,
                                             GParamSpec *pspec);
static void gst_artnetvideosink_get_property(GObject *object, guint property_id,
                                             GValue *value, GParamSpec *pspec);
static void gst_artnetvideosink_finalize(GObject *object);
static GstStateChangeReturn
gst_artnetvideosink_change_state(GstElement *element,
                                 GstStateChange transition);
static GstFlowReturn gst_artnetvideosink_render(GstBaseSink *parent,
                                                GstBuffer *buffer);
static GstFlowReturn gst_artnetvideosink_preroll(GstBaseSink *parent,
                                                 GstBuffer *buffer);
static gboolean gst_artnetvideosink_event(GstBaseSink *parent, GstEvent *event);
static gboolean gst_artnetvideosink_query(GstBaseSink *parent, GstQuery *query);
static gboolean gst_artnetvideosink_start(GstBaseSink *basesink);
static gboolean gst_artnetvideosink_stop(GstBaseSink *basesink);
static gboolean gst_artnetvideosink_set_caps(GstBaseSink *basesink,
                                             GstCaps *caps);

static void gst_artnetvideosink_class_init(GstArtnetVideoSinkClass *klass) {
  printf("init class\n");

  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS(klass);
  GstBaseSinkClass *gstbasesink_class = GST_BASE_SINK_CLASS(klass);
  GstVideoSinkClass *gstvideosink_class = GST_VIDEO_SINK_CLASS(klass);

  gobject_class->set_property = gst_artnetvideosink_set_property;
  gobject_class->get_property = gst_artnetvideosink_get_property;
  gobject_class->finalize = gst_artnetvideosink_finalize;

  gst_element_class_set_static_metadata(gstelement_class, "ArtNet Sink",
                                        "Sink/Artnet", "Description", "Author");
  gst_element_class_add_static_pad_template(gstelement_class, &sinkpadtemplate);

  gstbasesink_class->render = GST_DEBUG_FUNCPTR(gst_artnetvideosink_render);
  gstbasesink_class->preroll = GST_DEBUG_FUNCPTR(gst_artnetvideosink_preroll);
  gstbasesink_class->event = GST_DEBUG_FUNCPTR(gst_artnetvideosink_event);
  gstbasesink_class->query = GST_DEBUG_FUNCPTR(gst_artnetvideosink_query);
  gstbasesink_class->start = GST_DEBUG_FUNCPTR(gst_artnetvideosink_start);
  gstbasesink_class->stop = GST_DEBUG_FUNCPTR(gst_artnetvideosink_stop);
  gstbasesink_class->set_caps = GST_DEBUG_FUNCPTR(gst_artnetvideosink_set_caps);
}

static void gst_artnetvideosink_init(GstArtnetVideoSink *artnet_video_sink) {
  artnet_video_sink->pixels =
      gst_buffer_new_allocate(NULL, 256 * 256 * 3, NULL);
}

static void gst_artnetvideosink_finalize(GObject *obj) {
  GstArtnetVideoSink *artnet_video_sink = GST_ARTNETVIDEOSINK(obj);
  gst_buffer_unref(artnet_video_sink->pixels);
  G_OBJECT_CLASS(parent_class)->finalize(obj);
}

static gboolean gst_artnetvideosink_start(GstBaseSink *basesink) {
  GST_LOG_OBJECT(basesink, "start");
  return TRUE;
}

static gboolean gst_artnetvideosink_stop(GstBaseSink *basesink) {
  GST_LOG_OBJECT(basesink, "stop");
  return TRUE;
}

static gboolean gst_artnetvideosink_set_caps(GstBaseSink *basesink,
                                             GstCaps *caps) {
  GstArtnetVideoSink *sink = GST_ARTNETVIDEOSINK(basesink);
  GstVideoInfo info;

  GstVideoFormat fmt;
  gint w, h, par_n, par_d;
  GST_LOG_OBJECT(sink, "caps: %" GST_PTR_FORMAT, caps);
  if (!gst_video_info_from_caps(&info, caps)) {
    GST_WARNING_OBJECT(sink, "parse_caps failed");
    return FALSE;
  }
  fmt = GST_VIDEO_INFO_FORMAT(&info);
  w = GST_VIDEO_INFO_WIDTH(&info);
  h = GST_VIDEO_INFO_HEIGHT(&info);
  par_n = GST_VIDEO_INFO_PAR_N(&info);
  par_d = GST_VIDEO_INFO_PAR_N(&info);
#ifndef G_DISABLE_ASSERT
  {
    gint s;
    s = GST_VIDEO_INFO_COMP_PSTRIDE(&info, 0);
    g_assert((fmt == GST_VIDEO_FORMAT_RGB && s == 3) ||
             (fmt == GST_VIDEO_FORMAT_RGBA && s == 4));
  }
#endif
  GST_VIDEO_SINK_WIDTH(sink) = w;
  GST_VIDEO_SINK_HEIGHT(sink) = h;
  //  sink->par_n = par_n;
  //  sink->par_d = par_d;
  //  sink->has_alpha = GST_VIDEO_INFO_HAS_ALPHA (&info);
  GST_INFO_OBJECT(sink, "format             : %d", fmt);
  GST_INFO_OBJECT(sink, "width x height     : %d x %d", w, h);
  GST_INFO_OBJECT(sink, "pixel-aspect-ratio : %d/%d", par_n, par_d);
  //  sink->info = info;

  return TRUE;
}

void gst_artnetvideosink_set_property(GObject *object, guint prop_id,
                                      const GValue *value, GParamSpec *pspec) {
  GstArtnetVideoSink *artnet_video_sink = GST_ARTNETVIDEOSINK(object);

  switch (prop_id) {
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  } // end switch prop_id...
}

void gst_artnetvideosink_get_property(GObject *object, guint prop_id,
                                      GValue *value, GParamSpec *pspec) {
  GstArtnetVideoSink *artnet_video_sink = GST_ARTNETVIDEOSINK(object);

  switch (prop_id) {
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  } // end switch prop_id...
}

static GstFlowReturn gst_artnetvideosink_render(GstBaseSink *parent,
                                                GstBuffer *buffer) {
  // printf("_render\n");
  GstArtnetVideoSink *artnet_video_sink = GST_ARTNETVIDEOSINK(parent);

  GstClockTime timestamp = GST_BUFFER_TIMESTAMP(buffer);
  float timestampSeconds = (float)timestamp / 1000000000;
  // printf("start timestamp = %lld (%f)\n", timestamp, timestampSeconds);

  GstClockTime dur = GST_BUFFER_DURATION(buffer);
  float durSeconds = (float)dur / 1000000000;
  // printf("duration = %lld (%f)\n", dur, durSeconds);

  float fps = 1.0f / durSeconds;
  // printf("fps = %f\n", fps);

  GstClockTime frame = timestamp / dur;
  // printf("frame = %lld\n", frame);

  gsize s = gst_buffer_get_size(buffer);
  // printf("r, %d bytes\n", s);

  GstMemory *mem = gst_buffer_get_all_memory(buffer);

  GstMapInfo info;
  gst_memory_map(mem, &info, GST_MAP_READ);

  printf("First pixels: %02X%02X%02X %02X%02X%02X %02X%02X%02X %02X%02X%02X\n",
         info.data[0], info.data[1], info.data[2], info.data[3], info.data[4],
         info.data[5], info.data[6], info.data[7], info.data[8], info.data[9],
         info.data[10], info.data[11]);

  GstBuffer *bufferpacket = gst_buffer_copy(buffer);
  FramePacket *packet = new FramePacket();
  packet->frame = frame;
  packet->buffer = bufferpacket;
  g_async_queue_push(networkQueue, packet);

  gst_memory_unmap(mem, &info);

  gst_memory_unref(mem);

  return GST_FLOW_OK;
}

static GstFlowReturn gst_artnetvideosink_preroll(GstBaseSink *parent,
                                                 GstBuffer *buffer) {
  return gst_artnetvideosink_render(parent, buffer);
}

static gboolean gst_artnetvideosink_event(GstBaseSink *parent,
                                          GstEvent *event) {
  GstArtnetVideoSink *artnet_video_sink = GST_ARTNETVIDEOSINK(parent);
  printf("event %d\n", GST_EVENT_TYPE(event));
  return GST_BASE_SINK_CLASS(parent_class)->event(parent, event);
}

static gboolean gst_artnetvideosink_query(GstBaseSink *parent,
                                          GstQuery *query) {
  gboolean ret;

  switch (GST_QUERY_TYPE(query)) {
  case GST_QUERY_SEEKING: {
    // Seeking is not supported
    GstFormat fmt;
    gst_query_parse_seeking(query, &fmt, NULL, NULL, NULL);
    gst_query_set_seeking(query, fmt, FALSE, 0, -1);
    ret = TRUE;
    break;
  }
  default:
    ret = GST_BASE_SINK_CLASS(parent_class)->query(parent, query);
    break;
  }

  return ret;
}

static GstStateChangeReturn
gst_artnetvideosink_change_state(GstElement *element,
                                 GstStateChange transition) {
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstArtnetVideoSink *artnet_video_sink = GST_ARTNETVIDEOSINK(element);

  ret = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
  return ret;
} // end gst_artnetvideosink_change_state.

static void artnet_sending_thread(GTask *task, gpointer source_object,
                                  gpointer task_data,
                                  GCancellable *cancellable) {
  Player *player = (Player *)task_data;
  GError *error = NULL;

  while (true) {
    gpointer ptr = g_async_queue_timeout_pop(networkQueue, 100000);
    if (ptr != NULL) {
      FramePacket *packet = (FramePacket *)ptr;
      if (packet) {
        if (packet->buffer) {
          gsize sz = gst_buffer_get_size(packet->buffer);
          // printf("Got packet, %d bytes.\n", sz);

          GstMemory *mem = gst_buffer_get_all_memory(packet->buffer);
          GstMapInfo info;
          gst_memory_map(mem, &info, GST_MAP_READ);
          player->handleVideoFrame(packet->frame, info.data, 256, 256);
          gst_memory_unmap(mem, &info);
          gst_memory_unref(mem);

          gst_buffer_unref(packet->buffer);
        }

        delete (packet);
      }
    }
  }
}

/* Structure to contain all our information, so we can pass it around */
typedef struct _CustomData {
  GstElement *playbin; /* Our one and only element */

  gint n_video; /* Number of embedded video streams */
  gint n_audio; /* Number of embedded audio streams */
  gint n_text;  /* Number of embedded subtitle streams */

  gint current_video; /* Currently playing video stream */
  gint current_audio; /* Currently playing audio stream */
  gint current_text;  /* Currently playing subtitle stream */

  GMainLoop *main_loop; /* GLib's Main Loop */
} CustomData;

/* playbin flags */
typedef enum {
  GST_PLAY_FLAG_VIDEO = (1 << 0), /* We want video output */
  GST_PLAY_FLAG_AUDIO = (1 << 1), /* We want audio output */
  GST_PLAY_FLAG_TEXT = (1 << 2)   /* We want subtitle output */
} GstPlayFlags;

/* Forward definition for the message and keyboard processing functions */
static gboolean handle_message(GstBus *bus, GstMessage *msg, CustomData *data);
static gboolean handle_keyboard(GIOChannel *source, GIOCondition cond,
                                CustomData *data);

static gboolean register_artnet_plugin_init(GstPlugin *plugin) {
  int res = gst_element_register(plugin, "artnetvideosink", GST_RANK_NONE,
                                 GST_TYPE_ARTNETVIDEOSINK);
  printf("gst_element_register = %d\n", res);
  return res;
}

/* Extract some metadata from the streams and print it on the screen */
static void analyze_streams(CustomData *data) {
  gint i;
  GstTagList *tags;
  gchar *str;
  guint rate;

  /* Read some properties */
  g_object_get(data->playbin, "n-video", &data->n_video, NULL);
  g_object_get(data->playbin, "n-audio", &data->n_audio, NULL);
  g_object_get(data->playbin, "n-text", &data->n_text, NULL);

  g_print("%d video stream(s), %d audio stream(s), %d text stream(s)\n",
          data->n_video, data->n_audio, data->n_text);

  g_print("\n");
  for (i = 0; i < data->n_video; i++) {
    tags = NULL;
    /* Retrieve the stream's video tags */
    g_signal_emit_by_name(data->playbin, "get-video-tags", i, &tags);
    if (tags) {
      g_print("video stream %d:\n", i);
      gst_tag_list_get_string(tags, GST_TAG_VIDEO_CODEC, &str);
      g_print("  codec: %s\n", str ? str : "unknown");
      g_free(str);
      gst_tag_list_free(tags);
    }
  }

  g_print("\n");
  for (i = 0; i < data->n_audio; i++) {
    tags = NULL;
    /* Retrieve the stream's audio tags */
    g_signal_emit_by_name(data->playbin, "get-audio-tags", i, &tags);
    if (tags) {
      g_print("audio stream %d:\n", i);
      if (gst_tag_list_get_string(tags, GST_TAG_AUDIO_CODEC, &str)) {
        g_print("  codec: %s\n", str);
        g_free(str);
      }
      if (gst_tag_list_get_string(tags, GST_TAG_LANGUAGE_CODE, &str)) {
        g_print("  language: %s\n", str);
        g_free(str);
      }
      if (gst_tag_list_get_uint(tags, GST_TAG_BITRATE, &rate)) {
        g_print("  bitrate: %d\n", rate);
      }
      gst_tag_list_free(tags);
    }
  }

  g_print("\n");
  for (i = 0; i < data->n_text; i++) {
    tags = NULL;
    /* Retrieve the stream's subtitle tags */
    g_signal_emit_by_name(data->playbin, "get-text-tags", i, &tags);
    if (tags) {
      g_print("subtitle stream %d:\n", i);
      if (gst_tag_list_get_string(tags, GST_TAG_LANGUAGE_CODE, &str)) {
        g_print("  language: %s\n", str);
        g_free(str);
      }
      gst_tag_list_free(tags);
    }
  }

  g_object_get(data->playbin, "current-video", &data->current_video, NULL);
  g_object_get(data->playbin, "current-audio", &data->current_audio, NULL);
  g_object_get(data->playbin, "current-text", &data->current_text, NULL);

  g_print("\n");
  g_print(
      "Currently playing video stream %d, audio stream %d and text stream %d\n",
      data->current_video, data->current_audio, data->current_text);
  g_print("Type any number and hit ENTER to select a different audio stream\n");
}

/* Process messages from GStreamer */
static gboolean handle_message(GstBus *bus, GstMessage *msg, CustomData *data) {
  GError *err;
  gchar *debug_info;

  switch (GST_MESSAGE_TYPE(msg)) {
  case GST_MESSAGE_ERROR:
    gst_message_parse_error(msg, &err, &debug_info);
    g_printerr("Error received from element %s: %s\n",
               GST_OBJECT_NAME(msg->src), err->message);
    g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
    g_clear_error(&err);
    g_free(debug_info);
    g_main_loop_quit(data->main_loop);
    break;
  case GST_MESSAGE_EOS: {
    g_print("End-Of-Stream reached.\n");

    /* restart playback if at end */
    if (!gst_element_seek(data->playbin, 1.0, GST_FORMAT_TIME,
                          GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, 0,
                          GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE)) {
      g_print("Seek failed!\n");
    }
  } break;
  case GST_MESSAGE_STATE_CHANGED: {
    GstState old_state, new_state, pending_state;
    gst_message_parse_state_changed(msg, &old_state, &new_state,
                                    &pending_state);
    if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data->playbin)) {
      if (new_state == GST_STATE_PLAYING) {
        /* Once we are in the playing state, analyze the streams */
        analyze_streams(data);
      }
    }
  } break;
  }

  /* We want to keep receiving messages */
  return TRUE;
}

/* Process keyboard input */
static gboolean handle_keyboard(GIOChannel *source, GIOCondition cond,
                                CustomData *data) {
  gchar *str = NULL;

  if (g_io_channel_read_line(source, &str, NULL, NULL, NULL) ==
      G_IO_STATUS_NORMAL) {
    int index = g_ascii_strtoull(str, NULL, 0);
    if (index < 0 || index >= data->n_audio) {
      g_printerr("Index out of bounds\n");
    } else {
      /* If the input was a valid audio stream index, set the current audio
       * stream */
      g_print("Setting current audio stream to %d\n", index);
      g_object_set(data->playbin, "current-audio", index, NULL);
    }
  }
  g_free(str);
  return TRUE;
}

Player::Player(Configuration *configuration, ArtnetSender *sender) {
  this->configuration = configuration;
  this->sender = sender;
  this->lastFrame = 99999999;
}

Player::~Player() {}

void Player::handleVideoFrame(uint16_t frame, uint8_t *pixels, uint16_t width,
                              uint16_t height) {
  if (frame < lastFrame) {
    printf("Player: Rewinded.\n");
    for (int u = 0; u < this->configuration->events.size(); u++) {
      TimedEvent *e = &this->configuration->events[u];
      e->rewind();
    }
  }

  lastFrame = frame;

  printf("Handle video frame #%d\n", frame);
  for (int u = 0; u < this->configuration->events.size(); u++) {
    TimedEvent *e = &this->configuration->events[u];
    e->beforeFrame(frame);
  }

  this->sender->handleVideoFrame(frame, pixels, width, height);
}

int Player::run() {

  CustomData data;
  GstBus *bus;
  GstStateChangeReturn ret;
  gint flags;
  GIOChannel *io_stdin;

  networkQueue = g_async_queue_new();

  gboolean res = gst_plugin_register_static(
      GST_VERSION_MAJOR, GST_VERSION_MINOR, "artnet", "DMX and shit",
      &register_artnet_plugin_init, "1.1", "MIT/X11", "x", "y", "z");

  printf("gst_plugin_register_static = %d\n", res);

  data.playbin = gst_element_factory_make("playbin", NULL);
  if (!data.playbin) {
    g_printerr("Not all elements could be created.\n");
    return -1;
  }

  char buf[1024];
  char buf2[1024];
  realpath(this->configuration->video.c_str(), buf);
  sprintf(buf2, "file://%s", buf);
  printf("Loading \"%s\"\n", buf2);
  g_object_set(data.playbin, "uri", buf2, NULL);

  GstElement *artnetsink =
      gst_element_factory_make("artnetvideosink", "videosink");
  if (!artnetsink) {
    g_printerr("video sink could be created.\n");
    return -1;
  }

  g_object_set(data.playbin, "video-sink", artnetsink, NULL);

  bus = gst_element_get_bus(data.playbin);
  gst_bus_add_watch(bus, (GstBusFunc)handle_message, &data);

  ret = gst_element_set_state(data.playbin, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr("Unable to set the pipeline to the playing state.\n");
    gst_object_unref(data.playbin);
    return -1;
  }

  /* Add a keyboard watch so we get notified of keystrokes */
#ifdef G_OS_WIN32
  io_stdin = g_io_channel_win32_new_fd(fileno(stdin));
#else
  io_stdin = g_io_channel_unix_new(fileno(stdin));
#endif
  g_io_add_watch(io_stdin, G_IO_IN, (GIOFunc)handle_keyboard, &data);

  /* Create a GLib Main Loop and set it to run */
  data.main_loop = g_main_loop_new(NULL, FALSE);

  gpointer user_data = NULL;
  gpointer cake_data = this;
  GTask *task = g_task_new(data.main_loop, NULL, NULL, user_data);
  g_task_set_task_data(task, cake_data,
                       NULL); // (GDestroyNotify) cake_data_free);
  g_task_set_return_on_cancel(task, TRUE);
  g_task_run_in_thread(task, artnet_sending_thread);

  g_main_loop_run(data.main_loop);

  /* Free resources */
  g_main_loop_unref(data.main_loop);
  g_io_channel_unref(io_stdin);
  gst_object_unref(bus);
  gst_element_set_state(data.playbin, GST_STATE_NULL);
  gst_object_unref(data.playbin);

  return 0;
}
