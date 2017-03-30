#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <gst/gst.h>

// to setup framebuffer
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/mxcfb.h>
#include <linux/ipu.h>

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
        "width", G_TYPE_INT, 1280,
        "height", G_TYPE_INT, 720,
        "framerate", GST_TYPE_FRACTION, 30, 1,
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

static void
do_framebuffer_setup()
{
    /* code adapted from here: https://community.nxp.com/servlet/JiveServlet/download/385051-1-373307/imx6_alpha_blending_example.c.zip */

    struct fb_var_screeninfo fb0_var;
    struct fb_fix_screeninfo fb0_fix;
    struct fb_var_screeninfo fb1_var;
    struct fb_fix_screeninfo fb1_fix;
    char *ov_buf, *p_buf, *cur_buf;
    int fd_fb0, fd_fb1, ovsize, isize, screen_size = 0;
    int blank, i, color_count = 0;
    struct mxcfb_loc_alpha l_alpha;

    /* Configure the size of overlay buffer */
    unsigned int g_display_width = 1024;
    unsigned int g_display_height = 600;

    // Open Framebuffer and gets its address
    if ((fd_fb0 = open("/dev/fb0", O_RDWR, 0)) < 0) {
        printf("Unable to open /dev/fb0\n");
        goto done;
    }

    if ( ioctl(fd_fb0, FBIOGET_FSCREENINFO, &fb0_fix) < 0) {
        printf("Get FB fix info failed!\n");
        close(fd_fb0);
        goto done;
    }

    if ( ioctl(fd_fb0, FBIOGET_VSCREENINFO, &fb0_var) < 0) {
        printf("Get FB var info failed!\n");
        close(fd_fb0);
        goto done;
    }

    printf("\nFB0 information \n");
    printf("fb0->xres = %d\n",  fb0_var.xres);
    printf("fb0->xres_virtual = %d\n",  fb0_var.xres_virtual);
    printf("fb0->yres = %d\n",  fb0_var.yres);
    printf("fb0->yres_virtual = %d\n",  fb0_var.yres_virtual);
    printf("fb0->bits_per_pixel = %d\n",  fb0_var.bits_per_pixel);
    printf("fb0->pixclock = %d\n",  fb0_var.pixclock);
    printf("fb0->height = %d\n",  fb0_var.height);
    printf("fb0->width = %d\n",  fb0_var.width);
    printf(" Pixel format : RGBX_%d%d%d%d\n",fb0_var.red.length,
                                                 fb0_var.green.length,
                                                 fb0_var.blue.length,
                                                 fb0_var.transp.length);
    printf(" Begin of bitfields(Byte ordering):-\n");
    printf("  Red    : %d\n",fb0_var.red.offset);
    printf("  Blue   : %d\n",fb0_var.blue.offset);
    printf("  Green  : %d\n",fb0_var.green.offset);
    printf("  Transp : %d\n",fb0_var.transp.offset);

    /*
     * Update all the buffers (double/triple) of /dev/fb0 for testing
     */
    isize = fb0_var.xres * fb0_var.yres_virtual * fb0_var.bits_per_pixel/8;
    printf("\nBackground Screen size is %d \n", isize);

    /* Map the device to memory */
    p_buf = (char *)mmap(0, isize, PROT_READ | PROT_WRITE, MAP_SHARED, fd_fb0, 0);
    if ((int) p_buf == -1) {
        printf("Error: failed to map framebuffer device to memory.\n");
        close(fd_fb0);
        goto done;
    }
    printf("The framebuffer device was mapped to memory successfully.\n");

    // Open overlay Framebuffer and gets its address
    if ((fd_fb1 = open("/dev/fb1", O_RDWR, 0)) < 0) {
        printf("Unable to open /dev/fb1\n");
        close(fd_fb0);
        goto done;
    }

    if (ioctl(fd_fb1, FBIOGET_FSCREENINFO, &fb1_fix) < 0) {
        printf("Get FB fix info failed!\n");
        close(fd_fb0);
        close(fd_fb1);
        goto done;
    }

    if ( ioctl(fd_fb1, FBIOGET_VSCREENINFO, &fb1_var) < 0) {
        printf("Get FB1 var info failed!\n");
        close(fd_fb0);
        close(fd_fb1);
        goto done;
    }

    printf("\nFB1 information (Before change) \n");
    printf("fb1->xres = %d\n",  fb1_var.xres);
    printf("fb1->xres_virtual = %d\n",  fb1_var.xres_virtual);
    printf("fb1->xyres = %d\n",  fb1_var.yres);
    printf("fb1->yres_virtual = %d\n",  fb1_var.yres_virtual);
    printf("fb1->bits_per_pixel = %d\n",  fb1_var.bits_per_pixel);
    printf("fb1->pixclock = %d\n",  fb1_var.pixclock);
    printf("fb1->height = %d\n",  fb1_var.height);
    printf("fb1->width = %d\n",  fb1_var.width);
    printf(" Pixel format : RGBX_%d%d%d%d\n",fb1_var.red.length,
                                                 fb1_var.green.length,
                                                 fb1_var.blue.length,
                                                 fb1_var.transp.length);
    printf(" Begin of bitfields(Byte ordering):-\n");
    printf("  Red    : %d\n",fb1_var.red.offset);
    printf("  Blue   : %d\n",fb1_var.blue.offset);
    printf("  Green  : %d\n",fb1_var.green.offset);
    printf("  Transp : %d\n",fb1_var.transp.offset);


    /* Change overlay/foreground buffer's settings */
    fb1_var.xres = g_display_width;
    fb1_var.yres = g_display_height;
    fb1_var.xres_virtual = g_display_width;
    /* Triple buffers enabled */
    fb1_var.yres_virtual = g_display_height * 3;
    fb1_var.activate |= (FB_ACTIVATE_NOW | FB_ACTIVATE_FORCE);
    fb1_var.bits_per_pixel  = 32;
    fb1_var.red.length = 8;
    fb1_var.blue.length = 8;
    fb1_var.green.length = 8;
    fb1_var.transp.length = 8;
    fb1_var.red.offset = 16;
    fb1_var.blue.offset = 0;
    fb1_var.green.offset = 8;
    fb1_var.transp.offset = 24;

    if (ioctl(fd_fb1, FBIOPUT_VSCREENINFO,
                &fb1_var) < 0) {
        printf("Put var of fb1 failed\n");
        close(fd_fb0);
        close(fd_fb1);
        goto done;
    }

    /* Unblank the fb1 */
    blank = FB_BLANK_UNBLANK;
    if (ioctl(fd_fb1, FBIOBLANK, blank) < 0) {
        printf("Blanking fb1 failed\n");
        close(fd_fb0);
        close(fd_fb1);
        goto done;

    }

    if ( ioctl(fd_fb1, FBIOGET_VSCREENINFO, &fb1_var) < 0) {
        printf("Get FB1 var info failed!\n");
        close(fd_fb0);
        close(fd_fb1);
        goto done;
    }

    printf("\nFB1 information (After change) \n");
    printf("fb1->xres = %d\n",  fb1_var.xres);
    printf("fb1->xres_virtual = %d\n",  fb1_var.xres_virtual);
    printf("fb1->xyres = %d\n",  fb1_var.yres);
    printf("fb1->yres_virtual = %d\n",  fb1_var.yres_virtual);
    printf("fb1->bits_per_pixel = %d\n",  fb1_var.bits_per_pixel);
    printf("fb1->pixclock = %d\n",  fb1_var.pixclock);
    printf("fb1->height = %d\n",  fb1_var.height);
    printf("fb1->width = %d\n",  fb1_var.width);
    printf(" Pixel format : RGBX_%d%d%d%d\n",fb1_var.red.length,
                                                 fb1_var.green.length,
                                                 fb1_var.blue.length,
                                                 fb1_var.transp.length);
    printf(" Begin of bitfields(Byte ordering):-\n");
    printf("  Red    : %d\n",fb1_var.red.offset);
    printf("  Blue   : %d\n",fb1_var.blue.offset);
    printf("  Green  : %d\n",fb1_var.green.offset);
    printf("  Transp : %d\n",fb1_var.transp.offset);



    ovsize = fb1_var.xres * fb1_var.yres_virtual * fb1_var.bits_per_pixel/8;
    screen_size = fb1_var.xres * fb1_var.yres * fb1_var.bits_per_pixel/8;
    printf("\nOverlay Screen size is %d \n", ovsize);

    /* Map the device to memory */
    ov_buf = (char *)mmap(0, ovsize, PROT_READ | PROT_WRITE, MAP_SHARED, fd_fb1, 0);
    if ((int) ov_buf == -1) {
        printf("Error: failed to map framebuffer device to memory.\n");
        close(fd_fb0);
        close(fd_fb1);
        goto done;
    }
    printf("The framebuffer device was mapped to memory successfully.\n");


    /* Local Alpha methods */

    /* Alpha in Pixel */
    /*
     * Alpha value is part of each pixel data. Use 32-bit bpp
     */
    l_alpha.enable = 1;
    l_alpha.alpha_in_pixel = 1;
    l_alpha.alpha_phy_addr0 = 0;
    l_alpha.alpha_phy_addr1 = 0;
    if (ioctl(fd_fb1, MXCFB_SET_LOC_ALPHA,
                &l_alpha) < 0) {
        printf("Set local alpha failed\n");
        close(fd_fb0);
        close(fd_fb1);
        goto done;
    }

    close(fd_fb0);
    close(fd_fb1);
    return;
done:
    exit(-1);
}

int main(int argc, char* argv[])
{
    qputenv("QT_QPA_EGLFS_FB", "/dev/fb1");
    qputenv("QT_QPA_PLATFORM", "eglfs");

    do_framebuffer_setup();

    QGuiApplication app(argc,argv);

    gst_init (&argc, &argv);

    QQmlApplicationEngine engine;
    engine.load(QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + QLatin1String("/transparent.qml")));

    do_gstreamer_pipeline();

    return app.exec();
}
