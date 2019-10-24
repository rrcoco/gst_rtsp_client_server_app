#include "RTPServer.h"
#include <gst/gst.h>
#include <opencv2/opencv.hpp>
#include <string>
#include <gst/app/gstappsrc.h>

static auto BPP = 3;
static int FPS = 30;
static int WIDTH = 1280;
static int HEIGHT = 720;
static FFCallBack callback = NULL;
static int VPORT = 5000;
static char* host;

GST_DEBUG_CATEGORY(appsrc_pipeline_debug);
#define GST_CAT_DEFAULT appsrc_pipeline_debug


typedef struct
{
	GstElement* appsrc;
	guint sourceid;
	GstClockTime timestamp;
	GstElement* pipeline;
	GMainLoop* loop;
	guint bus_watch_id;
} MyContext;

static MyContext* ctx;

void RTPInit(int width, int height, int framerate,const char* hostname,int length,int port,FFCallBack cb)
{
	WIDTH = width;
	HEIGHT = height;
	FPS = framerate;
	callback = cb;

	host = new char[length+1];
	strncpy(host, hostname, length);
	host[length] = '\0';

	VPORT = port;
}

/* */
static gboolean bus_call(GstBus* bus, GstMessage* msg, gpointer data) {
	GMainLoop* loop = (GMainLoop*)data;

	switch (GST_MESSAGE_TYPE(msg)) {

	case GST_MESSAGE_EOS:
		g_print("End of stream\n");
		g_main_loop_quit(loop);
		break;

	case GST_MESSAGE_ERROR: {
		gchar* debug;
		GError* error;

		gst_message_parse_error(msg, &error, &debug);
		g_free(debug);

		g_printerr("Error: %s\n", error->message);
		g_error_free(error);

		g_main_loop_quit(loop);

		break;
	}
	default:
		break;
	}

	return TRUE;
}

static gboolean
read_data(MyContext*)
{
	static int count = 0;
	static int index = 0;

	GstBuffer* buffer;
	guint size;
	GstFlowReturn ret;
	GstMapInfo map;

	index = (count++) % 700;

	auto name = "frame" + std::to_string(index) + ".jpg";

	cv::Mat image = cv::imread("C:\\Users\\Hakan\\Downloads\\output\\" + name);
	cv::cvtColor(image, image, cv::COLOR_BGR2RGB);

	size_t sizeInBytes = image.total() * image.elemSize();

	size = WIDTH * HEIGHT * BPP;

	assert(size == sizeInBytes);

	buffer = gst_buffer_new_allocate(NULL, size, NULL);

	gst_buffer_map(buffer, &map, GST_MAP_WRITE);

	memcpy((guchar*)map.data, image.data, gst_buffer_get_size(buffer));

	GST_BUFFER_PTS(buffer) = ctx->timestamp;
	GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale_int(1, GST_SECOND, FPS);	//(1.0/FPS)*(1000000000);

	ctx->timestamp += GST_BUFFER_DURATION(buffer);

	g_signal_emit_by_name(ctx->appsrc, "push-buffer", buffer, &ret);

	gst_buffer_unmap(buffer, &map);
	gst_buffer_unref(buffer);

	g_print("%d\n",count);

	return TRUE;
}

/* This signal callback is called when appsrc needs data, we add an idle handler
 * to the mainloop to start pushing data into the appsrc */
static void
start_feed(GstElement* pipeline, guint size, MyContext* ctx)
{
	if (ctx->sourceid == 0) {
		GST_DEBUG("start feeding");
		ctx->sourceid = g_idle_add((GSourceFunc)read_data, ctx);
	}
}

/* This callback is called when appsrc has enough data and we can stop sending.
 * We remove the idle handler from the mainloop */
static void
stop_feed(GstElement* pipeline, MyContext* ctx)
{
	if (ctx->sourceid != 0) {
		GST_DEBUG("stop feeding");
		g_source_remove(ctx->sourceid);
		ctx->sourceid = 0;
	}
}

/* */
void RTPStreamVideo()
{
	GstElement* vsink;
	GstBus* bus;
	
	GST_DEBUG_CATEGORY_INIT(appsrc_pipeline_debug, "appsrc-pipeline", 0,
		"appsrc pipeline example");

	gst_init(nullptr, nullptr);
	
	ctx = g_new0(MyContext, 1);
	ctx->timestamp = 0;

	ctx->loop = g_main_loop_new(NULL, FALSE);

	ctx->pipeline = gst_parse_launch("appsrc name=mysrc max-latency=100 block=true ! \
									  videoconvert ! x264enc ! h264parse ! rtph264pay name=pay0 pt=96 ! \
									  udpsink name=vsink host=127.0.0.1 port=5000 ", NULL);

	g_assert(ctx->pipeline);

	ctx->appsrc = gst_bin_get_by_name(GST_BIN(ctx->pipeline), "mysrc");
	
	g_assert(ctx->appsrc);
	g_assert(GST_IS_APP_SRC(ctx->appsrc));

	g_signal_connect(ctx->appsrc, "need-data", G_CALLBACK(start_feed), ctx);
	g_signal_connect(ctx->appsrc, "enough-data", G_CALLBACK(stop_feed), ctx);

	bus = gst_pipeline_get_bus(GST_PIPELINE(ctx->pipeline));
	ctx->bus_watch_id = gst_bus_add_watch(bus, (GstBusFunc)bus_call, ctx->loop);

	vsink = gst_bin_get_by_name(GST_BIN(ctx->pipeline), "vsink");
	
	g_assert(vsink);

	g_object_set(G_OBJECT(ctx->appsrc), "caps",
		gst_caps_new_simple("video/x-raw",
			"format", G_TYPE_STRING, "RGB",
			"width", G_TYPE_INT, WIDTH,
			"height", G_TYPE_INT, HEIGHT,
			"framerate", GST_TYPE_FRACTION, FPS, 1,
			NULL), NULL);

	g_object_set(G_OBJECT(vsink), "host", host, NULL);
	g_object_set(G_OBJECT(vsink), "port", VPORT, NULL);

	gst_element_set_state(ctx->pipeline, GST_STATE_PLAYING);

	g_main_loop_run(ctx->loop);
}

void RTPFeedData(unsigned char* data, int sizeInBytes)
{
}

void RTSClose()
{
	gst_element_set_state(ctx->pipeline, GST_STATE_NULL);
	gst_object_unref(GST_OBJECT(ctx->pipeline));
	g_source_remove(ctx->bus_watch_id);
	g_main_loop_unref(ctx->loop);
}
