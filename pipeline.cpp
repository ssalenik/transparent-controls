#include "pipeline.h"

#include <QDebug>

struct _FileSink
{
    // do not unref these two
    GstElement *pipeline;
    GstElement *tee;

    GstPad *teepad;
    GstElement *queue;
    GstElement *conv1;
    GstElement *conv2;
    GstElement *enc;
    GstElement *sink;
    gboolean removing;
};

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

    m_pTee = gst_element_factory_make ("tee", "tee");

    // tee one
    m_pQueue1 = gst_element_factory_make ("queue", "queue 1");
    m_pVideoconvert1 = gst_element_factory_make ("imxvideoconvert_g2d", "convert 1");
    m_pVideoSink = gst_element_factory_make ("imxg2dvideosink", "imxg2dvideosink");

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

    if (!m_pPipeline || !m_pV4l2 || !m_pExtraControls || !m_pFilter1 || !m_pVpudec || !m_pQueue1 ||
        !m_pVideoconvert1 || !m_pFilter2 || !m_pVideoSink ) {
        g_printerr ("One element could not be created. Exiting.\n");
        exit(-1);
    }

    g_object_set(m_pV4l2, "extra-controls", m_pExtraControls, NULL);

    /* we add a message handler */
    m_pBus = gst_pipeline_get_bus (GST_PIPELINE (m_pPipeline));
    m_busWatchId = gst_bus_add_watch (m_pBus, bus_call, nullptr);
    gst_object_unref (m_pBus);

    /* configure sink */
    g_object_set (m_pVideoSink, "use-vsync", TRUE, NULL);

    /* we add all elements into the pipeline */
    gst_bin_add_many (GST_BIN(m_pPipeline), m_pV4l2, m_pVpudec, m_pTee, m_pQueue1, m_pVideoconvert1,
        m_pVideoSink, NULL);

    if (!gst_element_link_filtered(m_pV4l2, m_pVpudec, m_pFilter1)) {
        g_printerr ("Could not link v4l2 caps.\n");
        exit(-1);
    }

    if (!gst_element_link_many (m_pVpudec, m_pTee, m_pQueue1, m_pVideoconvert1, NULL)) {
        g_printerr ("Could not link pipeline.\n");
        exit(-1);
    }

    if (!gst_element_link_filtered(m_pVideoconvert1, m_pVideoSink, m_pFilter2)) {
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

static GstPadProbeReturn
unlink_cb(GstPad *, GstPadProbeInfo *, gpointer user_data)
{
    FileSink *filesink = (FileSink *)user_data;
    GstPad *sinkpad;

    if (!g_atomic_int_compare_and_exchange (&filesink->removing, FALSE, TRUE))
        return GST_PAD_PROBE_OK;

    sinkpad = gst_element_get_static_pad (filesink->queue, "sink");
    gst_pad_unlink (filesink->teepad, sinkpad);
    gst_object_unref (sinkpad);

    gst_bin_remove (GST_BIN (filesink->pipeline), filesink->queue);
    gst_bin_remove (GST_BIN (filesink->pipeline), filesink->conv1);
    gst_bin_remove (GST_BIN (filesink->pipeline), filesink->conv2);
    gst_bin_remove (GST_BIN (filesink->pipeline), filesink->enc);
    gst_bin_remove (GST_BIN (filesink->pipeline), filesink->sink);

    gst_element_set_state (filesink->sink, GST_STATE_NULL);
    gst_element_set_state (filesink->conv1, GST_STATE_NULL);
    gst_element_set_state (filesink->conv2, GST_STATE_NULL);
    gst_element_set_state (filesink->enc, GST_STATE_NULL);
    gst_element_set_state (filesink->queue, GST_STATE_NULL);

    gst_object_unref (filesink->queue);
    gst_object_unref (filesink->conv1);
    gst_object_unref (filesink->conv2);
    gst_object_unref (filesink->enc);
    gst_object_unref (filesink->sink);

    gst_element_release_request_pad (filesink->tee, filesink->teepad);
    gst_object_unref (filesink->teepad);

    return GST_PAD_PROBE_REMOVE;
}

void Pipeline::record()
{
    if (m_recording) return;

    qDebug() << "start recording";
    m_recording = true;

    m_pFileSink = (FileSink *)g_new0 (FileSink, 1);
    m_pFileSink->pipeline = m_pPipeline;
    m_pFileSink->tee = m_pTee;

    GstPad *sinkpad;
    GstPadTemplate *templ;

    templ = gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (m_pTee), "src_%u");

    m_pFileSink->teepad = gst_element_request_pad (m_pTee, templ, NULL, NULL);

    // tee two
    m_pFileSink->queue = gst_element_factory_make ("queue", "queue 2");
    m_pFileSink->conv1 = gst_element_factory_make ("imxvideoconvert_g2d", "convert g2d");
    m_pFileSink->conv2 = gst_element_factory_make ("imxvideoconvert_ipu", "convert ipu");
    m_pFileSink->enc = gst_element_factory_make ("imxvpuenc_mpeg4", "imxvpuenc_mpeg4");
    m_pFileSink->sink = gst_element_factory_make ("filesink", "filesink");

    g_object_set(m_pFileSink->sink, "location", "test.mpeg", NULL);

    m_pFileSink->removing = false;

    gst_bin_add_many (GST_BIN (m_pPipeline),
        GST_ELEMENT(gst_object_ref (m_pFileSink->queue)),
        GST_ELEMENT(gst_object_ref (m_pFileSink->conv1)),
        GST_ELEMENT(gst_object_ref (m_pFileSink->conv2)),
        GST_ELEMENT(gst_object_ref (m_pFileSink->enc)),
        GST_ELEMENT(gst_object_ref (m_pFileSink->sink)),
        NULL);

    gst_element_link_many (m_pFileSink->queue, m_pFileSink->conv1, m_pFileSink->conv2, m_pFileSink->enc, m_pFileSink->sink, NULL);

    gst_element_sync_state_with_parent (m_pFileSink->queue);
    gst_element_sync_state_with_parent (m_pFileSink->conv1);
    gst_element_sync_state_with_parent (m_pFileSink->conv2);
    gst_element_sync_state_with_parent (m_pFileSink->enc);
    gst_element_sync_state_with_parent (m_pFileSink->sink);

    sinkpad = gst_element_get_static_pad (m_pFileSink->queue, "sink");
    gst_pad_link (m_pFileSink->teepad, sinkpad);
    gst_object_unref (sinkpad);
}

void Pipeline::stop()
{
    if (!m_recording) return;

    qDebug() << "stop recording";
    m_recording = false;

    gst_pad_add_probe (m_pFileSink->teepad, GST_PAD_PROBE_TYPE_IDLE, unlink_cb, m_pFileSink,
        (GDestroyNotify) g_free);

    m_pFileSink = nullptr;
}

void Pipeline::setBrightness(int level)
{
    gst_structure_set(m_pExtraControls, "brightness", G_TYPE_INT, level, NULL);
    g_object_set(m_pV4l2, "extra-controls", m_pExtraControls, NULL);
}

#include "moc_pipeline.cpp"
