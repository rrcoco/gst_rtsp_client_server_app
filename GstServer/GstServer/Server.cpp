#include "Server.h"
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <gst/app/gstappsrc.h>
#include <string>
#include <cassert>
#include <gst/rtp/gstrtpbuffer.h>

/* Default values */
static auto BPP = 3;
static auto WIDTH = 1280;
static auto HEIGHT = 720;
static auto FPS = 20;
static auto BITRATE = 2048;

static constexpr auto  MAXBUF = 256;
static constexpr auto DELIM = "=";

struct MyContext
{
	GstElement* appsrc;
	guint sourceid;
	GstClockTime timestamp;
};

struct Config
{
	bool init = false;
	char rtsp_server_port[MAXBUF];
	char rtsp_server_mount_point[MAXBUF];
	char rtsp_server_username[MAXBUF];
	char rtsp_server_password[MAXBUF];
	char rtsp_server_width[MAXBUF];
	char rtsp_server_height[MAXBUF];
	char rtsp_server_fps[MAXBUF];
	char rtsp_server_bitrate[MAXBUF];
	char udp_address[MAXBUF];
	char udp_video_port[MAXBUF];
	char udp_audio_port[MAXBUF];
};

static Config config;
static MyContext* ctx;

static GstRTSPServer* gst_server;
static GstRTSPMountPoints* gst_mounts;
static GstRTSPMediaFactory* gst_factory;
static GstRTSPAuth* gst_auth;
static GstRTSPToken* gst_token;
static gchar* gst_basic;
static guint gst_server_id;
static GstRTSPPermissions* gst_permissions;
static GMainLoop* gst_loop;
static GstElement* gst_pipeline;

static event_cb_t on_receive_frame_cb;

void LoadConfig(const char* filename)
{
	FILE* file = fopen(filename, "r");

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
				memcpy(config.rtsp_server_port, cfline, strlen(cfline));
			else if (i == 1)
				memcpy(config.rtsp_server_mount_point, cfline, strlen(cfline));
			else if (i == 2)
				memcpy(config.rtsp_server_username, cfline, strlen(cfline));
			else if (i == 3)
				memcpy(config.rtsp_server_password, cfline, strlen(cfline));
			else if (i == 4)
				memcpy(config.rtsp_server_width, cfline, strlen(cfline));
			else if (i == 5)
				memcpy(config.rtsp_server_height, cfline, strlen(cfline));
			else if (i == 6)
				memcpy(config.rtsp_server_fps, cfline, strlen(cfline));
			else if (i == 7)
				memcpy(config.rtsp_server_bitrate, cfline, strlen(cfline));
			else if (i == 8)
				memcpy(config.udp_address, cfline, strlen(cfline));
			else if (i == 9)
				memcpy(config.udp_video_port, cfline, strlen(cfline));
			else if (i == 10)
				memcpy(config.udp_audio_port, cfline, strlen(cfline));

			i++;
		} // End while
		
		fclose(file);
	} // End if file
}

static gboolean
remove_func(GstRTSPSessionPool* pool, GstRTSPSession* session,
	GstRTSPServer* server)
{
	return GST_RTSP_FILTER_REMOVE;
}

static gboolean
remove_sessions(GstRTSPServer* server)
{
	GstRTSPSessionPool* pool;

	g_print("removing all sessions\n");
	pool = gst_rtsp_server_get_session_pool(server);
	gst_rtsp_session_pool_filter(pool,
		(GstRTSPSessionPoolFilterFunc)remove_func, server);
	g_object_unref(pool);

	return FALSE;
}

static gboolean
timeout(GstRTSPServer* server)
{
	GstRTSPSessionPool* pool;

	pool = gst_rtsp_server_get_session_pool(server);
	gst_rtsp_session_pool_cleanup(pool);
	g_object_unref(pool);

	return TRUE;
}

static gboolean need_data(MyContext*)
{
	on_receive_frame_cb();

	return TRUE;
}

/* start to feed the buffer */
static void start_feed(GstElement* pipeline, guint size, MyContext* ctx)
{
	if (ctx->sourceid == 0) {
		g_print("start feeding\n");
		ctx->sourceid = g_idle_add((GSourceFunc)need_data, ctx);
	}
}

/* wait for to empty the buffer*/
static void stop_feed(GstElement* pipeline, MyContext* ctx)
{
	if (ctx->sourceid != 0) {
		g_print("stop feeding\n");
		g_source_remove(ctx->sourceid);
		ctx->sourceid = 0;
	}
}

/* called when a new media pipeline is constructed. We can query the
 * pipeline and configure our appsrc */
static void media_configure(GstRTSPMediaFactory* factory, GstRTSPMedia* media, gpointer user_data)
{
	GstElement* element;

	ctx = g_new0(MyContext, 1);
	ctx->timestamp = 0;

	/* get the element used for providing the streams of the media */
	element = gst_rtsp_media_get_element(media);

	/* get our appsrc, we named it 'mysrc' with the name property */
	ctx->appsrc = gst_bin_get_by_name_recurse_up(GST_BIN(element), "mysrc");

	/* this instructs appsrc that we will be dealing with timed buffer */
	gst_util_set_object_arg(G_OBJECT(ctx->appsrc), "format", "time");

	/* configure the caps of the video */
	g_object_set(G_OBJECT(ctx->appsrc), "caps",
		gst_caps_new_simple("video/x-raw",
			"format", G_TYPE_STRING, "RGB",
			"width", G_TYPE_INT, WIDTH,
			"height", G_TYPE_INT, HEIGHT,
			"framerate", GST_TYPE_FRACTION, FPS, 1, NULL), NULL);

	/* make sure ther datais freed when the media is gone */
	g_object_set_data_full(G_OBJECT(media), "my-extra-data", ctx, (GDestroyNotify)g_free);

	/* update bitrate of stream */
	auto x264enc = gst_bin_get_by_name_recurse_up(GST_BIN(element), "enc");
	g_assert(x264enc);
	g_object_set(G_OBJECT(x264enc), "bitrate", BITRATE, NULL);
	g_object_unref(x264enc);
	
	g_signal_connect(ctx->appsrc, "need-data", G_CALLBACK(start_feed), ctx);
	g_signal_connect(ctx->appsrc, "enough-data", G_CALLBACK(stop_feed), ctx);

	gst_object_unref(element);
}

/*	Frames will be pushed into pipeline with this method
	TODO: 24bpp rgb frames will be pushed.
	Size information will be checked.
*/
void FeedData(unsigned char* data, int sizeInBytes)
{
	static guint size = WIDTH * HEIGHT * BPP;

	GstBuffer* buffer;
	GstFlowReturn ret;
	GstMapInfo map;

	assert(size == sizeInBytes);

	buffer = gst_buffer_new_allocate(NULL, size, NULL);

	gst_buffer_map(buffer, &map, GST_MAP_WRITE);
	memcpy((guchar*)map.data, data, gst_buffer_get_size(buffer));
	
	GST_BUFFER_PTS(buffer) = ctx->timestamp;
	GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale_int(1, GST_SECOND, FPS);	//(1.0/FPS)*(1000000000);

	ctx->timestamp += GST_BUFFER_DURATION(buffer);

	auto sizex = gst_buffer_get_size(buffer);

	g_signal_emit_by_name(ctx->appsrc, "push-buffer", buffer, &ret);

	gst_buffer_unmap(buffer, &map);
	gst_buffer_unref(buffer);
}

/* Before starting streaming init the mechanism */
void Init(event_cb_t frame_feeder_cb, const char* confFile)
{
	on_receive_frame_cb = frame_feeder_cb;

	LoadConfig(confFile);

	assert(config.init);

	WIDTH = atoi(config.rtsp_server_width);
	HEIGHT = atoi(config.rtsp_server_height);
	FPS = atoi(config.rtsp_server_fps);
	BITRATE = atoi(config.rtsp_server_bitrate);
}

/* Streaming both video and audio */
/* TODO: Audio src should be directsoundsrc => otherwise it will destroy image */
void RTSPStream(bool video_enabled, bool audio_enabled, bool auth)
{
	assert(config.init);
	assert(video_enabled | audio_enabled);

	gst_init(nullptr, nullptr);

	gst_loop = g_main_loop_new(NULL, FALSE);

	gst_server = gst_rtsp_server_new();
	gst_rtsp_server_set_service(gst_server, config.rtsp_server_port); //set the port #

	if (auth) {
		/* make a new authentication manager. it can be added to control access to all the factories on the server or on individual factories. */
		gst_auth = gst_rtsp_auth_new();

		/* make user token */
		gst_token = gst_rtsp_token_new(GST_RTSP_TOKEN_MEDIA_FACTORY_ROLE, G_TYPE_STRING, config.rtsp_server_username, NULL); //"user", NULL);
		gst_basic = gst_rtsp_auth_make_basic(config.rtsp_server_username, config.rtsp_server_password);
		//gst_basic = gst_rtsp_auth_make_basic ("user", "password");

		gst_rtsp_auth_add_basic(gst_auth, gst_basic, gst_token);
		g_free(gst_basic);
		gst_rtsp_token_unref(gst_token);

		/* configure in the server */
		gst_rtsp_server_set_auth(gst_server, gst_auth);
	}

	gst_mounts = gst_rtsp_server_get_mount_points(gst_server);
	gst_factory = gst_rtsp_media_factory_new();

	std::string pipeline_str = "(";

	// noise-reduction=10000 tune=zerolatency byte-stream=true threads=4 key-int-max=15 intra-refresh=true pass=5 quantizer=22 speed-preset=4 
	if (video_enabled) {
		pipeline_str += " appsrc name=mysrc block=true ! videoconvert ! x264enc name=enc tune=zerolatency speed-preset=4 pass=5 threads=4 ! rtph264pay name=pay0 pt=96 ";
	}
		
	if (audio_enabled) {
		pipeline_str += " directsoundsrc ! audio/x-raw, rate=8000 ! alawenc ! rtppcmapay";

		if (video_enabled)
			pipeline_str += " name=pay1 pt=97 ";
		else
			pipeline_str += " name=pay0 pt=96 ";
	}

	pipeline_str += ")";

	/* audio src on windows = directsoundsrc. ! autoaudiosrc might crash video stream */
	gst_rtsp_media_factory_set_launch(gst_factory, pipeline_str.data());

	//directsoundsrc ! audio/x-raw, rate=8000 ! alawenc ! rtppcmapay name = pay1 pt = 97
	//directsoundsrc ! audioresample ! audioconvert ! rtpL24pay name=pay1 pt=97

	if (video_enabled)
		g_signal_connect(gst_factory, "media-configure", (GCallback)media_configure, NULL);

	if (auth) {
		/* add permissions for the user media role */
		gst_permissions = gst_rtsp_permissions_new();
		gst_rtsp_permissions_add_role(gst_permissions, config.rtsp_server_username,
			//gst_rtsp_permissions_add_role (gst_permissions, "user",
			GST_RTSP_PERM_MEDIA_FACTORY_ACCESS, G_TYPE_BOOLEAN, TRUE,
			GST_RTSP_PERM_MEDIA_FACTORY_CONSTRUCT, G_TYPE_BOOLEAN, TRUE, NULL);
		gst_rtsp_media_factory_set_permissions(gst_factory, gst_permissions);
		gst_rtsp_permissions_unref(gst_permissions);
	}

	/* attach the test factory to the /test url */
	gst_rtsp_mount_points_add_factory(gst_mounts, config.rtsp_server_mount_point, gst_factory);
	//gst_rtsp_mount_points_add_factory(gst_mounts, "/test", gst_factory);

	/* don't need the ref to the mounts anymore */
	g_object_unref(gst_mounts);

	/* attach the server to the default maincontext */
	if ((gst_server_id = gst_rtsp_server_attach(gst_server, NULL)) == 0)
		exit(-1);

	/* add a timeout for the session cleanup */
	//g_timeout_add_seconds(2, (GSourceFunc)timeout, gst_server);
	//g_timeout_add_seconds(10, (GSourceFunc)remove_sessions, gst_server);

	/* start serving */
	if (auth)
		g_print("stream ready at rtsp://%s:%s@127.0.0.1:%s%s\n",
			config.rtsp_server_username,
			config.rtsp_server_password,
			config.rtsp_server_port,
			config.rtsp_server_mount_point);
	else
		g_print("stream ready at rtsp://127.0.0.1:%s%s\n",
			config.rtsp_server_port,
			config.rtsp_server_mount_point);

	g_main_loop_run(gst_loop);
}

/*	Close the pipeline */
void RTSPClose() {
	g_source_remove(gst_server_id);
	g_main_loop_quit(gst_loop);
}

static gboolean
bus_message(GstBus* bus, GstMessage* message, MyContext* ctx)
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

	ctx = g_new0(MyContext, 1);
	ctx->timestamp = 0;

	gst_loop = g_main_loop_new(NULL, FALSE);

	std::string pipeline_str = "";

	if (video_enabled)
		pipeline_str += "appsrc name=mysrc block=true ! videoconvert ! queue ! x264enc name=enc tune=zerolatency speed-preset=4 pass=5 threads=4 ! rtph264pay pt=96 ! udpsink name=vsink ";

	if (audio_enabled)
		pipeline_str += "directsoundsrc do-timestamp=true ! queue ! audioconvert ! audioresample ! alawenc ! rtppcmapay ! udpsink name=asink ";

	gst_pipeline = gst_parse_launch(pipeline_str.data(), NULL);
	g_assert(gst_pipeline);

	auto gst_bus = gst_pipeline_get_bus(GST_PIPELINE(gst_pipeline));
	g_assert(gst_bus);

	gst_bus_add_watch(gst_bus, (GstBusFunc)bus_message, ctx);
	gst_object_unref(gst_bus);

	if (video_enabled) {

		assert(strlen(config.udp_address) != 0);
		assert(strlen(config.udp_video_port) != 0);

		ctx->appsrc = gst_bin_get_by_name(GST_BIN(gst_pipeline), "mysrc");
		g_assert(ctx->appsrc);

		g_object_set(G_OBJECT(ctx->appsrc), "caps",
			gst_caps_new_simple("video/x-raw",
				"format", G_TYPE_STRING, "RGB",
				"width", G_TYPE_INT, WIDTH,
				"height", G_TYPE_INT, HEIGHT,
				"framerate", GST_TYPE_FRACTION, FPS, 1, NULL),
			"format", GST_FORMAT_TIME, NULL);

		g_signal_connect(ctx->appsrc, "need-data", G_CALLBACK(start_feed), ctx);
		g_signal_connect(ctx->appsrc, "enough-data", G_CALLBACK(stop_feed), ctx);

		auto video_sink = gst_bin_get_by_name(GST_BIN(gst_pipeline), "vsink");
		g_assert(video_sink);
		g_object_set(G_OBJECT(video_sink), "host", config.udp_address, "port", atoi(config.udp_video_port), NULL);
		g_object_unref(video_sink);

		/* update bitrate of stream */
		auto x264enc = gst_bin_get_by_name_recurse_up(GST_BIN(gst_pipeline), "enc");
		g_assert(x264enc);
		g_object_set(G_OBJECT(x264enc), "bitrate", BITRATE, NULL);
		g_object_unref(x264enc);
	}

	if (audio_enabled) {

		assert(strlen(config.udp_address) != 0);
		assert(strlen(config.udp_audio_port) != 0);

		auto audio_sink = gst_bin_get_by_name(GST_BIN(gst_pipeline), "asink");
		g_assert(audio_sink);
		g_object_set(G_OBJECT(audio_sink), "host", config.udp_address, "port", atoi(config.udp_audio_port), NULL);
		g_object_unref(audio_sink);
	}

	gst_element_set_state(gst_pipeline, GST_STATE_PLAYING);

	g_main_loop_run(gst_loop);
}

/* */
void UDPStop()
{
	gst_element_set_state(gst_pipeline, GST_STATE_NULL);
	g_main_loop_unref(gst_loop);
}
