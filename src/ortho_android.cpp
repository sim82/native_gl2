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

#include <jni.h>
#include <android/log.h>
#include <android/sensor.h>
#include "android_native_app_glue.h"
#include <stdint.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

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

class egl_context {
    
    
public:
    void check_error( const char *name ) {
        if( name == 0 ) {
            name = "unknown";
        }
        
        EGLint err = eglGetError();
        
        if( err != EGL_SUCCESS ) {
            LOGE( "egl error: %s %x\n", name, err );   
        }
        
    }
    
    
    egl_context( android_app *app ) : initialized_(false) {
        init_display(app);
    }
    ~egl_context() {
        uninit_display();
    }
    
    
    void make_current() {
        
        if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
            LOGI("Unable to eglMakeCurrent");
         
        }
    }
    
    bool initialized() const {
        return initialized_;
    }
 
    
    EGLint get_w() const {
        return w;
    }
    
    EGLint get_h() const {
        return h;
    }
    
    void swap_buffers() {
        
        eglSwapBuffers(display, surface);
        
        check_error( "eglSwapBuffers" );
//         LOGI( "swap: %x\n", err );
        
    }
    
private:
    egl_context() : initialized_(false), init_count_(0) {}
    int init_display( android_app *app ) {
        const EGLint attribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
//             
           
            EGL_NONE
        };
        
        if( app->window == 0 ) {
            LOGE( "window == 0\n" );
        }
        
        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        check_error( "eglGetDisplay" );
        
        eglInitialize(display, 0, 0);
        check_error( "eglInitialize" );
        
        EGLConfig config;
        EGLint num_config;
        EGLint format;
        
        /* Here, the application chooses the configuration it desires. In this
         * sample, we have a very simplified selection process, where we pick
         * the first EGLConfig that matches our criteria */
        eglChooseConfig(display, attribs, &config, 1, &num_config);
        LOGI( "num config: %d\n", num_config );
        /* EGL_NATIVE_VISUAL_ID is an attribute of the EGLConfig that is
         * guaranteed to be accepted by ANativeWindow_setBuffersGeometry().
         * As soon as we picked a EGLConfig, we can safely reconfigure the
         * ANativeWindow buffers to match, using EGL_NATIVE_VISUAL_ID. */
        eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
        
        ANativeWindow_setBuffersGeometry(app->window, 0, 0, format);
        
        surface = eglCreateWindowSurface(display, config, app->window, NULL);
        
        const EGLint ctx_attribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE
        };
        context = eglCreateContext(display, config, NULL, ctx_attribs);
        
        if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
            LOGI("Unable to eglMakeCurrent");
            return -1;
        }
        
        eglQuerySurface(display, surface, EGL_WIDTH, &w);
        eglQuerySurface(display, surface, EGL_HEIGHT, &h);
        
        LOGI( "size: %d %d\n", w, h );
        
        // Initialize GL state.
        
//         glEnable(GL_CULL_FACE);
//         
//         glDisable(GL_DEPTH_TEST);
        
        initialized_ = true;
        
        ++init_count_;
        
        LOGI( "initialization done %d\n", init_count_ );
        
        //LOGI( "huge data: %p\n", huge_data_.data() );
//         std::fill( huge_data_.begin(), huge_data_.end(), 1 );
        
      //  huge_data_.resize( 1024 * 1024 * 50 );
        return 0;
    }
    
    void uninit_display() {
        if( !initialized_ ) {
            return;
        }
        
        initialized_ = false;
//         visible_ = false;
        
        eglMakeCurrent( display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
        eglDestroyContext( display, context );
        eglDestroySurface( display, surface );
        eglTerminate(display);
        
        display = EGL_NO_DISPLAY;
        context = EGL_NO_CONTEXT;
        surface = EGL_NO_SURFACE;
        //huge_data_ = std::vector<char>();
    }
    
   
    
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    EGLint w, h;
    
    bool initialized_;
    
//     std::vector<char> huge_data_;
    int init_count_;
    
};

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

        
class gl_transient_state {
  
    
public:
    gl_transient_state(android_app *app) 
    : context_( app ),
      program_(gVertexShader, gFragmentShader ),
      visible_(false)
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
    
private:
    egl_context context_;
    gl_program program_;
    CL_Mat4f mvp_mat;
    bool visible_;
};


static int32_t engine_handle_input(struct android_app* app, AInputEvent* event) {
    return 0;
}

// bool g_destroyed = false;


static const char *command_to_string( int32_t cmd ) {
    
    switch( cmd ) {
        case APP_CMD_INPUT_CHANGED: return "APP_CMD_INPUT_CHANGED";
        case APP_CMD_INIT_WINDOW: return "APP_CMD_INIT_WINDOW";
        case APP_CMD_TERM_WINDOW: return "APP_CMD_TERM_WINDOW";
        case APP_CMD_WINDOW_RESIZED: return "APP_CMD_WINDOW_RESIZED";
        case APP_CMD_WINDOW_REDRAW_NEEDED: return "APP_CMD_WINDOW_REDRAW_NEEDED";
        case APP_CMD_CONTENT_RECT_CHANGED: return "APP_CMD_CONTENT_RECT_CHANGED";
        case APP_CMD_GAINED_FOCUS: return "APP_CMD_GAINED_FOCUS";
        case APP_CMD_LOST_FOCUS: return "APP_CMD_LOST_FOCUS";
        case APP_CMD_CONFIG_CHANGED: return "APP_CMD_CONFIG_CHANGED";
        case APP_CMD_LOW_MEMORY: return "APP_CMD_LOW_MEMORY";
        case APP_CMD_START: return "APP_CMD_START";
        case APP_CMD_RESUME: return "APP_CMD_RESUME";
        case APP_CMD_SAVE_STATE: return "APP_CMD_SAVE_STATE";
        case APP_CMD_PAUSE: return "APP_CMD_PAUSE";
        case APP_CMD_STOP: return "APP_CMD_STOP";
        case APP_CMD_DESTROY: return "APP_CMD_DESTROY";
        default: return "unknown";
    }
}

struct engine_state {
    float grey;
};


// const static size_t hd_size = 1024 * 1024 * 1;
// class engine {
// public:
//     
//     void create_phys() {
//         // Define the ground body.
//         b2BodyDef groundBodyDef;
//         groundBodyDef.position.Set(0.0f, -15.0f);
//         
//         // Call the body factory which allocates memory for the ground body
//         // from a pool and creates the ground box shape (also from a pool).
//         // The body is also added to the world.
//         b2Body* groundBody = world_.CreateBody(&groundBodyDef);
//         
//         // Define the ground box shape.
//         b2PolygonShape groundBox;
//         
//         // The extents are the half-widths of the box.
//         groundBox.SetAsBox(50.0f, 10.0f);
//         
//         // Add the ground fixture to the ground body.
//         groundBody->CreateFixture(&groundBox, 0.0f);
//         
//         // Define the dynamic body. We set its position and call the body factory.
//         b2BodyDef bodyDef;
//         bodyDef.type = b2_dynamicBody;
//         bodyDef.position.Set(0.0f, 4.0f);
//         body_ = world_.CreateBody(&bodyDef);
//         
//         body_->SetAngularVelocity( 4.0 );
//         
//         // Define another box shape for our dynamic body.
//         b2PolygonShape dynamicBox;
//         dynamicBox.SetAsBox(0.5f, 0.5f);
//         
//         // Define the dynamic body fixture.
//         b2FixtureDef fixtureDef;
//         fixtureDef.shape = &dynamicBox;
//         
//         // Set the box density to be non-zero, so it will be dynamic.
//         fixtureDef.density = 1.0f;
//         
//         // Override the default friction.
//         fixtureDef.friction = 0.3f;
// 
//         // Add the shape to the body.
//         body_->CreateFixture(&fixtureDef);
//     
//     }
//     
//     void add_box() {
//         b2BodyDef bodyDef;
//         bodyDef.type = b2_dynamicBody;
//         bodyDef.position.Set(0.0f, 4.0f);
//         b2Body *body = world_.CreateBody(&bodyDef);
//         
//         body->SetAngularVelocity( 4.0 );
//         
//         // Define another box shape for our dynamic body.
//         b2PolygonShape dynamicBox;
//         dynamicBox.SetAsBox(0.5f, 0.5f);
//         
//         // Define the dynamic body fixture.
//         b2FixtureDef fixtureDef;
//         fixtureDef.shape = &dynamicBox;
//         
//         // Set the box density to be non-zero, so it will be dynamic.
//         fixtureDef.density = 1.0f;
//         
//         // Override the default friction.
//         fixtureDef.friction = 0.3f;
// 
//         // Add the shape to the body.
//         body->CreateFixture(&fixtureDef);
//         
//         bodies_.push_back( body );
//     }
//     engine() : frame_count_(0), grey(0), hue_(0.0), huge_data_(hd_size, 1), world_(b2Vec2(0.0f, -10.0f))
//     {
//         create_phys();
//         test_assets();
//     }
//     engine( const engine_state &state ):  frame_count_(0), huge_data_(hd_size, 1), world_(b2Vec2(0.0f, -10.0f)) {
//      
//         grey = std::min( 1.0f, std::max( 0.0f, state.grey ));   
//         
//         create_phys();
//         test_assets();
//     }
//     void test_assets() {
//         AAsset* a = AAssetManager_open( g_asset_mgr, "raw/test.jpg", AASSET_MODE_RANDOM );
//         assert( a != 0 );
//         
//         off_t len = AAsset_getLength(a);
//         
//         const char *buf = (const char *)AAsset_getBuffer(a);
//         LOGI( "asset: %p %d\n", a, len );
//         
//         for( size_t i = 0; i < len; ++i ) {
//             LOGI( "x: %c\n", buf[i] );
//         }
// //         AAssetDir *dir = AAssetManager_openDir(g_asset_mgr, "assets" );
// //         
// //         while( true ) {
// //             const char *name = AAssetDir_getNextFileName(dir);
// //             
// //             if( name == 0 ) {
// //                 break;
// //             }
// //             
// //             LOGI( "asset: %s\n", name );
// //         }
// 
// 
//     }
// //     void deserialize( const engine_state &state ) {
// //         
// //     }
//     
//     engine_state serialize() {
//         engine_state state;
//         state.grey = grey;
//         
//         return state;
//     }
//     void render( gl_transient_state *gts ) {
//     
//         if( !gts->visible() ) {
//             return;
//         }
//         LOGI( "engine::render\n" );
//         
//         gts->render_pre();
//         
//         
//         LOGI( "engine::render 1\n" );
//         //CL_Mat4f mv_mat = CL_Mat4f::ortho(-5.0, 5.0, -5.0, 5.0, 0, 200);
//         
//         
//         //     LOGI( "mat: %s\n", xtostring(mv_mat).c_str() );
//         
//         glFrontFace( GL_CCW );
//         glCullFace(GL_BACK);
//         glEnable(GL_CULL_FACE);
//         
//         
//         CL_Vec3f rgb_col = hsv_to_rgb( hue_, .7, 1.0 );
//         
//         hue_ += 0.08;
//         while( hue_ > 1.0 ) {
//             hue_ -= 1.0;
//         }
//         
//         grey += 0.01f;
//         if (grey > 1.0f) {
//             grey = 0.0f;
//         }
//         CL_Mat4f mvp_mat;
//         {
//             //         CL_Mat4f mv_mat = CL_Mat4f::ortho(-10, 10, -10, 10, 0.2, 200 );
//             
//             // FIXME: the mv and p names are mixed up!
//             CL_Mat4f mv_mat = CL_Mat4f::perspective( 60, 1.5, 0.2, 500 );
// //             CL_Mat4f p_mat = CL_Mat4f::look_at( 0, 0, grey * 10, 0, 0, 0, 0.0, 1.0, 0.0 );
//             CL_Mat4f p_mat = CL_Mat4f::look_at( 0, 0, 10, 0, 0, 0, 0.0, 1.0, 0.0 );
//             //mvp_mat = mv_mat * p_mat;
//             mvp_mat = CL_Mat4f::identity();
//             
//             CL_Mat4f rot = CL_Mat4f::rotate( CL_Angle::from_degrees(grey * 720.0), 0, 0.0, 1.0 );
//             CL_Mat4f trans = CL_Mat4f::translate( 1.0, 0.0, 0.0 );
//             
//             //CL_Mat4f rot1 = CL_Mat4f::rotate( CL_Angle::from_degrees( grey * 360), 1.0, 0.0, 0.0 );
//             
//             //mvp_mat = trans * rot * p_mat * mv_mat;
//             mvp_mat = p_mat * mv_mat;
//         }
//         
//         
//         //     LOGI( "grey: %f\n", grey );
//         //glClearColor(grey, grey, grey, 1.0f);
//         glClearColor( 0, 0, 0, 1.0f);
//         glClear(GL_COLOR_BUFFER_BIT);
//         checkGlError("glClearColor");
//         
//         //     g_ctx.swap_buffers();
//         //return;
//         
//         glClear( GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
//         checkGlError("glClear");
//         
//        // program_.use();
//         
//         glUniform4f( gts->program()->uniform_handle("color"), rgb_col.r, rgb_col.g, rgb_col.b, 1.0 );
//         checkGlError("glUniform4f" );
//         
//         glUniformMatrix4fv( gts->program()->mvp_handle(), 1, GL_FALSE, mvp_mat.matrix );
//         checkGlError("glUniformMatrix4fv" );
//         GLuint position_handle = gts->program()->a_position_handle();
//         LOGI( "engine::render 2\n" );
//         
// //         glVertexAttribPointer( gts->program()->position_handle(), 2, GL_FLOAT, GL_FALSE, 0, gTriangleVertices);
//         glVertexAttribPointer( position_handle, 2, GL_FLOAT, GL_FALSE, 0, g_box_vertices);
//         checkGlError("glVertexAttribPointer");
//         glEnableVertexAttribArray(position_handle);
//         checkGlError("glEnableVertexAttribArray");
//         glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
//         checkGlError("glDrawArrays");
//         
// #if 1
//         float32 timeStep = 1.0f / 60.0f;
//         int32 velocityIterations = 6;
//         int32 positionIterations = 2;
//         
//         
//         LOGI( "engine::render 3\n" );
//         // Instruct the world to perform a single step of simulation.
//         // It is generally best to keep the time step and iterations fixed.
//         world_.Step(timeStep, velocityIterations, positionIterations);
//         
//         
//         LOGI( "engine::render 4\n" );
//         // Now print the position and angle of the body.
//         b2Vec2 position = body_->GetPosition();
//         float32 angle = body_->GetAngle();
//         
//         
//         CL_Mat4f tr_mat = CL_Mat4f::translate( position.x, position.y, 0.0 );
//         //     LOGI( "render\n" );
//         
//         CL_Mat4f all_mat = CL_Mat4f::rotate( CL_Angle::from_radians( angle), 0, 0.0, 1.0 ) 
//                          * CL_Mat4f::translate( position.x, position.y, 0.0 ) * mvp_mat;
//         if( 1 )
//         {
//             glUniform4f( gts->program()->uniform_handle("color"), rgb_col.r, rgb_col.g, rgb_col.b, 1.0 );
//             checkGlError("glUniform4f" );
//             
//             glUniformMatrix4fv( gts->program()->mvp_handle(), 1, GL_FALSE, all_mat.matrix );
//             checkGlError("glUniformMatrix4fv" );
//             
//             //         glVertexAttribPointer( gts->program()->position_handle(), 2, GL_FLOAT, GL_FALSE, 0, gTriangleVertices);
//             glVertexAttribPointer( position_handle, 2, GL_FLOAT, GL_FALSE, 0, g_box_vertices);
//             checkGlError("glVertexAttribPointer");
//             glEnableVertexAttribArray(position_handle);
//             checkGlError("glEnableVertexAttribArray");
//             glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
//             checkGlError("glDrawArrays");
//         }
//         
//         float hue2 = hue_;
//         while( hue2 > 1.0 ) {
//             hue2 -= 1.0;
//         }
//         
//         LOGI( "engine::render 5\n" );
//         for( size_t i = 0, s = bodies_.size(); i < s; ++i ) {
// //          LOGI( "body\n" );
//             // Now print the position and angle of the body.
//             b2Vec2 position = bodies_[i]->GetPosition();
//             float32 angle = bodies_[i]->GetAngle();
//             
//             
//             CL_Mat4f tr_mat = CL_Mat4f::translate( position.x, position.y, 0.0 );
//             //     LOGI( "render\n" );
//             
//             CL_Mat4f all_mat = CL_Mat4f::rotate( CL_Angle::from_radians( angle), 0, 0.0, 1.0 ) 
//                              * CL_Mat4f::translate( position.x, position.y, 0.0 ) * mvp_mat;
//             
//             {
//                 CL_Vec3f rgb_col = hsv_to_rgb( hue2, .7, 1.0 );   
//                 
//                 hue2 += 0.01;
//                 while( hue2 > 1.0 ) {
//                     hue2 -= 1.0;
//                 }
//                 glUniform4f( gts->program()->uniform_handle("color"), rgb_col.r, rgb_col.g, rgb_col.b, 1.0 );
//                 checkGlError("glUniform4f" );
//                 
//                 glUniformMatrix4fv( gts->program()->mvp_handle(), 1, GL_FALSE, all_mat.matrix );
//                 checkGlError("glUniformMatrix4fv" );
//                 
//                 //         glVertexAttribPointer( gts->program()->position_handle(), 2, GL_FLOAT, GL_FALSE, 0, gTriangleVertices);
//                 glVertexAttribPointer( position_handle, 2, GL_FLOAT, GL_FALSE, 0, g_box_vertices);
//                 checkGlError("glVertexAttribPointer");
//                 glEnableVertexAttribArray(position_handle);
//                 checkGlError("glEnableVertexAttribArray");
//                 glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
//                 checkGlError("glDrawArrays");
//             }   
//         }
//         
// #endif   
//         LOGI( "engine::render 5\n" );
//         
//         gts->render_post();
//     
//         LOGI( "engine::render 6\n" );
//         if( frame_count_ % 60 == 0 ) {
//             add_box();
//         }
//         
//         ++frame_count_;
//     }
//     
// private:
//     int frame_count_;
//     
//     float grey;
//     float hue_;
//     std::vector<char>  huge_data_;
//     
//     
//     
//     b2World world_;
//     b2Body* body_;
//     
//     std::vector<b2Body *> bodies_;
// };
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
        vbob_ts_ = vbo_builder_tristrip( scene_static_ );
        
    }
    
    void clear_emit() {
        light_dynamic_.clear_emit();
    }
    void render_light( const vec3f &pos, const vec3f &color ) {
        light_utils::render_light(light_dynamic_.emit(), scene_static_, pos - base_pos_, color);
    }
    
    void update() {

        rad_core_->set_emit( *light_dynamic_.emit() );

        rad_core_->copy( light_dynamic_.rad() );

        
//         vbob_.update_color( light_dynamic_.rad()->begin(), light_dynamic_.rad()->end());
        vbob_ts_.update_color( light_dynamic_.rad()->begin(), light_dynamic_.rad()->end());

        
    }
    void draw( gl_program &prog ) {
//         vbob_.draw_arrays();
        vbob_ts_.draw_arrays(prog);
    }
private:
    vec3i base_pos_;
    
    scene_static scene_static_;
    
    light_static light_static_;
    light_dynamic light_dynamic_;
    
    std::unique_ptr<rad_core> rad_core_;
    
    
    
//     vbo_builder vbob_;
    vbo_builder_tristrip vbob_ts_;
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
    engine_ortho( engine_state &state ) {
        init();
    }
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
    void render( gl_transient_state *gts ) {
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
       
        //p1_.move();
        p1_.set_rot( roll_, pitch_, yaw_ );
        
        auto mat_mvp = setup_perspective(p1_);
        glUniformMatrix4fv( gts->program()->mvp_handle(), 1, GL_FALSE, mat_mvp.matrix ); check_gl_error;
            
        
        unit_->update();
        unit_->draw(*gts->program());
#endif
        
        gts->render_post();
       
    }
    
    engine_state serialize() {
        return engine_state();
    }
    void drop_transient_gl_state() {
        unit_.reset(nullptr);
    }
private:
    
    std::unique_ptr<render_unit> unit_;
    player p1_;
    vec3f light_weird_;
    float roll_, pitch_, yaw_;
};

std::auto_ptr<gl_transient_state> g_gl_transient_state;
std::auto_ptr<engine_ortho> g_engine(0);


static void engine_handle_cmd(struct android_app* app, int32_t cmd) {
    
    LOGI( ">>>>> command: %s\n", command_to_string(cmd) );
    switch (cmd) {
    case APP_CMD_INIT_WINDOW:
        // The window is being shown, get it ready.
//         g_ctx.init_display(app);
//         setupGraphics();
        
        g_gl_transient_state.reset(new gl_transient_state(app));
        break;
    case APP_CMD_TERM_WINDOW:
        //g_ctx.uninit_display();
        g_gl_transient_state.reset(0);
        
        break;
        
    case APP_CMD_GAINED_FOCUS:
//         LOGI( "focus gained\n" );
        //g_ctx.visible(true);
        assert( g_gl_transient_state.get() != 0 );
        g_gl_transient_state->visible(true);
        break;
        
    case APP_CMD_LOST_FOCUS:
//         LOGI( "focus lost\n" );
        assert( g_gl_transient_state.get() != 0 );
        g_gl_transient_state->visible(false);
        break;
        
    case APP_CMD_DESTROY:
//         LOGI( "destroy: %d\n", g_ctx.initialized() );
//         g_destroyed = true;
        assert( g_gl_transient_state.get() == 0 );
        
        g_engine.reset(0);
        break;
        
    case APP_CMD_SAVE_STATE:
    {
        assert( g_engine.get() != 0 );
        
        
        const size_t es_size = sizeof( engine_state );
        
        app->savedState = malloc( es_size );
        *((engine_state *)app->savedState) = g_engine->serialize();
        app->savedStateSize = es_size;
        
//         app->savedState = malloc( 10 );
//         std::string t( "saved\0" );
//         std::copy( t.begin(), t.end(), (char*)app->savedState );
//         app->savedStateSize = 10;
//         
//         LOGI( "save state: %d\n", g_ctx.initialized() );
        break;
    }   
    case APP_CMD_START:
        LOGI( "engine at start: %p %d\n", (void*)g_engine.get(), app->savedStateSize );
        
        if( g_engine.get() == 0 ) {
            if( app->savedState != 0 ) {
                
                LOGI( "start from saved state: %d\n", app->savedStateSize );
                assert( app->savedStateSize == sizeof( engine_state ) );
                
                
                g_engine.reset( new engine_ortho(*((engine_state *)app->savedState)) );
            } else {
            
                g_engine.reset( new engine_ortho() );
            }
        }
            
        
//         g_destroyed = false;
//         LOGI( "start:\n" );
        break;
        
    case APP_CMD_RESUME:
        
        
        assert( g_engine.get() != 0 );
        
        
        
//         g_destroyed = false;
//         LOGI( "resume:\n" );
        break;
        
//     case APP_CMD_SAVE_ST:
//         LOGI( "save state: %d\n", g_ctx.initialized() );
//         break;
    }
}

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

void android_main(struct android_app* state) {

    
    
    try {
    //     struct engine engine;
// 
//     // Make sure glue isn't stripped.
    app_dummy();
// 
//     memset(&engine, 0, sizeof(engine));
//     state->userData = &engine;
    state->onAppCmd = engine_handle_cmd;
    state->onInputEvent = engine_handle_input;
    
    pan::init_log();
    
    for( size_t i = 0; i < 10; ++i ) {
        pan::lout << "pan::lout test " << i << std::endl;
    }
    ptr_nuller<AAssetManager> pn( &g_asset_mgr, state->activity->assetManager);
    
//     engine.app = state;
// 
//     // Prepare to monitor accelerometer
    auto sensor_manager = ASensorManager_getInstance();
    auto mag_sensor = ASensorManager_getDefaultSensor( sensor_manager,
            ASENSOR_TYPE_MAGNETIC_FIELD);
    auto sensor_event_queue = ASensorManager_createEventQueue( sensor_manager,
            state->looper, LOOPER_ID_USER, NULL, NULL);
    ASensorEventQueue_enableSensor(sensor_event_queue, mag_sensor);

    // loop waiting for stuff to do.
    
    while (true) {
        // Read all pending events.
        int ident;
        int events;
        struct android_poll_source* source;

        // If not animating, we will block forever waiting for events.
        // If animating, we loop until all events are read, then continue
        // to draw the next frame of animation.
        bool blocking = true;
        
        if( g_gl_transient_state.get() != 0 ) {
            blocking = !g_gl_transient_state->visible();
        }
//         blocking = true;
        int poll_timeout = blocking ? -1 : 0;
        
//         LOGI( "blocking: %d\n", poll_timeout );
        
        
        
        while ((ident=ALooper_pollAll( poll_timeout, NULL, &events, (void**)&source)) >= 0 ) {
            
          
            
            // Process this event.
            if (source != NULL) {
                source->process(state, source);
            }
//             LOGI( "destroyed: %d\n", g_destroyed );
            if (state->destroyRequested != 0) {
//                 if( g_ctx.initialized() ) {
//                     g_ctx.uninit_display();
//                     
//                     
//                 }
                
                g_engine->drop_transient_gl_state();
                g_gl_transient_state.reset(0);
                LOGI( "destroy: returning\n" );
                return;
            }
            
            blocking = true;
            if( g_gl_transient_state.get() != 0 ) {
                blocking = !g_gl_transient_state->visible();
            }
            poll_timeout = blocking ? -1 : 0;
        
            LOGI( "timeout: %d\n", poll_timeout );
            
//             // If a sensor has data, process it now.
            if (ident == LOOPER_ID_USER) {
                if ( mag_sensor != nullptr) {
                    ASensorEvent event;
                    while (ASensorEventQueue_getEvents(sensor_event_queue, &event, 1) > 0) {
                        LOGI("accelerometer: x=%f y=%f z=%f", event.magnetic.azimuth, event.magnetic.pitch, event.magnetic.roll );
                    
                        if( g_engine.get() != nullptr ) {
                            g_engine->set_roll_pitch_yaw( event.magnetic.roll, event.magnetic.pitch, event.magnetic.azimuth );
                        }
                        
                    }
                }
            }

//             // Check if we are exiting.
//             if (state->destroyRequested != 0) {
    //                 engine_term_display(&engine);
    //                 return;
    //             }
    
    
        }
        
       
        if( g_gl_transient_state.get() != 0 && g_gl_transient_state->visible() ) {
        
            assert( g_engine.get() != 0 );
            
            
            try {
                g_engine->render( g_gl_transient_state.get() );
            } catch( std::runtime_error x ) {
                LOGI( "caught runtime error: %s\n", x.what() );
                return;
            }
            //LOGI( "initilaized\n" );
           // g_gl_transient_state->renderFrame();
        } else {
                LOGI( "not initilaized\n" );
        }
        
        
        
    }
    
    //     LOGI( "destroyed2: %d\n", g_destroyed );
LOGI( ">>>>>>>>>>>>>>> return\n" );
    } catch( std::runtime_error x ) {
        LOGI( "big catch: %s\n", x.what() );
        return;
    } 

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
