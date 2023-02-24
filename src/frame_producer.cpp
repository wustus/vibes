//
//  frame_producer.cpp
//  vibes-proto
//
//  Created by Justus Stahlhut on 19.02.23.
//

#include "frame_producer.hpp"

FrameProducer::FrameProducer(const char* video_path, int frames_in_buffer) {
    
    // open video reader and create context
    if (!open_video_reader(video_path, &video_ctx)) {
        throw std::runtime_error("Couldn't open video reader");
    }
    
    FrameProducer::frames_in_buffer = frames_in_buffer;
}

FrameProducer::~FrameProducer() {
    close_reader(&video_ctx);
    shutdown_thread();
}

void FrameProducer::buffer_frames() {
    
    uint8_t* buffer;
    int64_t pts;
    
    if (posix_memalign((void**)&buffer, ALIGNMENT, video_ctx.rgb_frame_size) != 0) {
        std::cout << "Couldn't align buffer" << std::endl;
    }
    
    while (!video_ctx.end_of_file && !pending_thread_close) {
        if (frame_buffer.size() < 2 * frames_in_buffer) {
            void* data;
            
            if (!read_frame(&video_ctx, buffer, &pts)) {
                throw std::runtime_error("Couldn't read frame");
            }
            
            if (posix_memalign((void**)&data, ALIGNMENT, video_ctx.rgb_frame_size) != 0) {
                throw std::runtime_error("Couldn't align frame");
            }
            
            memcpy(data, buffer, video_ctx.rgb_frame_size);
            
            frame_buffer.push_back(data);
            pts_buffer.push_back(pts);
        }
    }
    
    free(buffer);
}

bool FrameProducer::produce_frame(void*& data, int64_t*& pts) {
    
    // wait until frame has been created
    while (frame_buffer.size() == 0) {
        if (video_ctx.end_of_file) {
            return false;
        }
        continue;
    }
    
    void* temp_data_buffer = *&frame_buffer.front();
    int64_t* temp_pts_buffer = &pts_buffer.front();
    
    frame_buffer.pop_front();
    pts_buffer.pop_front();
    
    data = temp_data_buffer;
    pts = temp_pts_buffer;
    
    return true;
}

void FrameProducer::start_thread() {
    producer = std::thread(&FrameProducer::buffer_frames, this);
}

void FrameProducer::join_thread() {
    producer.join();
}

void FrameProducer::shutdown_thread() {
    if (producer.joinable()) {
        pending_thread_close = true;
        producer.join();
    }
}

float FrameProducer::get_video_width() {
    return video_ctx.width;
}

float FrameProducer::get_video_height()	 {
    return video_ctx.height;
}

int FrameProducer::get_rgb_frame_size() {
    return video_ctx.rgb_frame_size;
}

int FrameProducer::get_timebase_num() {
    return video_ctx.time_base.num;
}

int FrameProducer::get_timebase_den() {
    return video_ctx.time_base.den;
}
