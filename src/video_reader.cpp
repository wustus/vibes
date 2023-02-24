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
    int &rgb_frame_size = video_ctx->rgb_frame_size;
    AVRational &time_base = video_ctx->time_base;
    auto &format_ctx = video_ctx->format_ctx;
    auto &codec_ctx = video_ctx->codec_ctx;
    auto &codec_params = video_ctx->codec_params;
    int &video_stream_index = video_ctx->video_stream_index;
    auto &packet = video_ctx->packet;
    auto &frame = video_ctx->frame;
    
    format_ctx = avformat_alloc_context();
    
    if (!format_ctx) {
        std::cout << "Couldn't allocate AvFormatContext." << std::endl;
        return false;
    }
    
    if (avformat_open_input(&format_ctx, filename, NULL, NULL) != 0) {
        std::cout << "Couldn't open file" << std::endl;
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
            width = codec_params->width;
            height = codec_params->height;
            rgb_frame_size = width * height * 4;
            video_stream_index = i;
            time_base = stream[i].time_base;
            break;
        }
    }
    
    if (video_stream_index == -1) {
        std::cout << "Couldn't find video stream" << std::endl;
        return false;
    }
    
    codec_ctx = avcodec_alloc_context3(codec);
    
    if (!codec_ctx) {
        std::cout << "Couldn't allocate Codec Context" << std::endl;
        return false;
    }
    
    if (avcodec_parameters_to_context(codec_ctx, codec_params) < 0) {
        std::cout << "Couldn't initialize Codec Context" << std::endl;
        return false;
    }
    
    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        std::cout << "Couldn't open Codec" << std::endl;
        return false;
    }
    
    frame = av_frame_alloc();
    
    if(!frame) {
        std::cout << "Couldn't allocate frame" << std::endl;
        return false;
    }
    
    packet = av_packet_alloc();
    
    if (!packet) {
        std::cout << "Couldn't allocate packet" << std::endl;
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
            std::cout << "Couldn't decode packket " << av_make_error_string(error_buffer, 256, response) << std::endl;
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
    av_frame_free(&video_ctx->frame);
    av_packet_free(&video_ctx->packet);
    avcodec_free_context(&video_ctx->codec_ctx);
}

