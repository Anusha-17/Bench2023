/* ===========================================================================
 * Copyright (c) 2016-2022 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
=========================================================================== */
#include "test_util.h"
#include "camera_metadata_tags.h"

#ifndef __INTEGRITY /* Using expat on Integrity for parsing XML*/
#include <string.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#else
#include <string.h>
#include "expat.h"
#include "camera_config_dump.h"
#include "camera_config_cvbs_dump.h"
#endif

#ifndef __INTEGRITY
#define XML_GET_INT_ATTR(_var_, _node_, _attr_, _opt_, _type_, _default_) \
do { \
    xmlChar* _p_ = xmlGetProp(_node_, (const xmlChar *)_attr_); \
    if (_p_) { \
        _var_ = (_type_)strtoul((const char *)_p_, NULL, 0); \
        xmlFree(_p_); \
    } else if (_opt_) {\
        _var_ = (_type_)_default_; \
    } else  { \
        QCARCAM_ERRORMSG("could not get attribute " _attr_); \
        return -1; \
    } \
} while(0)

#define XML_GET_FLOAT_ATTR(_var_, _node_, _attr_, _opt_, _default_) \
do { \
    xmlChar* _p_ = xmlGetProp(_node_, (const xmlChar *)_attr_); \
    if (_p_) { \
        _var_ = strtof((const char *)_p_, NULL); \
        xmlFree(_p_); \
    } else if (_opt_) {\
        _var_ = _default_; \
    } else  { \
        QCARCAM_ERRORMSG("could not get attribute " _attr_); \
        return -1; \
    } \
} while(0)

#define XML_GET_STRING_ATTR(_var_, _node_, _attr_, _opt_, _default_) \
do { \
    xmlChar* _p_ = xmlGetProp(_node_, (const xmlChar *)_attr_); \
    if (_p_) { \
        snprintf(_var_, sizeof(_var_), "%s", _p_); \
        xmlFree(_p_); \
    } else if (_opt_) {\
        snprintf(_var_, sizeof(_var_), "%s", _default_); \
    } else  { \
        QCARCAM_ERRORMSG("could not get attribute " _attr_); \
        return -1; \
    } \
} while(0)
#else
#define XML_GET_INT_ATTR(_var_, _attr_, _label_, _opt_, type, _default_) \
{ \
    int _p_ = xml_find_attr(_label_, _attr_); \
    if (_p_ != -1) { \
        _var_ = (type)atoi((const char *)_attr_[_p_ + 1]); \
    } else if (_opt_) {\
        _var_ = (type)_default_; \
    } else  { \
        QCARCAM_ERRORMSG("could not get attribute " _label_); \
    } \
}

#define XML_GET_FLOAT_ATTR(_var_,_attr_, _label_, _opt_, _default_) \
{ \
    int _p_ = xml_find_attr(_label_, _attr_); \
    if (_p_ != -1) { \
        _var_ = atof((const char *)_attr_[_p_ + 1]); \
    } else if (_opt_) {\
        _var_ = _default_; \
    } else  { \
        QCARCAM_ERRORMSG("could not get attribute " _label_); \
    } \
}

#define XML_GET_STRING_ATTR(_var_, _attr_, _label_, _opt_, _default_) \
{ \
    int _p_ = xml_find_attr(_label_, _attr_); \
    if (_p_ != -1) { \
        snprintf(_var_, sizeof(_var_), "%s", _attr_[_p_ + 1]); \
    } else if (_opt_) {\
        snprintf(_var_, sizeof(_var_), "%s", _default_); \
    } else  { \
        QCARCAM_ERRORMSG("could not get attribute " _label_); \
    } \
}
#endif

struct FmtMappingType {
    const char* s;
    QCarCamColorFmt_e fmt;
};

static FmtMappingType sg_outputFormatTable[] =
{
    {"mipi8", QCARCAM_FMT_MIPIRAW_8},
    {"mipi10", QCARCAM_FMT_MIPIRAW_10},
    {"mipi12", QCARCAM_FMT_MIPIRAW_12},
    {"mipi14", QCARCAM_FMT_MIPIRAW_14},
    {"mipi16", QCARCAM_FMT_MIPIRAW_16},
    {"mipi20", QCARCAM_FMT_MIPIRAW_20},
    {"uyvy", QCARCAM_FMT_UYVY_8},
    {"yuyv", QCARCAM_FMT_YUYV_8},
    {"nv12", QCARCAM_FMT_NV12},
    {"nv21", QCARCAM_FMT_NV21},
    {"yu12", QCARCAM_FMT_YU12},
    {"p010lsb", QCARCAM_FMT_P010LSB},
    {"p01210lsb", QCARCAM_FMT_P01210LSB},
    {"p01208lsb", QCARCAM_FMT_P01208LSB},
    {"p010", QCARCAM_FMT_P010},
    {"p01210", QCARCAM_FMT_P01210},
    {"p01208", QCARCAM_FMT_P01208},
    {"plain16_10", QCARCAM_FMT_PLAIN16_10},
    {"plain16_12", QCARCAM_FMT_PLAIN16_12},
    {"plain16_14", QCARCAM_FMT_PLAIN16_14},
    {"plain16_16", QCARCAM_FMT_PLAIN16_16},
    {"plain32_20", QCARCAM_FMT_PLAIN32_20},
    {"rgba8888", QCARCAM_FMT_RGBX_8888},
    {"rgbx1010102", QCARCAM_FMT_RGBX_1010102},
    {"rgb888", QCARCAM_FMT_RGB_888},
    {"rgb565", QCARCAM_FMT_RGB_565},
    {"r8g8b8", QCARCAM_FMT_R8_G8_B8},
    {"ubwc_nv12", QCARCAM_FMT_UBWC_NV12},
    {"ubwc_tp10", QCARCAM_FMT_UBWC_TP10}
};


struct MetadataMappingType {
    const char* s;
    int32_t tagId;
    int32_t isVendorTag;
};

static MetadataMappingType sg_metadataMapTable[] =
{
    {"ANDROID_CONTROL_AE_MODE", ANDROID_CONTROL_AE_MODE},
    {"ANDROID_CONTROL_AE_LOCK", ANDROID_CONTROL_AE_LOCK},
    {"ANDROID_CONTROL_AWB_MODE", ANDROID_CONTROL_AWB_MODE},
    {"ANDROID_CONTROL_AWB_LOCK", ANDROID_CONTROL_AWB_LOCK},
    {"ANDROID_CONTROL_EFFECT_MODE", ANDROID_CONTROL_EFFECT_MODE},
    {"ANDROID_CONTROL_MODE", ANDROID_CONTROL_MODE},
    {"ANDROID_CONTROL_SCENE_MODE", ANDROID_CONTROL_SCENE_MODE},
    {"ANDROID_CONTROL_AE_ANTIBANDING_MODE", ANDROID_CONTROL_AE_ANTIBANDING_MODE},
    {"ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION", ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION},
    {"ANDROID_CONTROL_AE_REGIONS", ANDROID_CONTROL_AE_REGIONS},
    {"ANDROID_NOISE_REDUCTION_MODE", ANDROID_NOISE_REDUCTION_MODE},
    {"QCARCAM_METADATA_TAG_SATURATION_LEVEL", QCARCAM_METADATA_TAG_SATURATION_LEVEL, 1},
    {"QCARCAM_METADATA_TAG_CONTRAST_LEVEL", QCARCAM_METADATA_TAG_CONTRAST_LEVEL, 1},
    {"QCARCAM_METADATA_TAG_SHARPNESS_STRENGTH", QCARCAM_METADATA_TAG_SHARPNESS_STRENGTH, 1},
    {"QCARCAM_METADATA_TAG_ICA_LDC_TRANSFORM_MODE", QCARCAM_METADATA_TAG_ICA_LDC_TRANSFORM_MODE, 1}
};

static int parse_isp_metadata_tag_element(xmlNodePtr cur, test_util_metadata_tag_t* pTag);


static QCarCamColorFmt_e parse_output_format(xmlNodePtr cur)
{
    QCarCamColorFmt_e fmt = (QCarCamColorFmt_e)-1;
    xmlChar* _p_ = xmlGetProp(cur, (const xmlChar *)"format");
    if (!_p_) {
        return fmt;
    }

    uint32_t arraySize = sizeof(sg_outputFormatTable)/sizeof(sg_outputFormatTable[0]);
    for (uint32_t i = 0; i < arraySize; i++)
    {
        if (!strncmp((const char*)_p_, sg_outputFormatTable[i].s, strlen(sg_outputFormatTable[i].s)))
        {
            fmt = sg_outputFormatTable[i].fmt;
            break;
        }
    }

    //if cannot match string then treat as hexadecimal
    if ((QCarCamColorFmt_e)-1 == fmt)
    {
        fmt = (QCarCamColorFmt_e)strtoul((const char *)_p_, NULL, 0);
    }

    xmlFree(_p_);

    return fmt;
}

static test_util_color_fmt_t translate_format_string(char* format)
{
    if (!strncmp(format, "rgba8888", strlen("rgba8888")))
    {
        return TESTUTIL_FMT_RGBX_8888;
    }
    else if (!strncmp(format, "rgb888", strlen("rgb888")))
    {
        return TESTUTIL_FMT_RGB_888;
    }
    else if (!strncmp(format, "r8g8b8", strlen("r8g8b8")))
    {
        return TESTUTIL_FMT_R8_G8_B8;
    }
    else if (!strncmp(format, "rgb565", strlen("rgb565")))
    {
        return TESTUTIL_FMT_RGB_565;
    }
    else if (!strncmp(format, "uyvy", strlen("uyvy")))
    {
        return TESTUTIL_FMT_UYVY_8;
    }
    else if (!strncmp(format, "nv12", strlen("nv12")))
    {
        return TESTUTIL_FMT_NV12;
    }
    else if (!strncmp(format, "yu12", strlen("yu12")))
    {
        return TESTUTIL_FMT_YU12;
    }
    else if (!strncmp(format, "p010lsb", strlen("p010lsb")))
    {
        return TESTUTIL_FMT_P010_LSB;
    }
    else if (!strncmp(format, "p01208lsb", strlen("p01208lsb")))
    {
        return TESTUTIL_FMT_P01208_LSB;
    }
    else if (!strncmp(format, "p01210lsb", strlen("p01210lsb")))
    {
        return TESTUTIL_FMT_P01210_LSB;
    }
    else if (!strncmp(format, "p010", strlen("p010")))
    {
        return TESTUTIL_FMT_P010;
    }
    else if (!strncmp(format, "p01208", strlen("p01208")))
    {
        return TESTUTIL_FMT_P01208;
    }
    else if (!strncmp(format, "p01210", strlen("p01210")))
    {
        return TESTUTIL_FMT_P01210;
    }
    else if (!strncmp(format, "plain16_12", strlen("plain16_12")))
    {
        return TESTUTIL_FMT_PLAIN_12;
    }
    else if (!strncmp(format, "ubwc_nv12", strlen("ubwc_nv12")))
    {
        return TESTUTIL_FMT_UBWC_NV12;
    }
    else if (!strncmp(format, "ubwc_tp10", strlen("ubwc_tp10")))
    {
        return TESTUTIL_FMT_UBWC_TP10;
    }
    else
    {
        QCARCAM_ERRORMSG("Default to RGB565");
        return TESTUTIL_FMT_RGB_565;
    }
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_di_sw_weave_30fps
///
/// @brief Deinterlace 2 fields from souce buf into dest new frame with software weave 30fps method
///
/// @param di_info         Input souce/dest buffer context
/// @param test_util_get_buf_ptr_func    Helper func to get source/destination buffer virtual address
///
/// @return Void
///////////////////////////////////////////////////////////////////////////////
void test_util_di_sw_weave_30fps(test_util_sw_di_t *di_info)
{
    unsigned char *odd_field_ptr = NULL;
    unsigned char *even_field_ptr = NULL;
    unsigned char *source_ptr = NULL;
    unsigned char *dest_ptr = NULL;
    test_util_buf_ptr_t buf_ptr;
    uint32_t line;

    buf_ptr.buf_idx = di_info->source_buf_idx;
    test_util_get_buf_ptr(di_info->qcarcam_window, &buf_ptr);
    source_ptr = buf_ptr.p_va[0];
    line = buf_ptr.stride[0];

    test_util_get_buf_ptr(di_info->display_window, &buf_ptr);
    dest_ptr = buf_ptr.p_va[0];


    if (di_info->field_type == QCARCAM_INTERLACE_FIELD_ODD_EVEN)
    {
        odd_field_ptr = source_ptr + line * DEINTERLACE_ODD_HEADER_HEIGHT;
        even_field_ptr = source_ptr + line * (DEINTERLACE_ODD_HEIGHT + DEINTERLACE_EVEN_HEADER_HEIGHT);
    }
    else if (di_info->field_type == QCARCAM_INTERLACE_FIELD_EVEN_ODD)
    {
        even_field_ptr = source_ptr + line * DEINTERLACE_EVEN_HEADER_HEIGHT;
        odd_field_ptr = source_ptr + line * (DEINTERLACE_EVEN_HEIGHT + DEINTERLACE_ODD_HEADER_HEIGHT);
    }
    else
    {
        /* shouldn't display this frame, so do nothing */
        return;
    }

    for(int i = 0; i < DEINTERLACE_FIELD_HEIGHT; i++)
    {
        memcpy(dest_ptr, odd_field_ptr, line);
        dest_ptr += line;
        odd_field_ptr += line;
        memcpy(dest_ptr, even_field_ptr, line);
        dest_ptr += line;
        even_field_ptr += line;
    }
}

#ifndef __INTEGRITY
static int parse_input_instance_element(xmlDocPtr doc, xmlNodePtr cur, QCarCamInputStream_t* input_config)
{
    (void)(doc);
    XML_GET_INT_ATTR(input_config->inputId, cur, "qcarcam_id", 0, unsigned int, 0);
    XML_GET_INT_ATTR(input_config->srcId, cur, "src_id", 0, unsigned int, 0);
    XML_GET_INT_ATTR(input_config->inputMode, cur, "input_mode", 0, unsigned int, 0);
    return 0;
}

static int parse_isp_instance_element(xmlDocPtr doc, xmlNodePtr cur, QCarCamIspUsecaseConfig_t* isp_config)
{
    (void)(doc);

    XML_GET_INT_ATTR(isp_config->id, cur, "id", 0, unsigned int, 0);
    XML_GET_INT_ATTR(isp_config->cameraId, cur, "camera_id", 0, unsigned int, 0);
    XML_GET_INT_ATTR(isp_config->usecaseId, cur, "use_case", 0, QCarCamIspUsecase_e, 0);

    return 0;
}

static int parse_stream_element(xmlDocPtr doc, xmlNodePtr cur, test_util_stream_input_t* stream_params,int numInstances)
{
    (void)(doc);

    if ((!xmlStrcmp(cur->name, (const xmlChar *)"display_setting")))
    {
        char format[16];

        XML_GET_INT_ATTR(stream_params->window_params[numInstances].display_id, cur, "display_id", 0, int, 0);
        XML_GET_INT_ATTR(stream_params->window_params[numInstances].is_offscreen, cur, "offscreen", 1, int, 0);
        XML_GET_FLOAT_ATTR(stream_params->window_params[numInstances].window_size[0], cur, "window_width", 1, 1.0f);
        XML_GET_FLOAT_ATTR(stream_params->window_params[numInstances].window_size[1], cur, "window_height", 1, 1.0f);
        XML_GET_FLOAT_ATTR(stream_params->window_params[numInstances].window_pos[0], cur, "window_pos_x", 1, 0.0f);
        XML_GET_FLOAT_ATTR(stream_params->window_params[numInstances].window_pos[1], cur, "window_pos_y", 1, 0.0f);
        XML_GET_FLOAT_ATTR(stream_params->window_params[numInstances].window_source_size[0], cur, "src_width", 1, 1.0f);
        XML_GET_FLOAT_ATTR(stream_params->window_params[numInstances].window_source_size[1], cur, "src_height", 1, 1.0f);
        XML_GET_FLOAT_ATTR(stream_params->window_params[numInstances].window_source_pos[0], cur, "src_x", 1, 0.0f);
        XML_GET_FLOAT_ATTR(stream_params->window_params[numInstances].window_source_pos[1], cur, "src_y", 1, 0.0f);
        XML_GET_INT_ATTR(stream_params->window_params[numInstances].zorder, cur, "zorder", 1, int, 0);
        XML_GET_INT_ATTR(stream_params->window_params[numInstances].pipeline_id, cur, "pipeline", 1, int, -1);
        XML_GET_INT_ATTR(stream_params->window_params[numInstances].n_buffers_display, cur, "nbufs", 1, int, 5);
        XML_GET_STRING_ATTR(format, cur, "format", 1, "uyvy");

        stream_params->window_params[numInstances].format = translate_format_string(format);
    }
    else if ((!xmlStrcmp(cur->name, (const xmlChar *)"output_setting")))
    {
        XML_GET_INT_ATTR(stream_params->output_params[numInstances].n_buffers, cur, "nbufs", 1, int, 5);
        XML_GET_INT_ATTR(stream_params->output_params[numInstances].width, cur, "width", 1, int, -1);
        XML_GET_INT_ATTR(stream_params->output_params[numInstances].height, cur, "height", 1, int, -1);
        XML_GET_INT_ATTR(stream_params->output_params[numInstances].stride, cur, "stride", 1, int, -1);
        XML_GET_INT_ATTR(stream_params->output_params[numInstances].frame_rate_config.frameDropPattern, cur, "framedrop_pattern", 1, int, 0);
        XML_GET_INT_ATTR(stream_params->output_params[numInstances].frame_rate_config.frameDropPeriod, cur, "framedrop_period", 1, char, 0);
        XML_GET_INT_ATTR(stream_params->output_params[numInstances].num_batch_frames, cur, "batch_frames", 1, int, 1);
        XML_GET_INT_ATTR(stream_params->output_params[numInstances].detect_first_phase_timer, cur, "detect_first_phase_timer", 1, int, 10);
        XML_GET_INT_ATTR(stream_params->output_params[numInstances].frame_increment, cur, "frame_increment", 1, int, 0);
        XML_GET_INT_ATTR(stream_params->output_params[numInstances].buffer_flags, cur, "buffer_flags", 1, unsigned int, 0);
        XML_GET_INT_ATTR(stream_params->output_params[numInstances].cropRegion.width, cur, "crop_width", 1, int, 0);
        XML_GET_INT_ATTR(stream_params->output_params[numInstances].cropRegion.height, cur, "crop_height", 1, int, 0);
        XML_GET_INT_ATTR(stream_params->output_params[numInstances].cropRegion.x, cur, "crop_x", 1, int, 0);
        XML_GET_INT_ATTR(stream_params->output_params[numInstances].cropRegion.y, cur, "crop_y", 1, int, 0);

        stream_params->output_params[numInstances].format = parse_output_format(cur);
    }

    return 0;
}

static int parse_input_element(xmlDocPtr doc, xmlNodePtr parent, test_util_xml_input_t* input)
{
    xmlNodePtr cur = parent->xmlChildrenNode;
    unsigned int numstreamInstances = 0;

    (void)(doc);

    while (cur != NULL) {
        if ((!xmlStrcmp(cur->name, (const xmlChar *)"properties"))) {
            int rc = 0;
            unsigned int numInstances = 0;
            XML_GET_INT_ATTR(input->properties.num_inputs, cur, "num_inputs", 1, int, 1);
            XML_GET_INT_ATTR(input->properties.use_event_callback, cur, "event_callback", 1, int, 1);
            XML_GET_INT_ATTR(input->properties.frame_timeout, cur, "frame_timeout", 1, int, -1);
            XML_GET_INT_ATTR(input->properties.op_mode, cur, "op_mode", 1, QCarCamOpmode_e, QCARCAM_OPMODE_RAW_DUMP);
            XML_GET_INT_ATTR(input->properties.subscribe_parameter_change, cur, "subscribe_parameter_change", 1, int, 0);
            XML_GET_INT_ATTR(input->properties.delay_time, cur, "delay_time", 1, int, 0);
            XML_GET_INT_ATTR(input->properties.recovery, cur, "recovery", 1, bool, 0);
            XML_GET_INT_ATTR(input->properties.request_mode, cur, "request_mode", 1, bool, 0);
            XML_GET_STRING_ATTR(input->properties.node_name, cur, "v4l2_node", 1, "/dev/video51");
            xmlNodePtr Child = cur->xmlChildrenNode;
            while (Child != NULL)
            {
                if ((!xmlStrcmp(Child->name, (const xmlChar *)"input")))
                {
                    rc = parse_input_instance_element(doc, Child, &input->properties.input_param[numInstances]);
                    if (!rc)
                    {
                        numInstances++;
                    }
                    else
                    {
                        QCARCAM_ERRORMSG("Missing parameter in config file");
                    }

                    if (numInstances >= QCARCAM_MAX_INPUT_STREAMS)
                    {
                        QCARCAM_ERRORMSG("Parsed max number of Input instances (%d). Skip rest", numInstances);
                        break;
                    }
                }
                Child = Child->next;
            }

            if (input->properties.num_inputs != numInstances)
            {
                QCARCAM_ERRORMSG("Missing parameter in config file");
            }
        }
        else if ((!xmlStrcmp(cur->name, (const xmlChar *)"stream")))
        {
            int rc = 0;
            xmlNodePtr child = cur->xmlChildrenNode;
            XML_GET_INT_ATTR(input->stream_params.buffer_id[numstreamInstances], cur, "bufferlist_id", 0, uint32_t, 0);
            while (child != NULL)
            {
                rc = parse_stream_element(doc, child, &input->stream_params, numstreamInstances);
                if (!rc)
                {
                    QCARCAM_INFOMSG("Parsing config file");
                }
                else
                {
                    QCARCAM_ERRORMSG("Missing parameter in config file");
                }

                if (numstreamInstances >= QCARCAM_MAX_BUFF_INSTANCES)
                {
                    QCARCAM_ERRORMSG("Wrong format of config file."
                                     "Parsed max number of stream instances (%d). Skip rest", numstreamInstances);
                    break;
                }
                child = child->next;
            }
            numstreamInstances++;
        }
        else if ((!xmlStrcmp(cur->name, (const xmlChar *)"inject_setting")))
        {
            XML_GET_INT_ATTR(input->inject_params.pattern, cur, "pattern", 0, QCarCamColorPattern_e, 0);
            XML_GET_INT_ATTR(input->inject_params.bitdepth, cur, "bitdepth", 0, QCarCamColorBitDepth_e, 0);
            XML_GET_INT_ATTR(input->inject_params.pack, cur, "pack", 0, QCarCamColorPack_e, 0);
            XML_GET_INT_ATTR(input->inject_params.buffer_size[0], cur, "width", 0, int, 0);
            XML_GET_INT_ATTR(input->inject_params.buffer_size[1], cur, "height", 0, int, 0);
            XML_GET_INT_ATTR(input->inject_params.stride, cur, "stride", 0, int, 0);
            XML_GET_INT_ATTR(input->inject_params.n_buffers, cur, "nbufs", 1, int, 1);
            XML_GET_STRING_ATTR(input->inject_params.filename, cur, "filename", 1, "bayer_input.raw");
            XML_GET_INT_ATTR(input->inject_params.n_frames, cur, "nframes", 1, int, 1);
            XML_GET_INT_ATTR(input->inject_params.repeat, cur, "repeat", 1, int, -1);
            XML_GET_INT_ATTR(input->inject_params.framerate, cur, "framerate", 1, int, 30);
            XML_GET_INT_ATTR(input->inject_params.singlebuf, cur, "singlebuf", 1, int, 0);
        }
        else if ((!xmlStrcmp(cur->name, (const xmlChar *)"exposure_setting")))
        {
            XML_GET_FLOAT_ATTR(input->exp_params.exposure_time, cur, "exp_time", 1, 31.147f);
            XML_GET_FLOAT_ATTR(input->exp_params.gain, cur, "gain", 1, 1.5f);
            XML_GET_INT_ATTR(input->exp_params.manual_exp, cur, "enable_manual", 1, int, -1);
        }
        else if ((!xmlStrcmp(cur->name, (const xmlChar *)"isp_setting")))
        {
            int rc = 0;
            unsigned int numInstances = 0;
            unsigned int tagNum = 0;
            xmlNodePtr Child = cur->xmlChildrenNode;
            while (Child != NULL)
            {
                if ((!xmlStrcmp(Child->name, (const xmlChar *)"isp_instance")))
                {
                    if (numInstances >= QCARCAM_MAX_ISP_INSTANCES)
                    {
                        QCARCAM_ERRORMSG("Parsed max number of ISP instances (%d). Skip rest", numInstances);
                        continue;
                    }

                    rc = parse_isp_instance_element(doc, Child, &input->isp_params.isp_config[numInstances]);
                    if (!rc)
                    {
                        numInstances++;
                    }
                    else
                    {
                        QCARCAM_ERRORMSG("Missing parameter in config file");
                    }
                }
                else if ((!xmlStrcmp(Child->name, (const xmlChar *)"metadata_tag")))
                {
                    if (tagNum >= MAX_METADATA_TAG_NUM)
                    {
                        QCARCAM_ALWZMSG("Parsed max number of meta data tag (%d). Skip rest", tagNum);
                        continue;
                    }

                    rc = parse_isp_metadata_tag_element(Child, &input->isp_params.metadata_tags[tagNum]);
                    if (!rc)
                    {
                        tagNum++;
                    }
                    else
                    {
                        QCARCAM_ALWZMSG("failed to parse isp metadata tag in config file");
                    }

                    input->isp_params.num_tags = tagNum;
                }
                Child = Child->next;
            }
            input->isp_params.num_isp_instances = numInstances;
        }

        input->stream_params.num_buffer_instances = numstreamInstances;
        cur = cur->next;
    }

    return 0;
}

static int parse_latency_mode(xmlDocPtr doc, xmlNodePtr parent, test_util_global_config_t *global_config)
{
    xmlNodePtr cur = parent->xmlChildrenNode;
    while (cur != NULL) {
        if ((!xmlStrcmp(cur->name, (const xmlChar *)"properties"))) {
            XML_GET_INT_ATTR(global_config->latency_measurement_mode, cur, "latency_mode", 0, uint32_t, 0);
        }

        cur = cur->next;
    }

    return 0;
}

static int string2TagID(const char* psz, test_util_metadata_tag_t* pTag)
{
    int rc = -1;
    uint32_t arraySize = sizeof(sg_metadataMapTable)/sizeof(sg_metadataMapTable[0]);

    for (uint32_t i = 0; i < arraySize; i++)
    {
        if (!strncmp((const char*)psz, sg_metadataMapTable[i].s, strlen(sg_metadataMapTable[i].s)))
        {
            pTag->isVendorTag = sg_metadataMapTable[i].isVendorTag;
            if (pTag->isVendorTag)
            {
                pTag->qccId = sg_metadataMapTable[i].tagId;
            }
            else
            {
                pTag->tagId = sg_metadataMapTable[i].tagId;
            }

            rc = 0;
            break;
        }
    }

    return rc;
}


static int parse_isp_metadata_tag_element(xmlNodePtr cur, test_util_metadata_tag_t* pTag)
{
    int rc = 0;
    char szTag[128] = {};

    XML_GET_STRING_ATTR(szTag, cur, "tag", 1, "UNKNOWN");
    rc = string2TagID(szTag, pTag);

    if (0 == rc)
    {
        switch (pTag->tagId)
        {
            default:
            {
                int32_t * p_data = (int32_t*)pTag->data;
                XML_GET_INT_ATTR(*p_data, cur, "data", 1, int32_t, 0);
                pTag->count = 1;
            }
            break;
        }
    }

    return rc;
}


int test_util_parse_xml_config_file(const char *filename, test_util_xml_input_t *xml_inputs, unsigned int max_num_inputs)
{
    return test_util_parse_xml_config_file_v2(filename, xml_inputs, max_num_inputs, NULL);
}

int test_util_parse_xml_config_file_v2(const char *filename, test_util_xml_input_t *xml_inputs, unsigned int max_num_inputs, test_util_global_config_t *global_config)
{
    int rc = 0;
    unsigned int numInputs = 0;

    xmlDocPtr doc;
    xmlNodePtr cur;

    doc = xmlParseFile(filename);

    if (doc == NULL)
    {
        QCARCAM_ERRORMSG("Document not parsed successfully");
        return -1;
    }

    cur = xmlDocGetRootElement(doc);

    if (cur == NULL)
    {
        QCARCAM_ERRORMSG("Empty config file");
        xmlFreeDoc(doc);
        return -1;
    }

    if (xmlStrcmp(cur->name, (const xmlChar *) "qcarcam_inputs"))
    {
        QCARCAM_ERRORMSG("Wrong config file format, root node != qcarcam_inputs");
        xmlFreeDoc(doc);
        return -1;
    }

    cur = cur->xmlChildrenNode;
    while (cur != NULL)
    {
        if ((!xmlStrcmp(cur->name, (const xmlChar *)"input_device")))
        {
            rc = parse_input_element(doc, cur, &xml_inputs[numInputs]);
            if (!rc)
            {
                numInputs++;
            }
            else
            {
                QCARCAM_ERRORMSG("Missing parameter in config file");
            }

            if (numInputs >= max_num_inputs)
            {
                QCARCAM_ERRORMSG("Parsed max number of inputs (%d). Skip rest", numInputs);
                break;
            }
        }
        else if ((!xmlStrcmp(cur->name, (const xmlChar *)"extra_mode")))
        {
            if (global_config)
            {
                rc = parse_latency_mode(doc, cur, global_config);
                if (rc)
                {
                    QCARCAM_ERRORMSG("Missing parameter in config file");
                }
            }
        }
        cur = cur->next;
    }

    xmlFreeDoc(doc);
    return numInputs;
}
#else // #ifndef __INTEGRITY

#define MAX_ATTR 30                // Maximum number of attributes for each element in XML
#define MAX_READ_BUFF_SIZE 10000   // Maximum buffer size for file read in bytes

struct xml_data
{
    int root_found;
    int numInputs;
    XML_Parser parser;
    test_util_xml_input_t* inputs;
    test_util_global_config_t* global_config;
    
};

int xml_find_attr(const char *label, const char **attr)
{
    int rc = -1;
    for (int x = 0; x < MAX_ATTR; x += 2)
    {
        if (attr[x] == NULL)
        {
            break;
        }
        else if(!strncmp(label, attr[x], strlen(attr[x])))
        {
            rc = x;
            break;
        }
    }
    return rc;
}

static void xml_read_element(const char *name, const char **attr, xml_data* p_data)
{

    test_util_xml_input_t* input = &p_data->inputs[p_data->numInputs];

    if (!strncmp(name, "properties", strlen("properties")))
    {
        XML_GET_INT_ATTR(input->properties.qcarcam_input_id, attr, "input_id", 0, uint32_t, 0);
        XML_GET_INT_ATTR(input->properties.use_event_callback, attr, "event_callback", 1, int, 1);
        XML_GET_INT_ATTR(input->properties.frame_timeout, attr, "frame_timeout", 1, int, -1);
        XML_GET_INT_ATTR(input->properties.op_mode, attr, "op_mode", 1, QCarCamOpmode_e, QCARCAM_OPMODE_MAX);
        XML_GET_INT_ATTR(input->properties.subscribe_parameter_change, attr, "subscribe_parameter_change", 1, int, 0);
        XML_GET_INT_ATTR(input->properties.delay_time, attr, "delay_time", 1, int, 0);
        XML_GET_INT_ATTR(input->properties.recovery, attr, "recovery", 1, bool, 0);
        XML_GET_INT_ATTR(p_data->global_config->latency_measurement_mode, attr, "latency_mode", 0, CameraLatencyMeasurementModeType, 0);
        
    }
    else if (!strncmp(name, "display_setting", strlen("display_setting")))
    {
        char format[16];

        XML_GET_INT_ATTR(input->window_params.display_id, attr, "display_id", 0, int, 0);
        XML_GET_FLOAT_ATTR(input->window_params.window_size[0], attr, "window_width", 1, 1.0f);
        XML_GET_FLOAT_ATTR(input->window_params.window_size[1], attr, "window_height", 1, 1.0f);
        XML_GET_FLOAT_ATTR(input->window_params.window_pos[0], attr, "window_pos_x", 1, 0.0f);
        XML_GET_FLOAT_ATTR(input->window_params.window_pos[1], attr, "window_pos_y", 1, 0.0f);
        XML_GET_FLOAT_ATTR(input->window_params.window_source_size[0], attr, "src_width", 1, 1.0f);
        XML_GET_FLOAT_ATTR(input->window_params.window_source_size[1], attr, "src_height", 1, 1.0f);
        XML_GET_FLOAT_ATTR(input->window_params.window_source_pos[0], attr, "src_x", 1, 0.0f);
        XML_GET_FLOAT_ATTR(input->window_params.window_source_pos[1], attr, "src_y", 1, 0.0f);
        XML_GET_INT_ATTR(input->window_params.zorder, attr, "zorder", 1, int, 0);
        XML_GET_INT_ATTR(input->window_params.pipeline_id, attr, "pipeline", 1, int, -1);
        XML_GET_INT_ATTR(input->window_params.n_buffers_display, attr, "nbufs", 1, int, 5);
        XML_GET_STRING_ATTR(format, attr, "format", 1, "uyvy");

        input->window_params.format = translate_format_string(format);
    }
    else if (!strncmp(name, "output_setting", strlen("output_setting")))
    {
        XML_GET_INT_ATTR(input->output_params.n_buffers, attr, "nbufs", 1, int, 5);
        XML_GET_INT_ATTR(input->output_params.width, attr, "width", 1, int, -1);
        XML_GET_INT_ATTR(input->output_params.height, attr, "height", 1, int, -1);
        XML_GET_INT_ATTR(input->output_params.stride, attr, "stride", 1, int, -1);
        XML_GET_INT_ATTR(input->output_params.format, attr, "format", 1, QCarCamColorFmt_e, -1);
        XML_GET_INT_ATTR(input->output_params.frame_rate_config.frameDropMode, attr, "framedrop_mode", 1, QCarCamFrameDropMode_e, QCARCAM_KEEP_ALL_FRAMES);
        XML_GET_INT_ATTR(input->output_params.frame_rate_config.frameDropPeriod, attr, "framedrop_period", 1, char, 0);
        XML_GET_INT_ATTR(input->output_params.frame_rate_config.frameDropPattern, attr, "framedrop_pattern", 1, int, 0);
        XML_GET_INT_ATTR(input->output_params.num_batch_frames, attr, "batch_frames", 1, int, 1);
        XML_GET_INT_ATTR(input->output_params.detect_first_phase_timer, attr, "detect_first_phase_timer", 1, int, 10);
        XML_GET_INT_ATTR(input->output_params.frame_increment, attr, "frame_increment", 1, int, 0);
        XML_GET_INT_ATTR(input->output_params.cropRegion.width, attr, "crop_width", 1, int, 0);
        XML_GET_INT_ATTR(input->output_params.cropRegion.height, attr, "crop_height", 1, int, 0);
        XML_GET_INT_ATTR(input->output_params.cropRegion.x, attr, "crop_x", 1, int, 0);
        XML_GET_INT_ATTR(input->output_params.cropRegion.y, attr, "crop_y", 1, int, 0);
    }
    else if (!strncmp(name, "inject_setting", strlen("inject_setting")))
    {
        XML_GET_INT_ATTR(input->inject_params.pattern, attr, "pattern", 0, QCarCamColorPattern_e, 0);
        XML_GET_INT_ATTR(input->inject_params.bitdepth, attr, "bitdepth", 0, QCarCamColorBitDepth_e, 0);
        XML_GET_INT_ATTR(input->inject_params.pack, attr, "pack", 0, QCarCamColorPack_e, 0);
        XML_GET_INT_ATTR(input->inject_params.buffer_size[0], attr, "width", 0, int, 0);
        XML_GET_INT_ATTR(input->inject_params.buffer_size[1], attr, "height", 1, int, 0);
        XML_GET_INT_ATTR(input->inject_params.stride, attr, "stride", 0, int, 0);
        XML_GET_INT_ATTR(input->inject_params.n_buffers, attr, "nbufs", 1, int, 1);
        XML_GET_STRING_ATTR(input->inject_params.filename, attr, "filename", 1, "bayer_input.raw");
        XML_GET_INT_ATTR(input->inject_params.n_frames, attr, "nframes", 1, int, 1);
        XML_GET_INT_ATTR(input->inject_params.repeat, attr, "repeat", 1, int, -1);
        XML_GET_INT_ATTR(input->inject_params.framerate, attr, "framerate", 1, int, 30);
        XML_GET_INT_ATTR(input->inject_params.singlebuf, attr, "singlebuf", 1, int, 0);
    }
    else if (!strncmp(name, "exposure_setting", strlen("exposure_setting")))
    {
        XML_GET_FLOAT_ATTR(input->exp_params.exposure_time, attr, "exp_time", 1, 31.147f);
        XML_GET_FLOAT_ATTR(input->exp_params.gain, attr, "gain", 1, 1.5f);
        XML_GET_INT_ATTR(input->exp_params.manual_exp, attr, "enable_manual", 1, int, -1);
    }
}

static void open_element(void *data, const char *name, const char **attr)
{
    xml_data *p_data = (xml_data *)data;

    if (!strncmp(name, "qcarcam_inputs", strlen("qcarcam_inputs")))
    {
        p_data->root_found = 1;
    }

    if (p_data->root_found)
    {
        xml_read_element(name, attr, p_data);

    }
    else
    {
        QCARCAM_ERRORMSG("Document not parsed successfully");
        XML_StopParser(p_data->parser, 0);
    }
}

static void close_element(void *data, const char *name)
{
    xml_data *p_data = (xml_data *)data;

    if (!strncmp(name, "input_device", strlen("input_device")))
    {
        p_data->numInputs++;
    }
}

int test_util_parse_xml_config_file(const char* filename, test_util_xml_input_t* xml_inputs, unsigned int max_num_inputs)
{
    FILE *fp = NULL;
    xml_data data = {};
    char read_buffer[MAX_READ_BUFF_SIZE] = {0};
    size_t length = 0;

    data.inputs = xml_inputs;

    data.parser = XML_ParserCreate(NULL);
    XML_SetUserData(data.parser, &data);
    XML_SetElementHandler(data.parser, open_element, close_element);

    fp = fopen(filename, "rb");

    if (fp != NULL)
    {
        QCARCAM_INFOMSG("Reading config file : ​%s", filename);

        length = fread(&read_buffer, sizeof(char), MAX_READ_BUFF_SIZE, fp);

        if (!XML_Parse(data.parser, read_buffer, length, 1))
        {
            QCARCAM_ERRORMSG("%s at line %lu\n", XML_ErrorString(XML_GetErrorCode(data.parser)), XML_GetCurrentLineNumber(data.parser));
            return 0;
        }

        fclose(fp);
    }
    else
    {
        if (!strncmp(filename, "/ghs_test/Camera/config/qcarcam_config_single_cvbs.xml",
                     strlen("/ghs_test/Camera/config/qcarcam_config_single_cvbs.xml")))
        {
            length = static_cast<size_t>(sizeof(xml_dump_cvbs)/sizeof(xml_dump_cvbs[0]));
            if (!XML_Parse(data.parser, xml_dump_cvbs, length, 1))
            {
                QCARCAM_ERRORMSG("%s at line %lu\n", XML_ErrorString(XML_GetErrorCode(data.parser)), XML_GetCurrentLineNumber(data.parser));
                return 0;
            }
        }
        else
        {
            length = static_cast<size_t>(sizeof(xml_dump)/sizeof(xml_dump[0]));
            if (!XML_Parse(data.parser, xml_dump, length, 1))
            {
                QCARCAM_ERRORMSG("%s at line %lu\n", XML_ErrorString(XML_GetErrorCode(data.parser)), XML_GetCurrentLineNumber(data.parser));
                return 0;
            }
        }
    }

    XML_ParserFree(data.parser);
    return data.numInputs;
}

int test_util_parse_xml_config_file_v2(const char *filename, test_util_xml_input_t *xml_inputs, unsigned int max_num_inputs, test_util_global_config_t *global_config)
{
    FILE *fp = NULL;
    xml_data data = {};
    char read_buffer[MAX_READ_BUFF_SIZE] = {0};
    size_t length = 0;

    data.inputs = xml_inputs;
    data.global_config = global_config;

    data.parser = XML_ParserCreate(NULL);
    XML_SetUserData(data.parser, &data);
    XML_SetElementHandler(data.parser, open_element, close_element);

    fp = fopen(filename, "rb");

    if (fp != NULL)
    {
        QCARCAM_INFOMSG("Reading config file : ​%s", filename);

        length = fread(&read_buffer, sizeof(char), MAX_READ_BUFF_SIZE, fp);

        if (!XML_Parse(data.parser, read_buffer, length, 1))
        {
            QCARCAM_ERRORMSG("%s at line %lu\n", XML_ErrorString(XML_GetErrorCode(data.parser)), XML_GetCurrentLineNumber(data.parser));
            return 0;
        }

        fclose(fp);
    }
    else
    {
        if (!strncmp(filename, "/ghs_test/Camera/config/qcarcam_config_single_cvbs.xml",
                     strlen("/ghs_test/Camera/config/qcarcam_config_single_cvbs.xml")))
        {
            length = static_cast<size_t>(sizeof(xml_dump_cvbs)/sizeof(xml_dump_cvbs[0]));
            if (!XML_Parse(data.parser, xml_dump_cvbs, length, 1))
            {
                QCARCAM_ERRORMSG("%s at line %lu\n", XML_ErrorString(XML_GetErrorCode(data.parser)), XML_GetCurrentLineNumber(data.parser));
                return 0;
            }
        }
        else
        {
            length = static_cast<size_t>(sizeof(xml_dump)/sizeof(xml_dump[0]));
            if (!XML_Parse(data.parser, xml_dump, length, 1))
            {
                QCARCAM_ERRORMSG("%s at line %lu\n", XML_ErrorString(XML_GetErrorCode(data.parser)), XML_GetCurrentLineNumber(data.parser));
                return 0;
            }
        }
    }

    XML_ParserFree(data.parser);
    return data.numInputs;
}
#endif

///////////////////////////////////////////////////////////////////////////////
/// test_util_get_time
///
/// @brief Get current time
///
/// @param pTime          Pointer to current time
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
int test_util_get_time(unsigned long long *pTime)
{
    int ret = -1;

    if( pTime )
    {
        uint64_t tm = 0;
        if (0 == OSAL_TimeGetTimeMilliSecond(&tm, QCX_CLOCK_MONOTONIC))
        {
            *pTime = tm;
            ret = 0;
        }
    }

    return ret;
}
