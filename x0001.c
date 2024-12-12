#include "xcommon.h"

#define NAME "x0001"
#define VIEWPORT_WIDTH 2048
#define VIEWPORT_HEIGHT 2048

static GLuint accum_tex;
static GLuint accum_fbo;

static GLuint postprocess_fs;
static const char* postprocess_fs_src =
NL "#version 460 compatibility"
NL "#extension NV_bindless_texture : require"
NL
NL "layout(location = 0) uniform sampler2DMS accum_texh;"
NL
NL "void main() {"
NL "    vec3 color = texelFetch(accum_texh, ivec2(gl_FragCoord.xy), gl_SampleID).rgb;"
NL "    color = color / (color + 1.0);"
NL "    gl_FragColor = vec4(color, 1.0);"
NL "}"
;

#define A1 -2.1
#define A2 1.4
#define A3 1.1

#define F1 0.4
#define F2 1.1
#define F3 1.0

#define A4 1.1
#define A5 1.2
#define A6 0.9

#define F4 1.1
#define F5 1.0
#define F6 0.7

#define VEL 0.05

static double pxn;
static double pyn;
static double tn;

static uint64_t iter;

static int32_t update(float dt) {
    (void)dt;

    glEnable(GL_BLEND);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, accum_fbo);
    glUseProgram(0);

    glMatrixLoadIdentityEXT(GL_PROJECTION);
    glLoadIdentity();

    glMatrixOrthoEXT(GL_PROJECTION, -3.5, 3.5, -3.0, 4.0, -1.0, 1.0);

    glColor3f(0.007f, 0.007f, 0.007f);
    glBegin(GL_POINTS);

    for (uint32_t i = 0; i < 10000; ++i) {
        if (iter >= 5000000) break;

        glVertex2d(pxn, pyn);

        double r0 = 0.001 * ((double)rand() / (double)RAND_MAX);
        double r1 = 0.001 * ((double)rand() / (double)RAND_MAX);

        double xn1 = (A1 + r0) * sin(F1 * pxn) + A2 * cos(F2 * pyn) + A3 * sin(F3 * tn);
        double yn1 = A4 * cos(F4 * pxn) + (A5 + r1) * sin(F5 * pyn) + A6 * cos(F6 * tn);
        double tn1 = (double)iter * VEL;

        pxn = xn1;
        pyn = yn1;
        tn = tn1;

        iter += 1;
    }

    glEnd();

    glDisable(GL_BLEND);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, xgl_msaa_fbo);
    glUseProgram(postprocess_fs);
    glMatrixLoadIdentityEXT(GL_PROJECTION);
    glLoadIdentity();
    glBegin(GL_TRIANGLES);
    glVertex2f(-1.0, -1.0);
    glVertex2f(3.0, -1.0);
    glVertex2f(-1.0, 3.0);
    glEnd();

    return 0;
}

static int32_t init(void) {
    glPointSize(1.0f);

    glBlendFunc(GL_ONE, GL_ONE);
    glBlendEquation(GL_FUNC_ADD);

    glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &accum_tex);
    glTextureStorage2DMultisample(accum_tex, XGL_NUM_MSAA_SAMPLES, GL_RGBA16F, VIEWPORT_WIDTH, VIEWPORT_HEIGHT, GL_FALSE);
    glClearTexImage(accum_tex, 0, GL_RGBA, GL_FLOAT, NULL);

    glCreateFramebuffers(1, &accum_fbo);
    glNamedFramebufferTexture(accum_fbo, GL_COLOR_ATTACHMENT0, accum_tex, 0);

    GLuint64 accum_texh = glGetTextureHandleNV(accum_tex);
    glMakeTextureHandleResidentNV(accum_texh);

    postprocess_fs = glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &postprocess_fs_src);
    glProgramUniformHandleui64NV(postprocess_fs, 0, accum_texh);

    return 0;
}

static void shutdown(void) {
    glDeleteFramebuffers(1, &accum_fbo);
    glDeleteTextures(1, &accum_tex);
}

int32_t main(void) {
    return xgl_run(&(xgl_RunInput){
        .name = NAME,
        .viewport_width = VIEWPORT_WIDTH,
        .viewport_height = VIEWPORT_HEIGHT,
        .init = init,
        .shutdown = shutdown,
        .update = update,
        .swap_interval = 1,
    });
}
