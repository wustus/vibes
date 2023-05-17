//
//  shader.h
//  opengl-foolery
//
//  Created by Justus Stahlhut on 15.01.23.
//

#ifndef shader_h
#define shader_h

#include <glad/glad.h>

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

class Shader {
private:
    unsigned int rgb_program_id;
    unsigned int texture_program_id;
    
    // current used program
    unsigned int ID;
    
    unsigned int create_and_compile_shader(unsigned int shader_type, const char* shader_code) {
        
        unsigned int shader;
        shader = glCreateShader(shader_type);
        glShaderSource(shader, 1, &shader_code, NULL);
        
        // compile vertex shader and output error log
        glCompileShader(shader);
        
        int success;
        char info_log[512];
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        
        if (!success) {
            glGetShaderInfoLog(shader, 512, NULL, info_log);
            
            if (shader_type == GL_VERTEX_SHADER) {
                std::cerr << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << info_log << std::endl;
                return -1;
            }
            
            std::cerr << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << info_log << std::endl;
            return -1;
        }
        
        return shader;
    }
    
    void link_program(unsigned int program_id) {
        
        int success;
        char info_log[512];
        
        glLinkProgram(program_id);
        
        // link the compiled shaders and output errors
        glGetProgramiv(program_id, GL_LINK_STATUS, &success);
        
        if (!success) {
            glGetProgramInfoLog(program_id, 512, NULL, info_log);
            std::cout << "ERROR::PROGRAM::LINKER::LINKING_FAILED\n" << info_log << std::endl;
        }
    }
    
    
public:
    // constructor reads and builds the shader
    Shader(const char* rgb_vertex_path, const char* rgb_fragment_path, const char* texture_vertex_path, const char* texture_fragment_path) {
        
        // retreive shader source code from file path
        std::string rgb_vertex_code, texture_vertex_code, rgb_fragment_code, tex_fragment_code;
        std::ifstream rgb_v_shader_file, tex_v_shader_file, tex_f_shader_file, rgb_f_shader_file;
        
        // ensure ifstraem objects can throw exceptions
        rgb_v_shader_file.exceptions (std::ifstream::failbit | std::ifstream::badbit);
        tex_v_shader_file.exceptions (std::ifstream::failbit | std::ifstream::badbit);
        rgb_f_shader_file.exceptions (std::ifstream::failbit | std::ifstream::badbit);
        tex_f_shader_file.exceptions (std::ifstream::failbit | std::ifstream::badbit);
        
        try {
            // open files
            rgb_v_shader_file.open(rgb_vertex_path);
            tex_v_shader_file.open(texture_vertex_path);
            rgb_f_shader_file.open(rgb_fragment_path);
            tex_f_shader_file.open(texture_fragment_path);
            
            // read files buffer content into streams
            std::stringstream rgb_v_shader_stream, tex_v_shader_stream, rgb_f_shader_stream, tex_f_shader_stream;
            rgb_v_shader_stream << rgb_v_shader_file.rdbuf();
            tex_v_shader_stream << tex_v_shader_file.rdbuf();
            rgb_f_shader_stream << rgb_f_shader_file.rdbuf();
            tex_f_shader_stream << tex_f_shader_file.rdbuf();
            
            //close file handlers
            rgb_v_shader_file.close();
            tex_v_shader_file.close();
            rgb_f_shader_file.close();
            tex_f_shader_file.close();
            
            //convert stream to string
            rgb_vertex_code = rgb_v_shader_stream.str();
            texture_vertex_code = tex_v_shader_stream.str();
            rgb_fragment_code = rgb_f_shader_stream.str();
            tex_fragment_code = tex_f_shader_stream.str();
        } catch(std::ifstream::failure e) {
            std::cout << "ERROR::SHADER::FILE_NOT_SUCESSFULLY_READ" << std::endl;
        }
        
        const char* rgb_v_shader_code = rgb_vertex_code.c_str();
        const char* tex_v_shader_code = texture_vertex_code.c_str();
        const char* rgb_f_shader_code = rgb_fragment_code.c_str();
        const char* tex_f_shader_code = tex_fragment_code.c_str();
        
        // compile shaders
        unsigned int rgb_vertex_shader = create_and_compile_shader(GL_VERTEX_SHADER, rgb_v_shader_code);
        unsigned int tex_vertex_shader = create_and_compile_shader(GL_VERTEX_SHADER, tex_v_shader_code);
        unsigned int rgb_fragment_shader = create_and_compile_shader(GL_FRAGMENT_SHADER, rgb_f_shader_code);
        unsigned int tex_fragment_shader = create_and_compile_shader(GL_FRAGMENT_SHADER, tex_f_shader_code);
        
        
        // create shader programs
        rgb_program_id = glCreateProgram();
        texture_program_id = glCreateProgram();
        
        // link the compiled shaders and output errors
        glAttachShader(rgb_program_id, rgb_vertex_shader);
        glAttachShader(rgb_program_id, rgb_fragment_shader);
        
        link_program(rgb_program_id);
        
        glAttachShader(texture_program_id, tex_vertex_shader);
        glAttachShader(texture_program_id, tex_fragment_shader);
        
        link_program(texture_program_id);
        
        // delete shaders (after they've been linked)
        glDeleteShader(rgb_vertex_shader);
        glDeleteShader(tex_vertex_shader);
        glDeleteShader(rgb_fragment_shader);
        glDeleteShader(tex_fragment_shader);
    }
    
    // use / activate shader
    void use_rgb_shader() {
        glUseProgram(rgb_program_id);
        ID = rgb_program_id;
    }
    
    void use_texture_shader() {
        glUseProgram(texture_program_id);
        ID = texture_program_id;
    }
    
    // utitlity uniform functions
    void set_bool(const std::string &name, bool value)  const {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), (int) value);
    }
    
    void set_int(const std::string &name, int value) const {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
    }
    
    void set_float(const std::string &name, float value) const {
        glUniform1f(glGetUniformLocation(ID, name.c_str()), (float) value);
    }
};

#endif /* shader_h */

