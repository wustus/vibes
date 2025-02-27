
#include <math.h>
#include "synchronization_handler.hpp"

#include "shader.h"
#include "frame_producer.hpp"

#ifdef __APPLE__
    #include <OpenGL/gl.h>
#endif

#ifdef __unix
    #include <GLES3/gl3.h>
#endif

#include <GLFW/glfw3.h>
#include "tic_tac_toe_vertices.h"
#include <ctime>
#include <iomanip>

int RGB_FRAME_SIZE;

float MONITOR_WIDTH;
float MONITOR_HEIGHT;

#ifdef __APPLE__
    float VIEWPORT_WIDTH = 1920;
    float VIEWPORT_HEIGHT = 1080;
#endif

#ifdef __unix
    float VIEWPORT_WIDTH = 1024;
    float VIEWPORT_HEIGHT = 768;
#endif

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    glViewport(0, 0, width, height);
}

void process_input(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
}

int get_next_aligned_number(int alignment) {    
    return RGB_FRAME_SIZE + RGB_FRAME_SIZE % alignment;
}

const char* decode_error(GLenum err) {
    
    switch(err) {
        case 0x500:
            return "GL_INVALID_ENUM::Given when an enumeration parameter is not a legal enumeration for that function. This is  given only for local problems; if the spec allows the enumeration in certain circumstances, where other parameters or state dictate those circumstances, then GL_INVALID_OPERATION is the result instead.";
        case 0x0501:
            return "GL_INVALID_VALUE::Given when a value parameter is not a legal value for that function. This is only given for local problems; if the spec allows the value in certain circumstances, where other parameters or state dictate those circumstances, then GL_INVALID_OPERATION is the result instead.";
        case 0x0502:
            return "GL_INVALID_OPERATION::Given when the set of state for a command is not legal for the parameters given to that command. It is also given for commands where combinations of parameters define what the legal parameters are.";
        case 0x0503:
            return "GL_STACK_OVERFLOW::Given when a stack pushing operation cannot be done because it would overflow the limit of that stack's size.";
        case 0x0504:
            return "GL_STACK_UNDERFLOW::Given when a stack popping operation cannot be done because the stack is already at its lowest point.";
        case 0x0505:
            return "GL_OUT_OF_MEMORY::Given when performing an operation that can allocate memory, and the memory cannot be allocated. The results of OpenGL functions that return this error are undefined; it is allowable for partial execution of an operation to happen in this circumstance.";
        case 0x506:
            return "GL_INVALID_FRAMEBUFFER_OPERATION::Given when doing anything that would attempt to read from or write/render to a framebuffer that is not complete.";
        case 0x0507:
            return "GL_CONTEXT_LOST::Given if the OpenGL context has been lost, due to a graphics card reset.";
        case 0x8031:
            return "GL_TABLE_TOO_LARGE::Part of the ARB_imaging extension.";
    }
    
    return "GL_ERROR_NOT_FOUND";
}

bool get_error(const char* method) {
    GLenum err;
    while((err = glGetError()) != GL_NO_ERROR) {
        std::cout << method << "::" << decode_error(err) << std::endl;
        return 1;
    }
    
    return 0;
}

void glfw_error_callback(int error, const char* description) {
    std::cout << "GLFW Error: " << description << std::endl;
}


int main(int argc, const char* argv[]) {
    
    if (argc < 2) {
        throw std::runtime_error("Please add the Number of Devices.");
    }
    
    const int NUMBER_OF_DEVICES = std::stoi(argv[1]);
    float VIDEO_WIDTH, VIDEO_HEIGHT;

    // find devices
    Network network = Network(NUMBER_OF_DEVICES);

    std::cout << "Setting Local Address." << std::endl;
    network.set_local_addr();
    std::cout << "\tLocal Address: " << network.get_network_config()->address << std::endl;
    network.discover_devices();
     
    // frames in PBO
    const int FRAMES_IN_BUFFER = 16;
    
    // create frame producer and initialize video context
#ifdef __APPLE__
    FrameProducer frame_producer = FrameProducer("/Users/justus/dev/vibes/assets/video_split.mov", NUMBER_OF_DEVICES, "/Users/justus/dev/vibes/assets/POSITION", FRAMES_IN_BUFFER);
#endif
    
#ifdef __unix
    FrameProducer frame_producer = FrameProducer("../assets/video_split.mov", NUMBER_OF_DEVICES, "../assets/POSITION", FRAMES_IN_BUFFER);
#endif
    
    // start producing frames
    frame_producer.start_thread();
    
    VIDEO_WIDTH = frame_producer.get_video_width();
    VIDEO_HEIGHT = frame_producer.get_video_height();
    
    RGB_FRAME_SIZE = frame_producer.get_rgb_frame_size();
    
    float VIEWPORT_WIDTH_RATIO = 1 - VIDEO_WIDTH / VIEWPORT_WIDTH;
    float VIEWPORT_HEIGHT_RATIO = 1 - VIDEO_HEIGHT / VIEWPORT_HEIGHT;
    
    float ASPECT_RATIO = VIEWPORT_HEIGHT / VIEWPORT_WIDTH;
    
    // get the next bigger number of RGBA_FRAME_SIZE in the series x^n where x in 2^i
    int BUFFER_SIZE = get_next_aligned_number(128);
    
    int TIMEBASE_NUM = frame_producer.get_timebase_num();
    int TIMEBASE_DEN = frame_producer.get_timebase_den();
    
    
    /*
     *  GLFW / OpenGL Confiuration
     */
    
    
    if (!glfwInit()) {
        std::cout << "Failed to initialize GLFW." << std::endl;
        return 1;
    }
    
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#endif
    
#ifdef __unix
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif
    
    glfwWindowHint(GLFW_DOUBLEBUFFER, GL_TRUE);
    
    glfwSetErrorCallback(glfw_error_callback);
    
    // get primary display for fullscreen
    GLFWmonitor *primary = glfwGetPrimaryMonitor();
    // create window
    GLFWwindow* window = glfwCreateWindow(VIEWPORT_WIDTH, VIEWPORT_HEIGHT, "VIBES", NULL, NULL);
    
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    
    // TODO: WRONG - GET REAL MONITOR RESOLUTION
    const GLFWvidmode *video_mode = glfwGetVideoMode(primary);
    MONITOR_WIDTH = video_mode->width;
    MONITOR_HEIGHT = video_mode->height;
    
    // hide cursor
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    
    glfwMakeContextCurrent(window);
    
#ifdef __APPLE__
    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
#endif
    
#ifdef __unix
    if (!gladLoadGLES2Loader((GLADloadproc) glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
#endif
    
    glfwSwapInterval(0);
    glClearColor(.0f, .0f, .0f, 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClear(GL_COLOR_BUFFER_BIT);
    glfwPollEvents();
    glfwSwapBuffers(window);
    
    // load and compile shaders
#ifdef __APPLE__
    Shader shader("/Users/justus/dev/vibes/src/shaders/rgb/vertex_shader.vs", "/Users/justus/dev/vibes/src/shaders/rgb/fragment_shader.fs",
                  "/Users/justus/dev/vibes/src/shaders/texture/vertex_shader.vs", "/Users/justus/dev/vibes/src/shaders/texture/fragment_shader.fs");
#endif
    
#ifdef __unix
    Shader shader("../src/shaders/rgb/vertex_shader_es.vs", "../src/shaders/rgb/fragment_shader_es.fs",
                  "../src/shaders/texture/vertex_shader_es.vs", "../src/shaders/texture/fragment_shader_es.fs");
#endif
    
    // change viewPort (renderable area) with window size
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    
    float vertices[] = {
        // vertices                                            texture
         1 - VIEWPORT_WIDTH_RATIO,  1 - VIEWPORT_HEIGHT_RATIO, 1, 1,
         1 - VIEWPORT_WIDTH_RATIO, -1 + VIEWPORT_HEIGHT_RATIO, 1, 0,
        -1 + VIEWPORT_WIDTH_RATIO, -1 + VIEWPORT_HEIGHT_RATIO, 0, 0,
        -1 + VIEWPORT_WIDTH_RATIO,  1 - VIEWPORT_HEIGHT_RATIO, 0, 1
    };
    
    unsigned int indices[] = {
        0, 1, 3, // first triangle
        1, 2, 3, // second triangle
    };
    
    Tic_Tac_Toe_Vertices ttt_vertices = Tic_Tac_Toe_Vertices(100);
    
    const float* TIC_TAC_TOE_FIELD = ttt_vertices.TIC_TAC_TOE_FIELD;
    const unsigned int* TIC_TAC_TOE_FIELD_INDICES = ttt_vertices.TIC_TAC_TOE_FIELD_INDICES;
    
    unsigned int RGB_VBO;
    glGenBuffers(1, &RGB_VBO);
    
    // create element buffer object, which is sent to GPU as a whole
    // EBO uses indices to draw triangles in a given order to avoid overlap
    unsigned int RGB_EBO;
    glGenBuffers(1, &RGB_EBO);
    
    unsigned int RGB_VAO;
    glGenVertexArrays(1, &RGB_VAO);
    
    // bind vertex array object
    glBindVertexArray(RGB_VAO);
    
    // copy vertices array in a buffer for OpenGL to use
    glBindBuffer(GL_ARRAY_BUFFER, RGB_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(TIC_TAC_TOE_FIELD) * sizeof(float), TIC_TAC_TOE_FIELD, GL_STATIC_DRAW);
    
    // position attributes
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*) 0);
    glEnableVertexAttribArray(0);
    
    // vertex sequence
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, RGB_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(TIC_TAC_TOE_FIELD_INDICES), TIC_TAC_TOE_FIELD_INDICES, GL_STATIC_DRAW);
    
    shader.use_rgb_shader();
    shader.set_float("u_aspect_ratio", ASPECT_RATIO);
    
    glBindVertexArray(RGB_VAO);
    
    // determine master
    SynchronizationHandler sync_handler = SynchronizationHandler(network);
    std::thread ttt_thread([&sync_handler]() { sync_handler.determine_master(); });
    
    std::mutex* mtx = &sync_handler.ttt.mtx;
    
    {
        std::unique_lock<std::mutex> lock(*mtx);
        while (!sync_handler.ttt.new_move) sync_handler.ttt.cv.wait(lock);
    }
    
    float* player_vertices = ttt_vertices.get_current_player_indices(sync_handler.ttt.player);
    const unsigned int* player_indices = sync_handler.ttt.player == 'X' ? ttt_vertices.TIC_TAC_TOE_PLAYER_ONE_INDICES : ttt_vertices.TIC_TAC_TOE_PLAYER_TWO_INDICES;
    size_t player_vertices_size = sync_handler.ttt.player == 'X' ? ttt_vertices.get_player_one_vertices_size() : ttt_vertices.get_player_two_number_vertices();
    size_t player_indices_size = sync_handler.ttt.player == 'X' ? ttt_vertices.get_player_one_indices_size() : ttt_vertices.get_player_two_indices_size();
    
    bool winner_determined = false;
    float r,g,b,alpha = 0.0f;
    
    while (!glfwWindowShouldClose(window)) {
        
        process_input(window);
        
        // clear the colorbuffer
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        if (sync_handler.ttt.is_over) {
            shader.set_bool("u_use_ratio", false);
            
            winner_determined = true;
            if (sync_handler.ttt.is_won) {
                r = 25.0f  / 255.0f;
                g = 135.0f / 255.0f;
                b = 84.0f  / 255.0f;
            } else {
                r = 220.0f / 255.0f;
                g = 53.0f  / 255.0f;
                b = 69.0f  / 255.0f;
            }
        
            glBindBuffer(GL_ARRAY_BUFFER, RGB_VBO);
            glBufferData(GL_ARRAY_BUFFER, 24 * sizeof(float), ttt_vertices.get_tic_tac_toe_screen(r, g, b, alpha), GL_STATIC_DRAW);
            
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, RGB_EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * sizeof(float), ttt_vertices.TIC_TAC_TOE_SCREEN_INDICES, GL_STATIC_DRAW);
            
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            
            alpha += 0.01f;
            glfwWaitEventsTimeout(0.005f);
        }
        
        shader.set_bool("u_use_ratio", true);
        
        glBindBuffer(GL_ARRAY_BUFFER, RGB_VBO);
        glBufferData(GL_ARRAY_BUFFER, player_vertices_size * sizeof(float), player_vertices, GL_STATIC_DRAW);
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, RGB_EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, player_indices_size * sizeof(float), player_indices, GL_STATIC_DRAW);
        
        glDrawElements(GL_TRIANGLES, (int) player_indices_size, GL_UNSIGNED_INT, 0);
        
        
        // copy vertices array in a buffer for OpenGL to use
        glBindBuffer(GL_ARRAY_BUFFER, RGB_VBO);
        glBufferData(GL_ARRAY_BUFFER, ttt_vertices.get_tic_tac_toe_field_vertices_size() * sizeof(float), TIC_TAC_TOE_FIELD, GL_STATIC_DRAW);
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, RGB_EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, ttt_vertices.get_tic_tac_toe_field_indices_size() * sizeof(float), TIC_TAC_TOE_FIELD_INDICES, GL_STATIC_DRAW);
        
        glDrawElements(GL_TRIANGLES, (int)(ttt_vertices.get_tic_tac_toe_field_indices_size()), GL_UNSIGNED_INT, 0);
        
        auto ttt_field = sync_handler.ttt.field;
        
        std::vector<float*> vertices;
        std::vector<const unsigned int*> indices;
        
        std::vector<size_t> vertex_sizes;
        std::vector<size_t> index_sizes;
        
        for (int i=0; i!=9; i++) {
            if (ttt_field[i] == 'X') {
                vertices.push_back(ttt_vertices.get_tic_tac_toe_player_one(i));
                indices.push_back(ttt_vertices.TIC_TAC_TOE_PLAYER_ONE_INDICES);
                vertex_sizes.push_back(ttt_vertices.get_player_one_vertices_size());
                index_sizes.push_back(ttt_vertices.get_player_one_indices_size());
            } else if (ttt_field[i] == 'O') {
                vertices.push_back(ttt_vertices.get_tic_tac_toe_player_two(i));
                indices.push_back(ttt_vertices.TIC_TAC_TOE_PLAYER_TWO_INDICES);
                vertex_sizes.push_back(ttt_vertices.get_player_two_number_vertices());
                index_sizes.push_back(ttt_vertices.get_player_two_indices_size());
            }
        }
        
        // DRAW MOVES
        for (int i=0; i!=vertices.size(); i++) {
            auto move_vertices = vertices[i];
            auto move_indices = indices[i];
            auto move_vertex_size = vertex_sizes[i];
            auto move_index_size = index_sizes[i];
            
            glBindBuffer(GL_ARRAY_BUFFER, RGB_VBO);
            glBufferData(GL_ARRAY_BUFFER, move_vertex_size * sizeof(float), move_vertices, GL_STATIC_DRAW);
            
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, RGB_EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, move_index_size * sizeof(float), move_indices, GL_STATIC_DRAW);
            
            glDrawElements(GL_TRIANGLES, (int) move_index_size, GL_UNSIGNED_INT, 0);
        }
        
        glfwSwapBuffers(window);
        glfwPollEvents();
        
        // delete allocated vertex-memory
        for (float* x : vertices) {
            delete[] x;
        }
        
        if (!winner_determined) {
            std::unique_lock<std::mutex> lock(*mtx);
            while (!sync_handler.ttt.new_move && !sync_handler.ttt.is_over) sync_handler.ttt.cv.wait(lock);
        }
        
        sync_handler.ttt.new_move = false;
        if (alpha > 1.0f) {
            /*
            // empty window
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            glfwSwapBuffers(window);
            glfwPollEvents();
            */
            break;
        }
    }

    ttt_thread.join();
    
    delete[] player_vertices;
    
    sync_handler.set_offset();
    uint32_t offset = sync_handler.get_offset();
    
    uint32_t playback_start_time = sync_handler.get_start_time();
    time_t unix_time = (time_t) playback_start_time;
    
    std::cout << "Playback Start Time: " << std::ctime(&unix_time) << std::endl;

    
    // VIDEO RENDER
    
    // create and bind texture
    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    // how to handle overscaling
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    // texture filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    
    // frame row alignment
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    
    // create vertex buffer object, which is sent to GPU as a whole
    unsigned int VBO;
    glGenBuffers(1, &VBO);
    
    // create element buffer object, which is sent to GPU as a whole
    // EBO uses indices to draw triangles in a given order to avoid overlap
    unsigned int EBO;
    glGenBuffers(1, &EBO);
    
    unsigned int VAO;
    glGenVertexArrays(1, &VAO);
    
    // bind vertex array object
    glBindVertexArray(VAO);
    
    // copy vertices array in a buffer for OpenGL to use
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices) + 8, vertices, GL_STATIC_DRAW);
    
    // position attributes
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*) 0);
    glEnableVertexAttribArray(0);
    
    // vertex sequence
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);
    
    // copy index array in element buffer for OpenGL to use
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    
    unsigned int PBO;
    glGenBuffers(1, &PBO);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, PBO);
    
    glBufferData(GL_PIXEL_UNPACK_BUFFER, BUFFER_SIZE * FRAMES_IN_BUFFER, NULL, GL_STREAM_DRAW);
    
    // bind texture
    glBindTexture(GL_TEXTURE_2D, texture);
    
    // load texture into OpenGL
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, VIDEO_WIDTH, VIDEO_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    if (get_error("glTexImage")) {
        return -1;
    }
    
    glBindVertexArray(VAO);
    
    shader.use_texture_shader();
    /*
     *  Render Loop Parameters
     */
    
    int current_frame = 0;
    
    bool clear_ghosts = false;
    
    float start_time, end_time;
    
    std::deque<int64_t> pts_buffer;
    
    // render loop
    while (!glfwWindowShouldClose(window)) {
        
        static bool initial_frame = true;
        
        if (initial_frame) {
            /*
            while ((uint32_t) time(NULL) < playback_start_time) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            */
            glfwSetTime(0.0);
        }
        
        start_time = glfwGetTime();
        
        process_input(window);
        
        // clear the colorbuffer
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        if (get_error("glClear")) {
            return -1;
        }
        
        void* frame;
        int64_t* pts;
        double pt_in_seconds;
        
        if (!initial_frame) {
            if (!frame_producer.produce_frame(frame, pts)) {
                break;
            }
            pts_buffer.push_back(*pts);
        }
         
        if (initial_frame) {
            
            glBufferData(GL_PIXEL_UNPACK_BUFFER, BUFFER_SIZE * FRAMES_IN_BUFFER, NULL, GL_STREAM_DRAW);
            if (get_error("glBufferData")) {
                return -1;
            }
            
            for (int i=0; i!=FRAMES_IN_BUFFER; i++) {
                frame_producer.produce_frame(frame, pts);
                pts_buffer.push_back(*pts);
                glBufferSubData(GL_PIXEL_UNPACK_BUFFER, BUFFER_SIZE * i, BUFFER_SIZE, frame);
                free(frame);
                if (get_error("glBufferSubData")) {
                    return -1;
                }
            }
            
            initial_frame = false;
        }
        
        pt_in_seconds = pts_buffer.front() * (double) TIMEBASE_NUM / (double) TIMEBASE_DEN;
        pts_buffer.pop_front();
        
        if (pt_in_seconds > glfwGetTime()) {
            glfwWaitEventsTimeout(pt_in_seconds - glfwGetTime());
        }
        
        
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, (void*)(intptr_t)(BUFFER_SIZE * (current_frame % FRAMES_IN_BUFFER)));
        if (get_error("glTexSubImage")) {
            return -1;
        }
        
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        if (get_error("glDrawElements")) {
            return -1;
        }
        
        glfwSwapBuffers(window);
        glfwPollEvents();
        
        if (current_frame == 4) {
            clear_ghosts = true;
        }
        
        if (clear_ghosts) {
            glBufferSubData(GL_PIXEL_UNPACK_BUFFER, BUFFER_SIZE * ((FRAMES_IN_BUFFER + (current_frame - 4)) % FRAMES_IN_BUFFER), BUFFER_SIZE, frame);
            free(frame);
        }
        
        glFinish();
        if(get_error("glFinish")) {
            return -1;
        }
        
        end_time = glfwGetTime();
        
        std::cout << "---- FRAME " << current_frame + 1 << " ----" << std::endl;
        auto now = std::chrono::system_clock::now();
        auto t_s = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
        auto t_ym = std::chrono::duration_cast<std::chrono::microseconds>(now - t_s);
        std::time_t t = std::chrono::system_clock::to_time_t(t_s);
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&t), "%H:%M:%S");
        
        std::cout << "\tTime: " << ss.str() << "." << std::setw(6) << std::setfill('0') << t_ym.count() << std::endl;
        std::cout << "\tTPF:  " << (end_time - start_time) * 1000 << "ms" << std::endl;
        std::cout << "\t      " << 1 / (end_time - start_time) << "FPS" << std::endl;
        std::cout << "-----------------" << std::endl << std::endl;
        
        current_frame++;
    }
    
    frame_producer.shutdown_thread();
    
    glfwTerminate();
    
    return 0;
}
