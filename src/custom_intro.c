/*
 * custom_intro.c  -  Screen 1 of the custom intro cutscene.
 *
 * Scene:
 *   BG (with credits embedded in tilemap) slow fade-in.
 *   Leaf (left) and Rival (right) appear together with the BG.
 *   They throw Pokeballs.
 *   Pikachu spawns near Leaf and jumps diagonally to the center position.
 *   Eevee spawns near Rival and jumps diagonally to the center position.
 *   Both face each other (standoff) and the scene fades out.
 *
 * NOTES:
 *   - Credits are part of BG1 (tilemap) - no credit sprites.
 *   - OBJ VRAM: 4 sprites x 0x2000 = 0x8000 bytes - exactly at the limit.
 *   - Slow fade-in (delay=6) ensures no sprite appears out of nowhere.
 *   - Pokemon perform diagonal arc via x2/y2 with manual parabola in the callback.
 */

#include "global.h"
#include "main.h"
#include "palette.h"
#include "task.h"
#include "gpu_regs.h"
#include "decompress.h"
#include "sprite.h"
#include "sound.h"
#include "m4a.h"
#include "bg.h"
#include "malloc.h"
#include "scanline_effect.h"
#include "pokemon.h"
#include "trig.h"
#include "random.h"
#include "constants/songs.h"
#include "constants/rgb.h"
#include "custom_intro.h"
#include "title_screen.h"

/* =========================================================================
 * Tag IDs
 * ========================================================================= */
#define TAG_CI_LEAF    5100
#define TAG_CI_RIVAL   5101
#define TAG_CI_PIKACHU 5102
#define TAG_CI_EEVEE   5103

/* =========================================================================
 * Graphics
 * ========================================================================= */

/* Background 1 - grass field with credits embedded in tilemap */
static const u16 sCIBg1_Pal[]     = INCBIN_U16("graphics/custom_intro/screen1/bg1.gbapal");
static const u32 sCIBg1_Gfx[]     = INCBIN_U32("graphics/custom_intro/screen1/bg1.4bpp.smol");
static const u32 sCIBg1_Tilemap[] = INCBIN_U32("graphics/custom_intro/screen1/bg1.bin.smolTM");

/* Trainers (64x64, 4-frame vertical strip = 0x2000 bytes each) */
static const u16 sCILeaf_Pal[]    = INCBIN_U16("graphics/custom_intro/screen1/leaf.gbapal");
static const u32 sCILeaf_Gfx[]    = INCBIN_U32("graphics/custom_intro/screen1/leaf.4bpp.smol");

static const u16 sCIRival_Pal[]   = INCBIN_U16("graphics/custom_intro/screen1/rival.gbapal");
static const u32 sCIRival_Gfx[]   = INCBIN_U32("graphics/custom_intro/screen1/rival.4bpp.smol");

/* Pokemon (64x64, 4-frame vertical strip = 0x2000 bytes each) */
static const u16 sCIPikachu_Pal[] = INCBIN_U16("graphics/custom_intro/screen1/pikachu.gbapal");
static const u32 sCIPikachu_Gfx[] = INCBIN_U32("graphics/custom_intro/screen1/pikachu.4bpp.smol");

static const u16 sCIEevee_Pal[]   = INCBIN_U16("graphics/custom_intro/screen1/eevee.gbapal");
static const u32 sCIEevee_Gfx[]   = INCBIN_U32("graphics/custom_intro/screen1/eevee.4bpp.smol");

/* =========================================================================
 * Sprite sheets / palettes
 * OBJ VRAM: 4 x 0x2000 = 0x8000 bytes - exactly at the limit, no overflow.
 * ========================================================================= */
static const struct CompressedSpriteSheet sCISpriteSheets[] =
{
    {sCILeaf_Gfx,    0x2000, TAG_CI_LEAF},
    {sCIRival_Gfx,   0x2000, TAG_CI_RIVAL},
    {sCIPikachu_Gfx, 0x2000, TAG_CI_PIKACHU},
    {sCIEevee_Gfx,   0x2000, TAG_CI_EEVEE},
    {},
};

static const struct SpritePalette sCISpritePalettes[] =
{
    {sCILeaf_Pal,    TAG_CI_LEAF},    /* OBJ pal slot 0 */
    {sCIRival_Pal,   TAG_CI_RIVAL},   /* OBJ pal slot 1 */
    {sCIPikachu_Pal, TAG_CI_PIKACHU}, /* OBJ pal slot 2 */
    {sCIEevee_Pal,   TAG_CI_EEVEE},   /* OBJ pal slot 3 */
    {},
};

/* =========================================================================
 * OAM data - one struct per type for fixed paletteNum with no ambiguity
 * ========================================================================= */

static const struct OamData sCIOam_Leaf =
{
    .y          = DISPLAY_HEIGHT,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode    = ST_OAM_OBJ_NORMAL,
    .mosaic     = FALSE,
    .bpp        = ST_OAM_4BPP,
    .shape      = SPRITE_SHAPE(64x64),
    .size       = SPRITE_SIZE(64x64),
    .priority   = 1,
    .paletteNum = 0,
};

static const struct OamData sCIOam_Rival =
{
    .y          = DISPLAY_HEIGHT,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode    = ST_OAM_OBJ_NORMAL,
    .mosaic     = FALSE,
    .bpp        = ST_OAM_4BPP,
    .shape      = SPRITE_SHAPE(64x64),
    .size       = SPRITE_SIZE(64x64),
    .priority   = 1,
    .paletteNum = 1,
};

static const struct OamData sCIOam_Pikachu =
{
    .y          = DISPLAY_HEIGHT,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode    = ST_OAM_OBJ_NORMAL,
    .mosaic     = FALSE,
    .bpp        = ST_OAM_4BPP,
    .shape      = SPRITE_SHAPE(64x64),
    .size       = SPRITE_SIZE(64x64),
    .priority   = 1,
    .paletteNum = 2,
};

static const struct OamData sCIOam_Eevee =
{
    .y          = DISPLAY_HEIGHT,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode    = ST_OAM_OBJ_NORMAL,
    .mosaic     = FALSE,
    .bpp        = ST_OAM_4BPP,
    .shape      = SPRITE_SHAPE(64x64),
    .size       = SPRITE_SIZE(64x64),
    .priority   = 1,
    .paletteNum = 3,
};

/* =========================================================================
 * Animations
 * Tile offset per frame = 64 tiles (sprite 64x64 / tile 8x8 = 64 tiles per frame)
 * ========================================================================= */

/* Leaf */
static const union AnimCmd sCIAnim_Leaf_Idle[] =
{
    ANIMCMD_FRAME(0, 0),
    ANIMCMD_END,
};
static const union AnimCmd sCIAnim_Leaf_Throw[] =
{
    ANIMCMD_FRAME(0,   10),
    ANIMCMD_FRAME(64,   8),
    ANIMCMD_FRAME(128,  8),
    ANIMCMD_FRAME(192,  0),   /* hold on throw frame */
    ANIMCMD_END,
};
static const union AnimCmd *const sCIAnims_Leaf[] =
{
    sCIAnim_Leaf_Idle,
    sCIAnim_Leaf_Throw,
};

/* Rival */
static const union AnimCmd sCIAnim_Rival_Idle[] =
{
    ANIMCMD_FRAME(0, 0),
    ANIMCMD_END,
};
static const union AnimCmd sCIAnim_Rival_Throw[] =
{
    ANIMCMD_FRAME(0,   10),
    ANIMCMD_FRAME(64,   8),
    ANIMCMD_FRAME(128,  8),
    ANIMCMD_FRAME(192,  0),
    ANIMCMD_END,
};
static const union AnimCmd *const sCIAnims_Rival[] =
{
    sCIAnim_Rival_Idle,
    sCIAnim_Rival_Throw,
};

/*
 * Pokemon - single-frame animations controlled manually by the callback.
 *
 * The logic for which frame to display lives in SpriteCB_CI_Pokemon, based on
 * sJumpT. The sequences below expose each frame individually so that
 * StartSpriteAnim(sprite, N) shows exactly frame N and locks there:
 *
 *   anim 0  ->  frame 0  (idle, standoff position - visible from the start)
 *   anim 1  ->  frame 1  (takeoff - at the start of the jump)
 *   anim 2  ->  frame 2  (airborne - during most of the arc)
 *   anim 3  ->  frame 3  (landing - lock on last frame)
 */

/* Pikachu */
static const union AnimCmd sCIAnim_Pika_Frame0[] = { ANIMCMD_FRAME(0,   0), ANIMCMD_END };
static const union AnimCmd sCIAnim_Pika_Frame1[] = { ANIMCMD_FRAME(64,  0), ANIMCMD_END };
static const union AnimCmd sCIAnim_Pika_Frame2[] = { ANIMCMD_FRAME(128, 0), ANIMCMD_END };
static const union AnimCmd sCIAnim_Pika_Frame3[] = { ANIMCMD_FRAME(192, 0), ANIMCMD_END };

static const union AnimCmd *const sCIAnims_Pikachu[] =
{
    sCIAnim_Pika_Frame0,   /* anim 0 - idle / standoff */
    sCIAnim_Pika_Frame1,   /* anim 1 - takeoff         */
    sCIAnim_Pika_Frame2,   /* anim 2 - airborne        */
    sCIAnim_Pika_Frame3,   /* anim 3 - landing         */
};

/* Eevee */
static const union AnimCmd sCIAnim_Eevee_Frame0[] = { ANIMCMD_FRAME(0,   0), ANIMCMD_END };
static const union AnimCmd sCIAnim_Eevee_Frame1[] = { ANIMCMD_FRAME(64,  0), ANIMCMD_END };
static const union AnimCmd sCIAnim_Eevee_Frame2[] = { ANIMCMD_FRAME(128, 0), ANIMCMD_END };
static const union AnimCmd sCIAnim_Eevee_Frame3[] = { ANIMCMD_FRAME(192, 0), ANIMCMD_END };

static const union AnimCmd *const sCIAnims_Eevee[] =
{
    sCIAnim_Eevee_Frame0,
    sCIAnim_Eevee_Frame1,
    sCIAnim_Eevee_Frame2,
    sCIAnim_Eevee_Frame3,
};

/* =========================================================================
 * Forward declarations
 * ========================================================================= */
static void Task_CI_Load(u8 taskId);
static void Task_CI_FadeIn(u8 taskId);
static void Task_CI_ThrowBalls(u8 taskId);
static void Task_CI_Standoff(u8 taskId);
static void Task_CI_Cries(u8 taskId);
static void Task_CI_ShowScreen2(u8 taskId);
void CB2_StartCustomIntroScreen2(void); /* defined further below, in the Screen 2 section */
void CB2_StartCustomIntroScreen3(void); /* defined further below, in the Screen 3 section */
void CB2_StartCustomIntroScreen4(void); /* defined further below, in the Screen 4 section */
void CB2_StartCustomIntroScreen5(void); /* defined further below, in the Screen 5 section */
void CB2_StartCustomIntroScreen6(void); /* defined further below, in the Screen 6 section */
void CB2_StartCustomIntroScreen7(void); /* defined further below, in the Screen 7 section */

static void SpriteCB_CI_Trainer(struct Sprite *sprite);
static void SpriteCB_CI_Pokemon(struct Sprite *sprite);

/* =========================================================================
 * Sprite templates
 * ========================================================================= */
static const struct SpriteTemplate sCISpriteTemplate_Leaf =
{
    .tileTag    = TAG_CI_LEAF,
    .paletteTag = TAG_CI_LEAF,
    .oam        = &sCIOam_Leaf,
    .anims      = sCIAnims_Leaf,
    .callback   = SpriteCB_CI_Trainer,
};

static const struct SpriteTemplate sCISpriteTemplate_Rival =
{
    .tileTag    = TAG_CI_RIVAL,
    .paletteTag = TAG_CI_RIVAL,
    .oam        = &sCIOam_Rival,
    .anims      = sCIAnims_Rival,
    .callback   = SpriteCB_CI_Trainer,
};

static const struct SpriteTemplate sCISpriteTemplate_Pikachu =
{
    .tileTag    = TAG_CI_PIKACHU,
    .paletteTag = TAG_CI_PIKACHU,
    .oam        = &sCIOam_Pikachu,
    .anims      = sCIAnims_Pikachu,
    .callback   = SpriteCB_CI_Pokemon,
};

static const struct SpriteTemplate sCISpriteTemplate_Eevee =
{
    .tileTag    = TAG_CI_EEVEE,
    .paletteTag = TAG_CI_EEVEE,
    .oam        = &sCIOam_Eevee,
    .anims      = sCIAnims_Eevee,
    .callback   = SpriteCB_CI_Pokemon,
};

/* =========================================================================
 * Timing (frames at 60 fps)
 *
 *   0          fade-in starts (delay=4 -> ~20 frames to complete)
 *   60         trainers throw pokeballs
 *   100        pokemon start diagonal jump
 *   145        jump finished -> quick standoff
 *   145+30     fade out to black -> switch to BG2 -> fade in
 * ========================================================================= */
#define CI_TIMER_ANIM_START   60
#define CI_TIMER_JUMP_START   100
#define CI_TIMER_SCENE_END    145
#define CI_STANDOFF_DURATION  30

/* =========================================================================
 * Task data aliases
 * ========================================================================= */
#define tLeafId    data[0]
#define tRivalId   data[1]
#define tPikachuId data[2]
#define tEeveeId   data[3]
#define tTimer     data[4]

/* =========================================================================
 * Sprite data aliases
 *
 * For pokemon the diagonal jump uses:
 *   sJumpT   - frame counter since jump start (0..CI_JUMP_DUR)
 *   sStartX  - initial x position (px, relative to sprite->x)
 *   sEndX    - final x position   (offset to standoff position)
 *   sArcH    - peak height of the arc (in pixels, positive = upward)
 *   sEndY    - final y offset (0 = same row, positive = lower)
 * ========================================================================= */
#define sJumpT  data[0]
#define sStartX data[1]
#define sEndX   data[2]
#define sArcH   data[3]
#define sEndY   data[4]

/*
 * Jump duration in frames.
 * The 4-frame animation lasts: 6+8+6 = 20 frames before locking.
 * CI_JUMP_DUR drives the parabola - can be longer than the animation itself.
 */
#define CI_JUMP_DUR 40

/* =========================================================================
 * VBlank / main callbacks
 * ========================================================================= */
static void MainCB2_EndCustomIntro(void);

static void VBlankCB_CustomIntro(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

void MainCB2_CustomIntro(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    UpdatePaletteFade();

    /* Skip: any key press (while not mid-fade) cuts straight to the title
     * screen, same convention as the vanilla MainCB2_Intro/MainCB2_EndIntro. */
    if (gMain.newKeys != 0 && !gPaletteFade.active)
        SetMainCallback2(MainCB2_EndCustomIntro);
}

static void MainCB2_EndCustomIntro(void)
{
    if (!UpdatePaletteFade())
        SetMainCallback2(CB2_InitTitleScreen);
}

/* =========================================================================
 * Entry point
 * ========================================================================= */
void CB2_StartCustomIntroScreen1(void)
{
    SetVBlankCallback(NULL);
    SetGpuReg(REG_OFFSET_DISPCNT, 0);
    CpuFill32(0, (void *)VRAM,       VRAM_SIZE);
    CpuFill32(0, (void *)OAM,        OAM_SIZE);
    CpuFill16(0, (void *)(PLTT + 2), PLTT_SIZE - 2);
    ResetPaletteFade();
    ScanlineEffect_Stop();
    ResetTasks();
    ResetSpriteData();
    FreeAllSpritePalettes();
    EnableInterrupts(INTR_FLAG_VBLANK);
    SetVBlankCallback(VBlankCB_CustomIntro);
    SetMainCallback2(MainCB2_CustomIntro);
    CreateTask(Task_CI_Load, 0);
}

/* =========================================================================
 * Task: Load
 *
 * Carrega BG e sprites ANTES do fade, depois inicia fade lento (delay=6).
 * delay=6 significa ~26 frames (~0.43 s) de fade preto->normal,
 * enough time so the player doesn't see sprites 'popping' on screen.
 * ========================================================================= */
static void Task_CI_Load(u8 taskId)
{
    u8 leafId, rivalId, pikId, eveId;
    u32 i;

    /* ---- Background ---- */
    DecompressDataWithHeaderVram(sCIBg1_Gfx,     (void *)(BG_CHAR_ADDR(0)));
    DecompressDataWithHeaderVram(sCIBg1_Tilemap, (void *)(BG_SCREEN_ADDR(16)));
    LoadPalette(sCIBg1_Pal, BG_PLTT_ID(0), sizeof(sCIBg1_Pal));

    SetGpuReg(REG_OFFSET_BG1CNT,
              BGCNT_PRIORITY(3)
            | BGCNT_CHARBASE(0)
            | BGCNT_SCREENBASE(16)
            | BGCNT_16COLOR
            | BGCNT_TXT256x256);
    SetGpuReg(REG_OFFSET_BG1HOFS, 0);
    SetGpuReg(REG_OFFSET_BG1VOFS, 0);

    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0
                                 | DISPCNT_OBJ_1D_MAP
                                 | DISPCNT_BG1_ON
                                 | DISPCNT_OBJ_ON);

    /* ---- Sprites ---- */
    for (i = 0; sCISpriteSheets[i].tag != 0; i++)
        LoadCompressedSpriteSheet(&sCISpriteSheets[i]);
    for (i = 0; sCISpritePalettes[i].tag != 0; i++)
        LoadSpritePalette(&sCISpritePalettes[i]);

    /* ---- Create sprites at their final standoff positions.
     *      Pokemon start invisible; become visible when jump starts. ---- */

    /* Leaf - left side, back to camera */
    leafId = CreateSprite(&sCISpriteTemplate_Leaf, 29, 130, 1);
    StartSpriteAnim(&gSprites[leafId], 0);
    gTasks[taskId].tLeafId = leafId;

    /* Rival - right side, flipped to face left */
    rivalId = CreateSprite(&sCISpriteTemplate_Rival, 200, 122, 1);
    gSprites[rivalId].hFlip = TRUE;
    StartSpriteAnim(&gSprites[rivalId], 0);
    gTasks[taskId].tRivalId = rivalId;

    /*
     * Pikachu - final position: centre-left, higher than before
     *   x=105, y=118  (raised from 132 to 118)
     *   Spawn: x2=-70 -> 70 px left of final position = near edge
     *          y2=+18 -> spawns lower on screen (more in front), away from trainer
     *   This ensures on the first frame Pikachu appears below and to the left
     *   of Leaf, without overlapping her.
     *   subpriority=1 -> in front of Eevee (which has subpriority=2)
     */
    pikId = CreateSprite(&sCISpriteTemplate_Pikachu, 80, 138, 1);
    gSprites[pikId].sJumpT  = 0;
    gSprites[pikId].sStartX = -5;    /* idle: x=75, to the right of Leaf - DO NOT CHANGE */
    gSprites[pikId].sEndX   = 15;    /* landing: x=95, pulled back from centre to avoid collision */
    gSprites[pikId].sArcH   = 32;    /* taller arc */
    gSprites[pikId].sEndY   = -12;
    gSprites[pikId].x2      = -5;
    gSprites[pikId].y2      = 10;
    StartSpriteAnim(&gSprites[pikId], 0);
    gTasks[taskId].tPikachuId = pikId;

    /*
     * Eevee - final position: centre-right, higher than Pikachu (looks further away)
     *   x=158, y=110  (8 px above Pikachu -> depth illusion)
     *   Eevee looks smaller/more distant because it is higher on screen.
     *   Spawn: x2=+70 -> 70 px right, near edge, away from Rival
     *          y2=+18 -> same depth offset as Pikachu
     *   subpriority=2 -> behind Pikachu
     */
    eveId = CreateSprite(&sCISpriteTemplate_Eevee, 165, 138, 2);
    gSprites[eveId].hFlip   = TRUE;
    gSprites[eveId].sJumpT  = 0;
    gSprites[eveId].sStartX = -10;   /* idle: x=155, to the left of Rival - DO NOT CHANGE */
    gSprites[eveId].sEndX   = -15;   /* landing: x=150, pulled back from centre to avoid collision */
    gSprites[eveId].sArcH   = 32;    /* taller arc */
    gSprites[eveId].sEndY   = -14;
    gSprites[eveId].x2      = -10;
    gSprites[eveId].y2      = 10;
    StartSpriteAnim(&gSprites[eveId], 0);
    gTasks[taskId].tEeveeId = eveId;

    /*
     * Faster fade: delay=4 -> ~16 frames per step.
     * With 16 steps total ≈ 64 frames (~1 s) of black->normal fade.
     */
    BeginNormalPaletteFade(PALETTES_ALL, 4, 16, 0, RGB_BLACK);

    gTasks[taskId].tTimer = 0;
    gTasks[taskId].func   = Task_CI_FadeIn;
}

/* =========================================================================
 * Task: FadeIn - wait for the fade to complete
 * ========================================================================= */
static void Task_CI_FadeIn(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        /* Start the intro BGM as soon as the scene is fully visible -
         * same approach as intro_frlg.c (m4aSongNumStart after fade). */
        m4aSongNumStart(MUS_LGPETITLE);
        gTasks[taskId].func = Task_CI_ThrowBalls;
    }
}

/* =========================================================================
 * Sprite callback: Trainers - no per-frame logic, engine handles anim
 * ========================================================================= */
static void SpriteCB_CI_Trainer(struct Sprite *sprite)
{
    (void)sprite;
}

/* =========================================================================
 * Sprite callback: Pokemon - diagonal parabola with per-phase frames
 *
 * Jump phases based on sJumpT (0 = not yet jumped):
 *
 *   sJumpT == 0        -> idle: x2 fixed at sStartX, frame 0, no advance
 *   1  <= t < PHASE2   -> takeoff:  frame 1, start of arc
 *   PHASE2 ≤ t < PHASE3  -> airborne:  frame 2, peak and descent
 *   t >= PHASE3          -> landing:   frame 3, lock and stop
 *
 * The callback only advances sJumpT when sJumpT > 0 (activated by ThrowBalls).
 * While sJumpT == 0 the sprite stays at its spawn point with frame 0.
 * ========================================================================= */

/* Phase thresholds within CI_JUMP_DUR=40 frames */
#define CI_JUMP_PHASE2  8    /* takeoff -> airborne */
#define CI_JUMP_PHASE3  32   /* airborne -> landing */

static void SpriteCB_CI_Pokemon(struct Sprite *sprite)
{
    s16 t256, x2, arcY, perspY;

    /* Not yet jumped - stay still at spawn point with frame 0 */
    if (sprite->sJumpT == 0)
    {
        sprite->x2 = sprite->sStartX;
        sprite->y2 = 0;
        return;
    }

    /* Jump finished - lock on frame 3 at final position */
    if (sprite->sJumpT > CI_JUMP_DUR)
    {
        sprite->x2 = sprite->sEndX;
        sprite->y2 = sprite->sEndY;
        return;
    }

    /* ---- Frame selection based on phase ---- */
    if (sprite->sJumpT == 1)
        StartSpriteAnim(sprite, 1);                     /* takeoff  */
    else if (sprite->sJumpT == CI_JUMP_PHASE2)
        StartSpriteAnim(sprite, 2);                     /* airborne */
    else if (sprite->sJumpT == CI_JUMP_PHASE3)
        StartSpriteAnim(sprite, 3);                     /* landing  */

    /* ---- Diagonal parabola ---- */
    /* t scaled to 256: 0->256 over CI_JUMP_DUR frames */
    t256 = ((s16)sprite->sJumpT * 256) / CI_JUMP_DUR;

    /* x2: lerp from sStartX to sEndX */
    x2 = sprite->sStartX
       + ((s16)(sprite->sEndX - sprite->sStartX) * t256 / 256);

    /* y2 parabola: -arcH * 4 * t * (1-t)   [integers, scale 256^2] */
    arcY = -(s16)sprite->sArcH * 4 * t256 * (256 - t256) / (256 * 256);

    /* perspective: landing slightly lower than spawn */
    perspY = (s16)sprite->sEndY * t256 / 256;

    sprite->x2 = x2;
    sprite->y2 = arcY + perspY;

    sprite->sJumpT++;
}

/* =========================================================================
 * Task: ThrowBalls
 * ========================================================================= */
static void Task_CI_ThrowBalls(u8 taskId)
{
    struct Task *task = &gTasks[taskId];
    task->tTimer++;

    /* Trainers throw */
    if (task->tTimer == CI_TIMER_ANIM_START)
    {
        PlaySE(SE_BALL_THROW);
        StartSpriteAnim(&gSprites[task->tLeafId],  1);
        StartSpriteAnim(&gSprites[task->tRivalId], 1);
    }

    /* Cries fire right after the throw, no long delay */
    if (task->tTimer == CI_TIMER_ANIM_START + 10)
        PlayCryInternal(SPECIES_EEVEE,   0, 120, CRY_PRIORITY_NORMAL, CRY_MODE_NORMAL);

    if (task->tTimer == CI_TIMER_ANIM_START + 25)
        PlayCryInternal(SPECIES_PIKACHU, 0, 120, CRY_PRIORITY_NORMAL, CRY_MODE_NORMAL);

    /* Pokemon start the diagonal jump - callback takes control of frames */
    if (task->tTimer == CI_TIMER_JUMP_START)
    {
        struct Sprite *pik = &gSprites[task->tPikachuId];
        struct Sprite *eve = &gSprites[task->tEeveeId];

        /* sJumpT = 1 activates the callback; it switches to frame 1 on the first tick */
        pik->sJumpT = 1;
        eve->sJumpT = 1;
    }

    if (task->tTimer >= CI_TIMER_SCENE_END)
    {
        task->tTimer = 0;
        task->func   = Task_CI_Standoff;
    }
}

/* =========================================================================
 * Task: Standoff - hold the final frame for ~0.5 s, then fade out
 * ========================================================================= */
static void Task_CI_Standoff(u8 taskId)
{
    struct Task *task = &gTasks[taskId];
    task->tTimer++;

    if (task->tTimer >= CI_STANDOFF_DURATION)
    {
        task->tTimer = 0;
        task->func   = Task_CI_Cries;
    }
}

/* =========================================================================
 * Task: Cries - Eevee cries first, then Pikachu, then fade out
 *
 * Timer:
 *   0         -> PlayCry for Eevee
 *   30 frames -> PlayCry for Pikachu  (~0.5 s later)
 *   75 frames -> start fade out
 * ========================================================================= */
static void Task_CI_Cries(u8 taskId)
{
    struct Task *task = &gTasks[taskId];
    task->tTimer++;

    /* Cries already fired in Task_CI_ThrowBalls - wait a short beat
       for the last cry to finish, then fade white into Screen 2. */
    if (task->tTimer == 1)
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_WHITE);

    if (task->tTimer >= 20 && !gPaletteFade.active)
    {
        task->tTimer = 0;
        task->func   = Task_CI_ShowScreen2;
    }
}

/* =========================================================================
 * Task: ShowScreen2 - called while screen is white after Screen 1 fade.
 * Destroys all Screen 1 sprites, loads Screen 2 assets, then hands off
 * to CB2_StartCustomIntroScreen2 which re-initialises everything cleanly.
 * ========================================================================= */
static void Task_CI_ShowScreen2(u8 taskId)
{
    ResetSpriteData();
    FreeAllSpritePalettes();
    SetMainCallback2(CB2_StartCustomIntroScreen2);
    DestroyTask(taskId);
}

/* =========================================================================
 * Undefine Screen 1 macros
 * ========================================================================= */
#undef tLeafId
#undef tRivalId
#undef tPikachuId
#undef tEeveeId
#undef tTimer

#undef sJumpT
#undef sStartX
#undef sEndX
#undef sArcH
#undef sEndY

#undef CI_JUMP_PHASE2
#undef CI_JUMP_PHASE3

/* ==========================================================================
 * ==========================================================================
 * SCREEN 2 - Red & Charizard scene
 *
 * Layout (back to front priority):
 *   BG1 (priority 3) - static orange sky background (furthest back)
 *   Sun sprite  (priority 2) - 64x64, grows small->large like the Moon in
 *                               intro.c: ST_OAM_AFFINE_NORMAL, SetOamMatrix
 *                               PA/PD 768->256 decrementing 3/frame.
 *   BG0 (priority 1) - Charizard as a tileset (too large for a sprite)
 *   Red sprite  (priority 0) - 64x64, 4 frames: shadow->reveal animation,
 *                               plays once then holds on frame 3.
 *   Fire sprite (priority 0) - 64x64, 4 frames: Charizard's fire breath,
 *                               invisible until Red animation finishes;
 *                               created after Red so it wins the same-
 *                               priority tie and draws in front of him.
 *
 * Sequence:
 *   Load -> fade-in from white -> Sun grows throughout -> Red 4-frame anim
 *   -> Fire 4-frame anim -> fade to white -> title screen.
 *
 * Asset paths: graphics/custom_intro/screen2/
 *   bg2.gbapal / bg2.4bpp.smol / bg2.bin.smolTM  (BG1 sky)
 *   chirazard.gbapal / chirazard.4bpp.smol / chirazard.bin.smolTM (BG0)
 *   sun.gbapal  / sun.4bpp.smol               (64x64 1-frame)
 *   red.gbapal  / red.4bpp.smol               (64x64 4-frame strip)
 *   fire.gbapal / fire.4bpp.smol              (64x64 4-frame strip)
 * ==========================================================================
 * ========================================================================== */

/* -------------------------------------------------------------------------
 * Tag IDs - Screen 2
 * ------------------------------------------------------------------------- */
#define TAG_S2_SUN   5200
#define TAG_S2_RED   5201
#define TAG_S2_FIRE  5202

/* -------------------------------------------------------------------------
 * Graphics - Screen 2
 * ------------------------------------------------------------------------- */

/* BG1: static orange sky */
static const u16 sS2Bg_Pal[]       = INCBIN_U16("graphics/custom_intro/screen2/bg2.gbapal");
static const u32 sS2Bg_Gfx[]       = INCBIN_U32("graphics/custom_intro/screen2/bg2.4bpp.smol");
static const u32 sS2Bg_Tilemap[]   = INCBIN_U32("graphics/custom_intro/screen2/bg2.bin.smolTM");

/* BG0: Charizard tileset */
static const u32 sS2Char_Gfx[]     = INCBIN_U32("graphics/custom_intro/screen2/chirazard.4bpp.smol");
static const u32 sS2Char_Tilemap[] = INCBIN_U32("graphics/custom_intro/screen2/chirazard.bin.smolTM");
static const u16 sS2Char_Pal[]     = INCBIN_U16("graphics/custom_intro/screen2/chirazard.gbapal");

/* Sun - 64x64, 1 frame */
static const u16 sS2Sun_Pal[]      = INCBIN_U16("graphics/custom_intro/screen2/sun.gbapal");
static const u32 sS2Sun_Gfx[]      = INCBIN_U32("graphics/custom_intro/screen2/sun.4bpp.smol");

/* Red - 64x64, 4-frame vertical strip (0x2000 bytes) */
static const u16 sS2Red_Pal[]      = INCBIN_U16("graphics/custom_intro/screen2/red.gbapal");
static const u32 sS2Red_Gfx[]      = INCBIN_U32("graphics/custom_intro/screen2/red.4bpp.smol");

/* Fire - 64x64, 4-frame vertical strip (0x2000 bytes) */
static const u16 sS2Fire_Pal[]     = INCBIN_U16("graphics/custom_intro/screen2/fire.gbapal");
static const u32 sS2Fire_Gfx[]     = INCBIN_U32("graphics/custom_intro/screen2/fire.4bpp.smol");

/* -------------------------------------------------------------------------
 * Sprite sheets / palettes - Screen 2
 * ------------------------------------------------------------------------- */
static const struct CompressedSpriteSheet sS2SpriteSheets[] =
{
    {sS2Sun_Gfx,  0x0800, TAG_S2_SUN},   /* 1 frame 64x64 = 2048 bytes */
    {sS2Red_Gfx,  0x2000, TAG_S2_RED},   /* 4 frames 64x64 */
    {sS2Fire_Gfx, 0x2000, TAG_S2_FIRE},  /* 4 frames 64x64 */
    {},
};

static const struct SpritePalette sS2SpritePalettes[] =
{
    {sS2Sun_Pal,  TAG_S2_SUN},
    {sS2Red_Pal,  TAG_S2_RED},
    {sS2Fire_Pal, TAG_S2_FIRE},
    {},
};

/* -------------------------------------------------------------------------
 * OAM data - Screen 2
 * ------------------------------------------------------------------------- */

/* Sun: affine normal so we can drive scale manually via SetOamMatrix.
 *
 * OAM priority is only 2 bits wide (valid range 0-3) - there is no
 * priority 4 on real hardware; using it overflows the bitfield, which is
 * exactly the build error this caused. The full back-to-front order for
 * this scene is:
 *   BG1 sky        priority 3  (furthest back)
 *   Sun sprite     priority 2  (behind Charizard, in front of sky)
 *   BG0 Charizard  priority 1
 *   Red sprite     priority 0  (in front of Charizard)
 *   Fire sprite    priority 0  (same as Red; created after Red so it wins
 *                               the tie and draws in front of him) */
static const struct OamData sS2OamData_Sun =
{
    .y          = DISPLAY_HEIGHT,
    .affineMode = ST_OAM_AFFINE_NORMAL,
    .objMode    = ST_OAM_OBJ_NORMAL,
    .bpp        = ST_OAM_4BPP,
    .shape      = SPRITE_SHAPE(64x64),
    .size       = SPRITE_SIZE(64x64),
    .matrixNum  = 0,
    .priority   = 2,
};

/* Red: normal sprite, no affine. Priority 0 - in front of Charizard (BG0,
 * priority 1). */
static const struct OamData sS2OamData_Red =
{
    .y          = DISPLAY_HEIGHT,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode    = ST_OAM_OBJ_NORMAL,
    .bpp        = ST_OAM_4BPP,
    .shape      = SPRITE_SHAPE(64x64),
    .size       = SPRITE_SIZE(64x64),
    .priority   = 0,
};

/* Fire: same priority as Red (0) - Fire is created after Red in
 * Task_S2_Load, and among sprites that share a priority, the one created
 * later (higher OAM index) draws in front, so Fire correctly appears in
 * front of Red. */
static const struct OamData sS2OamData_Fire =
{
    .y          = DISPLAY_HEIGHT,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode    = ST_OAM_OBJ_NORMAL,
    .bpp        = ST_OAM_4BPP,
    .shape      = SPRITE_SHAPE(64x64),
    .size       = SPRITE_SIZE(64x64),
    .priority   = 0,
};

/* -------------------------------------------------------------------------
 * Animations - Screen 2
 * ------------------------------------------------------------------------- */

/* Sun: single frame, loops (grow is driven manually in callback) */
static const union AnimCmd sS2Anim_Sun[] =
{
    ANIMCMD_FRAME(0, 1),
    ANIMCMD_JUMP(0),
};
static const union AnimCmd *const sS2Anims_Sun[] = { sS2Anim_Sun };

/* Red: 4 frames, much slower (~0.5s per frame) so the shadow->face reveal
 * actually reads as a deliberate reveal instead of flashing by. Holds on
 * the final frame once done. */
static const union AnimCmd sS2Anim_Red[] =
{
    ANIMCMD_FRAME(0,   30),
    ANIMCMD_FRAME(64,  30),
    ANIMCMD_FRAME(128, 30),
    ANIMCMD_FRAME(192,  0),   /* hold on frame 3 */
    ANIMCMD_END,
};
static const union AnimCmd *const sS2Anims_Red[] = { sS2Anim_Red };

/* Fire: 4 frames looping, slowed down to match the calmer pacing */
static const union AnimCmd sS2Anim_Fire[] =
{
    ANIMCMD_FRAME(0,   10),
    ANIMCMD_FRAME(64,  10),
    ANIMCMD_FRAME(128, 10),
    ANIMCMD_FRAME(192, 10),
    ANIMCMD_JUMP(0),
};
static const union AnimCmd *const sS2Anims_Fire[] = { sS2Anim_Fire };

/* -------------------------------------------------------------------------
 * Sprite callbacks - Screen 2
 * ------------------------------------------------------------------------- */
static void SpriteCB_S2_Sun(struct Sprite *sprite);

/* Sun grow: data[1] = current PA/PD scale, starts at 768, decrements to 256.
 * Identical logic to SpriteCB_Scene0Moon in intro.c. */
static void SpriteCB_S2_Sun(struct Sprite *sprite)
{
    s16 scale = sprite->data[1];
    if (scale > 256)
    {
        if (++sprite->data[2] % 2 == 0)
        {
            scale -= 3;
            if (scale < 256)
                scale = 256;
            sprite->data[1] = scale;
        }
    }
    SetOamMatrix((u8)sprite->oam.matrixNum, (u16)scale, 0, 0, (u16)scale);
}

/* Red: no movement - he appears in place and only plays his frame animation.
 * SpriteCallbackDummy is used directly in the template below. */
#define RED_REST_X 141  /* tune X position here if needed - increase to move Red further right */

/* -------------------------------------------------------------------------
 * Sprite templates - Screen 2
 * ------------------------------------------------------------------------- */
static const struct SpriteTemplate sS2SpriteTemplate_Sun =
{
    .tileTag    = TAG_S2_SUN,
    .paletteTag = TAG_S2_SUN,
    .oam        = &sS2OamData_Sun,
    .anims      = sS2Anims_Sun,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback   = SpriteCB_S2_Sun,
};

static const struct SpriteTemplate sS2SpriteTemplate_Red =
{
    .tileTag    = TAG_S2_RED,
    .paletteTag = TAG_S2_RED,
    .oam        = &sS2OamData_Red,
    .anims      = sS2Anims_Red,
    .callback   = SpriteCallbackDummy,
};

static const struct SpriteTemplate sS2SpriteTemplate_Fire =
{
    .tileTag    = TAG_S2_FIRE,
    .paletteTag = TAG_S2_FIRE,
    .oam        = &sS2OamData_Fire,
    .anims      = sS2Anims_Fire,
    .callback   = SpriteCallbackDummy,
};

/* -------------------------------------------------------------------------
 * Task data aliases - Screen 2
 * ------------------------------------------------------------------------- */
#define tS2SunId   data[0]
#define tS2RedId   data[1]
#define tS2FireId  data[2]
#define tS2Timer   data[3]

/* Phase durations (frames @ 60 fps) - paced deliberately, one beat at a
 * time, instead of everything happening on top of itself:
 *   1) Sun grows alone for a while first (S2_SUN_HOLD_DUR)
 *   2) Then Red slides in slowly and reveals his face
 *   3) Then Charizard breathes fire (S2_FIRE_DUR)
 *   4) Then fade to white
 */
#define S2_SUN_HOLD_DUR     90   /* ~1.5s of sun growing alone before Red appears */
#define S2_RED_ANIM_DONE    90   /* 3 reveal frames x 30 ticks each */
#define S2_FIRE_DUR        120   /* fire loops for 2s before fading out */

/* -------------------------------------------------------------------------
 * Forward declarations - Screen 2 tasks
 * ------------------------------------------------------------------------- */
static void Task_S2_Load(u8 taskId);
static void Task_S2_FadeIn(u8 taskId);
static void Task_S2_SunHold(u8 taskId);
static void Task_S2_RedAnim(u8 taskId);
static void Task_S2_FireAnim(u8 taskId);
static void Task_S2_FadeOut(u8 taskId);

/* -------------------------------------------------------------------------
 * VBlank / Main callbacks - Screen 2
 * ------------------------------------------------------------------------- */
static void VBlankCB_S2(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void MainCB2_S2(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    UpdatePaletteFade();

    /* Same skip convention as MainCB2_CustomIntro - Screen 2 runs on its
     * own main callback so it needs the check duplicated here. */
    if (gMain.newKeys != 0 && !gPaletteFade.active)
        SetMainCallback2(MainCB2_EndCustomIntro);
}

/* -------------------------------------------------------------------------
 * Entry point - Screen 2
 * Called by Task_CI_ShowScreen2 after Screen 1 has fully faded to white.
 * ------------------------------------------------------------------------- */
void CB2_StartCustomIntroScreen2(void)
{
    SetVBlankCallback(NULL);
    SetGpuReg(REG_OFFSET_DISPCNT, 0);
    CpuFill32(0, (void *)VRAM,       VRAM_SIZE);
    CpuFill32(0, (void *)OAM,        OAM_SIZE);
    CpuFill16(0, (void *)(PLTT + 2), PLTT_SIZE - 2);
    ResetPaletteFade();
    ScanlineEffect_Stop();
    ResetTasks();
    ResetSpriteData();
    FreeAllSpritePalettes();
    EnableInterrupts(INTR_FLAG_VBLANK);
    SetVBlankCallback(VBlankCB_S2);
    SetMainCallback2(MainCB2_S2);
    CreateTask(Task_S2_Load, 0);
}

/* -------------------------------------------------------------------------
 * Task: Load - load all assets, create sprites, start fade-in from white
 * ------------------------------------------------------------------------- */
static void Task_S2_Load(u8 taskId)
{
    u8 sunId, redId, fireId;
    u32 i;
    u16 *screen;

    /*
     * VRAM ADDRESS MAP - this is the actual bug behind the persistent
     * checkerboard glitch and wrong Charizard palette.
     *
     * BG charbase units are 0x4000 (16KB) each; BG screenbase units are
     * only 0x800 (2KB) each. The previous version used:
     *   charbase 0 (sky tiles)     -> byte range 0x00000-0x03FFF
     *   charbase 2 (Charizard tiles) -> byte range 0x08000-0x0BFFF
     *   screenbase 16 (sky map)    -> byte range 0x08000-0x087FF
     *   screenbase 20 (zard map)   -> byte range 0x0A000-0x0A7FF
     *
     * screenbase 16 starts at EXACTLY the same address as charbase 2, and
     * screenbase 20 also falls inside charbase 2's range. The sky's and
     * Charizard's own tilemaps were being written directly on top of
     * Charizard's tile graphics - corrupting them. That's what caused both
     * the checkerboard pattern (tilemap bytes being read back as garbled
     * tile graphics) and the wrong palette (the corruption disturbed the
     * palette-slot bits too).
     *
     * Fix: move both tilemaps to screenbase 24 and 28, which live inside
     * charbase 3 - a region we never use for tile graphics. This mirrors
     * the exact safe layout already proven in megarayvision.c
     * (MRV_BG_SCREENBASE = 28, MRV_OVL_SCREENBASE = 24).
     *
     *   charbase 0 (sky tiles)       -> 0x00000-0x03FFF
     *   charbase 2 (Charizard tiles) -> 0x08000-0x0BFFF
     *   screenbase 24 (sky map)      -> 0x0C000-0x0C7FF  (inside unused charbase 3)
     *   screenbase 28 (zard map)     -> 0x0E000-0x0E7FF  (inside unused charbase 3)
     * None of these ranges overlap anymore.
     */

    /* --- BG1: static sky background (priority 3, char base 0, screen base 24) ---
     * bg2.bin.smolTM is a COMPRESSED tilemap (same format as Screen 1's
     * bg1.bin.smolTM, which already works correctly), so it goes through
     * DecompressDataWithHeaderVram, which clears the destination and
     * writes the correct tile entries in one step. */
    DecompressDataWithHeaderVram(sS2Bg_Gfx,     (void *)(BG_CHAR_ADDR(0)));
    DecompressDataWithHeaderVram(sS2Bg_Tilemap, (void *)(BG_SCREEN_ADDR(24)));
    LoadPalette(sS2Bg_Pal, BG_PLTT_ID(0), sizeof(sS2Bg_Pal));

    SetGpuReg(REG_OFFSET_BG1CNT,
              BGCNT_PRIORITY(3)
            | BGCNT_CHARBASE(0)
            | BGCNT_SCREENBASE(24)
            | BGCNT_16COLOR
            | BGCNT_TXT256x256);
    SetGpuReg(REG_OFFSET_BG1HOFS, 0);
    SetGpuReg(REG_OFFSET_BG1VOFS, 0);

    /* --- BG0: Charizard tileset (priority 1, char base 2, screen base 28) ---
     * Charizard palette loaded into BG palette slot 4.
     *
     * chirazard.bin.smolTM is also COMPRESSED, so it likewise goes through
     * DecompressDataWithHeaderVram. After decompression we read the tile
     * entries back out of VRAM (now plain u16 values) and force bits 12-15
     * to palette slot 4, since the exporter bakes in whatever slot it
     * assumed (usually slot 0), not the slot we actually loaded the
     * palette into. Same technique as LoadMRVRayFly / LoadMRVRay in
     * megarayvision.c, applied post-decompression. */
    DecompressDataWithHeaderVram(sS2Char_Gfx,     (void *)(BG_CHAR_ADDR(2)));
    DecompressDataWithHeaderVram(sS2Char_Tilemap, (void *)(BG_SCREEN_ADDR(28)));

    screen = (u16 *)(BG_SCREEN_ADDR(28));
    for (i = 0; i < BG_SCREEN_SIZE / sizeof(u16); i++)
    {
        if (screen[i] & 0x03FF) /* skip blank/transparent tile 0 */
            screen[i] = (screen[i] & 0x0FFF) | (4 << 12); /* keep tile index + flip bits, force palette slot 4 */
    }

    LoadPalette(sS2Char_Pal, BG_PLTT_ID(4), sizeof(sS2Char_Pal));

    SetGpuReg(REG_OFFSET_BG0CNT,
              BGCNT_PRIORITY(1)
            | BGCNT_CHARBASE(2)
            | BGCNT_SCREENBASE(28)
            | BGCNT_16COLOR
            | BGCNT_TXT256x256);
    /* Move Charizard left and down so he no longer covers Red.
     * BG scroll works opposite to sprite positioning: increasing HOFS slides
     * the BG content LEFT on screen; a NEGATIVE VOFS (wrapped as a large
     * u16) slides the content DOWN. Tune these two numbers to taste -
     * larger CHAR_SHIFT_X moves Charizard further left, larger
     * CHAR_SHIFT_Y (more negative) moves him further down. */
    #define CHAR_SHIFT_X  30
    #define CHAR_SHIFT_Y  24
    SetGpuReg(REG_OFFSET_BG0HOFS, CHAR_SHIFT_X);
    SetGpuReg(REG_OFFSET_BG0VOFS, (u16)(-CHAR_SHIFT_Y));

    /* Enable BG0, BG1, and OBJ */
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0
                                 | DISPCNT_OBJ_1D_MAP
                                 | DISPCNT_BG0_ON
                                 | DISPCNT_BG1_ON
                                 | DISPCNT_OBJ_ON);

    /* --- Sprites --- */
    for (i = 0; sS2SpriteSheets[i].tag != 0; i++)
        LoadCompressedSpriteSheet(&sS2SpriteSheets[i]);
    for (i = 0; sS2SpritePalettes[i].tag != 0; i++)
        LoadSpritePalette(&sS2SpritePalettes[i]);

    /* Sun: centred horizontally, upper half of screen, behind Charizard,
     * in front of the sky. Sun's OAM priority is 2 (sS2OamData_Sun), which
     * sits behind BG0/Charizard (priority 1) and in front of BG1/sky
     * (priority 3). */
    sunId = CreateSprite(&sS2SpriteTemplate_Sun, 120, 60, 3);
    gSprites[sunId].data[1] = 768;  /* initial scale: small */
    gSprites[sunId].data[2] = 0;    /* sub-counter for grow */
    gSprites[sunId].invisible = FALSE;
    gTasks[taskId].tS2SunId = sunId;

    /* Red: visible from the very beginning, already at his final position.
     * He plays his reveal animation in place - no movement at all.
     *
     * Tune RED_REST_X (horizontal) and RED_Y (vertical) as needed:
     *   - Increase RED_REST_X  to move Red further right
     *   - Decrease RED_REST_X  to move Red further left
     *   - Increase RED_Y       to move Red lower (closer to Charizard's body)
     *   - Decrease RED_Y       to move Red higher                           */
    #define RED_Y 128   /* vertical position - decrease to move Red up, increase to move down */
    redId = CreateSprite(&sS2SpriteTemplate_Red, RED_REST_X, RED_Y, 1);
    gSprites[redId].invisible = FALSE;   /* visible immediately with the BG */
    gSprites[redId].animPaused = TRUE;   /* hold on frame 0 until Task_S2_SunHold
                                           * explicitly starts the anim - otherwise
                                           * it auto-plays from creation and then
                                           * plays a 2nd time when StartSpriteAnim
                                           * is called later, causing a double run */
    gTasks[taskId].tS2RedId = redId;
    #undef RED_Y

    /* Fire: spawns at Charizard's mouth position.
     *
     * FINE-TUNE HERE if the fire is still off:
     *   Increase FIRE_X  -> move fire right  (toward Charizard's head/mouth)
     *   Decrease FIRE_X  -> move fire left   (toward his tail)
     *   Decrease FIRE_Y  -> move fire up     (mouth is higher than the tail)
     *   Increase FIRE_Y  -> move fire down
     *
     * Charizard's BG0 is scrolled by CHAR_SHIFT_X left and CHAR_SHIFT_Y down,
     * so the fire must compensate by the same amounts to stay aligned.      */
    {
        #define FIRE_X  (148 - CHAR_SHIFT_X)  /* Charizard's mouth: right side, upper area - increase to go further right */
        #define FIRE_Y  ( 38 + CHAR_SHIFT_Y)  /* mouth is near the top of the sprite - decrease to go further up         */

        fireId = CreateSprite(&sS2SpriteTemplate_Fire, FIRE_X, FIRE_Y, 0);
        #undef FIRE_X
        #undef FIRE_Y
    }
    gSprites[fireId].invisible = TRUE;
    gTasks[taskId].tS2FireId = fireId;
    #undef CHAR_SHIFT_X
    #undef CHAR_SHIFT_Y

    /* Palette is already white from Screen 1 fade - fade in from white */
    BeginNormalPaletteFade(PALETTES_ALL, 4, 16, 0, RGB_WHITE);

    gTasks[taskId].tS2Timer = 0;
    gTasks[taskId].func     = Task_S2_FadeIn;
}

/* -------------------------------------------------------------------------
 * Task: FadeIn - wait for fade-in to complete, then let the sun grow alone
 * ------------------------------------------------------------------------- */
static void Task_S2_FadeIn(u8 taskId)
{
    if (gPaletteFade.active)
        return;

    gTasks[taskId].tS2Timer = 0;
    gTasks[taskId].func     = Task_S2_SunHold;
}

/* -------------------------------------------------------------------------
 * Task: SunHold - sun grows by itself (its sprite callback runs every
 * frame regardless of this task) for S2_SUN_HOLD_DUR frames before Red
 * is revealed and starts sliding in. This is what gives the scene its
 * "one beat at a time" pacing instead of everything happening at once.
 * ------------------------------------------------------------------------- */
static void Task_S2_SunHold(u8 taskId)
{
    gTasks[taskId].tS2Timer++;

    if (gTasks[taskId].tS2Timer >= S2_SUN_HOLD_DUR)
    {
        /* Red is already visible - just start his reveal animation now */
        gSprites[gTasks[taskId].tS2RedId].animPaused = FALSE;
        StartSpriteAnim(&gSprites[gTasks[taskId].tS2RedId], 0);

        gTasks[taskId].tS2Timer = 0;
        gTasks[taskId].func     = Task_S2_RedAnim;
    }
}

/* -------------------------------------------------------------------------
 * Task: RedAnim - wait for Red to slide in and finish his reveal anim,
 * then show fire
 * ------------------------------------------------------------------------- */
static void Task_S2_RedAnim(u8 taskId)
{
    gTasks[taskId].tS2Timer++;

    if (gTasks[taskId].tS2Timer >= S2_RED_ANIM_DONE)
    {
        /* Charizard roars, then breathes fire.
         * Cry plays first; SE_M_FLAMETHROWER fires one frame later so the
         * flamethrower sound starts right as the fire sprite becomes visible. */
        PlayCry_ByMode(SPECIES_CHARIZARD, 0, CRY_MODE_NORMAL);
        gSprites[gTasks[taskId].tS2FireId].invisible = FALSE;
        StartSpriteAnim(&gSprites[gTasks[taskId].tS2FireId], 0);
        PlaySE(SE_M_FLAMETHROWER);
        gTasks[taskId].tS2Timer = 0;
        gTasks[taskId].func     = Task_S2_FireAnim;
    }
}

/* -------------------------------------------------------------------------
 * Task: FireAnim - let fire loop for S2_FIRE_DUR ticks, then fade to white
 * ------------------------------------------------------------------------- */
static void Task_S2_FireAnim(u8 taskId)
{
    gTasks[taskId].tS2Timer++;

    if (gTasks[taskId].tS2Timer >= S2_FIRE_DUR)
    {
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_WHITE);
        gTasks[taskId].tS2Timer = 0;
        gTasks[taskId].func     = Task_S2_FadeOut;
    }
}

/* -------------------------------------------------------------------------
 * Task: FadeOut - wait for white fade, then go to title screen
 * ------------------------------------------------------------------------- */
static void Task_S2_FadeOut(u8 taskId)
{
    if (gPaletteFade.active)
        return;

    /* Hand off to Screen 3 - the Team Rocket sequence */
    SetMainCallback2(CB2_StartCustomIntroScreen3);
    DestroyTask(taskId);
}

/* =========================================================================
 * Undefine Screen 2 macros
 * ========================================================================= */
#undef tS2SunId
#undef tS2RedId
#undef tS2FireId
#undef tS2Timer
#undef S2_SUN_HOLD_DUR
#undef S2_RED_ANIM_DONE
#undef S2_FIRE_DUR
#undef RED_REST_X

/* ==========================================================================
 * SCREEN 3 - Team Rocket scene
 *
 * Layout (all sprites, no extra tilesets):
 *   BG1  : bg3 - static Team Rocket HQ backdrop, visible the whole scene.
 *   OBJ  : jessie&james  - centred (x=120), 64x64, 2-frame loop, priority 1
 *          weezing        - right side (x=196), 64x64, 2-frame loop, priority 1
 *          arbok          - right side (x=196), below weezing (y higher value),
 *                           64x64, 2-frame loop, priority 2 (behind weezing)
 *          roses xN       - 16x16 petals drifting left->right across the scene,
 *                           mimicking the PinkLeaves field-weather technique.
 *
 * Timing (~4 s total scene, plus fade-in + fade-out):
 *   All sprites appear simultaneously on fade-in.
 *   Roses drift continuously while everything is visible.
 *   After S3_SCENE_DUR frames, Rocket theme stops, fade to black, scene ends.
 *
 * OBJ VRAM budget:
 *   jessienjames : 64x64 4bpp, 2 frames = 0x1000 bytes  (TAG 5200)
 *   weezing      : 64x64 4bpp, 2 frames = 0x1000 bytes  (TAG 5201)
 *   arbok        : 64x64 4bpp, 2 frames = 0x1000 bytes  (TAG 5202)
 *   roses        : 16x16 4bpp, 4 frames = 0x200  bytes  (TAG 5203)
 *   TOTAL = 0x3200 bytes  - well within the 0x8000 OBJ VRAM limit.
 *
 * Rose movement (adapted from PinkLeaves field-weather effect):
 *   Each petal starts off the LEFT edge, drifts RIGHT at varying speeds,
 *   wraps back to x=-16 when it exits the RIGHT edge. A sine-table wave
 *   is added to y to give a gentle flutter. No gWeatherPtr dependency -
 *   we use sprite->data[] directly, same as the Stars intro effect.
 * ========================================================================== */

/* --------------------------------------------------------------------------
 * Screen 3 tag IDs
 * -------------------------------------------------------------------------- */
#define TAG_S3_JJ     5200
#define TAG_S3_WEEZING 5201
#define TAG_S3_ARBOK  5202
#define TAG_S3_ROSES  5203

/* --------------------------------------------------------------------------
 * Screen 3 assets
 * -------------------------------------------------------------------------- */

/* bg3 - static Team Rocket HQ backdrop */
static const u16 sS3Bg3_Pal[]     = INCBIN_U16("graphics/custom_intro/screen3/bg3.gbapal");
static const u32 sS3Bg3_Gfx[]     = INCBIN_U32("graphics/custom_intro/screen3/bg3.4bpp.smol");
static const u32 sS3Bg3_Tilemap[] = INCBIN_U32("graphics/custom_intro/screen3/bg3.bin.smolTM");

/* Jessie & James - 64x64, 2 frames vertical strip = 0x1000 bytes */
static const u16 sS3JJ_Pal[]      = INCBIN_U16("graphics/custom_intro/screen3/jessienjames.gbapal");
static const u32 sS3JJ_Gfx[]      = INCBIN_U32("graphics/custom_intro/screen3/jessienjames.4bpp.smol");

/* Weezing - 64x64, 2 frames vertical strip = 0x1000 bytes */
static const u16 sS3Weezing_Pal[] = INCBIN_U16("graphics/custom_intro/screen3/weezing.gbapal");
static const u32 sS3Weezing_Gfx[] = INCBIN_U32("graphics/custom_intro/screen3/weezing.4bpp.smol");

/* Arbok - 64x64, 2 frames vertical strip = 0x1000 bytes */
static const u16 sS3Arbok_Pal[]   = INCBIN_U16("graphics/custom_intro/screen3/arbok.gbapal");
static const u32 sS3Arbok_Gfx[]   = INCBIN_U32("graphics/custom_intro/screen3/arbok.4bpp.smol");

/* Roses / petals - 16x16, 4-frame tumble animation = 0x200 bytes */
static const u16 sS3Roses_Pal[]   = INCBIN_U16("graphics/custom_intro/screen3/roses.gbapal");
static const u32 sS3Roses_Gfx[]   = INCBIN_U32("graphics/custom_intro/screen3/roses.4bpp.smol");

/* --------------------------------------------------------------------------
 * Sprite sheet / palette arrays
 * -------------------------------------------------------------------------- */
static const struct CompressedSpriteSheet sS3SpriteSheets[] =
{
    {sS3JJ_Gfx,      0x1000, TAG_S3_JJ},
    {sS3Weezing_Gfx, 0x1000, TAG_S3_WEEZING},
    {sS3Arbok_Gfx,   0x1000, TAG_S3_ARBOK},
    {sS3Roses_Gfx,   0x200,  TAG_S3_ROSES},
    {},
};

static const struct SpritePalette sS3SpritePalettes[] =
{
    {sS3JJ_Pal,      TAG_S3_JJ},       /* OBJ pal slot 0 */
    {sS3Weezing_Pal, TAG_S3_WEEZING},  /* OBJ pal slot 1 */
    {sS3Arbok_Pal,   TAG_S3_ARBOK},    /* OBJ pal slot 2 */
    {sS3Roses_Pal,   TAG_S3_ROSES},    /* OBJ pal slot 3 */
    {},
};

/* --------------------------------------------------------------------------
 * OAM data
 * -------------------------------------------------------------------------- */
static const struct OamData sS3Oam_64x64 =
{
    .y          = DISPLAY_HEIGHT,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode    = ST_OAM_OBJ_NORMAL,
    .bpp        = ST_OAM_4BPP,
    .shape      = SPRITE_SHAPE(64x64),
    .size       = SPRITE_SIZE(64x64),
    .priority   = 1,
};

static const struct OamData sS3Oam_64x64_Behind =
{
    .y          = DISPLAY_HEIGHT,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode    = ST_OAM_OBJ_NORMAL,
    .bpp        = ST_OAM_4BPP,
    .shape      = SPRITE_SHAPE(64x64),
    .size       = SPRITE_SIZE(64x64),
    .priority   = 2,   /* behind weezing */
};

static const struct OamData sS3Oam_16x16 =
{
    .y          = DISPLAY_HEIGHT,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode    = ST_OAM_OBJ_NORMAL,
    .bpp        = ST_OAM_4BPP,
    .shape      = SPRITE_SHAPE(16x16),
    .size       = SPRITE_SIZE(16x16),
    .priority   = 0,   /* in front - petals pass over everything */
};

/* --------------------------------------------------------------------------
 * Animations
 * Offset per frame for 64x64 4bpp: 64 tiles x 0x20 bytes = 0x800 per frame.
 * In ANIMCMD_FRAME the first arg is the tile offset (tiles, not bytes):
 *   frame 0 -> tile offset 0
 *   frame 1 -> tile offset 64
 * -------------------------------------------------------------------------- */

/* Jessie & James - 2-frame loop, 8 ticks each */
static const union AnimCmd sS3Anim_JJ[] =
{
    ANIMCMD_FRAME(0,  8),
    ANIMCMD_FRAME(64, 8),
    ANIMCMD_JUMP(0),
};
static const union AnimCmd *const sS3Anims_JJ[] = { sS3Anim_JJ };

/* Weezing - 2-frame loop */
static const union AnimCmd sS3Anim_Weezing[] =
{
    ANIMCMD_FRAME(0,  10),
    ANIMCMD_FRAME(64, 10),
    ANIMCMD_JUMP(0),
};
static const union AnimCmd *const sS3Anims_Weezing[] = { sS3Anim_Weezing };

/* Arbok - 2-frame loop */
static const union AnimCmd sS3Anim_Arbok[] =
{
    ANIMCMD_FRAME(0,  10),
    ANIMCMD_FRAME(64, 10),
    ANIMCMD_JUMP(0),
};
static const union AnimCmd *const sS3Anims_Arbok[] = { sS3Anim_Arbok };

/*
 * Roses / petals - 4-frame tumble loop.
 * 16x16 sprite: each frame = 4 tiles, tile offset per frame = 4.
 * Three animation phases with different start offsets (same technique as
 * PinkLeaves) so not every petal looks identical.
 */
static const union AnimCmd sS3Anim_Roses0[] =
{
    ANIMCMD_FRAME(0,  10),
    ANIMCMD_FRAME(4,  10),
    ANIMCMD_FRAME(8,  10),
    ANIMCMD_FRAME(12, 10),
    ANIMCMD_JUMP(0),
};
static const union AnimCmd sS3Anim_Roses1[] =
{
    ANIMCMD_FRAME(4,  10),
    ANIMCMD_FRAME(8,  10),
    ANIMCMD_FRAME(12, 10),
    ANIMCMD_FRAME(0,  10),
    ANIMCMD_JUMP(0),
};
static const union AnimCmd sS3Anim_Roses2[] =
{
    ANIMCMD_FRAME(8,  10),
    ANIMCMD_FRAME(12, 10),
    ANIMCMD_FRAME(0,  10),
    ANIMCMD_FRAME(4,  10),
    ANIMCMD_JUMP(0),
};
static const union AnimCmd *const sS3Anims_Roses[] =
{
    sS3Anim_Roses0,
    sS3Anim_Roses1,
    sS3Anim_Roses2,
};

/* --------------------------------------------------------------------------
 * Sprite callbacks - forward declarations
 * -------------------------------------------------------------------------- */
static void SpriteCB_S3_Loop(struct Sprite *sprite);   /* does nothing, engine loops */
static void SpriteCB_S3_Rose(struct Sprite *sprite);

/* --------------------------------------------------------------------------
 * Sprite templates
 * -------------------------------------------------------------------------- */
static const struct SpriteTemplate sS3Template_JJ =
{
    .tileTag    = TAG_S3_JJ,
    .paletteTag = TAG_S3_JJ,
    .oam        = &sS3Oam_64x64,
    .anims      = sS3Anims_JJ,
    .callback   = SpriteCB_S3_Loop,
};

static const struct SpriteTemplate sS3Template_Weezing =
{
    .tileTag    = TAG_S3_WEEZING,
    .paletteTag = TAG_S3_WEEZING,
    .oam        = &sS3Oam_64x64,
    .anims      = sS3Anims_Weezing,
    .callback   = SpriteCB_S3_Loop,
};

static const struct SpriteTemplate sS3Template_Arbok =
{
    .tileTag    = TAG_S3_ARBOK,
    .paletteTag = TAG_S3_ARBOK,
    .oam        = &sS3Oam_64x64_Behind,
    .anims      = sS3Anims_Arbok,
    .callback   = SpriteCB_S3_Loop,
};

static const struct SpriteTemplate sS3Template_Rose =
{
    .tileTag    = TAG_S3_ROSES,
    .paletteTag = TAG_S3_ROSES,
    .oam        = &sS3Oam_16x16,
    .anims      = sS3Anims_Roses,
    .callback   = SpriteCB_S3_Rose,
};

/* --------------------------------------------------------------------------
 * Timing / tuning constants
 * -------------------------------------------------------------------------- */
#define S3_SCENE_DUR    240   /* ~4 s at 60 fps - time all sprites are visible */
#define S3_NUM_ROSES    8     /* number of petal sprites in flight at once     */

/* --------------------------------------------------------------------------
 * Task data aliases
 * -------------------------------------------------------------------------- */
#define tS3Timer data[0]

/* --------------------------------------------------------------------------
 * Sprite data aliases (roses only)
 *   rPosY    - y position in Q7 fixed-point (128 = 1 pixel)
 *   rDeltaY  - vertical drift speed in Q7 units per frame
 *   rWaveIdx - index into gSineTable for horizontal flutter
 *   rWaveDlt - how fast the wave index advances
 *   rSpeedX  - horizontal drift speed in pixels per frame (positive = right)
 * -------------------------------------------------------------------------- */
#define rPosY    data[0]
#define rDeltaY  data[1]
#define rWaveIdx data[2]
#define rWaveDlt data[3]
#define rSpeedX  data[4]

/* --------------------------------------------------------------------------
 * Forward declarations
 * -------------------------------------------------------------------------- */
static void Task_S3_FadeIn(u8 taskId);
static void Task_S3_Hold(u8 taskId);
static void Task_S3_FadeOut(u8 taskId);

/* --------------------------------------------------------------------------
 * Sprite callbacks
 * -------------------------------------------------------------------------- */
static void SpriteCB_S3_Loop(struct Sprite *sprite) { (void)sprite; }

static void SpriteCB_S3_Rose(struct Sprite *sprite)
{
    /* Vertical gentle drift */
    sprite->rPosY += sprite->rDeltaY;
    sprite->y = (s16)(sprite->rPosY / 128);

    /* Horizontal sine flutter around the main drift */
    sprite->rWaveIdx = (sprite->rWaveIdx + sprite->rWaveDlt) & 0xFF;
    sprite->x2 = gSineTable[sprite->rWaveIdx] / 96;

    /* Drift horizontally to the right */
    sprite->x += sprite->rSpeedX;

    /* Wrap: once off the right edge, reset to the left */
    if (sprite->x > DISPLAY_WIDTH + 8)
        sprite->x = -16;
}

/* --------------------------------------------------------------------------
 * Helper: spawn one rose sprite with randomised position and speed
 * -------------------------------------------------------------------------- */
static void S3_SpawnRose(u8 index)
{
    u16 rand   = Random();
    u8  sprId;
    s16 startX;
    s16 startY;

    /* Spread start X evenly across the screen, offset by random within slot */
    startX = (s16)(index * (DISPLAY_WIDTH / S3_NUM_ROSES)) - 8 + (rand & 0xF);
    startY = (s16)(16 + (rand & 0x3F));   /* random y in upper 2/3 of screen */

    sprId = CreateSprite(&sS3Template_Rose, startX, startY, 0);
    if (sprId == MAX_SPRITES)
        return;

    gSprites[sprId].rPosY    = startY * 128;
    gSprites[sprId].rDeltaY  = 16 + (rand & 0xF);        /* fall speed: ~0.1–0.2 px/frame */
    gSprites[sprId].rWaveIdx = (rand & 0xFF);
    gSprites[sprId].rWaveDlt = 1 + ((rand >> 8) & 1);    /* wave speed: 1 or 2 */
    gSprites[sprId].rSpeedX  = 1 + ((rand >> 4) & 1);    /* horizontal: 1 or 2 px/frame */
    StartSpriteAnim(&gSprites[sprId], rand % 3);
}

/* --------------------------------------------------------------------------
 * Entry point for Screen 3
 * -------------------------------------------------------------------------- */
void CB2_StartCustomIntroScreen3(void)
{
    u8 taskId, i;

    /* Full hardware reset */
    SetVBlankCallback(NULL);
    SetGpuReg(REG_OFFSET_DISPCNT, 0);
    CpuFill32(0, (void *)VRAM,       VRAM_SIZE);
    CpuFill32(0, (void *)OAM,        OAM_SIZE);
    CpuFill16(0, (void *)(PLTT + 2), PLTT_SIZE - 2);
    ResetPaletteFade();
    ScanlineEffect_Stop();
    ResetTasks();
    ResetSpriteData();
    FreeAllSpritePalettes();
    EnableInterrupts(INTR_FLAG_VBLANK);
    SetVBlankCallback(VBlankCB_CustomIntro);
    SetMainCallback2(MainCB2_CustomIntro);

    /* ---- Background: bg3 static backdrop ---- */
    DecompressDataWithHeaderVram(sS3Bg3_Gfx,     (void *)(BG_CHAR_ADDR(0)));
    DecompressDataWithHeaderVram(sS3Bg3_Tilemap, (void *)(BG_SCREEN_ADDR(16)));
    LoadPalette(sS3Bg3_Pal, BG_PLTT_ID(0), sizeof(sS3Bg3_Pal));

    SetGpuReg(REG_OFFSET_BG1CNT,
              BGCNT_PRIORITY(3)
            | BGCNT_CHARBASE(0)
            | BGCNT_SCREENBASE(16)
            | BGCNT_16COLOR
            | BGCNT_TXT256x256);
    SetGpuReg(REG_OFFSET_BG1HOFS, 0);
    SetGpuReg(REG_OFFSET_BG1VOFS, 0);

    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0
                                 | DISPCNT_OBJ_1D_MAP
                                 | DISPCNT_BG1_ON
                                 | DISPCNT_OBJ_ON);

    /* ---- Sprites ---- */
    for (i = 0; sS3SpriteSheets[i].tag != 0; i++)
        LoadCompressedSpriteSheet(&sS3SpriteSheets[i]);
    for (i = 0; sS3SpritePalettes[i].tag != 0; i++)
        LoadSpritePalette(&sS3SpritePalettes[i]);

    /* Jessie & James - centred horizontally (sprite centre at x=120) */
    {
        u8 jjId = CreateSprite(&sS3Template_JJ, 120, 128, 1);
        StartSpriteAnim(&gSprites[jjId], 0);
    }

    /* Weezing - right side, above Arbok.
     * y=88 puts it in the upper-right area of the scene. */
    {
        u8 wzId = CreateSprite(&sS3Template_Weezing, 196, 88, 1);
        StartSpriteAnim(&gSprites[wzId], 0);
    }

    /* Arbok - right side, below Weezing, behind it (priority 2).
     * y=136 places it at the lower-right, partially behind Weezing. */
    {
        u8 abId = CreateSprite(&sS3Template_Arbok, 196, 136, 2);
        StartSpriteAnim(&gSprites[abId], 0);
    }

    /* Roses - spawn all petals at staggered positions */
    for (i = 0; i < S3_NUM_ROSES; i++)
        S3_SpawnRose(i);


    /* Fade in from black */
    BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);

    taskId = CreateTask(Task_S3_FadeIn, 0);
    gTasks[taskId].tS3Timer = 0;
}

/* --------------------------------------------------------------------------
 * Task: FadeIn - wait for fade-in, then start the scene hold timer
 * -------------------------------------------------------------------------- */
static void Task_S3_FadeIn(u8 taskId)
{
    if (gPaletteFade.active)
        return;

    PlayCryInternal(SPECIES_MEOWTH, 0, 120, CRY_PRIORITY_NORMAL, CRY_MODE_NORMAL);

    gTasks[taskId].tS3Timer = 0;
    gTasks[taskId].func     = Task_S3_Hold;
}

/* --------------------------------------------------------------------------
 * Task: Hold - let sprites animate and roses drift for S3_SCENE_DUR frames
 * -------------------------------------------------------------------------- */
static void Task_S3_Hold(u8 taskId)
{
    gTasks[taskId].tS3Timer++;
    if (gTasks[taskId].tS3Timer >= S3_SCENE_DUR)
    {
        gTasks[taskId].tS3Timer = 0;
        gTasks[taskId].func     = Task_S3_FadeOut;
    }
}

/* --------------------------------------------------------------------------
 * Task: FadeOut - fade to black and end the intro
 * -------------------------------------------------------------------------- */
static void Task_S3_FadeOut(u8 taskId)
{
    if (gTasks[taskId].tS3Timer == 0)
    {
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].tS3Timer = 1;
        return;
    }

    if (!gPaletteFade.active)
    {
        SetMainCallback2(CB2_StartCustomIntroScreen4);
        DestroyTask(taskId);
    }
}

/* =========================================================================
 * Undefine Screen 3 macros
 * ========================================================================= */
#undef TAG_S3_JJ
#undef TAG_S3_WEEZING
#undef TAG_S3_ARBOK
#undef TAG_S3_ROSES
#undef S3_SCENE_DUR
#undef S3_NUM_ROSES
#undef tS3Timer
#undef rPosY
#undef rDeltaY
#undef rWaveIdx
#undef rWaveDlt
#undef rSpeedX

/* ==========================================================================
 * SCREEN 4 - Legendary Birds (V-formation) + Mewtwo/Mew
 *
 * Part A  (bg4 - sky backdrop):
 *   The three legendary birds fly across the screen in a V-formation,
 *   left->right.  When all three are off the right edge they reset, flip
 *   horizontally and fly back right->left.  After two full passes,
 *   PlayCry for each bird, fade to black.
 *
 * Part B  (bg5 - darker sky or alternate backdrop):
 *   Mewtwo rises slowly from the bottom-centre, PlayCry(MEWTWO).
 *   Mew orbits around Mewtwo using a sine/cosine circle.
 *   PlayCry(MEW) once Mew has completed one full orbit.
 *   Hold for a beat, then fade to black -> CB2_InitTitleScreen.
 *
 * V-formation positions (centre = Articuno, leader):
 *   Articuno : x offset  0, y offset  0   (tip of the V)
 *   Zapdos   : x offset -48, y offset +28  (left wing)
 *   Moltres  : x offset +48, y offset +28  (right wing)
 *
 * OBJ VRAM budget for Screen 4:
 *   articuno  4 frames 64x64 4bpp = 0x2000
 *   zapdos    4 frames 64x64 4bpp = 0x2000
 *   moltres   4 frames 64x64 4bpp = 0x2000
 *   mewtwo    2 frames 64x64 4bpp = 0x1000
 *   mew       2 frames 64x64 4bpp = 0x1000
 *   TOTAL = 0x8000  - exactly at the OBJ VRAM limit.
 * ========================================================================== */

/* --------------------------------------------------------------------------
 * Screen 4 tag IDs
 * -------------------------------------------------------------------------- */
#define TAG_S4_ARTICUNO 5300
#define TAG_S4_ZAPDOS   5301
#define TAG_S4_MOLTRES  5302
#define TAG_S4_MEWTWO   5303
#define TAG_S4_MEW      5304

/* --------------------------------------------------------------------------
 * Screen 4 assets
 * -------------------------------------------------------------------------- */

/* bg4 - sky backdrop for bird flight */
static const u16 sS4Bg4_Pal[]       = INCBIN_U16("graphics/custom_intro/screen4/bg4.gbapal");
static const u32 sS4Bg4_Gfx[]       = INCBIN_U32("graphics/custom_intro/screen4/bg4.4bpp.smol");
static const u32 sS4Bg4_Tilemap[]   = INCBIN_U32("graphics/custom_intro/screen4/bg4.bin.smolTM");

/* bg5 - backdrop for Mewtwo/Mew scene */
static const u16 sS4Bg5_Pal[]       = INCBIN_U16("graphics/custom_intro/screen4/bg5.gbapal");
static const u32 sS4Bg5_Gfx[]       = INCBIN_U32("graphics/custom_intro/screen4/bg5.4bpp.smol");
static const u32 sS4Bg5_Tilemap[]   = INCBIN_U32("graphics/custom_intro/screen4/bg5.bin.smolTM");

/* Bird sprites - 64x64, 4-frame vertical strips = 0x2000 bytes each */
static const u16 sS4Articuno_Pal[]  = INCBIN_U16("graphics/custom_intro/screen4/articuno.gbapal");
static const u32 sS4Articuno_Gfx[]  = INCBIN_U32("graphics/custom_intro/screen4/articuno.4bpp.smol");

static const u16 sS4Zapdos_Pal[]    = INCBIN_U16("graphics/custom_intro/screen4/zapdos.gbapal");
static const u32 sS4Zapdos_Gfx[]    = INCBIN_U32("graphics/custom_intro/screen4/zapdos.4bpp.smol");

static const u16 sS4Moltres_Pal[]   = INCBIN_U16("graphics/custom_intro/screen4/moltres.gbapal");
static const u32 sS4Moltres_Gfx[]   = INCBIN_U32("graphics/custom_intro/screen4/moltres.4bpp.smol");

/* Mewtwo - 64x64, 2-frame vertical strip = 0x1000 bytes */
static const u16 sS4Mewtwo_Pal[]    = INCBIN_U16("graphics/custom_intro/screen4/mewtwo.gbapal");
static const u32 sS4Mewtwo_Gfx[]    = INCBIN_U32("graphics/custom_intro/screen4/mewtwo.4bpp.smol");

/* Mew - 64x64, 2-frame vertical strip = 0x1000 bytes */
static const u16 sS4Mew_Pal[]       = INCBIN_U16("graphics/custom_intro/screen4/mew.gbapal");
static const u32 sS4Mew_Gfx[]       = INCBIN_U32("graphics/custom_intro/screen4/mew.4bpp.smol");

/* --------------------------------------------------------------------------
 * Sprite sheets / palettes
 * -------------------------------------------------------------------------- */
static const struct CompressedSpriteSheet sS4SpriteSheets_Birds[] =
{
    {sS4Articuno_Gfx, 0x2000, TAG_S4_ARTICUNO},
    {sS4Zapdos_Gfx,  0x2000, TAG_S4_ZAPDOS},
    {sS4Moltres_Gfx, 0x2000, TAG_S4_MOLTRES},
    {},
};
static const struct SpritePalette sS4SpritePalettes_Birds[] =
{
    {sS4Articuno_Pal, TAG_S4_ARTICUNO},
    {sS4Zapdos_Pal,   TAG_S4_ZAPDOS},
    {sS4Moltres_Pal,  TAG_S4_MOLTRES},
    {},
};

static const struct CompressedSpriteSheet sS4SpriteSheets_Psychic[] =
{
    {sS4Mewtwo_Gfx, 0x2000, TAG_S4_MEWTWO},   /* 4 frames x 64x64 4bpp = 0x2000 */
    {sS4Mew_Gfx,   0x2000, TAG_S4_MEW},
    {},
};
static const struct SpritePalette sS4SpritePalettes_Psychic[] =
{
    {sS4Mewtwo_Pal, TAG_S4_MEWTWO},
    {sS4Mew_Pal,    TAG_S4_MEW},
    {},
};

/* --------------------------------------------------------------------------
 * OAM data
 * -------------------------------------------------------------------------- */
static const struct OamData sS4Oam_64x64 =
{
    .y          = DISPLAY_HEIGHT,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode    = ST_OAM_OBJ_NORMAL,
    .bpp        = ST_OAM_4BPP,
    .shape      = SPRITE_SHAPE(64x64),
    .size       = SPRITE_SIZE(64x64),
    .priority   = 1,
};

/* --------------------------------------------------------------------------
 * Animations
 * Birds: 4-frame loop, 8 ticks per frame (matches Celebi pattern)
 * Mewtwo/Mew: 2-frame loop, 10 ticks per frame
 * -------------------------------------------------------------------------- */
static const union AnimCmd sS4Anim_Bird[] =
{
    ANIMCMD_FRAME(0,   8),
    ANIMCMD_FRAME(64,  8),
    ANIMCMD_FRAME(128, 8),
    ANIMCMD_FRAME(192, 8),
    ANIMCMD_JUMP(0),
};
static const union AnimCmd *const sS4Anims_Bird[] = { sS4Anim_Bird };

/* Mewtwo - anim 0: idle loop (frames 1-2, ticks 10 each) */
static const union AnimCmd sS4Anim_Mewtwo_Idle[] =
{
    ANIMCMD_FRAME(0,   10),
    ANIMCMD_FRAME(64,  10),
    ANIMCMD_JUMP(0),
};

/* Mewtwo - anim 1: settled loop (frames 3-4) used once Mew arrives */
static const union AnimCmd sS4Anim_Mewtwo_Settled[] =
{
    ANIMCMD_FRAME(128, 10),
    ANIMCMD_FRAME(192, 10),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd *const sS4Anims_Mewtwo[] =
{
    sS4Anim_Mewtwo_Idle,      /* anim 0 - Mewtwo idle (before Mew arrives) */
    sS4Anim_Mewtwo_Settled,   /* anim 1 - Mewtwo settled (Mew orbiting)    */
};

/* Mew - anim 0: fly-up loop (frames 1-2) while approaching Mewtwo */
static const union AnimCmd sS4Anim_Mew_Fly[] =
{
    ANIMCMD_FRAME(0,   8),
    ANIMCMD_FRAME(64,  8),
    ANIMCMD_JUMP(0),
};

/* Mew - anim 1: orbit loop (frames 3-4) once it reaches Mewtwo */
static const union AnimCmd sS4Anim_Mew_Orbit[] =
{
    ANIMCMD_FRAME(128, 8),
    ANIMCMD_FRAME(192, 8),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd *const sS4Anims_Mew[] =
{
    sS4Anim_Mew_Fly,    /* anim 0 - flying up toward Mewtwo */
    sS4Anim_Mew_Orbit,  /* anim 1 - orbiting around Mewtwo  */
};

/* --------------------------------------------------------------------------
 * Sprite callbacks - forward declarations
 * -------------------------------------------------------------------------- */
static void SpriteCB_S4_Bird(struct Sprite *sprite);
static void SpriteCB_S4_Mewtwo(struct Sprite *sprite);
static void SpriteCB_S4_Mew_FlyUp(struct Sprite *sprite);
static void SpriteCB_S4_Mew_Orbit(struct Sprite *sprite);

/* --------------------------------------------------------------------------
 * Sprite templates
 * -------------------------------------------------------------------------- */
static const struct SpriteTemplate sS4Template_Articuno =
{
    .tileTag    = TAG_S4_ARTICUNO,
    .paletteTag = TAG_S4_ARTICUNO,
    .oam        = &sS4Oam_64x64,
    .anims      = sS4Anims_Bird,
    .callback   = SpriteCB_S4_Bird,
};
static const struct SpriteTemplate sS4Template_Zapdos =
{
    .tileTag    = TAG_S4_ZAPDOS,
    .paletteTag = TAG_S4_ZAPDOS,
    .oam        = &sS4Oam_64x64,
    .anims      = sS4Anims_Bird,
    .callback   = SpriteCB_S4_Bird,
};
static const struct SpriteTemplate sS4Template_Moltres =
{
    .tileTag    = TAG_S4_MOLTRES,
    .paletteTag = TAG_S4_MOLTRES,
    .oam        = &sS4Oam_64x64,
    .anims      = sS4Anims_Bird,
    .callback   = SpriteCB_S4_Bird,
};
static const struct SpriteTemplate sS4Template_Mewtwo =
{
    .tileTag    = TAG_S4_MEWTWO,
    .paletteTag = TAG_S4_MEWTWO,
    .oam        = &sS4Oam_64x64,
    .anims      = sS4Anims_Mewtwo,
    .callback   = SpriteCB_S4_Mewtwo,
};
static const struct SpriteTemplate sS4Template_Mew =
{
    .tileTag    = TAG_S4_MEW,
    .paletteTag = TAG_S4_MEW,
    .oam        = &sS4Oam_64x64,
    .anims      = sS4Anims_Mew,
    .callback   = SpriteCB_S4_Mew_FlyUp,  /* starts flying up; task switches to Orbit */
};

/* --------------------------------------------------------------------------
 * Timing constants
 * -------------------------------------------------------------------------- */
#define S4_BIRD_SPEED       3    /* px/frame each bird moves horizontally              */
#define S4_MEW_ORBIT_R      50   /* radius of Mew's circular orbit around Mewtwo       */
#define S4_MEW_FLY_SPEED    3    /* px/frame Mew rises toward Mewtwo                   */
#define S4_MEW_ARRIVE_DIST  40   /* px distance from Mewtwo centre to trigger switch   */
#define S4_MEWTWO_Y         80   /* fixed Y where Mewtwo sits (centre of screen)       */
#define S4_HOLD_FRAMES      60   /* frames to hold after Mew's cry before fade-out     */

#define S4_DELAY_PRE_PASS   40   /* ~0.7 s before first V-formation enters             */
#define S4_DELAY_BETWEEN    50   /* ~0.8 s gap between LTR and RTL passes             */

/* --------------------------------------------------------------------------
 * Task data aliases (Screen 4)
 *   tS4Timer  - general frame counter
 *   tS4State  - sub-state within the current task
 *   tS4ArtId  - sprite index of Articuno (V-tip / later reused for Mewtwo)
 *   tS4ZapId  - sprite index of Zapdos   (later reused for Mew)
 *   tS4MolId  - sprite index of Moltres
 *   tS4Dir    - current flight direction: 0=left->right, 1=right->left
 *   tS4Pass   - how many individual passes have been COMPLETED (0,1,2)
 * -------------------------------------------------------------------------- */
#define tS4Timer  data[0]
#define tS4State  data[1]
#define tS4ArtId  data[2]
#define tS4ZapId  data[3]
#define tS4MolId  data[4]
#define tS4Dir    data[5]
#define tS4Pass   data[6]

/* Sprite data aliases (Mew orbit) */
#define sMewAngle  data[0]   /* current orbit angle, 0–255 (full circle)          */
#define sMewCentX  data[1]   /* x of Mewtwo sprite (updated each frame)           */
#define sMewCentY  data[2]   /* y of Mewtwo sprite (updated each frame)           */

/* --------------------------------------------------------------------------
 * Forward declarations for Screen 4 tasks
 * -------------------------------------------------------------------------- */
static void Task_S4_FadeIn(u8 taskId);
static void Task_S4_PrePassDelay(u8 taskId);
static void Task_S4_SpawnAndFly(u8 taskId);
static void Task_S4_WaitClear(u8 taskId);
static void Task_S4_BetweenDelay(u8 taskId);
static void Task_S4_BirdCries(u8 taskId);
static void Task_S4_FadeToMewtwo(u8 taskId);
static void Task_S4_PreMewDelay(u8 taskId);
static void Task_S4_MewtwoScene(u8 taskId);
static void Task_S4_FadeOut(u8 taskId);

/* --------------------------------------------------------------------------
 * Sprite callbacks
 * -------------------------------------------------------------------------- */

/*
 * SpriteCB_S4_Bird
 * Moves the bird horizontally at S4_BIRD_SPEED (stored in data[0] as signed).
 * A gentle vertical bob adds life to the flight.
 */
static void SpriteCB_S4_Bird(struct Sprite *sprite)
{
    sprite->x += (s8)sprite->data[0];  /* data[0] = +speed (LTR) or -speed (RTL) */
    if (++sprite->data[1] % 12 == 0)
        sprite->y2 = (sprite->y2 == 0) ? -1 : 0;  /* subtle 1px bob */
}

/*
 * SpriteCB_S4_Mewtwo
 * Mewtwo rises slowly from below until it reaches S4_MEWTWO_REST_Y.
 */
/*
 * SpriteCB_S4_Mewtwo
 * Mewtwo is stationary at S4_MEWTWO_Y - no movement, just loops its anim.
 * The task switches to anim 1 (settled) when Mew arrives.
 */
static void SpriteCB_S4_Mewtwo(struct Sprite *sprite)
{
    (void)sprite;  /* stationary - engine handles the anim loop */
}

/*
 * SpriteCB_S4_Mew_FlyUp
 * Mew rises vertically (anim 0, frames 1-2) until Task_S4_MewtwoScene
 * detects it is within S4_MEW_ARRIVE_DIST px of Mewtwo and switches both
 * sprites to their orbit anims + changes this callback to SpriteCB_S4_Mew_Orbit.
 */
static void SpriteCB_S4_Mew_FlyUp(struct Sprite *sprite)
{
    sprite->y -= S4_MEW_FLY_SPEED;
}

/*
 * SpriteCB_S4_Mew_Orbit
 * Mew orbits Mewtwo (anim 1, frames 3-4).
 * Centre stored in sMewCentX/Y, updated each frame by Task_S4_MewtwoScene.
 * sMewAngle advances 2 units/frame -> full orbit in 128 frames (~2.1 s).
 *
 * 3-D priority swap:
 *   angle 0–127  (front half) -> subpriority 0  in front of Mewtwo
 *   angle 128–255 (back half) -> subpriority 2  behind Mewtwo
 * Mewtwo sits at subpriority 1.
 */
static void SpriteCB_S4_Mew_Orbit(struct Sprite *sprite)
{
    s16 cx = (s16)sprite->sMewCentX;
    s16 cy = (s16)sprite->sMewCentY;
    s16 prevX = sprite->x;

    sprite->sMewAngle = (sprite->sMewAngle + 2) & 0xFF;

    sprite->x = cx + (s16)(S4_MEW_ORBIT_R * gSineTable[sprite->sMewAngle]               / 256);
    sprite->y = cy + (s16)(S4_MEW_ORBIT_R * gSineTable[(sprite->sMewAngle + 64) & 0xFF] / 256);

    /* Face the direction of horizontal travel. Skip the update at the two
     * turning points (leftmost/rightmost, dx == 0) so Mew doesn't flicker -
     * it just keeps facing whichever way it was already going.
     * NOTE: if Mew looks backwards in-game, the source art's default facing
     * is the opposite of what's assumed here - just swap FALSE/TRUE below. */
    if (sprite->x > prevX)
        sprite->hFlip = FALSE;   /* moving right -> face right */
    else if (sprite->x < prevX)
        sprite->hFlip = TRUE;    /* moving left  -> face left  */

    sprite->subpriority = (sprite->sMewAngle < 128) ? 0 : 2;
}

/* --------------------------------------------------------------------------
 * Internal helper: destroy the three bird sprites cleanly
 * -------------------------------------------------------------------------- */
static void S4_DestroyBirds(u8 taskId)
{
    DestroySprite(&gSprites[(u8)gTasks[taskId].tS4ArtId]);
    DestroySprite(&gSprites[(u8)gTasks[taskId].tS4ZapId]);
    DestroySprite(&gSprites[(u8)gTasks[taskId].tS4MolId]);
}

/* --------------------------------------------------------------------------
 * Internal helper: spawn the V-formation.
 *   dir 0 = left->right  (Articuno tip leads, wings trail behind to the left)
 *   dir 1 = right->left  (formation is mirrored horizontally)
 *
 * V-formation geometry (LTR, ">" shape facing right):
 *   Articuno  - tip: enters from the far-left edge, vertically centred
 *   Zapdos    - upper-left wing: 56 px behind Articuno, 28 px above
 *   Moltres   - lower-left wing: 56 px behind Articuno, 28 px below
 *
 * For RTL the formation is hFlip'd and directions/offsets are mirrored.
 * -------------------------------------------------------------------------- */
static void S4_SpawnBirds(u8 taskId, u8 dir)
{
    u8 artId, zapId, molId;
    s16 speed = (dir == 0) ? S4_BIRD_SPEED : -S4_BIRD_SPEED;

    /* tip X: starts just off the entry edge so no pop-in */
    s16 tipX  = (dir == 0) ? -64 : (s16)(DISPLAY_WIDTH + 64);

    /* wing offset: wings trail BEHIND the tip (opposite to movement direction) */
    s16 wOff  = (dir == 0) ? -56 : 56;   /* negative = left of tip for LTR */

    /* Articuno - tip of the V */
    artId = CreateSprite(&sS4Template_Articuno, tipX,        60, 1);
    gSprites[artId].hFlip    = (dir == 1);
    gSprites[artId].data[0]  = (s8)speed;
    StartSpriteAnim(&gSprites[artId], 0);

    /* Zapdos - upper wing */
    zapId = CreateSprite(&sS4Template_Zapdos,   tipX + wOff, 60 - 28, 1);
    gSprites[zapId].hFlip    = (dir == 1);
    gSprites[zapId].data[0]  = (s8)speed;
    StartSpriteAnim(&gSprites[zapId], 0);

    /* Moltres - lower wing */
    molId = CreateSprite(&sS4Template_Moltres,  tipX + wOff, 60 + 28, 1);
    gSprites[molId].hFlip    = (dir == 1);
    gSprites[molId].data[0]  = (s8)speed;
    StartSpriteAnim(&gSprites[molId], 0);

    gTasks[taskId].tS4ArtId = artId;
    gTasks[taskId].tS4ZapId = zapId;
    gTasks[taskId].tS4MolId = molId;
    gTasks[taskId].tS4Dir   = dir;
}

/* --------------------------------------------------------------------------
 * Entry point for Screen 4
 * -------------------------------------------------------------------------- */
void CB2_StartCustomIntroScreen4(void)
{
    u8 taskId, i;

    SetVBlankCallback(NULL);
    SetGpuReg(REG_OFFSET_DISPCNT, 0);
    CpuFill32(0, (void *)VRAM,       VRAM_SIZE);
    CpuFill32(0, (void *)OAM,        OAM_SIZE);
    CpuFill16(0, (void *)(PLTT + 2), PLTT_SIZE - 2);
    ResetPaletteFade();
    ScanlineEffect_Stop();
    ResetTasks();
    ResetSpriteData();
    FreeAllSpritePalettes();
    EnableInterrupts(INTR_FLAG_VBLANK);
    SetVBlankCallback(VBlankCB_CustomIntro);
    SetMainCallback2(MainCB2_CustomIntro);

    /* bg4 - sky backdrop for the bird scene */
    DecompressDataWithHeaderVram(sS4Bg4_Gfx,     (void *)(BG_CHAR_ADDR(0)));
    DecompressDataWithHeaderVram(sS4Bg4_Tilemap, (void *)(BG_SCREEN_ADDR(16)));
    LoadPalette(sS4Bg4_Pal, BG_PLTT_ID(0), sizeof(sS4Bg4_Pal));

    SetGpuReg(REG_OFFSET_BG1CNT,
              BGCNT_PRIORITY(3)
            | BGCNT_CHARBASE(0)
            | BGCNT_SCREENBASE(16)
            | BGCNT_16COLOR
            | BGCNT_TXT256x256);
    SetGpuReg(REG_OFFSET_BG1HOFS, 0);
    SetGpuReg(REG_OFFSET_BG1VOFS, 0);

    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0
                                 | DISPCNT_OBJ_1D_MAP
                                 | DISPCNT_BG1_ON
                                 | DISPCNT_OBJ_ON);

    /* Load bird sprite sheets and palettes */
    for (i = 0; sS4SpriteSheets_Birds[i].tag != 0; i++)
        LoadCompressedSpriteSheet(&sS4SpriteSheets_Birds[i]);
    for (i = 0; sS4SpritePalettes_Birds[i].tag != 0; i++)
        LoadSpritePalette(&sS4SpritePalettes_Birds[i]);

    taskId = CreateTask(Task_S4_FadeIn, 0);
    gTasks[taskId].tS4Timer = 0;
    gTasks[taskId].tS4Pass  = 0;
    /* Do NOT spawn birds yet - they are created after the pre-pass delay */

    BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
}

/* --------------------------------------------------------------------------
 * Task: FadeIn - wait for bg4 to fade in, then enter the pre-pass delay
 * -------------------------------------------------------------------------- */
static void Task_S4_FadeIn(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        gTasks[taskId].tS4Timer = 0;
        gTasks[taskId].func     = Task_S4_PrePassDelay;
    }
}

/* --------------------------------------------------------------------------
 * Task: PrePassDelay
 * Holds on the empty sky for S4_DELAY_PRE_PASS frames before the first
 * V-formation enters.  Adds dramatic tension before the birds appear.
 * -------------------------------------------------------------------------- */
static void Task_S4_PrePassDelay(u8 taskId)
{
    gTasks[taskId].tS4Timer++;

    if (gTasks[taskId].tS4Timer >= S4_DELAY_PRE_PASS)
    {
        /* Spawn the first formation (LTR) and start watching for it to clear */
        S4_SpawnBirds(taskId, 0);   /* dir 0 = left->right */
        gTasks[taskId].tS4Timer = 0;
        gTasks[taskId].func     = Task_S4_WaitClear;
    }
}

/* --------------------------------------------------------------------------
 * Task: SpawnAndFly (RTL second pass - entered from BetweenDelay)
 * Spawns the mirrored formation and falls into WaitClear.
 * -------------------------------------------------------------------------- */
static void Task_S4_SpawnAndFly(u8 taskId)
{
    S4_SpawnBirds(taskId, 1);   /* dir 1 = right->left */
    gTasks[taskId].tS4Timer = 0;
    gTasks[taskId].func     = Task_S4_WaitClear;
}

/* --------------------------------------------------------------------------
 * Task: WaitClear
 * Watches Articuno (the V-tip) until it has fully exited the opposite edge.
 * Only one formation exists at a time - no duplicates possible.
 *
 *   After pass 0 (LTR): destroy birds -> BetweenDelay -> RTL pass
 *   After pass 1 (RTL): destroy birds -> BirdCries    -> FadeToMewtwo
 * -------------------------------------------------------------------------- */
static void Task_S4_WaitClear(u8 taskId)
{
    struct Sprite *art = &gSprites[(u8)gTasks[taskId].tS4ArtId];
    u8 dir = (u8)gTasks[taskId].tS4Dir;

    /* Has the tip fully cleared the opposite edge? */
    bool8 cleared = (dir == 0) ? (art->x > DISPLAY_WIDTH + 64)
                               : (art->x < -64);
    if (!cleared)
        return;

    S4_DestroyBirds(taskId);
    gTasks[taskId].tS4Pass++;
    gTasks[taskId].tS4Timer = 0;

    if (gTasks[taskId].tS4Pass == 1)
    {
        /* First pass done - pause, then do the return pass */
        gTasks[taskId].func = Task_S4_BetweenDelay;
    }
    else
    {
        /* Both passes done - play bird cries then fade to Mewtwo */
        gTasks[taskId].func = Task_S4_BirdCries;
    }
}

/* --------------------------------------------------------------------------
 * Task: BetweenDelay
 * Empty-sky pause between the LTR and RTL passes.
 * -------------------------------------------------------------------------- */
static void Task_S4_BetweenDelay(u8 taskId)
{
    gTasks[taskId].tS4Timer++;

    if (gTasks[taskId].tS4Timer >= S4_DELAY_BETWEEN)
    {
        gTasks[taskId].tS4Timer = 0;
        gTasks[taskId].func     = Task_S4_SpawnAndFly;
    }
}

/* --------------------------------------------------------------------------
 * Task: BirdCries - stagger one cry per bird, then fade to black
 * -------------------------------------------------------------------------- */
static void Task_S4_BirdCries(u8 taskId)
{
    gTasks[taskId].tS4Timer++;

    if (gTasks[taskId].tS4Timer == 1)
        PlayCryInternal(SPECIES_ARTICUNO, 0, 120, CRY_PRIORITY_NORMAL, CRY_MODE_NORMAL);
    if (gTasks[taskId].tS4Timer == 25)
        PlayCryInternal(SPECIES_ZAPDOS,   0, 120, CRY_PRIORITY_NORMAL, CRY_MODE_NORMAL);
    if (gTasks[taskId].tS4Timer == 50)
        PlayCryInternal(SPECIES_MOLTRES,  0, 120, CRY_PRIORITY_NORMAL, CRY_MODE_NORMAL);

    if (gTasks[taskId].tS4Timer >= 90)
    {
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].tS4Timer = 0;
        gTasks[taskId].func     = Task_S4_FadeToMewtwo;
    }
}

/* --------------------------------------------------------------------------
 * Task: FadeToMewtwo
 * Waits for the black fade, swaps to bg5, loads Mewtwo/Mew sprites,
 * then enters PreMewDelay for a dramatic pause before anything appears.
 * -------------------------------------------------------------------------- */
static void Task_S4_FadeToMewtwo(u8 taskId)
{
    u8 i, mewTwoId, mewId;

    if (gPaletteFade.active)
        return;

    /* Free bird assets - VRAM is clear, ready for new assets */
    ResetSpriteData();
    FreeAllSpritePalettes();

    /* Load bg5 (Mewtwo/Mew backdrop) */
    DecompressDataWithHeaderVram(sS4Bg5_Gfx,     (void *)(BG_CHAR_ADDR(0)));
    DecompressDataWithHeaderVram(sS4Bg5_Tilemap, (void *)(BG_SCREEN_ADDR(16)));
    LoadPalette(sS4Bg5_Pal, BG_PLTT_ID(0), sizeof(sS4Bg5_Pal));
    SetGpuReg(REG_OFFSET_BG1HOFS, 0);
    SetGpuReg(REG_OFFSET_BG1VOFS, 0);

    /* Load Mewtwo and Mew sprites */
    for (i = 0; sS4SpriteSheets_Psychic[i].tag != 0; i++)
        LoadCompressedSpriteSheet(&sS4SpriteSheets_Psychic[i]);
    for (i = 0; sS4SpritePalettes_Psychic[i].tag != 0; i++)
        LoadSpritePalette(&sS4SpritePalettes_Psychic[i]);

    /* Mewtwo starts at its final position, already visible, looping anim 0 */
    mewTwoId = CreateSprite(&sS4Template_Mewtwo, 120, S4_MEWTWO_Y, 1);
    gSprites[mewTwoId].invisible = FALSE;
    StartSpriteAnim(&gSprites[mewTwoId], 0);

    /* Mew starts at the bottom of the screen and flies up to Mewtwo */
    mewId = CreateSprite(&sS4Template_Mew, 120, DISPLAY_HEIGHT + 32, 0);
    gSprites[mewId].invisible  = FALSE;
    gSprites[mewId].sMewAngle  = 0;
    gSprites[mewId].sMewCentX  = 120;
    gSprites[mewId].sMewCentY  = S4_MEWTWO_Y;   /* orbit centre = Mewtwo's position */
    StartSpriteAnim(&gSprites[mewId], 0);

    /* Reuse tS4ArtId / tS4ZapId for Mewtwo / Mew */
    gTasks[taskId].tS4ArtId = mewTwoId;
    gTasks[taskId].tS4ZapId = mewId;

    /* Fade in to bg5 (still black, sprites hidden) then hold for delay */
    BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
    gTasks[taskId].tS4Timer = 0;
    gTasks[taskId].func     = Task_S4_PreMewDelay;
}

/* --------------------------------------------------------------------------
 * Task: PreMewDelay
 * Short pause on the bg5 backdrop (both sprites still invisible) before
 * Mewtwo rises and Mew begins orbiting.  Builds anticipation.
 * -------------------------------------------------------------------------- */
static void Task_S4_PreMewDelay(u8 taskId)
{
    if (gPaletteFade.active)
        return;   /* wait for fade-in to complete first */

    PlayCryInternal(SPECIES_MEWTWO, 0, 120, CRY_PRIORITY_NORMAL, CRY_MODE_NORMAL);
    gTasks[taskId].tS4Timer = 0;
    gTasks[taskId].func     = Task_S4_MewtwoScene;
}

/* --------------------------------------------------------------------------
 * Task: MewtwoScene
 * Each frame: sync Mew's orbit centre to Mewtwo's current position so
 * Mew follows Mewtwo while it is still rising.
 * After one full orbit (~128 frames at 2 angle units/frame), play Mew's
 * cry, then hold S4_HOLD_FRAMES before fading out.
 * -------------------------------------------------------------------------- */
static void Task_S4_MewtwoScene(u8 taskId)
{
    u8 mewTwoId = (u8)gTasks[taskId].tS4ArtId;
    u8 mewId    = (u8)gTasks[taskId].tS4ZapId;
    s16 dy;

    /* Phase 0: Mew is still flying up - check if it has arrived */
    if (gTasks[taskId].tS4State == 0)
    {
        dy = gSprites[mewTwoId].y - gSprites[mewId].y;
        if (dy < 0) dy = -dy;

        if (dy <= S4_MEW_ARRIVE_DIST)
        {
            /* Mew reached Mewtwo - switch both to their orbit/settled anims */
            StartSpriteAnim(&gSprites[mewTwoId], 1);    /* Mewtwo: settled */
            StartSpriteAnim(&gSprites[mewId],    1);    /* Mew: orbit loop */
            gSprites[mewId].callback = SpriteCB_S4_Mew_Orbit;

            /* Snap Mew's orbit centre to Mewtwo's exact position */
            gSprites[mewId].sMewCentX = gSprites[mewTwoId].x;
            gSprites[mewId].sMewCentY = gSprites[mewTwoId].y;

            gTasks[taskId].tS4State = 1;
            gTasks[taskId].tS4Timer = 0;
        }
        return;
    }

    /* Phase 1: Mew is orbiting - count time and play cry + fade out */
    gTasks[taskId].tS4Timer++;

    if (gTasks[taskId].tS4Timer == 130)
        PlayCryInternal(SPECIES_MEW, 0, 120, CRY_PRIORITY_NORMAL, CRY_MODE_NORMAL);

    if (gTasks[taskId].tS4Timer >= 130 + S4_HOLD_FRAMES)
    {
        gTasks[taskId].tS4Timer = 0;
        gTasks[taskId].func     = Task_S4_FadeOut;
    }
}

/* --------------------------------------------------------------------------
 * Task: FadeOut - fade to black, then hand off to the title screen
 * -------------------------------------------------------------------------- */
static void Task_S4_FadeOut(u8 taskId)
{
    if (gTasks[taskId].tS4Timer == 0)
    {
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].tS4Timer = 1;
        return;
    }

    if (!gPaletteFade.active)
    {
        SetMainCallback2(CB2_StartCustomIntroScreen5);
        DestroyTask(taskId);
    }
}

/* =========================================================================
 * Undefine Screen 4 macros
 * ========================================================================= */
#undef TAG_S4_ARTICUNO
#undef TAG_S4_ZAPDOS
#undef TAG_S4_MOLTRES
#undef TAG_S4_MEWTWO
#undef TAG_S4_MEW
#undef S4_BIRD_SPEED
#undef S4_MEW_ORBIT_R
#undef S4_HOLD_FRAMES
#undef S4_DELAY_PRE_PASS
#undef S4_DELAY_BETWEEN
#undef tS4Timer
#undef tS4State
#undef tS4ArtId
#undef tS4ZapId
#undef tS4MolId
#undef tS4Dir
#undef tS4Pass
#undef sMewAngle
#undef sMewCentX
#undef sMewCentY

/* ==========================================================================
 * SCREEN 5 - Legendary Dogs on the ground + Lugia/Ho-Oh in the sky
 *
 * Part A  - Split BG + Entei & Raikou crossing jump:
 *   Single charbase (0).  Grass tilemap at screenbase 16, sky at screenbase 20.
 *   Both BGs share ONE gbapal file (grass+sky share the same 16-colour slot 0).
 *   BG1 (priority 2) = sky   - upper half visible via VOFS.
 *   BG0 (priority 3) = grass - lower half visible via VOFS.
 *   Entei starts LEFT, Raikou starts RIGHT.  They jump TOWARD each other,
 *   crossing in the air and landing on the opposite side.
 *   Jump sequence per sprite: frame0(crouch) -> frame1(launch) -> frame2(air)
 *   -> frame0(land 4 frames) -> frame3(idle, locked).
 *   Horizontal movement: each dog moves S5_DOG_XSPEED px/frame during the
 *   jump arc so they physically cross paths.
 *
 * Part B  - bg6 + Suicune (BG tile) + Lugia/Ho-Oh sprites:
 *   bg6 at charbase 0, screenbase 16 (BG1, priority 3) - ocean/sky backdrop.
 *   Suicune at charbase 2, screenbase 24 (BG0, priority 0) - lower-left corner,
 *   priority 0 = always in front of OBJ sprites.
 *   IMPORTANT: BG VRAM layout for two simultaneous tilesets:
 *     charbase 0 -> bg6 tiles  (uses 0x4000 bytes starting at VRAM+0x0000)
 *     charbase 2 -> Suicune tiles (uses space starting at VRAM+0x8000)
 *     screenbase 16 -> bg6 map   (at VRAM+0x8000)
 *     screenbase 24 -> Suicune map (at VRAM+0xC000)
 *   Palette: bg6 at BG_PLTT_ID(0), Suicune at BG_PLTT_ID(1).
 *   Lugia flies left->right at sea level.  Ho-Oh flies right->left upper sky.
 * ========================================================================== */

/* --------------------------------------------------------------------------
 * Tag IDs
 * -------------------------------------------------------------------------- */
#define TAG_S5_ENTEI   5400
#define TAG_S5_RAIKOU  5401
#define TAG_S5_LUGIA   5402
#define TAG_S5_HOOH    5403

/* --------------------------------------------------------------------------
 * Assets
 * -------------------------------------------------------------------------- */

/*
 * Grass and sky each have their own palette file.
 * Grass -> BG_PLTT_ID(0), sky -> BG_PLTT_ID(1).
 * Their tilemaps must reference palette slot 0 and 1 respectively
 * (the exporter sets this via the palette attribute in the tilemap).
 */
static const u16 sS5Grass_Pal[]      = INCBIN_U16("graphics/custom_intro/screen5/grass.gbapal");
static const u32 sS5Grass_Gfx[]      = INCBIN_U32("graphics/custom_intro/screen5/grass.4bpp.smol");
static const u16 sS5Grass_Tilemap[]  = INCBIN_U16("graphics/custom_intro/screen5/grass.bin");
static const u16 sS5Sky_Pal[]        = INCBIN_U16("graphics/custom_intro/screen5/sky.gbapal");
static const u32 sS5Sky_Gfx[]        = INCBIN_U32("graphics/custom_intro/screen5/sky.4bpp.smol");
static const u16 sS5Sky_Tilemap[]    = INCBIN_U16("graphics/custom_intro/screen5/sky.bin");

/* bg6 - Part B full-screen backdrop */
static const u16 sS5Bg6_Pal[]        = INCBIN_U16("graphics/custom_intro/screen5/bg6.gbapal");
static const u32 sS5Bg6_Gfx[]        = INCBIN_U32("graphics/custom_intro/screen5/bg6.4bpp.smol");
static const u16 sS5Bg6_Tilemap[]    = INCBIN_U16("graphics/custom_intro/screen5/bg6.bin");

/*
 * Suicune - static BG tile drawn in the lower-left of BG0.
 * Loaded at charbase 2 so it does not stomp on bg6 tiles at charbase 0.
 * Its palette goes to BG_PLTT_ID(1) - separate slot from bg6.
 */
static const u16 sS5Suicune_Pal[]      = INCBIN_U16("graphics/custom_intro/screen5/suicune.gbapal");
static const u32 sS5Suicune_Gfx[]      = INCBIN_U32("graphics/custom_intro/screen5/suicune.4bpp.smol");
static const u16 sS5Suicune_Tilemap[]  = INCBIN_U16("graphics/custom_intro/screen5/suicune.bin");

/* Dog sprites - 64x64, 4 frames = 0x2000 bytes each */
static const u16 sS5Entei_Pal[]      = INCBIN_U16("graphics/custom_intro/screen5/entei.gbapal");
static const u32 sS5Entei_Gfx[]      = INCBIN_U32("graphics/custom_intro/screen5/entei.4bpp.smol");
static const u16 sS5Raikou_Pal[]     = INCBIN_U16("graphics/custom_intro/screen5/raikou.gbapal");
static const u32 sS5Raikou_Gfx[]     = INCBIN_U32("graphics/custom_intro/screen5/raikou.4bpp.smol");

/* Flying sprites - 64x64, 4 frames = 0x2000 bytes each */
static const u16 sS5Lugia_Pal[]      = INCBIN_U16("graphics/custom_intro/screen5/lugia.gbapal");
static const u32 sS5Lugia_Gfx[]      = INCBIN_U32("graphics/custom_intro/screen5/lugia.4bpp.smol");
static const u16 sS5HoOh_Pal[]       = INCBIN_U16("graphics/custom_intro/screen5/hooh.gbapal");
static const u32 sS5HoOh_Gfx[]       = INCBIN_U32("graphics/custom_intro/screen5/hooh.4bpp.smol");

/* --------------------------------------------------------------------------
 * Sprite sheets / palettes
 * -------------------------------------------------------------------------- */
static const struct CompressedSpriteSheet sS5Sheets_Dogs[] =
{
    {sS5Entei_Gfx,  0x2000, TAG_S5_ENTEI},
    {sS5Raikou_Gfx, 0x2000, TAG_S5_RAIKOU},
    {},
};
static const struct SpritePalette sS5Pals_Dogs[] =
{
    {sS5Entei_Pal,  TAG_S5_ENTEI},
    {sS5Raikou_Pal, TAG_S5_RAIKOU},
    {},
};
static const struct CompressedSpriteSheet sS5Sheets_Birds[] =
{
    {sS5Lugia_Gfx, 0x2000, TAG_S5_LUGIA},
    {sS5HoOh_Gfx,  0x2000, TAG_S5_HOOH},
    {},
};
static const struct SpritePalette sS5Pals_Birds[] =
{
    {sS5Lugia_Pal, TAG_S5_LUGIA},
    {sS5HoOh_Pal,  TAG_S5_HOOH},
    {},
};

/* --------------------------------------------------------------------------
 * OAM
 * -------------------------------------------------------------------------- */
static const struct OamData sS5Oam_64x64 =
{
    .y          = DISPLAY_HEIGHT,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode    = ST_OAM_OBJ_NORMAL,
    .bpp        = ST_OAM_4BPP,
    .shape      = SPRITE_SHAPE(64x64),
    .size       = SPRITE_SIZE(64x64),
    .priority   = 1,
};

/* --------------------------------------------------------------------------
 * Animations - same pattern as Screen 4 birds
 * Dogs: 4 individually-addressed single-frame anims (task switches them).
 * Flyers: 4-frame loop.
 * -------------------------------------------------------------------------- */
static const union AnimCmd sS5Anim_Dog_F0[] = { ANIMCMD_FRAME(0,   0), ANIMCMD_END };
static const union AnimCmd sS5Anim_Dog_F1[] = { ANIMCMD_FRAME(64,  0), ANIMCMD_END };
static const union AnimCmd sS5Anim_Dog_F2[] = { ANIMCMD_FRAME(128, 0), ANIMCMD_END };
static const union AnimCmd sS5Anim_Dog_F3[] = { ANIMCMD_FRAME(192, 0), ANIMCMD_END };
static const union AnimCmd *const sS5Anims_Dog[] =
{
    sS5Anim_Dog_F0, sS5Anim_Dog_F1, sS5Anim_Dog_F2, sS5Anim_Dog_F3,
};

static const union AnimCmd sS5Anim_Fly[] =
{
    ANIMCMD_FRAME(0,   8),
    ANIMCMD_FRAME(64,  8),
    ANIMCMD_FRAME(128, 8),
    ANIMCMD_FRAME(192, 8),
    ANIMCMD_JUMP(0),
};
static const union AnimCmd *const sS5Anims_Fly[] = { sS5Anim_Fly };

/* --------------------------------------------------------------------------
 * Timing
 * -------------------------------------------------------------------------- */
#define S5_CROUCH_DUR  80   /* +1s idle before jump (60 extra frames) */
#define S5_RISE_DUR    18
#define S5_AIR_DUR     18
#define S5_LAND_DUR     4
#define S5_JUMP_HEIGHT 36    /* px upward at apex */
#define S5_DOG_XSPEED   4    /* px/frame — slightly reduced cross distance */
#define S5_HOLD_DOG    60
#define S5_FLYBY_DUR  200
#define S5_FLY_SPEED    3

#define S5_T_LAUNCH  (S5_CROUCH_DUR)
#define S5_T_APEX    (S5_CROUCH_DUR + S5_RISE_DUR)
#define S5_T_LAND    (S5_CROUCH_DUR + S5_RISE_DUR + S5_AIR_DUR)
#define S5_T_IDLE    (S5_CROUCH_DUR + S5_RISE_DUR + S5_AIR_DUR + S5_LAND_DUR)

/* --------------------------------------------------------------------------
 * Task data
 * -------------------------------------------------------------------------- */
#define tS5Timer   data[0]
#define tS5EntId   data[1]
#define tS5RaiId   data[2]
#define tS5LugId   data[3]
#define tS5HoId    data[4]

/* --------------------------------------------------------------------------
 * Forward declarations
 * -------------------------------------------------------------------------- */
static void Task_S5_FadeIn(u8 taskId);
static void Task_S5_DogsJump(u8 taskId);
static void Task_S5_FadeToBirds(u8 taskId);
static void Task_S5_BirdsFlyby(u8 taskId);
static void Task_S5_FadeOut(u8 taskId);
static void SpriteCB_S5_Dog(struct Sprite *sprite);
static void SpriteCB_S5_FlyRight(struct Sprite *sprite);
static void SpriteCB_S5_FlyLeft(struct Sprite *sprite);

/* --------------------------------------------------------------------------
 * Sprite callbacks
 * -------------------------------------------------------------------------- */
static void SpriteCB_S5_Dog(struct Sprite *sprite)    { (void)sprite; }

static void SpriteCB_S5_FlyRight(struct Sprite *sprite)
{
    sprite->x += S5_FLY_SPEED;
    /* Stop and freeze once fully off the right edge — no looping */
    if (sprite->x > DISPLAY_WIDTH + 64)
        sprite->callback = SpriteCallbackDummy;
}

static void SpriteCB_S5_FlyLeft(struct Sprite *sprite)
{
    sprite->x -= S5_FLY_SPEED;
    /* Stop and freeze once fully off the left edge — no looping */
    if (sprite->x < -64)
        sprite->callback = SpriteCallbackDummy;
}

/* --------------------------------------------------------------------------
 * Sprite templates
 * -------------------------------------------------------------------------- */
static const struct SpriteTemplate sS5Template_Entei =
{
    .tileTag = TAG_S5_ENTEI, .paletteTag = TAG_S5_ENTEI,
    .oam = &sS5Oam_64x64, .anims = sS5Anims_Dog, .callback = SpriteCB_S5_Dog,
};
static const struct SpriteTemplate sS5Template_Raikou =
{
    .tileTag = TAG_S5_RAIKOU, .paletteTag = TAG_S5_RAIKOU,
    .oam = &sS5Oam_64x64, .anims = sS5Anims_Dog, .callback = SpriteCB_S5_Dog,
};
static const struct SpriteTemplate sS5Template_Lugia =
{
    .tileTag = TAG_S5_LUGIA, .paletteTag = TAG_S5_LUGIA,
    .oam = &sS5Oam_64x64, .anims = sS5Anims_Fly, .callback = SpriteCB_S5_FlyRight,
};
static const struct SpriteTemplate sS5Template_HoOh =
{
    .tileTag = TAG_S5_HOOH, .paletteTag = TAG_S5_HOOH,
    .oam = &sS5Oam_64x64, .anims = sS5Anims_Fly, .callback = SpriteCB_S5_FlyLeft,
};

/* --------------------------------------------------------------------------
 * Jump arc helper - same parabola formula used in Screen 1
 * t = 0..S5_RISE_DUR+S5_AIR_DUR, returns negative y2 (upward on screen)
 * -------------------------------------------------------------------------- */
static s16 S5_JumpArcY(s16 t)
{
    s16 half = S5_RISE_DUR + S5_AIR_DUR;
    s16 t256 = t * 256 / half;
    return -(s16)(S5_JUMP_HEIGHT * 4 * t256 * (256 - t256) / (256 * 256));
}

/* --------------------------------------------------------------------------
 * Entry point for Screen 5
 * -------------------------------------------------------------------------- */
void CB2_StartCustomIntroScreen5(void)
{
    u8 taskId, entId, raiId, i;

    SetVBlankCallback(NULL);
    SetGpuReg(REG_OFFSET_DISPCNT, 0);
    CpuFill32(0, (void *)VRAM,       VRAM_SIZE);
    CpuFill32(0, (void *)OAM,        OAM_SIZE);
    CpuFill16(0, (void *)(PLTT + 2), PLTT_SIZE - 2);
    ResetPaletteFade();
    ScanlineEffect_Stop();
    ResetTasks();
    ResetSpriteData();
    FreeAllSpritePalettes();
    EnableInterrupts(INTR_FLAG_VBLANK);
    SetVBlankCallback(VBlankCB_CustomIntro);
    SetMainCallback2(MainCB2_CustomIntro);

    /*
     * Split BG - both tilesets share charbase 0 and ONE palette slot (0).
     * The artist must export grass+sky tiles together so their tile indices
     * do not collide.  Alternatively, grass occupies the lower 256 tiles
     * and sky starts after - the tilemap BIN files reference the correct
     * indices automatically from the exporter.
     *
     * Screenbase layout (each screenbase = 0x800 bytes):
     *   screenbase 16 -> grass tilemap  (0x8000 in VRAM)
     *   screenbase 20 -> sky   tilemap  (0xA000 in VRAM)
     *   charbase  0   -> combined tiles (0x0000..0x7FFF)
     *
     * BG0 (priority 3) = grass, shown in lower half.
     *   VOFS = 0 -> top of the grass tilemap sits at screen row 0, but the
     *   tilemap itself only has content in its lower rows, so the upper rows
     *   are transparent (colour 0), letting BG1 (sky) show through above.
     * BG1 (priority 2) = sky, shown in upper half.
     *   Same logic - sky tilemap fills upper rows, lower rows transparent.
     *
     * This avoids any VOFS hacks and matches how the vanilla intro does it:
     * each BG tilemap is simply designed to only have tiles in its own region.
     */
    /*
     * Both grass and sky tiles loaded into charbase 0 sequentially.
     * Sky tiles are appended right after grass tiles in VRAM, so their
     * tile indices in the sky tilemap are already offset by the number
     * of grass tiles (the exporter handles this automatically).
     * Both BGs use the same charbase 0, different screenbases.
     * A single combined palette covers both at BG_PLTT_ID(0).
     * The sky tilemap must have been exported with palette slot 0 too.
     */
/* Sky: charbase 1, screenbase 20, palette slot 1.
     * After decompression, force bits 12-15 of every non-blank tile entry
     * to slot 1 — same technique as Charizard in Screen 2. */
    {
        u16 *screen;
        u32 j;
        DecompressDataWithHeaderVram(sS5Sky_Gfx,     (void *)(BG_CHAR_ADDR(1)));
        DmaCopy16(3, sS5Sky_Tilemap, (void *)(BG_SCREEN_ADDR(20)), sizeof(sS5Sky_Tilemap));
        screen = (u16 *)(BG_SCREEN_ADDR(20));
        for (j = 0; j < sizeof(sS5Sky_Tilemap) / sizeof(u16); j++)
        {
            /* Force palette slot 1 on EVERY entry, including raw tile
             * index 0 — tile 0 is a real, visible sky tile in this
             * tileset, not a blank placeholder, so it must not be
             * skipped. */
            screen[j] = (screen[j] & 0x0FFF) | (1 << 12);
        }
        LoadPalette(sS5Sky_Pal, BG_PLTT_ID(1), sizeof(sS5Sky_Pal));
    }

    /* Grass: charbase 0, screenbase 16, palette slot 0. */
    DecompressDataWithHeaderVram(sS5Grass_Gfx,     (void *)(BG_CHAR_ADDR(0)));
    DmaCopy16(3, sS5Grass_Tilemap, (void *)(BG_SCREEN_ADDR(16)), sizeof(sS5Grass_Tilemap));
    LoadPalette(sS5Grass_Pal, BG_PLTT_ID(0), sizeof(sS5Grass_Pal));

    SetGpuReg(REG_OFFSET_BG0CNT,
              BGCNT_PRIORITY(3)
            | BGCNT_CHARBASE(0)
            | BGCNT_SCREENBASE(16)
            | BGCNT_16COLOR
            | BGCNT_TXT256x256);

    SetGpuReg(REG_OFFSET_BG1CNT,
              BGCNT_PRIORITY(2)
            | BGCNT_CHARBASE(1)
            | BGCNT_SCREENBASE(20)
            | BGCNT_16COLOR
            | BGCNT_TXT256x256);

    SetGpuReg(REG_OFFSET_BG0HOFS, 0);
    SetGpuReg(REG_OFFSET_BG0VOFS, 0);
    SetGpuReg(REG_OFFSET_BG1HOFS, 0);
    SetGpuReg(REG_OFFSET_BG1VOFS, 0);

    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0
                                 | DISPCNT_OBJ_1D_MAP
                                 | DISPCNT_BG0_ON
                                 | DISPCNT_BG1_ON
                                 | DISPCNT_OBJ_ON);

    /* Load dog sprites */
    for (i = 0; sS5Sheets_Dogs[i].tag != 0; i++)
        LoadCompressedSpriteSheet(&sS5Sheets_Dogs[i]);
    for (i = 0; sS5Pals_Dogs[i].tag != 0; i++)
        LoadSpritePalette(&sS5Pals_Dogs[i]);

    /*
     * Entei starts on the LEFT side, facing right (no hFlip).
     * Raikou starts on the RIGHT side, facing left (hFlip).
     * They jump toward each other and swap sides.
     * Ground y = 128 (bottom quarter of the 160-tall screen).
     */
    entId = CreateSprite(&sS5Template_Entei, 44, 128, 1);
    StartSpriteAnim(&gSprites[entId], 0);

    raiId = CreateSprite(&sS5Template_Raikou, 196, 128, 1);
    gSprites[raiId].hFlip = TRUE;
    StartSpriteAnim(&gSprites[raiId], 0);

    BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);

    taskId = CreateTask(Task_S5_FadeIn, 0);
    gTasks[taskId].tS5Timer = 0;
    gTasks[taskId].tS5EntId = entId;
    gTasks[taskId].tS5RaiId = raiId;
}

/* --------------------------------------------------------------------------
 * Task: FadeIn
 * -------------------------------------------------------------------------- */
static void Task_S5_FadeIn(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        gTasks[taskId].tS5Timer = 0;
        gTasks[taskId].func     = Task_S5_DogsJump;
    }
}

/* --------------------------------------------------------------------------
 * Task: DogsJump
 *
 * Entei moves RIGHT (+S5_DOG_XSPEED/frame) and Raikou moves LEFT
 * (-S5_DOG_XSPEED/frame) during the airborne phase so they cross paths.
 * Horizontal movement starts at T_LAUNCH and stops at T_LAND.
 * After landing (T_IDLE), their hFlip is swapped so they face the new side.
 * -------------------------------------------------------------------------- */
static void Task_S5_DogsJump(u8 taskId)
{
    s16 t     = (s16)gTasks[taskId].tS5Timer;
    u8  entId = (u8)gTasks[taskId].tS5EntId;
    u8  raiId = (u8)gTasks[taskId].tS5RaiId;

    gTasks[taskId].tS5Timer++;

    /* ---- Frame transitions ---- */
    if (t == S5_T_LAUNCH)
    {
        StartSpriteAnim(&gSprites[entId], 1);
        StartSpriteAnim(&gSprites[raiId], 1);
        PlayCryInternal(SPECIES_ENTEI,  0, 120, CRY_PRIORITY_NORMAL, CRY_MODE_NORMAL);
        PlayCryInternal(SPECIES_RAIKOU, 0, 120, CRY_PRIORITY_NORMAL, CRY_MODE_NORMAL);
    }
    else if (t == S5_T_APEX)
    {
        StartSpriteAnim(&gSprites[entId], 2);
        StartSpriteAnim(&gSprites[raiId], 2);
    }
    else if (t == S5_T_LAND)
    {
        StartSpriteAnim(&gSprites[entId], 0);
        StartSpriteAnim(&gSprites[raiId], 0);
        gSprites[entId].y2 = 0;
        gSprites[raiId].y2 = 0;
    }
    else if (t == S5_T_IDLE)
    {
        StartSpriteAnim(&gSprites[entId], 3);
        StartSpriteAnim(&gSprites[raiId], 3);
        /* Flip to face outward on the new side */
        gSprites[entId].hFlip = TRUE;
        gSprites[raiId].hFlip = FALSE;
    }

    /* ---- Parabolic Y arc + horizontal crossing ---- */
    if (t >= S5_T_LAUNCH && t < S5_T_LAND)
    {
        s16 arcY = S5_JumpArcY(t - (s16)S5_T_LAUNCH);
        gSprites[entId].y2 = arcY;
        gSprites[raiId].y2 = arcY;

        /* Horizontal movement: Entei right, Raikou left */
        gSprites[entId].x += S5_DOG_XSPEED;
        gSprites[raiId].x -= S5_DOG_XSPEED;
    }

    /* ---- Hold idle pose then fade ---- */
    if (t >= S5_T_IDLE + S5_HOLD_DOG)
    {
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].tS5Timer = 0;
        gTasks[taskId].func     = Task_S5_FadeToBirds;
    }
}

/* --------------------------------------------------------------------------
 * Task: FadeToBirds
 * -------------------------------------------------------------------------- */
static void Task_S5_FadeToBirds(u8 taskId)
{
    u8 i, lugId, hoId;

    if (gPaletteFade.active)
        return;

    /* BUGFIX (beast-screen VRAM glitch): the palette fade above only
     * blends colours to black -- it does NOT stop the PPU from scanning
     * out BG VRAM. The block below rewrites the tile data, tilemap, and
     * palette for both BGs; if any part of that lands mid-scanout, the
     * old and new data interleave for a frame and show up as a brief
     * flicker/corruption exactly when this screen swaps to Suicune. Force
     * a blank screen for the duration of the rewrite -- same technique
     * every CB2_StartCustomIntroScreenN entry point in this file already
     * uses -- then restore DISPCNT once everything is written. */
    SetGpuReg(REG_OFFSET_DISPCNT, 0);

    /* Free Part A resources */
    ResetSpriteData();
    FreeAllSpritePalettes();
    CpuFill32(0, (void *)VRAM, VRAM_SIZE);

    /*
     * Part B BG layout (mirrors the megarayvision.c pattern exactly):
     *   BG1 (priority 3) = bg6      - charbase 0, screenbase 28
     *   BG0 (priority 0) = Suicune  - charbase 2, screenbase 24
     *
     * charbase 0 at VRAM+0x0000 -> bg6 tiles
     * charbase 2 at VRAM+0x8000 -> Suicune tiles
     * screenbase 24 at VRAM+0xC000 -> Suicune map
     * screenbase 28 at VRAM+0xE000 -> bg6 map
     *
     * BG palette:
     *   BG_PLTT_ID(0) = bg6 palette
     *   BG_PLTT_ID(1) = Suicune palette
     */
    DecompressDataWithHeaderVram(sS5Bg6_Gfx,     (void *)(BG_CHAR_ADDR(0)));
    DmaCopy16(3, sS5Bg6_Tilemap, (void *)(BG_SCREEN_ADDR(28)), sizeof(sS5Bg6_Tilemap));
    LoadPalette(sS5Bg6_Pal, BG_PLTT_ID(0), sizeof(sS5Bg6_Pal));

    DecompressDataWithHeaderVram(sS5Suicune_Gfx,     (void *)(BG_CHAR_ADDR(2)));
    DmaCopy16(3, sS5Suicune_Tilemap, (void *)(BG_SCREEN_ADDR(24)), sizeof(sS5Suicune_Tilemap));
    /* Force palette slot 2 in every non-blank Suicune tilemap entry */
    {
        u16 *screen = (u16 *)(BG_SCREEN_ADDR(24));
        u32 j;
        for (j = 0; j < sizeof(sS5Suicune_Tilemap) / sizeof(u16); j++)
        {
            if (screen[j] & 0x03FF)
                screen[j] = (screen[j] & 0x0FFF) | (2 << 12);
        }
    }
    LoadPalette(sS5Suicune_Pal, BG_PLTT_ID(2), sizeof(sS5Suicune_Pal));

    SetGpuReg(REG_OFFSET_BG1CNT,
              BGCNT_PRIORITY(3)
            | BGCNT_CHARBASE(0)
            | BGCNT_SCREENBASE(28)
            | BGCNT_16COLOR
            | BGCNT_TXT256x256);

    /* priority 0 = in front of everything, including OBJ sprites */
    SetGpuReg(REG_OFFSET_BG0CNT,
              BGCNT_PRIORITY(0)
            | BGCNT_CHARBASE(2)
            | BGCNT_SCREENBASE(24)
            | BGCNT_16COLOR
            | BGCNT_TXT256x256);

    SetGpuReg(REG_OFFSET_BG0HOFS, 0);
    SetGpuReg(REG_OFFSET_BG0VOFS, 0);
    SetGpuReg(REG_OFFSET_BG1HOFS, 0);
    SetGpuReg(REG_OFFSET_BG1VOFS, 0);

    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0
                                 | DISPCNT_OBJ_1D_MAP
                                 | DISPCNT_BG0_ON
                                 | DISPCNT_BG1_ON
                                 | DISPCNT_OBJ_ON);

    /* Load flying sprites */
    for (i = 0; sS5Sheets_Birds[i].tag != 0; i++)
        LoadCompressedSpriteSheet(&sS5Sheets_Birds[i]);
    for (i = 0; sS5Pals_Birds[i].tag != 0; i++)
        LoadSpritePalette(&sS5Pals_Birds[i]);

    /* Lugia - sea level, left->right (lowered slightly per feedback) */
    lugId = CreateSprite(&sS5Template_Lugia, -64, 130, 1);
    StartSpriteAnim(&gSprites[lugId], 0);

    /* Ho-Oh - upper sky, right->left, hFlipped */
    hoId = CreateSprite(&sS5Template_HoOh, DISPLAY_WIDTH + 64, 36, 1);
    gSprites[hoId].hFlip = TRUE;
    StartSpriteAnim(&gSprites[hoId], 0);

    gTasks[taskId].tS5LugId = lugId;
    gTasks[taskId].tS5HoId  = hoId;

    PlayCryInternal(SPECIES_SUICUNE, 0, 120, CRY_PRIORITY_NORMAL, CRY_MODE_NORMAL);

    BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
    gTasks[taskId].tS5Timer = 0;
    gTasks[taskId].func     = Task_S5_BirdsFlyby;
}

/* --------------------------------------------------------------------------
 * Task: BirdsFlyby
 * -------------------------------------------------------------------------- */
static void Task_S5_BirdsFlyby(u8 taskId)
{
    if (gPaletteFade.active)
        return;

    gTasks[taskId].tS5Timer++;
    if (gTasks[taskId].tS5Timer >= S5_FLYBY_DUR)
    {
        gTasks[taskId].tS5Timer = 0;
        gTasks[taskId].func     = Task_S5_FadeOut;
    }
}

/* --------------------------------------------------------------------------
 * Task: FadeOut
 * -------------------------------------------------------------------------- */
static void Task_S5_FadeOut(u8 taskId)
{
    if (gTasks[taskId].tS5Timer == 0)
    {
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].tS5Timer = 1;
        return;
    }

    if (!gPaletteFade.active)
    {
        SetMainCallback2(CB2_StartCustomIntroScreen6);
        DestroyTask(taskId);
    }
}

/* =========================================================================
 * Undefine Screen 5 macros
 * ========================================================================= */
#undef TAG_S5_ENTEI
#undef TAG_S5_RAIKOU
#undef TAG_S5_LUGIA
#undef TAG_S5_HOOH
#undef S5_CROUCH_DUR
#undef S5_RISE_DUR
#undef S5_AIR_DUR
#undef S5_LAND_DUR
#undef S5_JUMP_HEIGHT
#undef S5_DOG_XSPEED
#undef S5_HOLD_DOG
#undef S5_FLYBY_DUR
#undef S5_FLY_SPEED
#undef S5_T_LAUNCH
#undef S5_T_APEX
#undef S5_T_LAND
#undef S5_T_IDLE
#undef tS5Timer
#undef tS5EntId
#undef tS5RaiId
#undef tS5LugId
#undef tS5HoId

/* ==========================================================================
 * SCREEN 6 -- Deoxys scene
 *
 * Assets (all in graphics/custom_intro/screen6/):
 *   galaxy  -- full-screen star backdrop BG, stays for the whole scene
 *   speed   -- 256x160 tileset of Deoxys Speed form with motion-blur flash
 *              (the white streak is baked into the art)
 *   deoxys  -- 64x64 sprite, 4 frames: Normal, Attack, Defence, Speed forms
 *   back    -- 64x64 sprite, 1 frame: Deoxys back sprite
 *   beam    -- 64x64 sprite, 1 frame: the energy beam (drawn horizontal)
 *
 * Sequence:
 *   1. galaxy fades in.  Deoxys back sprite appears centre-bottom.
 *   2. speed tileset replaces galaxy (while Deoxys is hidden):
 *      -- slides RIGHT across screen at S6_FLASH_SPEED px/frame
 *      -- at right edge: HOFS snaps to negative, slides RIGHT again
 *      -- S6_FLASH_PASSES full passes, alternating direction for variety
 *   3. Deoxys back sprite fades out; back hides.
 *      speed BG hidden; galaxy restored.
 *      Deoxys normal sprite appears centre (x=120, y=88).
 *      -- anim 0 (frame 0) held S6_FORM_HOLD frames
 *      -- anim 1 (frame 1) held S6_FORM_HOLD frames
 *      -- anim 2 (frame 2) held S6_FORM_HOLD frames
 *      -- anim 3 (frame 3): Speed form, locked.
 *   4. Beam sprite spawns off-screen RIGHT at Deoxys y+4, moves LEFT
 *      at S6_BEAM_SPEED px/frame until it hits Deoxys (x <= 120).
 *      Beam has OAM hFlip so the art faces left.
 *   5. On beam impact: PlayCry(DEOXYS), screen flash (white palette fade),
 *      then fade to black -> CB2_InitTitleScreen.
 *
 * OBJ VRAM:
 *   deoxys  4 frames 64x64 4bpp = 0x2000
 *   back    1 frame  64x64 4bpp = 0x800
 *   beam    1 frame  64x64 4bpp = 0x800
 *   TOTAL = 0x3000 -- well within 0x8000.
 * ========================================================================== */

/* --------------------------------------------------------------------------
 * Tag IDs
 * -------------------------------------------------------------------------- */
#define TAG_S6_DEOXYS  5500
#define TAG_S6_BACK    5501
#define TAG_S6_BEAM    5502

/* --------------------------------------------------------------------------
 * Timing constants
 * -------------------------------------------------------------------------- */
#define S6_FLASH_SPEED    16
#define S6_FLASH_PASSES    6
#define S6_BACK_HOLD      40
#define S6_FORM_HOLD      20
#define S6_FLASH_HOLD     30

/* Deoxys final resting position */
#define S6_DEOXYS_X  120
#define S6_DEOXYS_Y   88

/* Length of the speed-flash phase, in frames -- how long the speed
 * tileset scrolls before Deoxys' sprite is revealed. */
#define S6_DEOXYS_SWEEP_FRAMES  40

/* --------------------------------------------------------------------------
 * Assets
 * -------------------------------------------------------------------------- */
static const u16 sS6Galaxy_Pal[]     = INCBIN_U16("graphics/custom_intro/screen6/galaxy.gbapal");
static const u32 sS6Galaxy_Gfx[]     = INCBIN_U32("graphics/custom_intro/screen6/galaxy.4bpp.smol");
static const u16 sS6Galaxy_Tilemap[] = INCBIN_U16("graphics/custom_intro/screen6/galaxy.bin");

static const u16 sS6Speed_Pal[]      = INCBIN_U16("graphics/custom_intro/screen6/speed.gbapal");
static const u32 sS6Speed_Gfx[]      = INCBIN_U32("graphics/custom_intro/screen6/speed.4bpp.smol");
static const u16 sS6Speed_Tilemap[]  = INCBIN_U16("graphics/custom_intro/screen6/speed.bin");

static const u16 sS6Deoxys_Pal[]     = INCBIN_U16("graphics/custom_intro/screen6/deoxys.gbapal");
static const u32 sS6Deoxys_Gfx[]     = INCBIN_U32("graphics/custom_intro/screen6/deoxys.4bpp.smol");

static const u16 sS6Back_Pal[]       = INCBIN_U16("graphics/custom_intro/screen6/back.gbapal");
static const u32 sS6Back_Gfx[]       = INCBIN_U32("graphics/custom_intro/screen6/back.4bpp.smol");

static const u16 sS6Beam_Pal[]       = INCBIN_U16("graphics/custom_intro/screen6/beam.gbapal");
static const u32 sS6Beam_Gfx[]       = INCBIN_U32("graphics/custom_intro/screen6/beam.4bpp.smol");

/* --------------------------------------------------------------------------
 * Sprite sheets / palettes
 * -------------------------------------------------------------------------- */
static const struct CompressedSpriteSheet sS6Sheets[] =
{
    {sS6Deoxys_Gfx, 0x2000, TAG_S6_DEOXYS},
    {sS6Back_Gfx,   0x800,  TAG_S6_BACK},
    {sS6Beam_Gfx,   0x800,  TAG_S6_BEAM},
    {},
};
static const struct SpritePalette sS6Palettes[] =
{
    {sS6Deoxys_Pal, TAG_S6_DEOXYS},
    {sS6Back_Pal,   TAG_S6_BACK},
    {sS6Beam_Pal,   TAG_S6_BEAM},
    {},
};

/* --------------------------------------------------------------------------
 * OAM
 * -------------------------------------------------------------------------- */
static const struct OamData sS6Oam_64x64 =
{
    .y          = DISPLAY_HEIGHT,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode    = ST_OAM_OBJ_NORMAL,
    .bpp        = ST_OAM_4BPP,
    .shape      = SPRITE_SHAPE(64x64),
    .size       = SPRITE_SIZE(64x64),
    .priority   = 1,
};

/*
 * Beam / explosion OAM: AFFINE_DOUBLE + matrixNum 8.
 * AFFINE_DOUBLE doubles the bounding box so the circle can grow to
 * 128x128 visible pixels before wrapping. Priority 0: in front of everything.
 */
static const struct OamData sS6Oam_Beam =
{
    .y          = DISPLAY_HEIGHT,
    .affineMode = ST_OAM_AFFINE_DOUBLE,
    .objMode    = ST_OAM_OBJ_NORMAL,
    .bpp        = ST_OAM_4BPP,
    .shape      = SPRITE_SHAPE(64x64),
    .size       = SPRITE_SIZE(64x64),
    .matrixNum  = 8,
    .priority   = 0,
};

/* --------------------------------------------------------------------------
 * Animations
 * Deoxys: 4 individually-addressed single-frame anims (task switches them).
 * Back: 1 static frame.
 * Beam: 1 static frame, hFlip set at creation so art faces left.
 * -------------------------------------------------------------------------- */
static const union AnimCmd sS6Anim_D0[] = { ANIMCMD_FRAME(0,   0), ANIMCMD_END };
static const union AnimCmd sS6Anim_D1[] = { ANIMCMD_FRAME(64,  0), ANIMCMD_END };
static const union AnimCmd sS6Anim_D2[] = { ANIMCMD_FRAME(128, 0), ANIMCMD_END };
static const union AnimCmd sS6Anim_D3[] = { ANIMCMD_FRAME(192, 0), ANIMCMD_END };
static const union AnimCmd *const sS6Anims_Deoxys[] =
{
    sS6Anim_D0, sS6Anim_D1, sS6Anim_D2, sS6Anim_D3,
};

static const union AnimCmd sS6Anim_Static[] = { ANIMCMD_FRAME(0, 0), ANIMCMD_END };
static const union AnimCmd *const sS6Anims_Static[] = { sS6Anim_Static };

/* --------------------------------------------------------------------------
 * Sprite callbacks -- forward declarations
 * -------------------------------------------------------------------------- */
static void SpriteCB_S6_Static(struct Sprite *sprite);
static void SpriteCB_S6_Beam(struct Sprite *sprite);

/* --------------------------------------------------------------------------
 * Sprite templates
 * -------------------------------------------------------------------------- */
static const struct SpriteTemplate sS6Template_Deoxys =
{
    .tileTag     = TAG_S6_DEOXYS,
    .paletteTag  = TAG_S6_DEOXYS,
    .oam         = &sS6Oam_64x64,
    .anims       = sS6Anims_Deoxys,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback    = SpriteCB_S6_Static,
};
static const struct SpriteTemplate sS6Template_Back =
{
    .tileTag     = TAG_S6_BACK,
    .paletteTag  = TAG_S6_BACK,
    .oam         = &sS6Oam_64x64,
    .anims       = sS6Anims_Static,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback    = SpriteCB_S6_Static,
};
/*
 * Beam template: the beam.png is now a circle/ring.
 * Spawned at Deoxys centre; grows outward via affine scale (explosion).
 */
static const struct SpriteTemplate sS6Template_Beam =
{
    .tileTag     = TAG_S6_BEAM,
    .paletteTag  = TAG_S6_BEAM,
    .oam         = &sS6Oam_Beam,
    .anims       = sS6Anims_Static,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback    = SpriteCB_S6_Beam,
};
/* --------------------------------------------------------------------------
 * Task data aliases
 * -------------------------------------------------------------------------- */
#define tS6Timer     data[0]
#define tS6State     data[1]
#define tS6Passes    data[2]
#define tS6DeoxId    data[3]
#define tS6BackId    data[4]
#define tS6BeamId    data[5]
#define tS6BgScroll  data[6]

/* --------------------------------------------------------------------------
 * Forward declarations
 * -------------------------------------------------------------------------- */
static void Task_S6_FadeIn(u8 taskId);
static void Task_S6_BackHold(u8 taskId);
static void Task_S6_SpeedFlash(u8 taskId);
static void Task_S6_ShowForms(u8 taskId);
static void Task_S6_TransformFlash(u8 taskId);
static void Task_S6_BeamGrow(u8 taskId);
static void Task_S6_Impact(u8 taskId);
static void Task_S6_FadeOut(u8 taskId);

/* --------------------------------------------------------------------------
 * Sprite callbacks
 * -------------------------------------------------------------------------- */
static void SpriteCB_S6_Static(struct Sprite *sprite) { (void)sprite; }

/*
 * SpriteCB_S6_Beam -- explosion circle that grows outward from Deoxys centre.
 *
 * Uses the same SetOamMatrix technique as the sun in Screen 2.
 * data[0] = inverse scale in Q8 -- starts at 512 (tiny: 0.5x effective size
 *           since AFFINE_DOUBLE doubles dimensions) and decreases toward 64.
 * Lower Pa/Pd = larger rendered sprite.
 * data[1] = set to 1 when fully expanded (triggers impact in task).
 *
 * Scale progression: 512 -> 64 over S6_BEAM_GROW_FRAMES.
 * At Pa=64 with AFFINE_DOUBLE the circle fills ~128px = most of the screen.
 */
#define S6_BEAM_SCALE_START  512
#define S6_BEAM_SCALE_END     64
#define S6_BEAM_GROW_FRAMES   20

static void SpriteCB_S6_Beam(struct Sprite *sprite)
{
    s16 scale = sprite->data[0];

    if (scale > S6_BEAM_SCALE_END)
    {
        u16 progress;

        scale -= (S6_BEAM_SCALE_START - S6_BEAM_SCALE_END) / S6_BEAM_GROW_FRAMES;
        if (scale < S6_BEAM_SCALE_END)
            scale = S6_BEAM_SCALE_END;
        sprite->data[0] = scale;
        SetOamMatrix(8, (u16)scale, 0, 0, (u16)scale);

        /* Whiten the whole screen IN STEP with the circle's own growth --
         * same BlendPalettes call used for the white flash elsewhere in
         * this file, just driven by growth progress here instead of a
         * fixed value, so the circle and the whiteout read as one single
         * light explosion rather than two separate effects. */
        progress = (u16)(S6_BEAM_SCALE_START - scale) * 16
                 / (S6_BEAM_SCALE_START - S6_BEAM_SCALE_END);
        if (progress > 16)
            progress = 16;
        BlendPalettes(PALETTES_ALL, (u8)progress, RGB_WHITE);
    }
    else
    {
        BlendPalettes(PALETTES_ALL, 16, RGB_WHITE);
        sprite->data[1] = 1;   /* signal: fully expanded */
    }
}

/* --------------------------------------------------------------------------
 * Helper: load galaxy BG into the hardware registers
 * -------------------------------------------------------------------------- */
/*
 * S6_LoadGalaxy -- galaxy into BG1 (charbase 0, screenbase 24, palette slot 0).
 * Matches the safe layout proven in Screen 2 / megarayvision.c:
 *   charbase 0  -> 0x00000-0x03FFF (galaxy tiles)
 *   screenbase 24 -> 0x0C000-0x0C7FF (galaxy map, inside unused charbase 3)
 */
static void S6_LoadGalaxy(void)
{
    DecompressDataWithHeaderVram(sS6Galaxy_Gfx, (void *)(BG_CHAR_ADDR(0)));
    DmaCopy16(3, sS6Galaxy_Tilemap, (void *)(BG_SCREEN_ADDR(24)), sizeof(sS6Galaxy_Tilemap));
    LoadPalette(sS6Galaxy_Pal, BG_PLTT_ID(0), sizeof(sS6Galaxy_Pal));
    /* galaxy.png colour 0 is the transparency key (green). Force it to black
     * in gPlttBufferUnfaded so TransferPlttBuffer never writes the green. */
    gPlttBufferUnfaded[0] = RGB_BLACK;
    gPlttBufferFaded[0]   = RGB_BLACK;

    SetGpuReg(REG_OFFSET_BG1CNT,
              BGCNT_PRIORITY(3)
            | BGCNT_CHARBASE(0)
            | BGCNT_SCREENBASE(24)
            | BGCNT_16COLOR
            | BGCNT_TXT256x256);
    SetGpuReg(REG_OFFSET_BG1HOFS, 0);
    SetGpuReg(REG_OFFSET_BG1VOFS, 0);
}

/*
 * S6_LoadSpeed -- speed tileset into BG0 (charbase 2, screenbase 28, palette slot 4).
 * Identical layout to Screen 2's Charizard overlay:
 *   charbase 2  -> 0x08000-0x0BFFF (speed tiles)
 *   screenbase 28 -> 0x0E000-0x0E7FF (speed map, inside unused charbase 3)
 * After decompressing the tilemap, we patch each non-blank entry to force
 * palette slot 4 (bits 12-15), matching the actual slot we loaded the palette
 * into -- the same technique used for Charizard in Screen 2.
 */
static void S6_LoadSpeed(void)
{
    u16 *screen;
    u16  i;

    DecompressDataWithHeaderVram(sS6Speed_Gfx, (void *)(BG_CHAR_ADDR(2)));
    DmaCopy16(3, sS6Speed_Tilemap, (void *)(BG_SCREEN_ADDR(28)), sizeof(sS6Speed_Tilemap));

    screen = (u16 *)(BG_SCREEN_ADDR(28));
    for (i = 0; i < sizeof(sS6Speed_Tilemap) / sizeof(u16); i++)
    {
        if (screen[i] & 0x03FF)
            screen[i] = (screen[i] & 0x0FFF) | (4 << 12);
    }

    LoadPalette(sS6Speed_Pal, BG_PLTT_ID(4), sizeof(sS6Speed_Pal));

    SetGpuReg(REG_OFFSET_BG0CNT,
              BGCNT_PRIORITY(0)
            | BGCNT_CHARBASE(2)
            | BGCNT_SCREENBASE(28)
            | BGCNT_16COLOR
            | BGCNT_TXT256x256);
    SetGpuReg(REG_OFFSET_BG0HOFS, 0);
    SetGpuReg(REG_OFFSET_BG0VOFS, 0);

    /* Enable BG0 now that tiles are loaded */
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0
                                 | DISPCNT_OBJ_1D_MAP
                                 | DISPCNT_BG0_ON
                                 | DISPCNT_BG1_ON
                                 | DISPCNT_OBJ_ON);
}

/*
 * S6_HideSpeed -- disable BG0 so galaxy (BG1) is the only visible layer.
 */
static void S6_HideSpeed(void)
{
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0
                                 | DISPCNT_OBJ_1D_MAP
                                 | DISPCNT_BG1_ON
                                 | DISPCNT_OBJ_ON);
}

/* --------------------------------------------------------------------------
 * Entry point for Screen 6
 * -------------------------------------------------------------------------- */
void CB2_StartCustomIntroScreen6(void)
{
    u8 taskId, backId, i;

    SetVBlankCallback(NULL);
    SetGpuReg(REG_OFFSET_DISPCNT, 0);
    CpuFill32(0, (void *)VRAM,       VRAM_SIZE);
    CpuFill32(0, (void *)OAM,        OAM_SIZE);
    CpuFill16(0, (void *)(PLTT + 2), PLTT_SIZE - 2);
    ResetPaletteFade();
    ScanlineEffect_Stop();
    ResetTasks();
    ResetSpriteData();
    FreeAllSpritePalettes();
    EnableInterrupts(INTR_FLAG_VBLANK);
    SetVBlankCallback(VBlankCB_CustomIntro);
    SetMainCallback2(MainCB2_CustomIntro);

    /* Galaxy BG -- S6_LoadGalaxy sets BG1CNT, palette, HOFS/VOFS */
    S6_LoadGalaxy();

    /* Only BG1 (galaxy) visible at start — BG0 (speed) enabled inside S6_LoadSpeed */
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0
                                 | DISPCNT_OBJ_1D_MAP
                                 | DISPCNT_BG1_ON
                                 | DISPCNT_OBJ_ON);

    /* Load all sprite sheets */
    for (i = 0; sS6Sheets[i].tag != 0; i++)
        LoadCompressedSpriteSheet(&sS6Sheets[i]);
    for (i = 0; sS6Palettes[i].tag != 0; i++)
        LoadSpritePalette(&sS6Palettes[i]);

    /* Back sprite -- bottom edge of screen, centre-x.
     * Sprite is 64x64; GBA OAM y is the top-left corner of the sprite.
     * DISPLAY_HEIGHT = 160, so y = 160 - 64 = 96 puts pixel row 0 at
     * screen row 96 and pixel row 63 exactly at screen row 159 (bottom). */
    backId = CreateSprite(&sS6Template_Back,
                          S6_DEOXYS_X,
                          128,
                          1);
    StartSpriteAnim(&gSprites[backId], 0);

    BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);

    taskId = CreateTask(Task_S6_FadeIn, 0);
    gTasks[taskId].tS6Timer  = 0;
    gTasks[taskId].tS6Passes = 0;
    gTasks[taskId].tS6BackId = backId;
}

/* --------------------------------------------------------------------------
 * Task: FadeIn -- wait for galaxy to appear, show back sprite, hold briefly
 * -------------------------------------------------------------------------- */
static void Task_S6_FadeIn(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        gTasks[taskId].tS6Timer = 0;
        gTasks[taskId].func     = Task_S6_BackHold;
    }
}

/* --------------------------------------------------------------------------
 * Task: BackHold -- hold back sprite S6_BACK_HOLD frames then start flashing
 * -------------------------------------------------------------------------- */
static void Task_S6_BackHold(u8 taskId)
{
    gTasks[taskId].tS6Timer++;
    if (gTasks[taskId].tS6Timer >= S6_BACK_HOLD)
    {
        u8 deoxId;

        /* Hide back sprite */
        gSprites[(u8)gTasks[taskId].tS6BackId].invisible = TRUE;

        /* Swap to the speed tileset -- S6_LoadSpeed() enables BG0 itself,
         * on top of the galaxy. THIS tileset is the "Deoxys moving fast"
         * art (motion-blur streak baked into the tiles), so it's the
         * thing that stays visible and scrolling for the whole flash
         * phase -- nothing hides it here. */
        S6_LoadSpeed();

        /* Deoxys' own sprite (deoxys.png) doesn't show yet. Create her
         * already at her final resting spot but invisible -- Task_S6_
         * SpeedFlash just flips visibility on once the flash phase ends.
         * Simple flow: back -> speed tileset slides -> Deoxys appears. */
        deoxId = CreateSprite(&sS6Template_Deoxys,
                              S6_DEOXYS_X,
                              S6_DEOXYS_Y,
                              1);
        gSprites[deoxId].invisible = TRUE;

        gTasks[taskId].tS6DeoxId   = deoxId;
        gTasks[taskId].tS6Timer    = 0;
        gTasks[taskId].tS6BgScroll = 0;
        gTasks[taskId].func        = Task_S6_SpeedFlash;
    }
}

/* --------------------------------------------------------------------------
 * Task: SpeedFlash
 * Speed tileset lives on BG0 (S6_LoadSpeed sets BG0CNT) -- this IS the
 * "Deoxys moving fast" art, motion-blur streak baked right into the
 * tiles. That's why BG0HOFS is what scrolls here, not BG1HOFS (galaxy,
 * which stays put and isn't even visible right now since BG0 covers it).
 *
 * tS6BgScroll is a free-running accumulator: every frame it subtracts
 * S6_FLASH_SPEED and the result goes straight to BG0HOFS. No clamp, no
 * reset, no sign flip anywhere -- a value that only ever goes one
 * direction cannot visually go back and forth. The map wrapping every
 * 256px is the intended continuous "rushing past" loop, not a bug.
 *
 * tS6Timer just counts frames up to S6_DEOXYS_SWEEP_FRAMES. Once the
 * flash has run long enough, hide the speed layer and reveal Deoxys'
 * sprite -- she's been sitting at her resting spot, invisible, since
 * Task_S6_BackHold -- then hand off to the form-cycling task.
 * -------------------------------------------------------------------------- */
static void Task_S6_SpeedFlash(u8 taskId)
{
    s16 timer = gTasks[taskId].tS6Timer;

    /* BG: one direction, forever. If it visibly reads right-to-left
     * instead of left-to-right for you, the only change needed is to
     * flip this one sign (change -= to +=). */
    gTasks[taskId].tS6BgScroll -= (s16)S6_FLASH_SPEED;
    SetGpuReg(REG_OFFSET_BG0HOFS, (u16)gTasks[taskId].tS6BgScroll);
    SetGpuReg(REG_OFFSET_BG1HOFS, (u16)gTasks[taskId].tS6BgScroll);

    timer++;
    gTasks[taskId].tS6Timer = timer;

    if (timer >= S6_DEOXYS_SWEEP_FRAMES)
    {
        u8 deoxId = (u8)gTasks[taskId].tS6DeoxId;

        /* Done -- hide speed layer, galaxy visible again. Reveal Deoxys,
         * already sitting at her resting spot, in her Normal form. */
        S6_HideSpeed();

        gSprites[deoxId].invisible = FALSE;
        StartSpriteAnim(&gSprites[deoxId], 0);

        gTasks[taskId].tS6Timer = 0;
        gTasks[taskId].func     = Task_S6_ShowForms;
    }
}


/* --------------------------------------------------------------------------
 * Task: ShowForms
 * Cycles through Deoxys forms 0->1->2->3, holding each S6_FORM_HOLD frames.
 * On each switch: spawns a transform-flash circle (expands outward like
 * the Transform move effect, then destroys itself).
 * tS6State tracks which form we're currently showing (0..3).
 * -------------------------------------------------------------------------- */
static void Task_S6_ShowForms(u8 taskId)
{
    u8 deoxId = (u8)gTasks[taskId].tS6DeoxId;

    gTasks[taskId].tS6Timer++;

    if (gTasks[taskId].tS6Timer >= S6_FORM_HOLD)
    {
        gTasks[taskId].tS6Timer = 0;
        gTasks[taskId].tS6State++;

        if (gTasks[taskId].tS6State > 3)
        {
            /* Locked on form 3 (Speed) -- spawn explosion circle */
            u8 beamId;
            StartSpriteAnim(&gSprites[deoxId], 3);

            beamId = CreateSprite(&sS6Template_Beam,
                                  S6_DEOXYS_X,
                                  S6_DEOXYS_Y,
                                  0);
            gSprites[beamId].data[0] = S6_BEAM_SCALE_START;
            gSprites[beamId].data[1] = 0;
            SetOamMatrix(8, S6_BEAM_SCALE_START, 0, 0, S6_BEAM_SCALE_START);
            StartSpriteAnim(&gSprites[beamId], 0);

            gTasks[taskId].tS6BeamId = beamId;
            gTasks[taskId].tS6Timer  = 0;
            gTasks[taskId].func      = Task_S6_BeamGrow;
            return;
        }

        /* Palette-flash task does the StartSpriteAnim itself, at the peak
         * of the flash (fully white), so the frame swap is invisible. */
        gTasks[taskId].tS6Timer = 0;
        gTasks[taskId].func     = Task_S6_TransformFlash;
        return;
    }
}

/* --------------------------------------------------------------------------
 * Task: TransformFlash -- palette-only white flash directly on Deoxys'
 * own OBJ palette slot. No extra sprite, no beam.png involved: BlendPalettes
 * only touches Deoxys' 16 colors, so nothing else on screen is affected.
 * The actual frame swap (StartSpriteAnim) happens at the exact peak
 * (fully white), so the swap itself is invisible.
 * -------------------------------------------------------------------------- */
#define S6_TRANSFORM_FRAMES_UP    6
#define S6_TRANSFORM_FRAMES_HOLD  4
#define S6_TRANSFORM_FRAMES_DOWN  6

static void Task_S6_TransformFlash(u8 taskId)
{
    u8  deoxId  = (u8)gTasks[taskId].tS6DeoxId;
    u8  palNum  = gSprites[deoxId].oam.paletteNum;
    u32 palMask = 1u << OBJ_PLTT_ID(palNum);   /* only Deoxys' own OBJ palette */
    s16 timer   = gTasks[taskId].tS6Timer;

    if (timer < S6_TRANSFORM_FRAMES_UP)
    {
        BlendPalettes(palMask, (u8)((timer * 16) / S6_TRANSFORM_FRAMES_UP), RGB_WHITE);
    }
    else if (timer == S6_TRANSFORM_FRAMES_UP)
    {
        StartSpriteAnim(&gSprites[deoxId], (u8)gTasks[taskId].tS6State);
        BlendPalettes(palMask, 16, RGB_WHITE);
    }
    else if (timer < S6_TRANSFORM_FRAMES_UP + S6_TRANSFORM_FRAMES_HOLD)
    {
        BlendPalettes(palMask, 16, RGB_WHITE);
    }
    else if (timer < S6_TRANSFORM_FRAMES_UP + S6_TRANSFORM_FRAMES_HOLD
                    + S6_TRANSFORM_FRAMES_DOWN)
    {
        s16 down = timer - (S6_TRANSFORM_FRAMES_UP + S6_TRANSFORM_FRAMES_HOLD);
        BlendPalettes(palMask, (u8)(16 - (down * 16) / S6_TRANSFORM_FRAMES_DOWN), RGB_WHITE);
    }
    else
    {
        BlendPalettes(palMask, 0, RGB_WHITE);
        gTasks[taskId].tS6Timer = 0;
        gTasks[taskId].func     = Task_S6_ShowForms;
        return;
    }

    gTasks[taskId].tS6Timer++;
}

/* --------------------------------------------------------------------------
 * Task: BeamGrow -- wait for explosion circle to fully expand then flash white
 * -------------------------------------------------------------------------- */
static void Task_S6_BeamGrow(u8 taskId)
{
    u8 beamId = (u8)gTasks[taskId].tS6BeamId;

    if (gSprites[beamId].data[1])
    {
        PlayCryInternal(SPECIES_DEOXYS, 0, 120, CRY_PRIORITY_NORMAL, CRY_MODE_NORMAL);
        PlaySE(SE_M_EXPLOSION);
        gTasks[taskId].tS6Timer = 0;
        gTasks[taskId].func     = Task_S6_Impact;
    }
}

/* --------------------------------------------------------------------------
 * Task: Impact -- hold white flash then fade to black
 * -------------------------------------------------------------------------- */
static void Task_S6_Impact(u8 taskId)
{
    if (gPaletteFade.active)
        return;

    gTasks[taskId].tS6Timer++;
    if (gTasks[taskId].tS6Timer >= S6_FLASH_HOLD)
    {
        gTasks[taskId].tS6Timer = 0;
        gTasks[taskId].func     = Task_S6_FadeOut;
    }
}

/* --------------------------------------------------------------------------
 * Task: FadeOut
 * -------------------------------------------------------------------------- */
static void Task_S6_FadeOut(u8 taskId)
{
    if (gTasks[taskId].tS6Timer == 0)
    {
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].tS6Timer = 1;
        return;
    }

    if (!gPaletteFade.active)
    {
        SetMainCallback2(CB2_StartCustomIntroScreen7);
        DestroyTask(taskId);
    }
}

/* =========================================================================
 * Undefine Screen 6 macros
 * ========================================================================= */
#undef TAG_S6_DEOXYS
#undef TAG_S6_BACK
#undef TAG_S6_BEAM
#undef S6_FLASH_SPEED
#undef S6_FLASH_PASSES
#undef S6_BACK_HOLD
#undef S6_FORM_HOLD
#undef S6_DEOXYS_SWEEP_FRAMES
#undef S6_FLASH_HOLD
#undef S6_DEOXYS_X
#undef S6_DEOXYS_Y
#undef S6_BEAM_SCALE_START
#undef S6_BEAM_SCALE_END
#undef S6_BEAM_GROW_FRAMES
#undef S6_TRANSFORM_FRAMES_UP
#undef S6_TRANSFORM_FRAMES_HOLD
#undef S6_TRANSFORM_FRAMES_DOWN
#undef tS6Timer
#undef tS6State
#undef tS6Passes
#undef tS6DeoxId
#undef tS6BackId
#undef tS6BeamId

/* =========================================================================
 * SCREEN 7 — Giovanni presses the button
 *
 * Scene flow:
 *   1. Fade in on bgfinal1 (Giovanni hand raised).
 *   2. Hold S7_HOLD1 frames (dramatic pause).
 *   3. Cross-fade to bgfinal2 (Giovanni hand pressing button).
 *        — Button sprite switches to frame 1 (pressed) at the same moment.
 *        — Play SE_SWITCH (button press sound).
 *   4. Hold S7_HOLD2 frames.
 *   5. Fade to black → title screen.
 *
 * Assets  (graphics/custom_intro/screen7/):
 *   bgfinal1.4bpp.smol + bgfinal1.bin.smolTM + bgfinal1.gbapal   — bg, hand raised
 *   bgfinal2.4bpp.smol + bgfinal2.bin.smolTM + bgfinal2.gbapal   — bg, hand pressing
 *   botton.4bpp.smol   + botton.gbapal                           — button OBJ (frame 0 = off, frame 1 = pressed)
 *
 * The button sprite (botton) is a 2-frame 16×16 OBJ positioned over
 * Giovanni's fingertip.  Adjust S7_BTN_X / S7_BTN_Y to fine-tune.
 * ========================================================================= */

/* --------------------------------------------------------------------------
 * Tags
 * -------------------------------------------------------------------------- */
#define TAG_S7_BTN  5600

/* --------------------------------------------------------------------------
 * Graphics
 * -------------------------------------------------------------------------- */
static const u16 sS7Bg1_Pal[]      = INCBIN_U16("graphics/custom_intro/screen7/bgfinal1.gbapal");
static const u32 sS7Bg1_Gfx[]      = INCBIN_U32("graphics/custom_intro/screen7/bgfinal1.4bpp.smol");
static const u32 sS7Bg1_Tilemap[]  = INCBIN_U32("graphics/custom_intro/screen7/bgfinal1.bin.smolTM");

static const u16 sS7Bg2_Pal[]      = INCBIN_U16("graphics/custom_intro/screen7/bgfinal2.gbapal");
static const u32 sS7Bg2_Gfx[]      = INCBIN_U32("graphics/custom_intro/screen7/bgfinal2.4bpp.smol");
static const u32 sS7Bg2_Tilemap[]  = INCBIN_U32("graphics/custom_intro/screen7/bgfinal2.bin.smolTM");

/* Button: 2-frame 16×16 sprite (frame 0 = idle, frame 1 = pressed) */
static const u16 sS7Btn_Pal[]      = INCBIN_U16("graphics/custom_intro/screen7/botton.gbapal");
static const u32 sS7Btn_Gfx[]      = INCBIN_U32("graphics/custom_intro/screen7/botton.4bpp.smol");

/* --------------------------------------------------------------------------
 * Sprite sheet / palette
 * -------------------------------------------------------------------------- */
static const struct CompressedSpriteSheet sS7SpriteSheets[] =
{
    {sS7Btn_Gfx, 0x100, TAG_S7_BTN},  /* 16×32 PNG = 2 frames of 16×16 4bpp = 256 bytes */
    {},
};
static const struct SpritePalette sS7SpritePalettes[] =
{
    {sS7Btn_Pal, TAG_S7_BTN},
    {},
};

/* --------------------------------------------------------------------------
 * OAM: 16×16 button sprite, high priority (in front of BG)
 * -------------------------------------------------------------------------- */
static const struct OamData sS7OamData_Btn =
{
    .y          = DISPLAY_HEIGHT,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode    = ST_OAM_OBJ_NORMAL,
    .bpp        = ST_OAM_4BPP,
    .shape      = SPRITE_SHAPE(16x16),
    .size       = SPRITE_SIZE(16x16),
    .priority   = 0,   /* in front of all BG layers */
};

/* --------------------------------------------------------------------------
 * Animations
 * -------------------------------------------------------------------------- */
static const union AnimCmd sS7Anim_BtnIdle[] =
{
    ANIMCMD_FRAME(0, 0),   /* frame 0: tile offset 0 */
    ANIMCMD_END,
};
static const union AnimCmd sS7Anim_BtnPressed[] =
{
    ANIMCMD_FRAME(4, 0),   /* frame 1: tile offset 4 (16×16 = 4 tiles of 8×8) */
    ANIMCMD_END,
};
static const union AnimCmd *const sS7Anims_Btn[] =
{
    sS7Anim_BtnIdle,     /* anim 0 — button not pressed */
    sS7Anim_BtnPressed,  /* anim 1 — button pressed     */
};

/* --------------------------------------------------------------------------
 * Template
 * -------------------------------------------------------------------------- */
static void SpriteCB_S7_Btn(struct Sprite *sprite) { (void)sprite; }

static const struct SpriteTemplate sS7Template_Btn =
{
    .tileTag    = TAG_S7_BTN,
    .paletteTag = TAG_S7_BTN,
    .oam        = &sS7OamData_Btn,
    .anims      = sS7Anims_Btn,
    .callback   = SpriteCB_S7_Btn,
};

/* --------------------------------------------------------------------------
 * Timing constants
 *   S7_HOLD1   — pause on bgfinal1 before the button press
 *   S7_HOLD2   — pause on bgfinal2 after the button press
 *
 * Button position:
 *   S7_BTN_X / S7_BTN_Y — pixel position of the button OBJ on screen.
 *   Adjust to align with Giovanni's fingertip on bgfinal2.
 * -------------------------------------------------------------------------- */
#define S7_HOLD1    90    /* ~1.5 s on bgfinal1 before pressing         */
#define S7_HOLD2    60    /* ~1.0 s on bgfinal2 after pressing          */

/* Fine-tune these to place the button over Giovanni's fingertip:
 *   increase S7_BTN_X → move right
 *   increase S7_BTN_Y → move down                                      */
#define S7_BTN_X    107   /* X centre of button sprite — increase to move right  */
#define S7_BTN_Y     90   /* Y centre of button sprite — increase to move down   */

/* --------------------------------------------------------------------------
 * Task data aliases
 * -------------------------------------------------------------------------- */
#define tS7Timer  data[0]
#define tS7BtnId  data[1]

/* --------------------------------------------------------------------------
 * Forward declarations
 * -------------------------------------------------------------------------- */
static void Task_S7_FadeIn(u8 taskId);
static void Task_S7_Hold1(u8 taskId);
static void Task_S7_PressButton(u8 taskId);
static void Task_S7_Hold2(u8 taskId);
static void Task_S7_FadeOut(u8 taskId);

/* --------------------------------------------------------------------------
 * Entry point
 * -------------------------------------------------------------------------- */
void CB2_StartCustomIntroScreen7(void)
{
    u8 taskId, i, btnId;

    SetVBlankCallback(NULL);
    SetGpuReg(REG_OFFSET_DISPCNT, 0);
    CpuFill32(0, (void *)VRAM,       VRAM_SIZE);
    CpuFill32(0, (void *)OAM,        OAM_SIZE);
    CpuFill16(0, (void *)(PLTT + 2), PLTT_SIZE - 2);
    ResetPaletteFade();
    ScanlineEffect_Stop();
    ResetTasks();
    ResetSpriteData();
    FreeAllSpritePalettes();
    EnableInterrupts(INTR_FLAG_VBLANK);
    SetVBlankCallback(VBlankCB_CustomIntro);
    SetMainCallback2(MainCB2_CustomIntro);

    /* Load bgfinal1 into BG1 */
    DecompressDataWithHeaderVram(sS7Bg1_Gfx,    (void *)(BG_CHAR_ADDR(0)));
    DecompressDataWithHeaderVram(sS7Bg1_Tilemap, (void *)(BG_SCREEN_ADDR(16)));
    LoadPalette(sS7Bg1_Pal, BG_PLTT_ID(0), sizeof(sS7Bg1_Pal));

    SetGpuReg(REG_OFFSET_BG1CNT,
              BGCNT_PRIORITY(3)
            | BGCNT_CHARBASE(0)
            | BGCNT_SCREENBASE(16)
            | BGCNT_16COLOR
            | BGCNT_TXT256x256);
    SetGpuReg(REG_OFFSET_BG1HOFS, 0);
    SetGpuReg(REG_OFFSET_BG1VOFS, 0);

    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0
                                 | DISPCNT_OBJ_1D_MAP
                                 | DISPCNT_BG1_ON
                                 | DISPCNT_OBJ_ON);

    /* Load button sprite — created now so it appears from the start */
    for (i = 0; sS7SpriteSheets[i].tag != 0; i++)
        LoadCompressedSpriteSheet(&sS7SpriteSheets[i]);
    for (i = 0; sS7SpritePalettes[i].tag != 0; i++)
        LoadSpritePalette(&sS7SpritePalettes[i]);

    btnId = CreateSprite(&sS7Template_Btn, S7_BTN_X, S7_BTN_Y, 0);
    StartSpriteAnim(&gSprites[btnId], 0);   /* anim 0: button idle */

    taskId = CreateTask(Task_S7_FadeIn, 0);
    gTasks[taskId].tS7Timer = 0;
    gTasks[taskId].tS7BtnId = btnId;

    BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
}

/* --------------------------------------------------------------------------
 * Task: FadeIn — wait for bgfinal1 to fade in
 * -------------------------------------------------------------------------- */
static void Task_S7_FadeIn(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        gTasks[taskId].tS7Timer = 0;
        gTasks[taskId].func     = Task_S7_Hold1;
    }
}

/* --------------------------------------------------------------------------
 * Task: Hold1 — dramatic pause on bgfinal1 before Giovanni presses the button
 * -------------------------------------------------------------------------- */
static void Task_S7_Hold1(u8 taskId)
{
    gTasks[taskId].tS7Timer++;
    if (gTasks[taskId].tS7Timer >= S7_HOLD1)
    {
        gTasks[taskId].tS7Timer = 0;
        gTasks[taskId].func     = Task_S7_PressButton;
    }
}

/* --------------------------------------------------------------------------
 * Task: PressButton
 *   State 0: trigger button press visuals and sound (OAM/palette only,
 *            safe mid-frame).
 *   State 1: force the display blank before touching BG VRAM, swap
 *            bgfinal1 -> bgfinal2, then restore DISPCNT. This is the same
 *            technique used by every other screen transition in this file
 *            (see Task_S5_FadeToBirds) — waiting a frame and hoping it
 *            lands in VBlank is NOT enough, since RunTasks() runs in the
 *            main loop, not the VBlank ISR, so the old "wait one frame"
 *            version could still write mid-scanline and produce the
 *            yellow flash.
 * -------------------------------------------------------------------------- */
static void Task_S7_PressButton(u8 taskId)
{
    u8 btnId = (u8)gTasks[taskId].tS7BtnId;

    if (gTasks[taskId].tS7Timer == 0)
    {
        /* Frame N: play sound and switch button sprite immediately —
         * these touch OAM/palette only (safe mid-frame), not BG VRAM. */
        StartSpriteAnim(&gSprites[btnId], 1);
        PlaySE(SE_PC_LOGIN);
        gTasks[taskId].tS7Timer = 1;
        return;
    }

    /* Force a real blank for the duration of the BG VRAM rewrite — this
     * guarantees no partial/mixed tile data is ever scanned out, instead
     * of relying on task-frame timing to line up with hardware VBlank. */
    SetGpuReg(REG_OFFSET_DISPCNT, 0);

    DecompressDataWithHeaderVram(sS7Bg2_Gfx,    (void *)(BG_CHAR_ADDR(0)));
    DecompressDataWithHeaderVram(sS7Bg2_Tilemap, (void *)(BG_SCREEN_ADDR(16)));
    LoadPalette(sS7Bg2_Pal, BG_PLTT_ID(0), sizeof(sS7Bg2_Pal));

    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0
                                 | DISPCNT_OBJ_1D_MAP
                                 | DISPCNT_BG1_ON
                                 | DISPCNT_OBJ_ON);

    gTasks[taskId].tS7Timer = 0;
    gTasks[taskId].func     = Task_S7_Hold2;
}

/* --------------------------------------------------------------------------
 * Task: Hold2 — brief hold after the button press before fade-out
 * -------------------------------------------------------------------------- */
static void Task_S7_Hold2(u8 taskId)
{
    gTasks[taskId].tS7Timer++;
    if (gTasks[taskId].tS7Timer >= S7_HOLD2)
    {
        gTasks[taskId].tS7Timer = 0;
        gTasks[taskId].func     = Task_S7_FadeOut;
    }
}

/* --------------------------------------------------------------------------
 * Task: FadeOut — fade to black and hand off to the title screen
 * -------------------------------------------------------------------------- */
static void Task_S7_FadeOut(u8 taskId)
{
    if (gTasks[taskId].tS7Timer == 0)
    {
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].tS7Timer = 1;
        return;
    }

    if (!gPaletteFade.active)
    {
        SetMainCallback2(CB2_InitTitleScreen);
        DestroyTask(taskId);
    }
}

/* =========================================================================
 * Undefine Screen 7 macros
 * ========================================================================= */
#undef TAG_S7_BTN
#undef S7_HOLD1
#undef S7_HOLD2
#undef S7_BTN_X
#undef S7_BTN_Y
#undef tS7Timer
#undef tS7BtnId
