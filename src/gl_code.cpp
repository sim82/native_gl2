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
#include "android_native_app_glue.h"

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

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
#include "ClanLib/Core/Math/mat4.h"
#include "ClanLib/Core/Math/vec3.h"
// #include "gl_bits.h"

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

static const char gVertexShader[] = 
    "uniform mat4 mvp_matrix;\n"
    "attribute vec4 vPosition;\n"
    "void main() {\n"
    "  gl_Position = mvp_matrix * vPosition;\n"
    "}\n";

static const char gFragmentShader[] = 
    "precision mediump float;\n"
    "uniform vec4 color;\n"
    "void main() {\n"
    //"  gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);\n"
    "  gl_FragColor = color;\n"
    "}\n";



class gl_program {
public:
    gl_program() : program(0) {
        
    }
    
    gl_program( const char *vertex_src, const char *fragment_source ) {
        GLuint vertexShader = loadShader(GL_VERTEX_SHADER, vertex_src);
        if (!vertexShader) {
            throw std::runtime_error( "load vertex shader failed.\n" );
        }

        GLuint pixelShader = loadShader(GL_FRAGMENT_SHADER, fragment_source);
        if (!pixelShader) {
          throw std::runtime_error( "load fragment shader failed.\n" );
        }
        
        program = glCreateProgram();
        if (program) {
            glAttachShader(program, vertexShader);
            checkGlError("glAttachShader");
            glAttachShader(program, pixelShader);
            checkGlError("glAttachShader");
            glLinkProgram(program);
            GLint linkStatus = GL_FALSE;
            glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
            if (linkStatus != GL_TRUE) {
                GLint bufLength = 0;
                glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
                if (bufLength) {
                    
                    std::vector<char> buf( bufLength );
//                     char* buf = (char*) malloc(bufLength);
                    
                    glGetProgramInfoLog(program, bufLength, NULL, buf.data());
                    LOGE("Could not link program:\n%s\n", buf.data());
                        
                    
                }
            
            }
            
            gvPositionHandle = glGetAttribLocation(program, "vPosition");
            checkGlError("glGetAttribLocation");
            LOGI("glGetAttribLocation(\"vPosition\") = %d\n",
                 gvPositionHandle);
            
            gv_mvp_handle = glGetUniformLocation(program, "mvp_matrix");
            LOGI("glGetAttribLocation(\"mvp_matrix\") = %d\n", gv_mvp_handle);
            
            color_handle_ = glGetUniformLocation(program, "color");
            LOGI("glGetAttribLocation(\"color\") = %d\n", color_handle_);
            
            
        } else {
            throw std::runtime_error( "glCreateProgram failed" );
        }

    }
    ~gl_program() {
        // TODO: teardown gl resources
        glDeleteProgram(program);
        program = 0;
    }
    void use() {
        glUseProgram(program);
        checkGlError("glUseProgram");
    }
    
    GLuint mvp_handle() {
        return gv_mvp_handle;
    }
    GLuint position_handle() {
        return gvPositionHandle;
    }
    
    
    GLuint color_handle() {
        return color_handle_;
    }
private:
    GLuint loadShader(GLenum shaderType, const char* pSource) {
        GLuint shader = glCreateShader(shaderType);
        if (shader) {
            glShaderSource(shader, 1, &pSource, NULL);
            glCompileShader(shader);
            GLint compiled = 0;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
            if (!compiled) {
                GLint infoLen = 0;
                glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
                if (infoLen) {
                    char* buf = (char*) malloc(infoLen);
                    if (buf) {
                        glGetShaderInfoLog(shader, infoLen, NULL, buf);
                        LOGE("Could not compile shader %d:\n%s\n",
                             shaderType, buf);
                        free(buf);
                    }
                    glDeleteShader(shader);
                    shader = 0;
                }
            }
        }
        return shader;
    }
    
    
    GLuint program;
    GLuint gvPositionHandle;
    GLuint gv_mvp_handle;
    GLuint color_handle_;
};

// GLuint gProgram;
// GLuint gvPositionHandle;
// GLuint gv_mvp_handle;

template<typename t>
std::string xtostring( const t &x ) {
    std::stringstream ss;
    ss << x;
    
    return ss.str();
}

const GLfloat gTriangleVertices[] = { 0.0f, 0.5f, -0.5f, -0.5f,
        0.5f, -0.5f };

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


const static size_t hd_size = 1024 * 1024 * 1;
class engine {
public:
    
    
    engine() : grey(0), hue_(0.0), huge_data_(hd_size, 1) {
        test_assets();
    }
    engine( const engine_state &state ):  huge_data_(hd_size, 1) {
     
        grey = std::min( 1.0f, std::max( 0.0f, state.grey ));   
        
        test_assets();
    }
    void test_assets() {
        AAsset* a = AAssetManager_open( g_asset_mgr, "raw/test.jpg", AASSET_MODE_RANDOM );
        assert( a != 0 );
        
        off_t len = AAsset_getLength(a);
        
        const char *buf = (const char *)AAsset_getBuffer(a);
        LOGI( "asset: %p %d\n", a, len );
        
        for( size_t i = 0; i < len; ++i ) {
            LOGI( "x: %c\n", buf[i] );
        }
//         AAssetDir *dir = AAssetManager_openDir(g_asset_mgr, "assets" );
//         
//         while( true ) {
//             const char *name = AAssetDir_getNextFileName(dir);
//             
//             if( name == 0 ) {
//                 break;
//             }
//             
//             LOGI( "asset: %s\n", name );
//         }


    }
//     void deserialize( const engine_state &state ) {
//         
//     }
    
    engine_state serialize() {
        engine_state state;
        state.grey = grey;
        
        return state;
    }
    void render( gl_transient_state *gts ) {
    
        if( !gts->visible() ) {
            return;
        }
        gts->render_pre();
        
        //CL_Mat4f mv_mat = CL_Mat4f::ortho(-5.0, 5.0, -5.0, 5.0, 0, 200);
        
        
        //     LOGI( "mat: %s\n", xtostring(mv_mat).c_str() );
        
        glFrontFace( GL_CCW );
        glCullFace(GL_BACK);
        glEnable(GL_CULL_FACE);
        
        
        CL_Vec3f rgb_col = hsv_to_rgb( hue_, .7, 1.0 );
        
        hue_ += 0.08;
        while( hue_ > 1.0 ) {
            hue_ -= 1.0;
        }
        
        grey += 0.01f;
        if (grey > 1.0f) {
            grey = 0.0f;
        }
        CL_Mat4f mvp_mat;
        {
            //         CL_Mat4f mv_mat = CL_Mat4f::ortho(-10, 10, -10, 10, 0.2, 200 );
            CL_Mat4f mv_mat = CL_Mat4f::perspective( 60, 1.5, 0.2, 500 );
            CL_Mat4f p_mat = CL_Mat4f::look_at( 0, 0, grey * 10, 0, 0, 0, 0.0, 1.0, 0.0 );
//             CL_Mat4f p_mat = CL_Mat4f::look_at( 0, 0, 2, 0, 0, 0, 0.0, 1.0, 0.0 );
            //mvp_mat = mv_mat * p_mat;
            mvp_mat = CL_Mat4f::identity();
            
            CL_Mat4f rot = CL_Mat4f::rotate( CL_Angle::from_degrees(grey * 720.0), 0, 0.0, 1.0 );
            
            mvp_mat = rot * p_mat * mv_mat;
        }
        //     LOGI( "grey: %f\n", grey );
        //glClearColor(grey, grey, grey, 1.0f);
        glClearColor( 0, 0, 0, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        checkGlError("glClearColor");
        //     g_ctx.swap_buffers();
        //return;
        
        glClear( GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
        checkGlError("glClear");
        
       // program_.use();
        
        glUniform4f( gts->program()->color_handle(), rgb_col.r, rgb_col.g, rgb_col.b, 1.0 );
        
        glUniformMatrix4fv( gts->program()->mvp_handle(), 1, GL_FALSE, mvp_mat.matrix );
        checkGlError("glUniformMatrix4fv" );
        
        glVertexAttribPointer( gts->program()->position_handle(), 2, GL_FLOAT, GL_FALSE, 0, gTriangleVertices);
        checkGlError("glVertexAttribPointer");
        glEnableVertexAttribArray(gts->program()->position_handle());
        checkGlError("glEnableVertexAttribArray");
        glDrawArrays(GL_TRIANGLES, 0, 3);
        checkGlError("glDrawArrays");
        
        //     LOGI( "render\n" );
        
        gts->render_post();
    
    }
    
private:
    float grey;
    float hue_;
    std::vector<char>  huge_data_;
};

std::auto_ptr<gl_transient_state> g_gl_transient_state;
std::auto_ptr<engine> g_engine(0);


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
                
                
                g_engine.reset( new engine(*((engine_state *)app->savedState)) );
            } else {
            
                g_engine.reset( new engine() );
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
//     struct engine engine;
// 
//     // Make sure glue isn't stripped.
    app_dummy();
// 
//     memset(&engine, 0, sizeof(engine));
//     state->userData = &engine;
    state->onAppCmd = engine_handle_cmd;
    state->onInputEvent = engine_handle_input;
    
    ptr_nuller<AAssetManager> pn( &g_asset_mgr, state->activity->assetManager);
    
//     engine.app = state;
// 
//     // Prepare to monitor accelerometer
//     engine.sensorManager = ASensorManager_getInstance();
//     engine.accelerometerSensor = ASensorManager_getDefaultSensor(engine.sensorManager,
//             ASENSOR_TYPE_ACCELEROMETER);
//     engine.sensorEventQueue = ASensorManager_createEventQueue(engine.sensorManager,
//             state->looper, LOOPER_ID_USER, NULL, NULL);
// 
//     if (state->savedState != NULL) {
//         // We are starting with a previous saved state; restore from it.
//         engine.state = *(struct saved_state*)state->savedState;
//     }

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
//             if (ident == LOOPER_ID_USER) {
    //                 if (engine.accelerometerSensor != NULL) {
        //                     ASensorEvent event;
        //                     while (ASensorEventQueue_getEvents(engine.sensorEventQueue,
        //                             &event, 1) > 0) {
            //                         LOGI("accelerometer: x=%f y=%f z=%f",
            //                                 event.acceleration.x, event.acceleration.y,
//                                 event.acceleration.z);
//                     }
//                 }
//             }

//             // Check if we are exiting.
//             if (state->destroyRequested != 0) {
    //                 engine_term_display(&engine);
    //                 return;
    //             }
    
    
        }
        
       
        if( g_gl_transient_state.get() != 0 && g_gl_transient_state->visible() ) {
        
            assert( g_engine.get() != 0 );
            
            g_engine->render( g_gl_transient_state.get() );
            //LOGI( "initilaized\n" );
           // g_gl_transient_state->renderFrame();
        } else {
                LOGI( "not initilaized\n" );
        }
        
    }
    
//     LOGI( "destroyed2: %d\n", g_destroyed );
    LOGI( "return\n" );
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
