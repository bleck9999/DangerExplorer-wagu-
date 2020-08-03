#include <stdio.h>
#include <string.h>
#include "mainmenu.h"
#include "../utils/util.h"
#include "utils/tools.h"
#include "../utils/btn.h"
#include "emmc/emmc.h"
#include "../storage/emummc.h"
#include "script/functions.h"

#include "common/common.h"
#include "gfx/menu.h"

#include "utils/utils.h"
#include "gfx/gfxutils.h"
#include "fs/fsutils.h"
#include "fs/fsmenu.h"
#include "emmc/emmcoperations.h"
#include "emmc/emmcmenu.h"
#include "../storage/nx_sd.h"
//#include "../hid/joycon.h"
#include "../hid/hid.h"
/*
extern bool sd_mount();
extern void sd_unmount();
*/
extern int launch_payload(char *path);
extern bool sd_inited;
extern bool sd_mounted;
extern bool disableB;

int res = 0, meter = 0;

void MainMenu_SDCard(){
    fileexplorer("SD:/", 0);
}

void MainMenu_EMMC(){
    makeMmcMenu(SYSMMC);
}

void MainMenu_EMUMMC(){
    makeMmcMenu(EMUMMC);
}

void MainMenu_MountSD(){
    (sd_mounted) ? sd_unmount() : sd_mount();
}

void MainMenu_Tools(){
    res = menu_make(mainmenu_tools, 4, "-- Tools Menu --");

    switch(res){
        case TOOLS_DISPLAY_INFO:
            displayinfo();
            break;
        case TOOLS_DISPLAY_GPIO:
            displaygpio();
            break;
        case TOOLS_DUMPFIRMWARE:
            res = menu_make(fwDump_typeMenu, 4, "-- Fw Type --");
            if (res >= 2)
                dumpfirmware(SYSMMC, (res == 2));

            break;
    }
}

void MainMenu_SDFormat(){
    res = menu_make(mainmenu_format, 3, "-- Format Menu --");

    if (res > 0){
        if (format(res)){
            sd_unmount();
        }
    }
}

void MainMenu_Credits(){
    if (++meter >= 3)
        gfx_errDisplay("credits", 53, 0);
    gfx_message(COLOR_WHITE, mainmenu_credits);
}

void MainMenu_Exit(){
    if (sd_mounted){
        SETBIT(mainmenu_shutdown[4].property, ISHIDE, !fsutil_checkfile("/bootloader/update.bin"));
        SETBIT(mainmenu_shutdown[5].property, ISHIDE, !fsutil_checkfile("/atmosphere/reboot_payload.bin"));
    }
    else {
        for (int i = 4; i <= 5; i++)
            SETBIT(mainmenu_shutdown[i].property, ISHIDE, 1);
    }

    res = menu_make(mainmenu_shutdown, 6, "-- Shutdown Menu --");
    
    switch(res){
        case SHUTDOWN_REBOOT_RCM:
            reboot_rcm();
                    
        case SHUTDOWN_REBOOT_NORMAL:
            reboot_normal();

        case SHUTDOWN_POWER_OFF:
            power_off();

        case SHUTDOWN_HEKATE:
            launch_payload("/bootloader/update.bin");
                    
        case SHUTDOWN_AMS:
            launch_payload("/atmosphere/reboot_payload.bin");
    } //todo declock bpmp
    
}

func_void_ptr mainmenu_functions[] = {
    MainMenu_SDCard,
    MainMenu_EMMC,
    MainMenu_EMUMMC,
    MainMenu_MountSD,
    MainMenu_Tools,
    MainMenu_SDFormat,
    MainMenu_Credits,
    MainMenu_Exit,
};

void RunMenuOption(int option){
    if (option != 7)
        meter = 0;
    if (option > 0)
        mainmenu_functions[option - 1]();
}
void te_main(){
    int setter;

    //gfx_printf("Initing controller\n");
    hidInit();

    //gfx_printf("Getting biskeys\n");
    if (dump_biskeys() == -1){
        gfx_errDisplay("dump_biskey", ERR_BISKEY_DUMP_FAILED, 0);
        //mainmenu_main[1].property |= ISHIDE;
    }

    //gfx_printf("Mounting SD\n");
    sd_mount();

    //gfx_printf("Loading possible EMU\n");
    if (emummc_load_cfg()){
        mainmenu_main[2].property |= ISHIDE;
    }

    //gfx_printf("Dumping gpt\n");
    dumpGpt();

    //gfx_printf("Disconnecting EMMC\n");
    disconnect_mmc();

    //gfx_printf("Entering main menu\n");
    while (1){
        setter = sd_mounted;

        if (emu_cfg.enabled){
            SETBIT(mainmenu_main[2].property, ISHIDE, !setter);
        }

        SETBIT(mainmenu_main[0].property, ISHIDE, !setter);
        mainmenu_main[3].name = (menu_sd_states[!setter]);

        setter = sd_inited;
        SETBIT(mainmenu_main[5].property, ISHIDE, !setter);

        disableB = true;
        res = menu_make(mainmenu_main, 8, "-- Main Menu --") + 1;
        disableB = false;

        RunMenuOption(res);
    }
}
