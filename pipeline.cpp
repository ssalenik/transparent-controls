#include "pipeline.h"

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

Pipeline::Pipeline(QObject *parent)
    : QObject(parent)
{
    m_pPipeline = gst_pipeline_new("camera");
    m_pV4l2 = gst_element_factory_make ("v4l2src", "v4l2src");
    m_pVpudec = gst_element_factory_make ("vpudec", "vpudec");
    m_pQueue = gst_element_factory_make ("queue", "queue");
    m_pVideoconvert = gst_element_factory_make ("imxvideoconvert_g2d", "imxvideoconvert_g2d");
    m_pSink = gst_element_factory_make ("imxg2dvideosink", "imxg2dvideosink");

    m_pFilter1 = gst_caps_new_simple ("image/jpeg",
        "width", G_TYPE_INT, 1280,
        "height", G_TYPE_INT, 720,
        "framerate", GST_TYPE_FRACTION, 30, 1,
        NULL
    );

    m_pFilter2 = gst_caps_new_simple ("video/x-raw",
        "format", G_TYPE_STRING, "RGB16",
        NULL
    );

    m_pExtraControls = gst_structure_new("c", "brightness", G_TYPE_INT, 128, NULL);

    if (!m_pPipeline || !m_pV4l2 || !m_pExtraControls || !m_pFilter1 || !m_pVpudec || !m_pQueue || !m_pVideoconvert || !m_pFilter2 || !m_pSink) {
        g_printerr ("One element could not be created. Exiting.\n");
        exit(-1);
    }

    g_object_set(m_pV4l2, "extra-controls", m_pExtraControls, NULL);

    /* we add a message handler */
    m_pBus = gst_pipeline_get_bus (GST_PIPELINE (m_pPipeline));
    m_busWatchId = gst_bus_add_watch (m_pBus, bus_call, nullptr);
    gst_object_unref (m_pBus);

    /* configure sink */
    g_object_set (m_pSink, "use-vsync", TRUE, NULL);

    /* we add all elements into the pipeline */
    gst_bin_add_many (GST_BIN(m_pPipeline), m_pV4l2, m_pVpudec, m_pQueue, m_pVideoconvert, m_pSink, NULL);

    if (!gst_element_link_filtered(m_pV4l2, m_pVpudec, m_pFilter1)) {
        g_printerr ("Could not link v4l2 caps.\n");
        exit(-1);
    }

    if (!gst_element_link_many (m_pVpudec, m_pQueue, m_pVideoconvert, NULL)) {
        g_printerr ("Could not link pipeline.\n");
        exit(-1);
    }

    if (!gst_element_link_filtered(m_pVideoconvert, m_pSink, m_pFilter2)) {
        g_printerr ("Could not link videoconvert caps.\n");
        exit(-1);
    }

    g_print ("Now playing\n");
    gst_element_set_state (m_pPipeline, GST_STATE_PLAYING);
}

Pipeline::~Pipeline()
{
    g_print ("Pipeline destructor; stopping playback\n");
    gst_element_set_state (m_pPipeline, GST_STATE_NULL);

    g_print ("Deleting pipeline\n");
    gst_object_unref (GST_OBJECT (m_pPipeline));
    m_pPipeline = nullptr;
    g_source_remove (m_busWatchId);
    m_busWatchId = 0;
}

void Pipeline::record()
{
    //TODO
}

void Pipeline::stop()
{
    //TODO
}

void Pipeline::setBrightness(int level)
{
    gst_structure_set(m_pExtraControls, "brightness", G_TYPE_INT, level, NULL);
    g_object_set(m_pV4l2, "extra-controls", m_pExtraControls, NULL);
}

#include "moc_pipeline.cpp"
