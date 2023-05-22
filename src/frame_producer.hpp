//
//  frame_producer.hpp
//  vibes-proto
//
//  Created by Justus Stahlhut on 19.02.23.
//

#ifndef frame_producer_hpp
#define frame_producer_hpp

#include <stdlib.h>
#include <deque>
#include <thread>
#include <fstream>
#include <sstream>

#include "video_reader.hpp"

class FrameProducer {
public:
    FrameProducer(const char* video_path, const int number_of_devices, const char* position_path, const int frames_in_buffer);
    ~FrameProducer();
    bool produce_frame(void*& data, int64_t*& pts);
    void start_thread();
    void join_thread();
    void shutdown_thread();
    float get_video_width();
    float get_video_height();
    int get_rgb_frame_size();
    int get_timebase_num();
    int get_timebase_den();
private:
    VideoReaderContext video_ctx;
    const int number_of_devices;
    const int frames_in_buffer;
    std::deque<void*> frame_buffer;
    std::deque<int64_t> pts_buffer;
    std::thread producer;
    bool pending_thread_close = false;
    static constexpr int ALIGNMENT = 128;
    
    void buffer_frames();
};

#endif /* frame_producer_hpp */
