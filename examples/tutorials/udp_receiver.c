#include <gst/gst.h>

typedef struct _PipelineData{
    GstElement *pipeline;
    GstElement *source;
    GstElement *depayloader;
    GstElement *decoder;
    GstElement *sink;
} PipelineData;

int main(int argc, char*argv[]){
    PipelineData data;
    GstBus* bus;
    GstMessage *msg;
    gboolean terminate = FALSE;

    gst_init(&argc, &argv);
    data.source = gst_element_factory_make("udpsrc", NULL);
    data.depayloader = gst_element_factory_make("rtph264depay", NULL);
    data.decoder = gst_element_factory_make("avdec_h264", NULL);
    data.sink = gst_element_factory_make("xvimagesink", "sink");
    data.pipeline = gst_pipeline_new("udp-pipeline");

    if(!data.pipeline || !data.source || !data.depayloader || !data.decoder || !data.sink){
        g_printerr("Not all elements could be created. \n");
        return -1;
    }

    gst_bin_add_many(GST_BIN(data.pipeline), data.source, data.depayloader, data.decoder, data.sink, NULL);
    if( !gst_element_link_many(data.source, data.depayloader, data.decoder, data.sink, NULL))
    {
        g_error("Failed to link elements");
        return -1;
    }
    /*
    if(!gst_element_link(data.source, data.depayloader)){
        g_printerr("source and depayloader could not be linked \n");
        gst_object_unref(data.pipeline);
        return -1;
    }
    if(!gst_element_link(data.depayloader, data.decoder)){
        g_printerr("depayloader and decoder could not be linked \n");
        gst_object_unref(data.pipeline);
        return -1;
    }
    if(!gst_element_link(data.decoder, data.sink)){
        g_printerr("decoder and sink could not be linked \n");
        gst_object_unref(data.pipeline);
        return -1;
    }
    */

    g_object_set(data.source, "port", 9000, NULL);
    g_object_set(data.source, "caps", gst_caps_from_string("application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264, payload=(int)96"), NULL);
    
    g_object_set(data.sink, "sync", FALSE, NULL);


    gst_element_set_state(data.pipeline, GST_STATE_PLAYING);

    bus = gst_element_get_bus(data.pipeline);
    do{
        msg = gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
        //msg = gst_bus_timed_pop(bus, GST_CLOCK_TIME_NONE);
        
        /*Parse message */
        if(msg != NULL){
            GError *err;
            gchar *debug_info;
            

            switch(GST_MESSAGE_TYPE(msg)){
                case GST_MESSAGE_ERROR:
                    gst_message_parse_error(msg, &err, &debug_info);
                    g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
                    g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
                    g_clear_error(&err);
                    g_free(debug_info);
                    terminate = TRUE;
                    break;

                case GST_MESSAGE_EOS:
                    g_print("End-Of-Stream reached.\n");
                    terminate = TRUE;
                    break;

            
                default:
                    g_printerr("Unexpected message received. \n");
                    break;
            }
            gst_message_unref (msg);
        }

    } while (!terminate);

    /*Free resources */
    gst_object_unref (bus);
    gst_element_set_state(data.pipeline, GST_STATE_NULL);
    gst_object_unref (data.pipeline);
    return 0;
}