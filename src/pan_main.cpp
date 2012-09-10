/*
 * Portions Copyright (C) 2009 The Android Open Source Project
 * Copyright (C) 2012 Simon A. Berger
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// OpenGL ES 2.0 code
#include "canvas2d.h"
#include <jni.h>
#include <android/log.h>
#include <android/sensor.h>
#include "android_native_app_glue.h"
#include <stdint.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include <Box2D/Box2D.h>

#include <cassert>
#include <cstdio>
#include <memory>
#include <cstdlib>
#include <math.h>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <fstream>
#include <functional>
#include "ClanLib/Core/Math/mat4.h"
#include "ClanLib/Core/Math/vec3.h"
#include "gl_bits.h"
#include "rad_core.h"
#include "pan.h"

// #include "player_bits.h"
#define  LOG_TAG    "libgl2jni"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

AAssetManager *g_asset_mgr = 0;



CL_Vec3f hsv_to_rgb( float h, float s, float v ) {
    float r, g, b;
    int i;
    
 
    // Make sure our arguments stay in-range
    h = std::max(0.0f, std::min(1.0f, h));
    s = std::max(0.0f, std::min(1.0f, s));
    v = std::max(0.0f, std::min(1.0f, v));
 
    
 
    if(s == 0) {
        // Achromatic (grey)
        r = g = b = v;
        return CL_Vec3f(v, v, v);
    }
 
    h *= 6; // sector 0 to 5
    i = floor(h);
    float f = h - i; // factorial part of h
    float p = v * (1 - s);
    float q = v * (1 - s * f);
    float t = v * (1 - s * (1 - f));
 
    switch(i) {
    case 0:
        r = v;
        g = t;
        b = p;
        break;
        
    case 1:
        r = q;
        g = v;
        b = p;
        break;
        
    case 2:
        r = p;
        g = v;
        b = t;
        break;
        
    case 3:
        r = p;
        g = q;
        b = v;
        break;
        
    case 4:
        r = t;
        g = p;
        b = v;
        break;
        
    default: // case 5:
        r = v;
        g = p;
        b = q;
    }
 
    return CL_Vec3f( r, g, b );

}



// egl_context g_ctx;

static void printGLString(const char *name, GLenum s) {
    const char *v = (const char *) glGetString(s);
    LOGI("GL %s = %s\n", name, v);
}

static void checkGlError(const char* op) {
    for (GLint error = glGetError(); error; error
            = glGetError()) {
        LOGI("after %s() glError (0x%x)\n", op, error);
    }
}


// static const char gVertexShader[] = 
//     "uniform mat4 mvp_matrix;\n"
//     "attribute vec4 a_position;\n"
//     "attribute vec4 a_color;\n"
//     "void main() {\n"
//     "  gl_Position = mvp_matrix * a_position;\n"
//     "}\n";
// 
// static const char gFragmentShader[] = 
//     "precision mediump float;\n"
//     "uniform vec4 color;\n"
//     "void main() {\n"
//     "  gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);\n"
//    // "  gl_FragColor = color;\n"
//     "}\n";


static const char gVertexShader[] = 
    "uniform mat4 mvp_matrix;\n"
//     "attribute vec4 xxx;\n"
    "attribute vec4 a_position;\n"
    "attribute vec4 a_color;\n"
    "varying vec4 v_color;\n"
//     "void main() {\n"
//     "  gl_Position = mvp_matrix * gl_Vertex;\n"
//     "  gl_FrontColor = gl_Color;\n"
//     "}\n";

    "void main() {\n"
    "  gl_Position = mvp_matrix * a_position;\n"
    "  v_color = a_color;\n"
    "}\n";
static const char gFragmentShader[] = 
    "precision mediump float;\n"
    "varying vec4 v_color;\n"
    //"uniform vec4 color;\n"
    //"attribute vec4 color;\n"
    "void main() {\n"
//     "  gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);\n"
    "  gl_FragColor = v_color;\n"
    "}\n";


const GLfloat g_box_vertices[] = { -0.5f, 0.5f, 0, -0.5f, -0.5f, 0, 0.5f, 0.5f, 0, 0.5f, -0.5f, 0 };
    
template<typename P>
class compare_first_string {
public:
    bool operator()(const P &a, const P &b ) const {
        return a.first < b.first;
    }
    
//     bool operator()(const std::string &a, const P &b ) const {
//         return a < b.first;
//     }
//     
//     bool operator()(const P &a, const std::string &b ) const {
//         return a.first < b;
//     }
    
    bool operator()( const char *a, const P &b ) const {
        return a < b.first;
    }
    
    bool operator()(const P &a, const char *b ) const {
        return a.first < b;
    }
    
};
    
template<typename t>
std::string xtostring( const t &x ) {
    std::stringstream ss;
    ss << x;
    
    return ss.str();
}


        
class gl_transient_state : public pan::gl_transient_state_int {
  
    
public:
    gl_transient_state(android_app *app, pan::egl_context &context) 
    : context_(context),
      program_(gVertexShader, gFragmentShader ),
      visible_(false),
      c2d_ts_(context)
    {
        
        
        printGLString("Version", GL_VERSION);
        printGLString("Vendor", GL_VENDOR);
        printGLString("Renderer", GL_RENDERER);
        printGLString("Extensions", GL_EXTENSIONS);
        
      
        
        glViewport(0, 0, context_.get_w(), context_.get_h());
        
        
        checkGlError("glViewport");
        LOGI( ">>>>>>>>> engine()\n" );
        
        
    }
    ~gl_transient_state() {
        LOGI( ">>>>>>>>> ~engine()\n" );
    }
    void render_pre() {
        context_.make_current();
        program_.use();
        
    }
    void render_post() {
        context_.swap_buffers();
    }
    
    bool visible() const {
        return visible_;
    }
    
    void visible( bool v ) {
        visible_ = v;
    }
    
    gl_program *program() {
        return &program_;
    }
    
    
    struct transient_res {
        transient_res() : is_valid(false) {}
        
        vbo_builder_tristrip vbob_ts_;
        bool vbob_ts_valid_;
        bool is_valid;
    };
    
    transient_res &tres() {
        return tres_;
    }
    
    canvas2d::gl_ts &c2d_ts() {
        return c2d_ts_;
    }
    
private:
    pan::egl_context &context_;
    gl_program program_;
    CL_Mat4f mvp_mat;
    bool visible_;
    
    transient_res tres_;
    canvas2d::gl_ts c2d_ts_;
};




class light_dynamic {
public:
    light_dynamic() {}
    light_dynamic( size_t num ) : emit_( num ), rad_( num ) {}
    
    void clear_emit() {
        std::fill( emit_.begin(), emit_.end(), vec3f(0.0, 0.0, 0.0));
    }

    std::vector<vec3f> *emit() {
        return &emit_;
    }
    
    std::vector<vec3f> *rad() {
        return &rad_;
    }
    
private:
    std::vector<vec3f> emit_;
    std::vector<vec3f> rad_;
    
    
};

std::string hash_to_filename( uint64_t hash ) {
    std::stringstream ss;
    ss << "baked";
    for( size_t i = 0; i < 8; ++i ) {
        size_t c = hash & 0xff;
        
        
        ss << std::hex << c;
        
        
        
        hash >>= 1;
        
        
    }
    ss << ".bin";
    return ss.str();
}




class render_unit {
public:
    render_unit( std::istream &is, const vec3i &base_pos )
    : base_pos_(base_pos),
    scene_static_(base_pos)
    {
        LOGI( "render_unit start setup\n" );
        assert( is.good() );
//         height_fields_ = crystal_bits::load_crystal(is, pump_factor_);
//         std::cout << "hf: " << height_fields_.size() << "\n";
//         
//         
//         
//         scene_static_.init_solid(height_fields_);
//         
        
        const size_t pump_factor = 4;
        
         base_pos_.x *= pump_factor;
         base_pos_.z *= pump_factor;
        scene_static_.init_solid_from_crystal(is, pump_factor);
        LOGI( "render_unit init solid\n" );

//        scene_static_.init_planes();
        scene_static_.init_strips();
        uint64_t scene_hash = scene_static_.hash();
        auto bin_name = hash_to_filename(scene_hash);
        light_static_ = setup_formfactors(scene_static_.planes(), scene_static_.solid());
//         std::cout << "baked name: " << bin_name << "\n";
//         try {
//             std::ifstream is( bin_name.c_str() );
//             
//             
//             light_static_ = light_static( is, scene_hash );
//         } catch( std::runtime_error x ) {
//             
//             std::cerr << "load failed. recreating. error:\n" << x.what() << std::endl;
//             
//             light_static_ = setup_formfactors(scene_static_.planes(), scene_static_.solid());    
//         }
//         
//         if( !false ) {
//             std::ofstream os( bin_name.c_str() );
//             light_static_.write(os, scene_hash);
//         }
        
        
        light_static_.do_postprocessing();
        LOGI( "render_unit init postprocessing: %d %d\n", scene_static_.planes().size(), light_static_.num_planes());
        
        light_dynamic_ = light_dynamic(scene_static_.planes().size() );
        rad_core_ = make_rad_core_null(scene_static_, light_static_);
        
        
        
//         vbob_ = vbo_builder(scene_static_.planes().size() );
//         vbob_.update_index_buffer( scene_static_.planes().size());
//         vbob_.update_vertices( scene_static_.planes().begin(), scene_static_.planes().end());
//         
//         vbob_ts_ = vbo_builder_tristrip( scene_static_ );
        
    }
    vbo_builder_tristrip init_vbob() {
        return vbo_builder_tristrip( scene_static_ );
    }
    void clear_emit() {
        light_dynamic_.clear_emit();
    }
    void render_light( const vec3f &pos, const vec3f &color ) {
        light_utils::render_light(light_dynamic_.emit(), scene_static_, pos - base_pos_, color);
    }
    
    void update( vbo_builder_tristrip &vbob ) {

        rad_core_->set_emit( *light_dynamic_.emit() );

        rad_core_->copy( light_dynamic_.rad() );

        
//         vbob_.update_color( light_dynamic_.rad()->begin(), light_dynamic_.rad()->end());
        vbob.update_color( light_dynamic_.rad()->begin(), light_dynamic_.rad()->end());

        
    }
//     void draw( gl_program &prog ) {
// //         vbob_.draw_arrays();
//         vbob_ts_.draw_arrays(prog);
//     }
private:
    vec3i base_pos_;
    
    scene_static scene_static_;
    
    light_static light_static_;
    light_dynamic light_dynamic_;
    
    std::unique_ptr<rad_core> rad_core_;
    
    
    
//     vbo_builder vbob_;
//     vbo_builder_tristrip vbob_ts_;
};

class player {
public:
    player() : pos_( 0.0, 0.0, 5.0 ), rot_x_(0), rot_y_(0)
    {}
    
    const vec3f &pos() const {
        return pos_;
    }
    
    float rot_x() const {
        
        return rot_x_;
    }
    float rot_y() const {
        
        return rot_y_;
    }
    
    void set_rot( float roll, float pitch, float yaw ) {
        rot_x_ = (pitch / 1) * 3.1415;
        rot_y_ = (yaw / 1) * 3.1415;
    }
    void move() {
        //rot_x_ += 0.1;
//         rot_y_ += 0.1;
    }
private:
    vec3f pos_;
    float rot_x_;
    float rot_y_;
};



class engine_ortho {
public:
    void init() {
        
        
//         ASensorManager_createEventQueue()
        light_weird_ = vec3f(0,0,0);
        roll_ = pitch_ = yaw_ = 0;
    }
    void set_roll_pitch_yaw( float roll, float pitch, float yaw ) {
        roll_ = roll;
        pitch_ = pitch;
        yaw_ = yaw;
    }
    engine_ortho() {
        init();
    }
//     engine_ortho( engine_state &state ) {
//         init();
//     }
    CL_Mat4f setup_perspective( const player &camera ) {
//         glMatrixMode(GL_PROJECTION);                        //hello

        
        CL_Mat4f proj_p = CL_Mat4f::perspective( 60, 1.5, 0.2, 500 );
            //      CL_Mat4f proj = CL_Mat4f::ortho( -20.0 * pump_factor_, 20.0 * pump_factor_, -15.0 * pump_factor_, 15.0 * pump_factor_, 0, 200 );
            //CL_Mat4f proj = CL_Mat4f::ortho( -40, 40, -30, 30, 0, 200 );


//         glLoadMatrixf( proj_p.matrix );
        

        //          std::cout << "pos: " << player_pos << "\n";

//         glMatrixMode( GL_MODELVIEW );
        
        const vec3f &player_pos = camera.pos();
        CL_Mat4f proj_mv = CL_Mat4f::translate(-player_pos.x, -player_pos.y, -player_pos.z) * CL_Mat4f::rotate(CL_Angle(-camera.rot_x(), cl_degrees),CL_Angle(-camera.rot_y(),cl_degrees),CL_Angle(), cl_XYZ);
//         glLoadMatrixf(proj_mv.matrix);
        
        
        return proj_mv * proj_p;
    }
    void render( pan::gl_transient_state_int &gts_int ) {
        gl_transient_state *gts = dynamic_cast<gl_transient_state *>(&gts_int);
        
        
        if( !gts->visible() ) {
            return;
        }
        
        if( unit_.get() == nullptr ) {
            std::ifstream is( "/sdcard/house1.txt" );
            
            if( !is.good() ) {
                throw std::runtime_error( "cannot open level\n" );
            }
            //std::ifstream is( "cryistal-castle-hidden-ramp.txt" );
            
            unit_ = make_unique<render_unit>(is, vec3f( -40.0, -20.0, -40.0 ));
            LOGI( "render_unit init done\n" );   
        }
        
//         LOGI( "engine::render\n" );
        
        gts->render_pre();
        
        glFrontFace( GL_CCW );
        glCullFace(GL_BACK);
        glEnable(GL_CULL_FACE);
        
        
        glClearColor( 0, 0, 0, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glClear( GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
        
        
//         glBindBuffer(GL_ARRAY_BUFFER, 0);
//         glVertexAttribPointer( gts->program()->a_position_handle(), 3, GL_FLOAT, GL_FALSE, 0, g_box_vertices); check_gl_error;
//         
//         glEnableVertexAttribArray(gts->program()->a_color_handle()); check_gl_error;
//         glEnableVertexAttribArray(gts->program()->a_position_handle()); check_gl_error;
//         checkGlError("glEnableVertexAttribArray");
//         glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); check_gl_error;
        
#if 1
        unit_->clear_emit();

        //vec3f light_weird( 0, 0, 30 );
        light_weird_.x += 0.5;
        if(light_weird_.x > 20 ) {
            light_weird_.x = -20;
        }
        
        unit_->render_light( light_weird_, vec3f(1.0, 0.8, 0.6 ));
       
        p1_.move();
       // p1_.set_rot( roll_, pitch_, yaw_ );
        
        auto mat_mvp = setup_perspective(p1_);
        glUniformMatrix4fv( gts->program()->mvp_handle(), 1, GL_FALSE, mat_mvp.matrix ); check_gl_error;
            
        if( !gts->tres().is_valid ) {
            pan::lout << "(re) init transient gl resources" << std::endl;
            
            gts->tres().vbob_ts_ = unit_->init_vbob();
            gts->tres().is_valid = true;
        }
        
        
               
        unit_->update( gts->tres().vbob_ts_ );
        //unit_->draw(*gts->program());
        
        gts->tres().vbob_ts_.draw_arrays( *gts->program() );
#endif
        
        gts->render_post();
       
    }
/*    
    engine_state serialize() {
        return engine_state();
    }*/
    void drop_transient_gl_state() {
        unit_.reset(nullptr);
    }
private:
    
    std::unique_ptr<render_unit> unit_;
    player p1_;
    vec3f light_weird_;
    float roll_, pitch_, yaw_;
};

class engine_none {
public:
    void render ( pan::gl_transient_state_int &gl_ts_ ) {
        gl_transient_state &gls = dynamic_cast<gl_transient_state &>(gl_ts_);
        assert( &gls != nullptr );
        
        gls.render_pre();
        
        glClearColor( 0, 0, 0, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        c2d_.render(gls.c2d_ts());
        
        gls.render_post();
    }
    
private:
    canvas2d c2d_;
    
};

template<typename Type_>
class ptr_nuller {
public:
    ptr_nuller( Type_ **pptr, Type_* ptr ) : pptr_(pptr) 
    {
        *pptr_ = ptr;
        
    }
    
    ~ptr_nuller() {
        *pptr_ = 0;
    }
    
private:
    Type_ **pptr_;
};

// typedef void (/*SLAPIENTRY*/ *slAndroidSimpleBufferQueueCallback)(
//     SLAndroidSimpleBufferQueueItf caller,
//     void *pContext
// );




typedef std::function<void(float *, float *)> sound_render_func;

class osc_square {
public:
    osc_square() : per_(0), freq_(440) {}
    
    void freq( float f ) {
        freq_ = f;
    }
    
    sound_render_func func() {
        return [&]( float *begin, float *end ) {
            size_t size = std::distance( begin, end );
            
            size_t mono_samples = size;
            
            //float freq = 440.0;
            float sample_rate = 44100;
            
            size_t samples_per_period = sample_rate / freq_;
            
            for( size_t i = 0; i < mono_samples; ++i ) {
                if( per_ % samples_per_period < samples_per_period / 2 ) {
                    *begin++ = 1;
                } else {
                    *begin++ = -1;
                }
                ++per_;
            }
        };
    }
    
public:
    size_t per_;
    float freq_;
};


// class osc_saw {
// public:
//     osc_saw() : per_(0), freq_(440) {}
//     
//     void freq( float f ) {
//         freq_ = f;
//     }
//     
//     sound_render_func func() {
//         return [&]( float *begin, float *end ) {
//             size_t size = std::distance( begin, end );
//             
//             size_t mono_samples = size;
//             
//             //float freq = 440.0;
//             float sample_rate = 44100;
//             
//             size_t samples_per_period = sample_rate / freq_;
//             
//             for( size_t i = 0; i < mono_samples; ++i ) {
//                 *begin++ = 2 * float(per_ % samples_per_period) / samples_per_period - 1.0;
//                 
// //                 if( per_ % samples_per_period < samples_per_period / 2 ) {
// //                     *begin++ = 1;
// //                 } else {
// //                     *begin++ = -1;
// //                 }
//                 ++per_;
//             }
//         };
//     }
//     
// public:
//     size_t per_;
//     float freq_;
// };

class osc_saw {
public:
    osc_saw() : per_(0), freq_(440) {}
    
    void freq( float f ) {
        freq_ = f;
    }
    
    sound_render_func func() {
        return [&]( float *begin, float *end ) {
            size_t size = std::distance( begin, end );
            
            size_t mono_samples = size;
            
            //float freq = 440.0;
            float sample_rate = 44100;
            
            size_t samples_per_period = sample_rate / freq_;
    
            float inc_per_sample_ = 1.0 / samples_per_period;
            
            for( size_t i = 0; i < mono_samples; ++i ) {
                //*begin++ = 2 * float(per_ % samples_per_period) / samples_per_period - 1.0;
                
                *begin++ = 2 * per_ - 1.0;
                
                per_ += inc_per_sample_;
                if( per_ > 1.0 ) {
                    per_ -= 1.0;
                }
                
            }
        };
    }
    
public:
    float per_;
    
    float freq_;
};


// class moog_vcf {
// public:
//     moog_vcf( sound_render_func client_func ) : client_func_(client_func) {
//         
//         // Moog 24 dB/oct resonant lowpass VCF
//         // References: CSound source code, Stilson/Smith CCRMA paper.
//         // Modified by paul.kellett@maxim.abel.co.uk July 2000
// 
//         // Set coefficients given frequency & resonance [0.0...1.0]
// 
//         float frequency = 0.01;
//         float resonance = 2;
//         
//         q = 1.0f - frequency;
//         p = frequency + 0.8f * frequency * q;
//         f = p + p - 1.0f;
//         q = resonance * (1.0f + 0.5f * q * (1.0f - q + 5.6f * q * q));
//         
//         b0 = b1 = b2 = b3 = b4 = 0.0;
//         
//     }
// 
//     sound_render_func func() {
//         return [&](float *begin, float *end ) {
//             
//             client_func_( begin, end );
//             
//             // Filter (in [-1.0...+1.0])
//             
//             while( begin != end ) {
//                 float t1, t2;              //temporary buffers        
//                 
//                 float in = *begin;
//             
//                 
//                 in -= q * b4;                          //feedback
//                 t1 = b1;  b1 = (in + b0) * p - b1 * f;
//                 t2 = b2;  b2 = (b1 + t1) * p - b2 * f;
//                 t1 = b3;  b3 = (b2 + t2) * p - b3 * f;
//                 b4 = (b3 + t1) * p - b4 * f;
//                 b4 = b4 - b4 * b4 * b4 * 0.166667f;    //clipping
//                 b0 = in;
//                 
//                 *begin = b4;
//                 ++begin;
//             }
//             // Lowpass  output:  b4
//             // Highpass output:  in - b4;
//             // Bandpass output:  3.0f * (b3 - b4);  
//         };
//     }
// private:
//     
//     sound_render_func client_func_;
//     
//     float f, p, q;             //filter coefficients
//     float b0, b1, b2, b3, b4;  //filter buffers (beware denormals!)
//     
// };


class moog_vcf2 {
    
public:
    
    moog_vcf2( sound_render_func client_func ) : client_func_(client_func), freq_(1000) {
        std::fill( in.begin(), in.end(), 0 );
        std::fill( out.begin(), out.end(), 0 );
    }
    void freq( float f ) {
        freq_ = f;
    }
    
    sound_render_func func() {
        return [&]( float *begin, float *end ) {
            const float sample_rate = 44100;
            
            
            
            float fc = freq_ / (sample_rate / 2);
//             pan::lout << fc << std::endl;
            fc = std::min( 0.9f, std::max( 0.1f, fc ));
            
            float res = 3;
            
            double f = fc * 1.16;
            double fb = res * (1.0 - 0.15 * f * f);
            
            client_func_(begin, end);
            
            while( begin != end ) {
                float input = *begin;
                
                input -= out[3] * fb;
                input *= 0.35013 * (f*f)*(f*f);
                out[0] = input + 0.3 * in[0] + (1 - f) * out[0]; // Pole 1
                in[0]  = input;
                out[1] = out[0] + 0.3 * in[1] + (1 - f) * out[1];  // Pole 2
                in[1]  = out[0];
                out[2] = out[1] + 0.3 * in[2] + (1 - f) * out[2];  // Pole 3
                in[2]  = out[1];
                out[3] = out[2] + 0.3 * in[3] + (1 - f) * out[3];  // Pole 4
                in[3]  = out[2];
                
                *begin = out[3];
                ++begin;
            }
        };
    }
    
private:
    sound_render_func client_func_;
    
    std::array<float, 4> in; // check if this confuses the compiler too much 
    std::array<float, 4> out;
    
    float freq_;
};

class expand_stereo {
public:
    
    expand_stereo( sound_render_func client_func ) 
    : client_func_(client_func)
    {
    }
        
    
    sound_render_func func() {
        return [&]( float *begin, float *end ) {
            size_t size = std::distance( begin, end );
            size_t mono_samples = size / 2;
            
            if( mono_buf_.size() < mono_samples ) {
                mono_buf_.resize(mono_samples);
            }
            
            client_func_( &(*mono_buf_.begin()), &(*mono_buf_.end()) );
            auto mono_it = mono_buf_.begin();
            
            for( size_t i = 0; i < mono_samples; ++i ) {
                *begin++ = *mono_it;
                *begin++ = *mono_it;
                ++mono_it;
            }
        };
        
    }
    
private:
    sound_render_func client_func_;
    std::vector<float> mono_buf_;
};

class fill_noise : public pan::async_audio_output::fill_buffer_func {
public:
    virtual void operator()(int16_t *begin, int16_t *end ) {
        std::generate( begin, end, rand );
    }
};


class synth_voice {
public:
    synth_voice() : lp_filter_( osc1.func() ), exp_stereo( lp_filter_.func() ) {
        
    }
    
    sound_render_func func() {
//         return [&]( float *begin, float *end ) {
//             exp_stereo
//             
//         };
        
        
        return exp_stereo.func();
    }
    
    osc_saw &osc() {
        return osc1;
    }
    
    moog_vcf2 &lp_filter() {
        return lp_filter_;
    }
    
private:
    osc_saw osc1;
    moog_vcf2 lp_filter_;
    expand_stereo exp_stereo;
    
};


class mixer_stack {
public:
    mixer_stack() {
        
    }
    
    void push( sound_render_func func ) {
        funcs_.push_back(func);
        active_.push_back( false );
    }
    void active( size_t v, bool b ) {
        
//         pan::lout << "active: " << v << " " << b << std::endl;
        active_.at(v) = b;
    }
    sound_render_func func() {
        return [&] ( float *begin, float *end ) {
            size_t s = std::distance( begin, end );
            
            if( s > mix_buf.size() ) {
                mix_buf.resize(s);
            }
            
            std::fill( begin, end, 0 );
            
            for( size_t i = 0; i < funcs_.size(); ++i ) {
//                 pan::lout << "mix: " << i << " " << active_[i] << std::endl;
                if( active_[i] ) { 
                    funcs_[i]( &(*mix_buf.begin()), &(*mix_buf.end()) );
                    
                    std::transform( begin, end, mix_buf.begin(), begin, []( float a, float b ) {return a + (b*0.5);} );
                
                    
                }
                
                
                
            }
            
        };
        
    }
    
private:
    
    std::vector<bool> active_;
    std::vector<sound_render_func> funcs_;
    std::vector<float> mix_buf;
    
};

void android_main(struct android_app* state) {
    pan::init_log();
    
    
    try {
        pan::async_audio_output aao;
        
        fill_noise fn;
//         osc_square osc1;
//         osc_saw osc1;
//         moog_vcf2 lp_filter( osc1.func() );
//         expand_stereo exp_stereo( lp_filter.func() );
//         
        
        
        std::vector<synth_voice> voices( 8 );
        mixer_stack ms;
        
        for( auto &v : voices ) {
            ms.push( v.func() );
        }
        
        
        
//         aao.set_fill_buffer_func( fn );
        aao.set_fill_buffer_float_func( ms.func() );
        aao.start();
    
        app_dummy();
    
        auto gl_ts_fact = [&]( android_app *state, pan::egl_context &ctx ) {
            return new gl_transient_state(state, ctx); 
        };
    
//         engine_ortho engine;
        engine_none engine;
        
        auto render_func = [&](pan::gl_transient_state_int& ts) {
            engine.render(ts);
        };
        
        pan::app_thread at(state, render_func, gl_ts_fact );
        auto down_func = [&](int id, float x, float y) {
            pan::lout << "down" << std::endl;
            ms.active(id, true);
            voices.at(id).osc().freq( 32 + (x / 1024) * 220 );
            voices.at(id).lp_filter().freq( y * 8 );
        
        };
        auto move_func = [&](int id, float x, float y) { 
            voices.at(id).osc().freq( 32 + (x / 1024) * 220 );
            voices.at(id).lp_filter().freq( y * 8 );
//             pan::lout << "move: " << id << " " << x << " " << y << "\n"; 
            
        };
        auto up_func = [&](int id, float x, float y) { 
            ms.active( id, false );
//             pan::lout << "up :" << id << std::endl;
        };
        
        at.set_touch_handler( down_func, move_func, up_func );
        
        at.start();
    
    } catch( std::runtime_error x ) {
        pan::lout << "TERMINATE: caught toplevel std::runtime_error:\n" << x.what() << std::endl;
        return;
    }
    
    
    {
       
        
        
    }
    

    
    return;
    
}

// extern "C" {
//     JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_init(JNIEnv * env, jobject obj,  jint width, jint height);
//     JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_step(JNIEnv * env, jobject obj);
// };
// 
// JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_init(JNIEnv * env, jobject obj,  jint width, jint height)
// {
//     setupGraphics(width, height);
// }
// 
// JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_step(JNIEnv * env, jobject obj)
// {
//     renderFrame();
// }
