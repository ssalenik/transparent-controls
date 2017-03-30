#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <gst/gst.h>

static gboolean
bus_call (GstBus     *,
          GstMessage *msg,
          gpointer    )
{
    // g_print ("Got %s message\n", GST_MESSAGE_TYPE_NAME (msg));

    switch (GST_MESSAGE_TYPE (msg)) {

    case GST_MESSAGE_EOS:
        g_print ("GST End of stream\n");
        // g_main_loop_quit (loop);
        break;

    case GST_MESSAGE_ERROR: {
        gchar  *debug;
        GError *error;

        gst_message_parse_error (msg, &error, &debug);

        g_printerr ("GST Error (%s): %s (%s)", GST_OBJECT_NAME(msg->src), error->message, debug);
        g_free (debug);
        g_error_free (error);

        //g_main_loop_quit (loop);
        break;
    }
    default:
        break;
    }

    return TRUE;
}

static void
do_gstreamer_pipeline()
{
    GstElement *pipeline = gst_pipeline_new("camera");
    GstElement *v4l2 = gst_element_factory_make ("v4l2src", "v4l2src");
    GstElement *vpudec = gst_element_factory_make ("vpudec", "vpudec");
    GstElement *queue = gst_element_factory_make ("queue", "queue");
    GstElement *videoconvert = gst_element_factory_make ("imxvideoconvert_g2d", "imxvideoconvert_g2d");
    GstElement *sink = gst_element_factory_make ("imxg2dvideosink", "imxg2dvideosink");

    GstCaps *filter1 = gst_caps_new_simple ("image/jpeg",
        "width", G_TYPE_STRING, "1280",
        "height", G_TYPE_STRING, "720",
        "framerate", G_TYPE_STRING, "30/1",
        NULL
    );

    GstCaps *filter2 = gst_caps_new_simple ("video/x-raw",
        "format", G_TYPE_STRING, "RGB16",
        NULL
    );

    if (!pipeline || !v4l2 || !filter1 || !vpudec || !queue || !videoconvert || !filter2 || !sink) {
        g_printerr ("One element could not be created. Exiting.\n");
        exit(-1);
    }

    /* we add a message handler */
    GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    gst_bus_add_watch (bus, bus_call, nullptr);
    gst_object_unref (bus);

    /* configure sink */
    g_object_set (sink, "use-vsync", TRUE, NULL);

    /* we add all elements into the pipeline */
    gst_bin_add_many (GST_BIN(pipeline), v4l2, vpudec, queue, videoconvert, sink, NULL);

    if (!gst_element_link_filtered(v4l2, vpudec, filter1)) {
        g_printerr ("Could not link v4l2 caps.\n");
        exit(-1);
    }

    if (!gst_element_link_many (vpudec, queue, videoconvert, NULL)) {
        g_printerr ("Could not link pipeline.\n");
        exit(-1);
    }

    if (!gst_element_link_filtered(videoconvert, sink, filter2)) {
        g_printerr ("Could not link videoconvert caps.\n");
        exit(-1);
    }

    g_print ("Now playing\n");
    gst_element_set_state (pipeline, GST_STATE_PLAYING);
}

int main(int argc, char* argv[])
{
    QGuiApplication app(argc,argv);

    gst_init (&argc, &argv);

    QQmlApplicationEngine engine;
    engine.load(QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + QLatin1String("/transparent.qml")));

    do_gstreamer_pipeline();

    return app.exec();
}
