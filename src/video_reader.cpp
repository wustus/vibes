//
//  video_reader.cpp
//  opengl-foolery
//
//  Created by Justus Stahlhut on 21.01.23.
//

#include "video_reader.hpp"

bool open_video_reader(const char *filename, VideoReaderContext *video_ctx) {
    
    int &width = video_ctx->width;
    int &height = video_ctx->height;
    short &position = video_ctx->position;
    
    int &rgb_frame_size = video_ctx->rgb_frame_size;
    AVRational &time_base = video_ctx->time_base;
    auto &format_ctx = video_ctx->format_ctx;
    auto &codec_ctx = video_ctx->codec_ctx;
    auto &codec_params = video_ctx->codec_params;
    int &video_stream_index = video_ctx->video_stream_index;
    
    auto &buffersrc_ctx = video_ctx->buffersrc_ctx;
    auto &buffersink_ctx = video_ctx->buffersink_ctx;
    auto &filter_graph = video_ctx->filter_graph;
    auto &filter_inputs = video_ctx->filter_inputs;
    auto &filter_outputs = video_ctx->filter_outputs;
    
    auto &packet = video_ctx->packet;
    auto &frame = video_ctx->frame;
    
    format_ctx = avformat_alloc_context();
    
    // suppress missing accelerated video conversion message
    av_log_set_level(AV_LOG_ERROR);
    
    if (!format_ctx) {
        std::cerr << "Couldn't allocate AvFormatContext." << std::endl;
        return false;
    }
    
    if (avformat_open_input(&format_ctx, filename, NULL, NULL) != 0) {
        std::cerr << "Couldn't open file" << std::endl;
        return false;
    }
    
    video_stream_index = -1;
    const AVCodec *codec;
    
    for (int i=0; i!=format_ctx->nb_streams; i++) {
        
        auto stream = format_ctx->streams[i];
        codec_params = stream->codecpar;
        codec = avcodec_find_decoder(codec_params->codec_id);
        
        if (!codec) {
            continue;
        }
        
        if (codec_params->codec_type == AVMEDIA_TYPE_VIDEO) {
            // read video holds 2 x 2 videoså
            width = codec_params->width / 2;
            height = codec_params->height / 2;
            rgb_frame_size = width * height * 4;
            video_stream_index = i;
            time_base = stream[i].time_base;
            break;
        }
    }
    
    if (video_stream_index == -1) {
        std::cerr << "Couldn't find video stream" << std::endl;
        return false;
    }
    
    codec_ctx = avcodec_alloc_context3(codec);
    
    if (!codec_ctx) {
        std::cerr << "Couldn't allocate Codec Context" << std::endl;
        return false;
    }
    
    if (avcodec_parameters_to_context(codec_ctx, codec_params) < 0) {
        std::cerr << "Couldn't initialize Codec Context" << std::endl;
        return false;
    }
    
    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        std::cerr << "Couldn't open Codec" << std::endl;
        return false;
    }
    
    filter_graph = avfilter_graph_alloc();
    
    char args[512];
    snprintf(args, sizeof(args), "buffer=video_size=%dx%d:pix_fmt=%d:time_base=1/1:pixel_aspect=0/1[in];"
                                 "[in]crop=out_w=%d:out_h=%d:x=%d:y=%d[out];"
                                 "[out]buffersink", 2 * width, 2 * height, AV_PIX_FMT_YUV420P, width, height, width * (position % 2), height * (position > 1));
    
    if (avfilter_graph_parse2(filter_graph, args, &filter_inputs, &filter_outputs) < 0) {
        std::cerr << "Couldn't parse filter graph: " << std::strerror(errno) << std::endl;
        return false;
    }
    
    if (avfilter_graph_config(filter_graph, NULL) < 0) {
        std::cerr << "Couldn't set filter graph config: " << std::strerror(errno) << std::endl;
        return false;
    }
    
    buffersrc_ctx = avfilter_graph_get_filter(filter_graph, "Parsed_buffer_0");
    buffersink_ctx = avfilter_graph_get_filter(filter_graph, "Parsed_buffersink_2");
    
    packet = av_packet_alloc();
    
    if (!packet) {
        std::cerr << "Couldn't allocate packet" << std::endl;
        return false;
    }
    
    // Frame holding cropped image
    frame = av_frame_alloc();
    
    if(!frame) {
        std::cerr << "Couldn't allocate frame" << std::endl;
        return false;
    }
    
    return true;
}

int read_frame_and_set_flag(VideoReaderContext *video_ctx) {
    
    auto &format_ctx = video_ctx->format_ctx;
    auto &packet = video_ctx->packet;
    
    int response = av_read_frame(format_ctx, packet);
    
    if (response < 0) {
        video_ctx->end_of_file = true;
    }
    
    return response;
}

static AVPixelFormat correct_deprecated_format(AVPixelFormat pix_fmt) {
    switch (pix_fmt) {
        case AV_PIX_FMT_YUVJ420P:
            return AV_PIX_FMT_YUV420P;
        case AV_PIX_FMT_YUVJ422P:
            return AV_PIX_FMT_YUV422P;
        case AV_PIX_FMT_YUVJ444P:
            return AV_PIX_FMT_YUV444P;
        case AV_PIX_FMT_YUVJ440P:
            return AV_PIX_FMT_YUV440P;
        default:
            return pix_fmt;
    }
}

bool read_frame(VideoReaderContext *video_ctx, uint8_t *frame_buffer, int64_t *pts) {
    
    auto &codec_ctx = video_ctx->codec_ctx;
    int &video_stream_index = video_ctx->video_stream_index;
    
    auto &buffersrc_ctx = video_ctx->buffersrc_ctx;
    auto &buffersink_ctx = video_ctx->buffersink_ctx;
    
    auto &scaler_ctx = video_ctx->scaler_ctx;
    auto &packet = video_ctx->packet;
    auto &frame = video_ctx->frame;
    
    int response;
    char *error_buffer = new char[256];
    
    while (read_frame_and_set_flag(video_ctx) >= 0) {
        if (packet->stream_index != video_stream_index) {
            av_packet_unref(packet);
            continue;
        }
        
        response = avcodec_send_packet(codec_ctx, packet);
        
        if (response < 0) {
            std::cout << "Couldn't decode packet " << av_make_error_string(error_buffer, 256, response) << std::endl;
            return false;
        }
        
        response = avcodec_receive_frame(codec_ctx, frame);
        
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            continue;
        } else if (response < 0) {
            delete[](error_buffer);
            std::cout << "Failed to decode packet." << std::endl;
            return false;
        }
        
        av_packet_unref(packet);
        break;
    }
    
    delete[](error_buffer);
    
    if (av_buffersrc_add_frame(buffersrc_ctx, frame) < 0) {
        std::cerr << "Couldn't add frame to buffersrc: " << std::strerror(errno) << std::endl;
        return false;
    }
    
    if (av_buffersink_get_frame(buffersink_ctx, frame) < 0) {
        std::cerr << "Couldn't get frame from buffersink: " << std::strerror(errno) << std::endl;
        return false;
    }
    
    AVPixelFormat pix_fmt = correct_deprecated_format(codec_ctx->pix_fmt);
    scaler_ctx = sws_getContext(frame->width, frame->height, pix_fmt,
                                frame->width, frame->height, AV_PIX_FMT_RGB0, SWS_BILINEAR,
                                NULL, NULL, NULL);
    
    if (!scaler_ctx) {
        std::cout << "Could not allocate scaler" << std::endl;
        return false;
    }
    
    *pts = frame->pts;
    
    uint8_t *dst_buffer[4] = { frame_buffer, NULL, NULL, NULL };
    int dst_linesize[4] = { frame->width * 4, 0, 0, 0 };
    
    sws_scale(scaler_ctx, frame->data, frame->linesize, 0, frame->height, dst_buffer, dst_linesize);
    
    return true;
}

void close_reader(VideoReaderContext *video_ctx) {
    
    sws_freeContext(video_ctx->scaler_ctx);
    avformat_close_input(&video_ctx->format_ctx);
    avformat_free_context(video_ctx->format_ctx);
    avfilter_graph_free(&video_ctx->filter_graph);
    av_frame_free(&video_ctx->frame);
    av_packet_free(&video_ctx->packet);
    avcodec_free_context(&video_ctx->codec_ctx);
}

