/*
 * Titan Hook - Etapa 1: projeto base para PSP
 * Mostra um cubo 3D colorido girando. O analógico move o cubo.
 * Serve de base para as próximas etapas (personagem, câmera, gancho).
 */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspgum.h>
#include <psppower.h>

PSP_MODULE_INFO("TitanHook", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define BUF_WIDTH  512
#define SCR_WIDTH  480
#define SCR_HEIGHT 272

/* Lista de comandos da GPU (precisa estar alinhada em 16 bytes) */
static unsigned int __attribute__((aligned(16))) gu_list[262144];

typedef struct {
    unsigned int color; /* ABGR */
    float x, y, z;
} Vertex;

/* Os 8 cantos do cubo */
static const float corners[8][3] = {
    {-1, -1, -1}, { 1, -1, -1}, { 1,  1, -1}, {-1,  1, -1},
    {-1, -1,  1}, { 1, -1,  1}, { 1,  1,  1}, {-1,  1,  1}
};

/* 6 faces, cada uma com 4 cantos (viram 2 triângulos) */
static const int faces[6][4] = {
    {0, 1, 2, 3}, /* trás */
    {5, 4, 7, 6}, /* frente */
    {4, 0, 3, 7}, /* esquerda */
    {1, 5, 6, 2}, /* direita */
    {3, 2, 6, 7}, /* cima */
    {4, 5, 1, 0}  /* baixo */
};

static const unsigned int face_colors[6] = {
    0xff3030e0, 0xff30e030, 0xffe05030,
    0xff30e0e0, 0xffe030e0, 0xffe0e030
};

/* 6 faces x 2 triângulos x 3 vértices = 36 vértices */
static Vertex __attribute__((aligned(16))) cube[36];

static void build_cube(void)
{
    static const int order[6] = {0, 1, 2, 0, 2, 3};
    int n = 0;
    for (int f = 0; f < 6; f++) {
        for (int i = 0; i < 6; i++) {
            const float *c = corners[faces[f][order[i]]];
            cube[n].color = face_colors[f];
            cube[n].x = c[0];
            cube[n].y = c[1];
            cube[n].z = c[2];
            n++;
        }
    }
}

/* ---- Botão HOME: sair do jogo ---- */
static int exit_callback(int arg1, int arg2, void *common)
{
    sceKernelExitGame();
    return 0;
}

static int callback_thread(SceSize args, void *argp)
{
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

static void setup_callbacks(void)
{
    int thid = sceKernelCreateThread("update_thread", callback_thread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0)
        sceKernelStartThread(thid, 0, 0);
}

/* ---- Inicialização da GPU ---- */
static void init_gu(void)
{
    sceGuInit();
    sceGuStart(GU_DIRECT, gu_list);
    sceGuDrawBuffer(GU_PSM_8888, (void *)0, BUF_WIDTH);
    sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT, (void *)0x88000, BUF_WIDTH);
    sceGuDepthBuffer((void *)0x110000, BUF_WIDTH);
    sceGuOffset(2048 - (SCR_WIDTH / 2), 2048 - (SCR_HEIGHT / 2));
    sceGuViewport(2048, 2048, SCR_WIDTH, SCR_HEIGHT);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, SCR_WIDTH, SCR_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuShadeModel(GU_SMOOTH);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

int main(void)
{
    setup_callbacks();

    /* CPU e GPU no máximo */
    scePowerSetClockFrequency(333, 333, 166);

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    build_cube();
    init_gu();

    float px = 0.0f, py = 0.0f;
    float angle = 0.0f;
    SceCtrlData pad;

    while (1) {
        /* Entrada: analógico move o cubo (com zona morta) */
        sceCtrlPeekBufferPositive(&pad, 1);
        float ax = ((int)pad.Lx - 128) / 128.0f;
        float ay = ((int)pad.Ly - 128) / 128.0f;
        if (ax > -0.2f && ax < 0.2f) ax = 0.0f;
        if (ay > -0.2f && ay < 0.2f) ay = 0.0f;
        px += ax * 0.08f;
        py -= ay * 0.08f;

        angle += 1.0f;

        /* Desenho */
        sceGuStart(GU_DIRECT, gu_list);

        sceGuClearColor(0xff402010);
        sceGuClearDepth(0);
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

        sceGumMatrixMode(GU_PROJECTION);
        sceGumLoadIdentity();
        sceGumPerspective(75.0f, 16.0f / 9.0f, 0.5f, 1000.0f);

        sceGumMatrixMode(GU_VIEW);
        sceGumLoadIdentity();

        sceGumMatrixMode(GU_MODEL);
        sceGumLoadIdentity();
        {
            ScePspFVector3 pos = { px, py, -6.0f };
            ScePspFVector3 rot = {
                angle * 0.79f * (GU_PI / 180.0f),
                angle * 0.98f * (GU_PI / 180.0f),
                angle * 1.32f * (GU_PI / 180.0f)
            };
            sceGumTranslate(&pos);
            sceGumRotateXYZ(&rot);
        }

        sceGumDrawArray(GU_TRIANGLES,
                        GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
                        36, 0, cube);

        sceGuFinish();
        sceGuSync(0, 0);

        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    return 0;
}
