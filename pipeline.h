#pragma once

#include <QObject>
#include <gst/gst.h>

class Pipeline : public QObject
{
    Q_OBJECT
public:
    explicit Pipeline(QObject *parent = 0);
    ~Pipeline();

public Q_SLOTS:
    void record();
    void stop();
    void setBrightness(int);

private:
    GstElement *m_pPipeline;
    GstElement *m_pV4l2;
    GstElement *m_pVpudec;
    GstElement *m_pQueue1;
    GstElement *m_pVideoconvert1;
    GstElement *m_pVideoSink;
    GstCaps *m_pFilter1;
    GstElement *m_pTee;
    GstElement *m_pQueue2;
    GstElement *m_pVideoconvert2;
    GstElement *m_pEncoder;
    GstElement *m_pFileSink;

    GstBus *m_pBus;
    GstStructure *m_pExtraControls;

    guint m_busWatchId;
};
