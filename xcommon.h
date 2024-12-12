#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include "glext.h"
#include "wglext.h"

__declspec(dllexport) DWORD NvOptimusEnablement = 1;

#define NL "\n"

typedef struct {
    const char* name;
    uint32_t viewport_width;
    uint32_t viewport_height;
    int32_t (*init)(void);
    void (*shutdown)(void);
    int32_t (*update)(float);
    int32_t swap_interval;
} xgl_RunInput;

static PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;

static PFNGLCLEARBUFFERFVPROC glClearBufferfv;
static PFNGLMATRIXLOADIDENTITYEXTPROC glMatrixLoadIdentityEXT;
static PFNGLMATRIXORTHOEXTPROC glMatrixOrthoEXT;
static PFNGLCREATETEXTURESPROC glCreateTextures;
static PFNGLGETSTRINGIPROC glGetStringi;
static PFNGLTEXTURESTORAGE2DMULTISAMPLEPROC glTextureStorage2DMultisample;
static PFNGLCREATEFRAMEBUFFERSPROC glCreateFramebuffers;
static PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;
static PFNGLNAMEDFRAMEBUFFERTEXTUREPROC glNamedFramebufferTexture;
static PFNGLCLEARNAMEDFRAMEBUFFERFVPROC glClearNamedFramebufferfv;
static PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
static PFNGLBLITNAMEDFRAMEBUFFERPROC glBlitNamedFramebuffer;
static PFNGLBLENDEQUATIONPROC glBlendEquation;
static PFNGLCLEARTEXIMAGEPROC glClearTexImage;
static PFNGLGETTEXTUREHANDLENVPROC glGetTextureHandleNV;
static PFNGLMAKETEXTUREHANDLERESIDENTNVPROC glMakeTextureHandleResidentNV;
static PFNGLCREATESHADERPROGRAMVPROC glCreateShaderProgramv;
static PFNGLPROGRAMUNIFORMHANDLEUI64NVPROC glProgramUniformHandleui64NV;
static PFNGLUSEPROGRAMPROC glUseProgram;

static LRESULT CALLBACK xc__process_window_message(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_KEYDOWN:
            if (wparam == VK_ESCAPE) {
                PostQuitMessage(0);
                return 0;
            }
            break;
    }
    return DefWindowProc(hwnd, message, wparam, lparam);
}

static double xc_get_time(void) {
    static LARGE_INTEGER start_counter;
    static LARGE_INTEGER frequency;
    if (start_counter.QuadPart == 0) {
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&start_counter);
    }
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (double)(counter.QuadPart - start_counter.QuadPart) / (double)frequency.QuadPart;
}

static float xc__update_frame_stats(HWND window, const char* name) {
    static double previous_time = -1.0;
    static double header_refresh_time = 0.0;
    static uint32_t num_frames = 0;

    if (previous_time < 0.0) {
        previous_time = xc_get_time();
        header_refresh_time = previous_time;
    }

    double time = xc_get_time();
    float delta_time = (float)(time - previous_time);
    previous_time = time;

    if ((time - header_refresh_time) >= 1.0) {
        double fps = num_frames / (time - header_refresh_time);
        double ms = (1.0 / fps) * 1000.0;
        char header[128];
        snprintf(header, sizeof(header), "[%.1f fps  %.3f ms] %s", fps, ms, name);
        SetWindowText(window, header);
        header_refresh_time = time;
        num_frames = 0;
    }
    num_frames++;
    return delta_time;
}

#define XGL_NUM_MSAA_SAMPLES 8
static GLuint xgl_msaa_fbo;

static int32_t xgl_run(const xgl_RunInput* in) {
    assert(in);
    SetProcessDPIAware();

    RegisterClass(&(WNDCLASSA){
        .lpfnWndProc = xc__process_window_message,
        .hInstance = GetModuleHandle(NULL),
        .hCursor = LoadCursor(NULL, IDC_ARROW),
        .lpszClassName = in->name,
    });

    DWORD style = WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX;

    RECT rect = { .left = 0, .top = 0, .right = in->viewport_width, .bottom = in->viewport_height };
    AdjustWindowRect(&rect, style, FALSE);

    HWND hwnd = CreateWindowEx(0, in->name, in->name, style + WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, NULL, NULL);

    HDC hdc = GetDC(hwnd);

    PIXELFORMATDESCRIPTOR pfd = {
        .nSize = sizeof(PIXELFORMATDESCRIPTOR),
        .nVersion = 1,
        .dwFlags = PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_DRAW_TO_WINDOW,
        .iPixelType = PFD_TYPE_RGBA,
        .cColorBits = 24,
    };
    int32_t pixel_format = ChoosePixelFormat(hdc, &pfd);
    SetPixelFormat(hdc, pixel_format, &pfd);

    HGLRC ogl_context = wglCreateContext(hdc);
    wglMakeCurrent(hdc, ogl_context);

#define GET_FUNC(type, name) \
    name = (type)wglGetProcAddress(#name); \
    assert(name)

    GET_FUNC(PFNWGLSWAPINTERVALEXTPROC, wglSwapIntervalEXT);

    GET_FUNC(PFNGLCLEARBUFFERFVPROC, glClearBufferfv);
    GET_FUNC(PFNGLMATRIXLOADIDENTITYEXTPROC, glMatrixLoadIdentityEXT);
    GET_FUNC(PFNGLMATRIXORTHOEXTPROC, glMatrixOrthoEXT);
    GET_FUNC(PFNGLCREATETEXTURESPROC, glCreateTextures);
    GET_FUNC(PFNGLGETSTRINGIPROC, glGetStringi);
    GET_FUNC(PFNGLTEXTURESTORAGE2DMULTISAMPLEPROC, glTextureStorage2DMultisample);
    GET_FUNC(PFNGLCREATEFRAMEBUFFERSPROC, glCreateFramebuffers);
    GET_FUNC(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers);
    GET_FUNC(PFNGLNAMEDFRAMEBUFFERTEXTUREPROC, glNamedFramebufferTexture);
    GET_FUNC(PFNGLCLEARNAMEDFRAMEBUFFERFVPROC, glClearNamedFramebufferfv);
    GET_FUNC(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer);
    GET_FUNC(PFNGLBLITNAMEDFRAMEBUFFERPROC, glBlitNamedFramebuffer);
    GET_FUNC(PFNGLBLENDEQUATIONPROC, glBlendEquation);
    GET_FUNC(PFNGLCLEARTEXIMAGEPROC, glClearTexImage);
    GET_FUNC(PFNGLGETTEXTUREHANDLENVPROC, glGetTextureHandleNV);
    GET_FUNC(PFNGLMAKETEXTUREHANDLERESIDENTNVPROC, glMakeTextureHandleResidentNV);
    GET_FUNC(PFNGLCREATESHADERPROGRAMVPROC, glCreateShaderProgramv);
    GET_FUNC(PFNGLPROGRAMUNIFORMHANDLEUI64NVPROC, glProgramUniformHandleui64NV);
    GET_FUNC(PFNGLUSEPROGRAMPROC, glUseProgram);

#undef GET_FUNC

    printf("GL version: %s\n", (const char*)glGetString(GL_VERSION));
    printf("GL renderer: %s\n", (const char*)glGetString(GL_RENDERER));
    printf("GL vendor: %s\n", (const char*)glGetString(GL_VENDOR));
    printf("GLU version: %s\n", (const char*)gluGetString(GLU_VERSION));

    {
        bool has_path_rendering = false;
        bool has_mesh_shader = false;

        int32_t num_extensions = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);

        for (int32_t i = 0; i < num_extensions; ++i) {
            const char* ext = (const char*)glGetStringi(GL_EXTENSIONS, i);
            if (strcmp(ext, "GL_NV_path_rendering") == 0) has_path_rendering = true;
            if (strcmp(ext, "GL_NV_mesh_shader") == 0) has_mesh_shader = true;
        }

        if (has_path_rendering == false || has_mesh_shader == false) {
            MessageBox(hwnd, "Sorry but this application requires modern NVIDIA GPU with latest graphics drivers.", "Unsupported GPU", MB_OK | MB_ICONSTOP);
            ExitProcess(0);
        }
    }

    wglSwapIntervalEXT(in->swap_interval);

    glMatrixLoadIdentityEXT(GL_PROJECTION);
    glMatrixOrthoEXT(GL_PROJECTION, -(float)in->viewport_width * 0.5f, in->viewport_width * 0.5f, -(float)in->viewport_height * 0.5f, in->viewport_height * 0.5f, -1.0f, 1.0f);

    glEnable(GL_FRAMEBUFFER_SRGB);
    glEnable(GL_MULTISAMPLE);

    GLuint msaa_tex = 0;
    glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &msaa_tex);
    glTextureStorage2DMultisample(msaa_tex, XGL_NUM_MSAA_SAMPLES, GL_SRGB8_ALPHA8, in->viewport_width, in->viewport_height, GL_FALSE);

    glCreateFramebuffers(1, &xgl_msaa_fbo);
    glNamedFramebufferTexture(xgl_msaa_fbo, GL_COLOR_ATTACHMENT0, msaa_tex, 0);
    glClearNamedFramebufferfv(xgl_msaa_fbo, GL_COLOR, 0, (float[]){ 0, 0, 0, 0 });

    glBindFramebuffer(GL_FRAMEBUFFER, xgl_msaa_fbo);

    int retcode = 0;
    if (in->init) {
        retcode = in->init();
        if (retcode != 0) goto exit;
    }

    for (;;) {
        MSG msg = {0};
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) break;
        } else {
            float dt = xc__update_frame_stats(hwnd, in->name);

            glBindFramebuffer(GL_FRAMEBUFFER, xgl_msaa_fbo);
            if (in->update) retcode = in->update(dt);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            glBlitNamedFramebuffer(xgl_msaa_fbo, 0, 0, 0, in->viewport_width, in->viewport_height, 0, 0, in->viewport_width, in->viewport_height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

            SwapBuffers(hdc);

            if (glGetError() != GL_NO_ERROR) {
                MessageBox(hwnd, "OpenGL error detected.", "Error", MB_OK | MB_ICONERROR);
                if (retcode == 0) retcode = 1;
            }

            if (retcode != 0) break;
        }
    }

exit:
    if (in->shutdown) in->shutdown();

    glDeleteFramebuffers(1, &xgl_msaa_fbo);
    glDeleteTextures(1, &msaa_tex);

    wglMakeCurrent(hdc, NULL);
    if (ogl_context) wglDeleteContext(ogl_context);
    ReleaseDC(hwnd, hdc);

    return retcode;
}

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")
