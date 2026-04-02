#include "global.h"
#include "trainer_pokemon_sprites.h"
#include "bg.h"
#include "battle_records.h"
#include "battle_setup.h"
#include "constants/rgb.h"
#include "constants/songs.h"
#include "constants/trainers.h"
#include "data.h"
#include "decompress.h"
#include "event_data.h"
#include "field_effect.h"
#include "field_message_box.h"
#include "field_weather.h"
#include "gpu_regs.h"
#include "graphics.h"
#include "international_string_util.h"
#include "link.h"
#include "load_save.h"
#include "main.h"
#include "main_menu.h"
#include "menu.h"
#include "list_menu.h"
#include "mystery_event_menu.h"
#include "naming_screen.h"
#include "option_menu.h"
#include "overworld.h"
#include "palette.h"
#include "pokeball.h"
#include "pokedex.h"
#include "pokemon.h"
#include "random.h"
#include "rtc.h"
#include "save.h"
#include "scanline_effect.h"
#include "script.h"
#include "script_pokemon_util.h"
#include "sound.h"
#include "sprite.h"
#include "strings.h"
#include "string_util.h"
#include "task.h"
#include "teamrocket.h"
#include "text.h"
#include "text_window.h"
#include "title_screen.h"
#include "union_room.h"
#include "window.h"
#include "mystery_gift_menu.h"

#define BIRCH_DLG_BASE_TILE_NUM 0xFC


static const u32 sTeamRocket1Gfx[] = INCBIN_U32("graphics/speech/TeamRocket1.4bpp.lz");
static const u32 sTeamRocket2Gfx[] = INCBIN_U32("graphics/speech/TeamRocket2.4bpp.lz");
static const u32 sTeamRocket3Gfx[] = INCBIN_U32("graphics/speech/TeamRocket3.4bpp.lz");
static const u32 sTeamRocket4Gfx[] = INCBIN_U32("graphics/speech/TeamRocket4.4bpp.lz");
static const u32 sTeamRocket5Gfx[] = INCBIN_U32("graphics/speech/TeamRocket5.4bpp.lz");
static const u32 sTeamRocket6Gfx[] = INCBIN_U32("graphics/speech/TeamRocket6.4bpp.lz");
static const u32 sTeamRocket7Gfx[] = INCBIN_U32("graphics/speech/TeamRocket7.4bpp.lz");
static const u32 sTeamRocket8Gfx[] = INCBIN_U32("graphics/speech/TeamRocket8.4bpp.lz");
static const u32 sTeamRocket1Tilemap[] = INCBIN_U32("graphics/speech/introTR.bin.lz");
static const u8 sTeamRocketIntro1Pal[] = INCBIN_U8("graphics/speech/TeamRocket1.gbapal");
static const u8 sTeamRocketIntro2Pal[] = INCBIN_U8("graphics/speech/TeamRocket2.gbapal");
static const u8 sTeamRocketIntro3Pal[] = INCBIN_U8("graphics/speech/TeamRocket3.gbapal");
static const u8 sTeamRocketIntro4Pal[] = INCBIN_U8("graphics/speech/TeamRocket4.gbapal");
static const u8 sTeamRocketIntro5Pal[] = INCBIN_U8("graphics/speech/TeamRocket5.gbapal");
static const u8 sTeamRocketIntro6Pal[] = INCBIN_U8("graphics/speech/TeamRocket6.gbapal");
static const u8 sTeamRocketIntro7Pal[] = INCBIN_U8("graphics/speech/TeamRocket7.gbapal");
static const u8 sTeamRocketIntro8Pal[] = INCBIN_U8("graphics/speech/TeamRocket8.gbapal");
static void Task_TeamRocket_Intro(u8);
static void RocketIntro_ShowDialogueWindow(u8, u8);
static void RocketIntro_CreateDialogueWindowBorder(u8, u8, u8, u8, u8, u8);
static void CB2_TRIntro(void);
void TeamRocketIntro(void);

static void RocketIntro_CreateDialogueWindowBorder(u8 bg, u8 x, u8 y, u8 width, u8 height, u8 palNum)
{
    FillBgTilemapBufferRect(bg, BIRCH_DLG_BASE_TILE_NUM +  1, x-2,       y-1, 1,       1, palNum);
    FillBgTilemapBufferRect(bg, BIRCH_DLG_BASE_TILE_NUM +  3, x-1,       y-1, 1,       1, palNum);
    FillBgTilemapBufferRect(bg, BIRCH_DLG_BASE_TILE_NUM +  4, x,         y-1, width,   1, palNum);
    FillBgTilemapBufferRect(bg, BIRCH_DLG_BASE_TILE_NUM +  5, x+width-1, y-1, 1,       1, palNum);
    FillBgTilemapBufferRect(bg, BIRCH_DLG_BASE_TILE_NUM +  6, x+width,   y-1, 1,       1, palNum);
    FillBgTilemapBufferRect(bg, BIRCH_DLG_BASE_TILE_NUM +  7, x-2,       y,   1,       5, palNum);
    FillBgTilemapBufferRect(bg, BIRCH_DLG_BASE_TILE_NUM +  9, x-1,       y,   width+1, 5, palNum);
    FillBgTilemapBufferRect(bg, BIRCH_DLG_BASE_TILE_NUM + 10, x+width,   y,   1,       5, palNum);

    FillBgTilemapBufferRect(bg, BG_TILE_V_FLIP(BIRCH_DLG_BASE_TILE_NUM + 1), x-2,       y+height, 1,       1, palNum);
    FillBgTilemapBufferRect(bg, BG_TILE_V_FLIP(BIRCH_DLG_BASE_TILE_NUM + 3), x-1,       y+height, 1,       1, palNum);
    FillBgTilemapBufferRect(bg, BG_TILE_V_FLIP(BIRCH_DLG_BASE_TILE_NUM + 4), x,         y+height, width-1, 1, palNum);
    FillBgTilemapBufferRect(bg, BG_TILE_V_FLIP(BIRCH_DLG_BASE_TILE_NUM + 5), x+width-1, y+height, 1,       1, palNum);
    FillBgTilemapBufferRect(bg, BG_TILE_V_FLIP(BIRCH_DLG_BASE_TILE_NUM + 6), x+width,   y+height, 1,       1, palNum);
}

static void RocketIntro_ShowDialogueWindow(u8 windowId, u8 copyToVram)
{
    CallWindowFunction(windowId, RocketIntro_CreateDialogueWindowBorder);
    FillWindowPixelBuffer(windowId, PIXEL_FILL(1));
    PutWindowTilemap(windowId);
    if (copyToVram == TRUE)
        CopyWindowToVram(windowId, COPYWIN_FULL);
}

static const struct BgTemplate sRocketBgTemplate = {
    .bg = 1,
    .charBaseIndex = 0,
    .mapBaseIndex = 10,
    .priority = 1,
    .baseTile = 0,
};

static const struct BgTemplate sRocketTextBgTemplate = {
    .bg = 0,
    .charBaseIndex = 2,
    .mapBaseIndex = 31,
    .priority = 0,
    .baseTile = 0,
};

static const struct WindowTemplate sWindowTemplates[] =
{
    {
        .bg = 0,
        .tilemapLeft = 3,
        .tilemapTop = 15,
        .width = 24,
        .height = 4,
        .paletteNum = 14,
        .baseBlock = 0x0200
    },
    DUMMY_WIN_TEMPLATE,
};

void TeamRocketIntro(void)
{
    SetMainCallback2(CB2_TRIntro);
    CreateTask(Task_TeamRocket_Intro, 0);
}

static void VBlankCB_TRintro(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void CB2_TRIntro(void)
{
    RunTasks();
    RunTextPrinters();
    AnimateSprites();
    BuildOamBuffer();
    DoScheduledBgTilemapCopiesToVram();
    UpdatePaletteFade();
}


static void Task_TeamRocket_Intro(u8 taskId)
{
    switch(gTasks[taskId].data[3])
    {
    case 0:
        SetVBlankCallback(NULL);
        SetGpuReg(REG_OFFSET_DISPCNT, 0);
        SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
        SetGpuReg(REG_OFFSET_BG1VOFS, 0);
        SetGpuReg(REG_OFFSET_BG1HOFS, 0);
        ResetBgsAndClearDma3BusyFlags(0);
        InitBgFromTemplate(&sRocketBgTemplate);
        InitBgFromTemplate(&sRocketTextBgTemplate);
        ChangeBgX(0, 0, BG_COORD_SET);
        ChangeBgY(0, 0, BG_COORD_SET);
        ChangeBgX(1, 0, BG_COORD_SET);
        ChangeBgY(1, 0, BG_COORD_SET);
        InitWindows(sWindowTemplates);
        SetGpuReg(REG_OFFSET_WIN0H, 0);
        SetGpuReg(REG_OFFSET_WIN0V, 0);
        SetGpuReg(REG_OFFSET_WININ, 0);
        SetGpuReg(REG_OFFSET_WINOUT, 0);
        SetGpuReg(REG_OFFSET_BLDCNT, 0);
        SetGpuReg(REG_OFFSET_BLDALPHA, 0);
        SetGpuReg(REG_OFFSET_BLDY, 0);
        CpuFill16(0, (void*)VRAM, VRAM_SIZE);
        DecompressDataWithHeaderVram(sTeamRocket1Tilemap, (void *)(BG_SCREEN_ADDR(10)));
        DeactivateAllTextPrinters();
        ClearScheduledBgCopiesToVram();
        ScanlineEffect_Stop();
        ResetSpriteData();
        ResetPaletteFade();
        FreeAllSpritePalettes();
        ResetAllPicSprites();
        PlayBGM(MUS_TEAM_ROCKET_MOTTO);
        LoadPalette(GetOverworldTextboxPalettePtr(), BG_PLTT_ID(14), PLTT_SIZE_4BPP);
        LoadUserWindowBorderGfx(0, 0x2A8, BG_PLTT_ID(13));
        DrawStdFrameWithCustomTileAndPalette(0, TRUE, 0x2A8, 0xD);
        LoadPalette(sTeamRocketIntro1Pal, BG_PLTT_ID(0), PLTT_SIZEOF(16));
        // RocketIntro_ShowDialogueWindow(0, 1);
        ShowBg(1);
        ShowBg(0);
        SetVBlankCallback(VBlankCB_TRintro);
        gTasks[taskId].data[3]++;
        break;
    case 1:
        DecompressDataWithHeaderVram(sTeamRocket1Gfx, (void *)VRAM);
        AddTextPrinterParameterized(0, FONT_NORMAL, gText_TRProtect_The_World, 0, 0, 0, NULL);
        CopyWindowToVram(0, COPYWIN_FULL);
        gTasks[taskId].data[3]++;
        break;
    case 2:
        if (JOY_NEW(A_BUTTON))
            gTasks[taskId].data[3]++;
        break;
    case 3:
        LoadPalette(sTeamRocketIntro2Pal, BG_PLTT_ID(0), PLTT_SIZEOF(16));
        DecompressDataWithHeaderVram(sTeamRocket1Tilemap, (void *)(BG_SCREEN_ADDR(10)));
        DecompressDataWithHeaderVram(sTeamRocket2Gfx, (void *)VRAM);
        FillWindowPixelBuffer(0, PIXEL_FILL(1));
        AddTextPrinterParameterized(0, FONT_NORMAL, gText_TRUnite_The_People, 0, 0, 0, NULL);
        CopyWindowToVram(0, COPYWIN_FULL);
        gTasks[taskId].data[3]++;
        break;
    case 4:
        if (JOY_NEW(A_BUTTON))
            gTasks[taskId].data[3]++;
        break;
    case 5:
        LoadPalette(sTeamRocketIntro3Pal, BG_PLTT_ID(0), PLTT_SIZEOF(16));
        DecompressDataWithHeaderVram(sTeamRocket1Tilemap, (void *)(BG_SCREEN_ADDR(10)));
        DecompressDataWithHeaderVram(sTeamRocket3Gfx, (void *)VRAM);
        FillWindowPixelBuffer(0, PIXEL_FILL(1));
        AddTextPrinterParameterized(0, FONT_NORMAL, gText_TRTo_Denounce_Evil, 0, 0, 0, NULL);
        CopyWindowToVram(0, COPYWIN_FULL);
        gTasks[taskId].data[3]++;
        break;
    case 6:
        if (JOY_NEW(A_BUTTON))
            gTasks[taskId].data[3]++;
        break;
    case 7:
        LoadPalette(sTeamRocketIntro4Pal, BG_PLTT_ID(0), PLTT_SIZEOF(16));
        DecompressDataWithHeaderVram(sTeamRocket1Tilemap, (void *)(BG_SCREEN_ADDR(10)));
        DecompressDataWithHeaderVram(sTeamRocket4Gfx, (void *)VRAM);
        FillWindowPixelBuffer(0, PIXEL_FILL(1));
        AddTextPrinterParameterized(0, FONT_NORMAL, gText_TRTo_Extend_Reach, 0, 0, 0, NULL);
        CopyWindowToVram(0, COPYWIN_FULL);
        gTasks[taskId].data[3]++;
        break;
    case 8:
        if (JOY_NEW(A_BUTTON))
            gTasks[taskId].data[3]++;
        break;
    case 9:
        LoadPalette(sTeamRocketIntro4Pal, BG_PLTT_ID(0), PLTT_SIZEOF(16));
        DecompressDataWithHeaderVram(sTeamRocket1Tilemap, (void *)(BG_SCREEN_ADDR(10)));
        DecompressDataWithHeaderVram(sTeamRocket4Gfx, (void *)VRAM);
        FillWindowPixelBuffer(0, PIXEL_FILL(1));
        AddTextPrinterParameterized(0, FONT_NORMAL, gText_TRThe_Stars_Above, 0, 0, 0, NULL);
        CopyWindowToVram(0, COPYWIN_FULL);
        gTasks[taskId].data[3]++;
        break;
    case 10:
        if (JOY_NEW(A_BUTTON))
            gTasks[taskId].data[3]++;
        break;
    case 11:
        LoadPalette(sTeamRocketIntro5Pal, BG_PLTT_ID(0), PLTT_SIZEOF(16));
        DecompressDataWithHeaderVram(sTeamRocket1Tilemap, (void *)(BG_SCREEN_ADDR(10)));
        DecompressDataWithHeaderVram(sTeamRocket5Gfx, (void *)VRAM);
        FillWindowPixelBuffer(0, PIXEL_FILL(1));
        AddTextPrinterParameterized(0, FONT_NORMAL, gText_TRJessie, 0, 0, 0, NULL);
        CopyWindowToVram(0, COPYWIN_FULL);
        gTasks[taskId].data[3]++;
        break;
    case 12:
        if (JOY_NEW(A_BUTTON))
            gTasks[taskId].data[3]++;
        break;
    case 13:
        LoadPalette(sTeamRocketIntro6Pal, BG_PLTT_ID(0), PLTT_SIZEOF(16));
        DecompressDataWithHeaderVram(sTeamRocket1Tilemap, (void *)(BG_SCREEN_ADDR(10)));
        DecompressDataWithHeaderVram(sTeamRocket6Gfx, (void *)VRAM);
        FillWindowPixelBuffer(0, PIXEL_FILL(1));
        AddTextPrinterParameterized(0, FONT_NORMAL, gText_TRJames, 0, 0, 0, NULL);
        CopyWindowToVram(0, COPYWIN_FULL);
        gTasks[taskId].data[3]++;
        break;
    case 14:
        if (JOY_NEW(A_BUTTON))
            gTasks[taskId].data[3]++;
        break;
    case 15:
        LoadPalette(sTeamRocketIntro7Pal, BG_PLTT_ID(0), PLTT_SIZEOF(16));
        DecompressDataWithHeaderVram(sTeamRocket1Tilemap, (void *)(BG_SCREEN_ADDR(10)));
        DecompressDataWithHeaderVram(sTeamRocket7Gfx, (void *)VRAM);
        FillWindowPixelBuffer(0, PIXEL_FILL(1));
        AddTextPrinterParameterized(0, FONT_NORMAL, gText_TRBlast_Off, 0, 0, 0, NULL);
        CopyWindowToVram(0, COPYWIN_FULL);
        gTasks[taskId].data[3]++;
        break;
    case 16:
        if (JOY_NEW(A_BUTTON))
            gTasks[taskId].data[3]++;
        break;
    case 17:
        LoadPalette(sTeamRocketIntro8Pal, BG_PLTT_ID(0), PLTT_SIZEOF(16));
        LZ77UnCompVram(sTeamRocket1Tilemap, (void *)(BG_SCREEN_ADDR(10)));
        LZ77UnCompVram(sTeamRocket8Gfx, (void *)VRAM);
        FillWindowPixelBuffer(0, PIXEL_FILL(1));
        AddTextPrinterParameterized(0, FONT_NORMAL, gText_TRMeowth_Thats_Right, 0, 0, 0, NULL);
        CopyWindowToVram(0, COPYWIN_FULL);
        gTasks[taskId].data[3]++;
        break;
    case 18:
        if (JOY_NEW(A_BUTTON))
            gTasks[taskId].data[3]++;
        break;
    case 19:
        if (JOY_NEW(A_BUTTON))
        {
            FadeScreen(FADE_TO_BLACK, 0);
            gTasks[taskId].data[3]++;
        }
        break;
    default:
        if (!gPaletteFade.active)
        {
        ScriptContext_Enable();
        SetMainCallback2(CB2_ReturnToFieldContinueScriptPlayMapMusic);
        DestroyTask(taskId);
        }
        break;
    }
}
