#include "Client.h"
#include <gst/gst.h>
#include <gst/sdp/sdp.h>
#include <gst/app/gstappsink.h>
#include <gio/gio.h>
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <string>
#include <memory>

static constexpr auto  MAXBUF = 256;
static constexpr auto DELIM = "=";
static constexpr auto BPP = 3;
static constexpr auto SINK_MAX_BUFFER_COUNT = 1;

static auto WIDTH = 1280;
static auto HEIGHT = 720;
static auto FPS = 20;

static GMainLoop* gst_loop;
static GstBus* gst_bus;
static GstElement* gst_pipeline;
static GstElement* gst_rtspsrc;
static GstCaps* gst_video_caps;
static GstElement* gst_app_sink;

static std::unique_ptr<unsigned char[]> frame = nullptr;
static gchar rtsp_link[MAXBUF];

static event_cb_t on_stream_start_cb, on_stream_stop_cb;
static frame_cb_t on_frame_sample_cb;

static constexpr bool D3VIDEOSINK = false;

struct Config
{
	bool init = false;

	char rtsp_server_address[MAXBUF];
	char rtsp_server_port[MAXBUF];
	char rtsp_server_mount_point[MAXBUF];
	char rtsp_server_username[MAXBUF];
	char rtsp_server_password[MAXBUF];
	char rtsp_server_width[MAXBUF];
	char rtsp_server_height[MAXBUF];
	char rtsp_server_fps[MAXBUF];
	char udp_video_port[MAXBUF];
	char udp_audio_port[MAXBUF];
};

static Config config;

void LoadConfig(const char* confFile)
{
	FILE* file = fopen(confFile, "r");

	if (file != NULL)
	{
		config.init = true;

		char line[MAXBUF];
		int i = 0;

		while (fgets(line, sizeof(line), file) != NULL)
		{
			//Remove the trailing new lines
			line[strcspn(line, "\r\n")] = 0;
			
			char* cfline;
			cfline = strstr((char*)line, DELIM);
			cfline = cfline + strlen(DELIM);

			if (i == 0)
				memcpy(config.rtsp_server_address, cfline, strlen(cfline));
			else if (i == 1)
				memcpy(config.rtsp_server_port, cfline, strlen(cfline));
			else if (i == 2)
				memcpy(config.rtsp_server_mount_point, cfline, strlen(cfline));
			else if (i == 3)
				memcpy(config.rtsp_server_username, cfline, strlen(cfline));
			else if (i == 4)
				memcpy(config.rtsp_server_password, cfline, strlen(cfline));
			else if (i == 5)
				memcpy(config.rtsp_server_width, cfline, strlen(cfline));
			else if (i == 6)
				memcpy(config.rtsp_server_height, cfline, strlen(cfline));
			else if (i == 7)
				memcpy(config.rtsp_server_fps, cfline, strlen(cfline));
			else if (i == 8)
				memcpy(config.udp_video_port, cfline, strlen(cfline));
			else if (i == 9)
				memcpy(config.udp_audio_port, cfline, strlen(cfline));
			
			i++;
		} // End while
		fclose(file);
	} // End if file
}

/* */
static gboolean bus_call(GstBus* bus, GstMessage* msg, gpointer data) {
	GMainLoop* loop = (GMainLoop*)data;
	switch (GST_MESSAGE_TYPE(msg)) {
	case GST_MESSAGE_EOS:
		g_print("Stream Ends\n");
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

/* */
static void on_sdp_callback(GstElement  rtspsrc,
	GstSDPMessage  sdp,
	gpointer udata) {

	g_print("stream starting\n");
	
	on_stream_start_cb();
}

/* */
void Init(event_cb_t start_cb, event_cb_t stop_cb, frame_cb_t frame_cb, const char* conf_file)
{
	LoadConfig(conf_file);
	
	WIDTH = atoi(config.rtsp_server_width);
	HEIGHT = atoi(config.rtsp_server_height);
	FPS = atoi(config.rtsp_server_fps);

	const auto size = WIDTH * HEIGHT * BPP;
	frame = std::make_unique<unsigned char[]>(size);

	on_stream_start_cb = start_cb;
	on_stream_stop_cb = stop_cb;
	on_frame_sample_cb = frame_cb;
}

/* The appsink has received a buffer */
static GstFlowReturn new_sample(GstElement* sink,gpointer ptr) {
	GstSample* sample;
	 
	/* Retrieve the buffer */
	g_signal_emit_by_name(sink, "pull-sample", &sample);

	if (sample) {
		GstBuffer* buffer = gst_sample_get_buffer(sample);
		GstMapInfo gst_map;

		gst_buffer_map(buffer, &gst_map, GST_MAP_READ);
		memcpy(frame.get(), (char*)gst_map.data, gst_map.size);
		
		gst_buffer_unmap(buffer, &gst_map);
		gst_sample_unref(sample);

		on_frame_sample_cb(frame.get(),WIDTH,HEIGHT,BPP);

		return GST_FLOW_OK;
	}

	return GST_FLOW_ERROR;
}

/* */
void RTSPStream(bool video_enabled, bool audio_enabled, bool auth)
{
	assert(config.init);

	/* Initializing GStreamer */
	gst_init(nullptr, nullptr);

	gst_loop = g_main_loop_new(NULL, FALSE);

	snprintf(rtsp_link, MAXBUF, "rtsp://%s:%s%s",config.rtsp_server_address, config.rtsp_server_port, config.rtsp_server_mount_point);

	std::string pipeline_str = "rtspsrc latency=0 name=demux ";

	if (video_enabled && audio_enabled)
		pipeline_str += "demux. ! queue ! rtppcmadepay ! alawdec ! audioconvert ! audioresample ! autoaudiosink demux. ! queue ! rtph264depay ! h264parse ! avdec_h264 ! videoconvert ! appsink name=mysink sync=false ";
	else if (video_enabled)
		pipeline_str += "! queue ! rtph264depay ! avdec_h264 ! videoconvert !  appsink name=mysink sync=false";
	else if (audio_enabled)
		pipeline_str += "! queue ! rtppcmadepay ! alawdec ! audioconvert ! audioresample ! autoaudiosink";
	else
		exit(-1);

	/* Create Pipe's Elements */
	gst_pipeline = gst_parse_launch(pipeline_str.data(), NULL);
	g_assert(gst_pipeline);

	gst_rtspsrc = gst_bin_get_by_name(GST_BIN(gst_pipeline), "demux");
	g_assert(gst_rtspsrc);

	
	if (video_enabled && !D3VIDEOSINK) {
		gst_app_sink = gst_bin_get_by_name(GST_BIN(gst_pipeline), "mysink");
		g_assert(gst_app_sink);

		//gst_app_sink_set_max_buffers(GST_APP_SINK(gst_app_sink), SINK_MAX_BUFFER_COUNT);

		gst_video_caps = gst_caps_new_simple("video/x-raw",
			"format", G_TYPE_STRING, "RGB",
			"width", G_TYPE_INT, WIDTH,
			"height", G_TYPE_INT, HEIGHT,
			"bpp", G_TYPE_INT, BPP * 8, NULL);

		g_object_set(gst_app_sink, "emit-signals", TRUE, "caps", gst_video_caps, NULL);
		g_signal_connect(gst_app_sink, "new-sample", G_CALLBACK(new_sample), NULL);

		gst_caps_unref(gst_video_caps);
		gst_object_unref(gst_app_sink);
	}

	if (!g_signal_connect(gst_rtspsrc, "on-sdp", G_CALLBACK(on_sdp_callback), gst_rtspsrc))
		g_warning("Linking part (1) with part (A)-1 Fail...");

	/* Set video Source */
	g_object_set(G_OBJECT(gst_rtspsrc), "location", rtsp_link, NULL);
	g_object_set(G_OBJECT(gst_rtspsrc), "latency", 0, NULL);

	if (auth) {
		g_object_set(G_OBJECT(gst_rtspsrc), "user-id", config.rtsp_server_username, NULL);
		g_object_set(G_OBJECT(gst_rtspsrc), "user-pw", config.rtsp_server_password, NULL);
	}
	
	gst_object_unref(gst_rtspsrc);

	/* Putting a Message handler */
	gst_bus = gst_pipeline_get_bus(GST_PIPELINE(gst_pipeline));
	gst_bus_add_watch(gst_bus, bus_call, gst_loop);
	gst_object_unref(gst_bus);

	/* Run the pipeline */
	gst_element_set_state(gst_pipeline, GST_STATE_PLAYING);

	g_main_loop_run(gst_loop);
}

/* */
void RTSPClose()
{
	/* Ending Playback */
	g_print("End of the Streaming... ending the playback\n");
	gst_element_set_state(gst_pipeline, GST_STATE_NULL);

	/* Eliminating Pipeline */
	g_print("Eliminating Pipeline\n");
	gst_object_unref(GST_OBJECT(gst_pipeline));

	on_stream_stop_cb();
}

static gboolean
bus_message(GstBus* bus, GstMessage* message, gpointer data)
{
	GST_DEBUG("got message %s",
		gst_message_type_get_name(GST_MESSAGE_TYPE(message)));

	switch (GST_MESSAGE_TYPE(message)) {
	case GST_MESSAGE_ERROR: {
		GError* err = NULL;
		gchar* dbg_info = NULL;

		gst_message_parse_error(message, &err, &dbg_info);
		g_printerr("ERROR from element %s: %s\n", GST_OBJECT_NAME(message->src), err->message);
		g_printerr("Debugging info: %s\n", (dbg_info) ? dbg_info : "none");
		g_error_free(err);
		g_free(dbg_info);
		g_main_loop_quit(gst_loop);
		break;
	}

	case GST_MESSAGE_EOS:
		g_main_loop_quit(gst_loop);
		break;
	default:
		break;
	}

	return TRUE;
}


/* */
void UDPStream(bool video_enabled, bool audio_enabled)
{
	assert(config.init);
	assert(video_enabled | audio_enabled);
	
	gst_init(nullptr, nullptr);

	gst_loop = g_main_loop_new(NULL, FALSE);

	std::string pipeline_str = "";

	if (video_enabled)
		pipeline_str += "udpsrc name=vsrc ! queue ! rtph264depay ! avdec_h264 max-threads= 4 ! videoconvert ! appsink name=mysink sync=false ";

	if (audio_enabled)
		pipeline_str += "udpsrc name=asrc ! queue ! rtppcmadepay ! alawdec ! playsink ";

	gst_pipeline = gst_parse_launch(pipeline_str.data(), NULL);
	g_assert(gst_pipeline);

	auto gst_bus = gst_pipeline_get_bus(GST_PIPELINE(gst_pipeline));
	g_assert(gst_bus);

	gst_bus_add_watch(gst_bus, (GstBusFunc)bus_message, nullptr);
	gst_object_unref(gst_bus);

	if (video_enabled) {

		assert(strlen(config.udp_video_port) != 0);

		auto audiosrc = gst_bin_get_by_name(GST_BIN(gst_pipeline), "vsrc");
		g_assert(audiosrc);

		g_object_set(G_OBJECT(audiosrc), "caps",
			gst_caps_new_simple("application/x-rtp",
				"media", G_TYPE_STRING, "video",
				"payload", G_TYPE_INT, 96,
				"clock-rate", G_TYPE_INT, 90000, NULL),
			"port", atoi(config.udp_video_port),
			"do-timestamp", true, NULL);

		gst_app_sink = gst_bin_get_by_name(GST_BIN(gst_pipeline), "mysink");
		g_assert(gst_app_sink);

		gst_video_caps = gst_caps_new_simple("video/x-raw",
			"format", G_TYPE_STRING, "RGB",
			"width", G_TYPE_INT, WIDTH,
			"height", G_TYPE_INT, HEIGHT,
			"bpp", G_TYPE_INT, BPP * 8, NULL);

		g_object_set(gst_app_sink, "emit-signals", TRUE, "caps", gst_video_caps, NULL);
		g_signal_connect(gst_app_sink, "new-sample", G_CALLBACK(new_sample), NULL);
		
		gst_caps_unref(gst_video_caps);
		gst_object_unref(gst_app_sink);

	}

	if (audio_enabled) {

		assert(strlen(config.udp_audio_port) != 0);

		auto audiosrc = gst_bin_get_by_name(GST_BIN(gst_pipeline), "asrc");
		g_assert(audiosrc);

		g_object_set(G_OBJECT(audiosrc), "caps",
			gst_caps_new_simple("application/x-rtp",
				"format", G_TYPE_STRING, "S32LE",
				"media", G_TYPE_STRING, "audio",
				"encoding-name", G_TYPE_STRING, "PCMA",
				"layout", G_TYPE_STRING, "interleaved",
				"clock-rate", G_TYPE_INT, 8000, NULL),
			"port", atoi(config.udp_audio_port),
			"do-timestamp", true, NULL);
	}

	gst_element_set_state(gst_pipeline, GST_STATE_PLAYING);

	on_stream_start_cb();

	g_main_loop_run(gst_loop);
}

/* */
void UDPClose()
{
	on_stream_stop_cb();

	gst_element_set_state(gst_pipeline, GST_STATE_NULL);
	g_main_loop_unref(gst_loop);
}
