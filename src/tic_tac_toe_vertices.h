//
//  tic_tac_toe_vertices.h
//  vibes
//
//  Created by Justus Stahlhut on 06.05.23.
//

#ifndef tic_tac_toe_vertices_h
#define tic_tac_toe_vertices_h

#include <math.h>

class Tic_Tac_Toe_Vertices {
    
    std::vector <float> vertices;
    std::vector <unsigned int> vertex_indices;
    
    int num_segments;
    
public:
    Tic_Tac_Toe_Vertices(int num_segments) {
        
        Tic_Tac_Toe_Vertices::num_segments = num_segments;
        const float outer_radius = 0.33f;
        const float inner_radius = 0.31f;
        const float pi2 = M_PI * 2;
        
        for (int i=0; i<=num_segments; i++) {
            const float angle = pi2 * i / num_segments;
            
            // x y
            vertices.push_back(outer_radius * cos(angle));
            vertices.push_back(outer_radius * sin(angle));
            
            // r g b
            vertices.push_back(1.0f);
            vertices.push_back(1.0f);
            vertices.push_back(1.0f);
            
            // x y
            vertices.push_back(inner_radius * cos(angle));
            vertices.push_back(inner_radius * sin(angle));
            
            vertices.push_back(1.0f);
            vertices.push_back(1.0f);
            vertices.push_back(1.0f);
        }
        
        for (int i=0; i!=num_segments; i++) {
            unsigned int vertex_index = i * 2;
            
            vertex_indices.push_back(vertex_index);
            vertex_indices.push_back(vertex_index + 1);
            vertex_indices.push_back(vertex_index + 3);
            
            vertex_indices.push_back(vertex_index);
            vertex_indices.push_back(vertex_index + 2);
            vertex_indices.push_back(vertex_index + 3);
        }
    }
    
    float* get_tic_tac_toe_player_two(short index) {
        
        std::vector<float> tmp = vertices;
        for (int i=0; i!=tmp.size(); i+=5) {
            tmp[i] = index % 3 == 0 ? -0.66 : (index % 3 == 1 ? 0 : 0.67);
            tmp[i+1] = index < 3 ? 0.66 : (index < 6 ? 0 : -0.67);
        }
        
        float* verts = new float[tmp.size()];
        std::copy(tmp.begin(), tmp.end(), verts);
        
        return verts;
    }
    
    unsigned int* TIC_TAC_TOE_PLAYER_TWO_INDICES;
    
    
    size_t get_tic_tac_toe_field_vertices_size() {
        return 80;
    }
    
    size_t get_tic_tac_toe_field_indices_size() {
        return 24;
    }
    
    
    size_t get_player_one_vertices_size() {
        return 40;
    }
    
    size_t get_player_one_indices_size() {
        return 12;
    }
    
    
    size_t get_player_two_number_vertices() {
        return vertices.size();
    }
    
    size_t get_player_two_indices_size() {
        return vertex_indices.size();
    }

    
    static constexpr float TIC_TAC_TOE_FIELD[] = {
        // left line
        // x             y              r     g     b
        -0.33f + 0.01f,  0.99f        , 1.0f, 1.0f, 1.0f,
        -0.33f + 0.01f, -0.99f        , 1.0f, 1.0f, 1.0f,
        -0.33f - 0.01f, -0.99f        , 1.0f, 1.0f, 1.0f,
        -0.33f - 0.01f,  0.99f        , 1.0f, 1.0f, 1.0f,
        // right line
         0.33f + 0.01f,  0.99f        , 1.0f, 1.0f, 1.0f,
         0.33f + 0.01f, -0.99f        , 1.0f, 1.0f, 1.0f,
         0.33f - 0.01f, -0.99f        , 1.0f, 1.0f, 1.0f,
         0.33f - 0.01f,  0.99f        , 1.0f, 1.0f, 1.0f,
        // top line
         0.99f        ,  0.33f + 0.01f, 1.0f, 1.0f, 1.0f,
        -0.99f        ,  0.33f + 0.01f, 1.0f, 1.0f, 1.0f,
        -0.99f        ,  0.33f - 0.01f, 1.0f, 1.0f, 1.0f,
         0.99f        ,  0.33f - 0.01f, 1.0f, 1.0f, 1.0f,
        // bottom line
         0.99f        , -0.33f + 0.01f, 1.0f, 1.0f, 1.0f,
        -0.99f        , -0.33f + 0.01f, 1.0f, 1.0f, 1.0f,
        -0.99f        , -0.33f - 0.01f, 1.0f, 1.0f, 1.0f,
         0.99f        , -0.33f - 0.01f, 1.0f, 1.0f, 1.0f
    };

    static constexpr unsigned int TIC_TAC_TOE_FIELD_INDICES[] = {
        // left line
        0, 1, 3,
        1, 2, 3,
        // right line
        4, 5, 7,
        5, 6, 7,
        // top line
        8, 9, 11,
        9, 10, 11,
        // bottom line
        12, 13, 15,
        13, 14, 15
    };


    float* get_tic_tac_toe_player_one(short index) {
        
        float x_offset, y_offset = 0;
        
        x_offset = index % 3 == 0 ? -0.66 : (index % 3 == 1 ? 0 : 0.67);
        y_offset = index < 3 ? 0.66 : (index < 6 ? 0 : -0.67);
        
        float* arr = new float[] {
             // x                       y                         r     g     b
             // first line
             0.33f - 0.01f + x_offset,  0.33f         + y_offset, 1.0f, 1.0f, 1.0f,
             0.33f         + x_offset,  0.33f - 0.01f + y_offset, 1.0f, 1.0f, 1.0f,
            -0.33f + 0.01f + x_offset, -0.33f         + y_offset, 1.0f, 1.0f, 1.0f,
            -0.33f         + x_offset, -0.33f + 0.01f + y_offset, 1.0f, 1.0f, 1.0f,
             // second line
            -0.33f         + x_offset,  0.33f - 0.01f + y_offset, 1.0f, 1.0f, 1.0f,
            -0.33f + 0.01f + x_offset,  0.33f         + y_offset, 1.0f, 1.0f, 1.0f,
             0.33f         + x_offset, -0.33f + 0.01f + y_offset, 1.0f, 1.0f, 1.0f,
             0.33f - 0.01f + x_offset, -0.33f         + y_offset, 1.0f, 1.0f, 1.0f
        };
        
        return arr;
    };

    static constexpr unsigned int TIC_TAC_TOE_PLAYER_ONE_INDICES[] = {
        // first line
        0, 1, 3,
        1, 2, 3,
        // second line
        4, 5, 7,
        5, 6, 7
    };
};



#endif /* tic_tac_toe_vertices_h */
