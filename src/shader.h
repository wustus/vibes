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
public:
    // program id
    unsigned int ID;
    
    // constructor reads and builds the shader
    Shader(const char* vertex_path, const char* fragment_path) {
        
        // retreive shader source code from file path
        std::string vertex_code;
        std::string fragment_code;
        std::ifstream v_shader_file;
        std::ifstream f_shader_file;
        
        // ensure ifstraem objects can throw exceptions
        v_shader_file.exceptions (std::ifstream::failbit | std::ifstream::badbit);
        f_shader_file.exceptions (std::ifstream::failbit | std::ifstream::badbit);
        
        try {
            // open files
            v_shader_file.open(vertex_path);
            f_shader_file.open(fragment_path);
            
            // read files buffer content into streams
            std::stringstream v_shader_stream, f_shader_stream;
            v_shader_stream << v_shader_file.rdbuf();
            f_shader_stream << f_shader_file.rdbuf();
            
            //close file handlers
            v_shader_file.close();
            f_shader_file.close();
            
            //convert stream to string
            vertex_code = v_shader_stream.str();
            fragment_code = f_shader_stream.str();
        } catch(std::ifstream::failure e) {
            std::cout << "ERROR::SHADER::FILE_NOT_SUCESSFULLY_READ" << std::endl;
        }
        
        const char* v_shader_code = vertex_code.c_str();
        const char* f_shader_code = fragment_code.c_str();
        
        // compile shaders
        // create vertex shader (program that defines vertex positions)
        unsigned int vertex_shader;
        vertex_shader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex_shader, 1, &v_shader_code, NULL);
        
        // compile vertex shader and output error log
        glCompileShader(vertex_shader);
        
        int success;
        char info_log[512];
        glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
        
        if (!success) {
            glGetShaderInfoLog(vertex_shader, 512, NULL, info_log);
            std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << info_log << std::endl;
        }
        
        // create fragment shader (program the defines the coloring)
        unsigned int fragment_shader;
        fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment_shader, 1, &f_shader_code, NULL);
        
        // compile fragment shader and output error log
        glCompileShader(fragment_shader);
        
        glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
        
        if (!success) {
            glGetShaderInfoLog(fragment_shader, 512, NULL, info_log);
            std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << info_log << std::endl;
        }
        
        // create shader program
        ID = glCreateProgram();
        
        glAttachShader(ID, vertex_shader);
        glAttachShader(ID, fragment_shader);
        glLinkProgram(ID);
        
        // link the compiled shaders and output errors
        glGetProgramiv(ID, GL_LINK_STATUS, &success);
        
        if (!success) {
            glGetProgramInfoLog(ID, 512, NULL, info_log);
            std::cout << "ERROR::PROGRAM::LINKER::LINKING_FAILED\n" << info_log << std::endl;
        }
        
        // delete shaders (after they've been linked)
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
    }
    
    // use / activate shader
    void use() {
        glUseProgram(ID);
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

