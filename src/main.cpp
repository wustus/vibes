
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

float VIDEO_WIDTH;
float VIDEO_HEIGHT;

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
    
    int NUMBER_OF_DEVICES = std::stoi(argv[1]);
    
    // find devices
    Network network = Network(NUMBER_OF_DEVICES);
    
    std::cout << "Setting Local Address." << std::endl;
    network.set_local_addr();
    std::cout << "\tLocal Address: " << network.get_network_config()->address << std::endl;
    network.discover_devices();
    
    // determine master
    SynchronizationHandler sync_handler = SynchronizationHandler(network);
    
    sync_handler.determine_master();
    
    sync_handler.set_offset();
    uint32_t offset = sync_handler.get_offset();
    
    uint32_t playback_start_time = sync_handler.get_start_time()gi;
    time_t unix_time = (time_t) playback_start_time;
    
    std::cout << "Playback Start Time: " << std::ctime(&unix_time) << std::endl;
    
    // frames in PBO
    int FRAMES_IN_BUFFER = 16;
    
    // create frame producer and initialize video context
#ifdef __APPLE__
    FrameProducer frame_producer = FrameProducer("/Users/justus/dev/vibes/assets/guitar_pi.mov", FRAMES_IN_BUFFER);
#endif
    
#ifdef __unix
    FrameProducer frame_producer = FrameProducer("../assets/guitar_pi.mov", FRAMES_IN_BUFFER);
#endif
    
    // start producing frames
    frame_producer.start_thread();
    
    VIDEO_WIDTH = frame_producer.get_video_width();
    VIDEO_HEIGHT = frame_producer.get_video_height();
    
    RGB_FRAME_SIZE = frame_producer.get_rgb_frame_size();
    
    float VIEWPORT_WIDTH_RATIO = 1 - VIDEO_WIDTH / VIEWPORT_WIDTH;
    float VIEWPORT_HEIGHT_RATIO = 1 - VIDEO_HEIGHT / VIEWPORT_HEIGHT;
    
    // get the next bigger number of RGBA_FRAME_SIZE in the series x^n where x in 2^i
    int BUFFER_SIZE = get_next_aligned_number(128);
    
    int TIMEBASE_NUM = frame_producer.get_timebase_num();
    int TIMEBASE_DEN = frame_producer.get_timebase_den();
    
    
    /*
     *  GLFW / OpenGL Confiuration
     */
    
    glfwInit();
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
    
    glfwSwapInterval(0);
    
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
    
    // load and compile shaders
#ifdef __APPLE__
    Shader shader("/Users/justus/dev/vibes/src/shaders/vertex_shader.vs", "/Users/justus/dev/vibes/src/shaders/fragment_shader.fs");
#endif
    
#ifdef __unix
    Shader shader("../src/shaders/vertex_shader_es.vs", "../src/shaders/fragment_shader_es.fs");
#endif
    
    // change viewPort (renderable area) with window size
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    
    GLfloat vertices[] = {
        // vertices                                                   padding  texture
        1 - VIEWPORT_WIDTH_RATIO,  1 - VIEWPORT_HEIGHT_RATIO,  0,     0,       1, 1,
        1 - VIEWPORT_WIDTH_RATIO, -1 + VIEWPORT_HEIGHT_RATIO,  0,     0,       1, 0,
        -1 + VIEWPORT_WIDTH_RATIO, -1 + VIEWPORT_HEIGHT_RATIO,  0,     0,       0, 0,
        -1 + VIEWPORT_WIDTH_RATIO,  1 - VIEWPORT_HEIGHT_RATIO,  0,     0,       0, 1
    };
    
    GLuint indices[] = {
        0, 1, 3, // first triangle
        1, 2, 3  // second triangle
    };
    
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
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*) 0);
    glEnableVertexAttribArray(0);
    
    // vertex sequence
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)(4 * sizeof(GLfloat)));
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
    
    shader.use();
    
    
    /*
     *  Render Loop Parameters
     */
    
    int current_frame = 0;
    
    bool clear_ghosts = false;
    
    float start_time, end_time;
    
    std::deque<int64_t> pts_buffer;
    
    while (playback_start_time < (uint32_t) time(NULL)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // render loop
    while (!glfwWindowShouldClose(window)) {
        
        static bool initial_frame = true;
        
        if (initial_frame) {
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
        
        std::cout << end_time - start_time << "ms" << std::endl;
        std::cout << 1 / (end_time - start_time) << "FPS" << std::endl << std::endl;
        
        current_frame++;
    }
    
    frame_producer.shutdown_thread();
    
    glfwTerminate();
    
    return 0;
}
