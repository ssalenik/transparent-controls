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
    GstElement *m_pQueue;
    GstElement *m_pVideoconvert;
    GstElement *m_pSink;
    GstCaps *m_pFilter1;
    GstCaps *m_pFilter2;
    GstBus *m_pBus;

    guint m_busWatchId;
};
