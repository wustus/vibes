//
//  video_reader.hpp
//  opengl-foolery
//
//  Created by Justus Stahlhut on 21.01.23.
//

#ifndef video_reader_hpp
#define video_reader_hpp

#include <iostream>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libavutil/avutil.h>
}

struct VideoReaderContext {
    int width, height;
    int rgb_frame_size;
    AVRational time_base;
    AVFormatContext *format_ctx;
    AVCodecContext *codec_ctx;
    AVCodecParameters *codec_params;
    int video_stream_index;
    AVFrame *frame;
    AVPacket *packet;
    SwsContext *scaler_ctx;
    bool end_of_file = false;
};

bool open_video_reader(const char *filename, VideoReaderContext *video_ctx);
bool read_frame(VideoReaderContext *video_ctx, uint8_t *frame_buffer, int64_t *pts);
void close_reader(VideoReaderContext *video_ctx);

#endif /* video_reader_hpp */

