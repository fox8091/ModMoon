#include <string.h>
#include <3ds.h>
#include <stdio.h>
#include <unistd.h>
#include <sstream>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>

#include "utils.hpp"
#include "main.hpp"
#include "error.hpp"

using namespace std;

Handle event_fadefinished;

inline bool pathExist(const string filename){ //The compiler doesn't like me defining this via a forward definition, so copy/paste. Thanks GCC
    struct stat buffer;
    return (stat (filename.c_str(),& buffer)==0);
}

int maxslotcheck(u64 optionaltid)
{
	int currentFolderCount = 0;
	stringstream path2Check;
	stringstream NewPath2Check;
	string tid2check = optionaltid != 1 ? tid2str(optionaltid) : currenttitleidstr;
	do {
		currentFolderCount++;
		path2Check.str("");
		path2Check << modsfolder + tid2check << '/' << "Slot_" << currentFolderCount << '/';
	} while (pathExist(path2Check.str()) || currentFolderCount == currentslot);
	//the minus 1 is due to it returning the folder number that doesnt exist.
	return currentFolderCount - 1;
}


void threadfunc_fade(void* main)
{
	int alpha = 0;
	int* rgbvalues = (int*)main; //We expect an array of 3 ints for the RGB values
	//C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE_MINUS_CONSTANT_ALPHA, GPU_CONSTANT_COLOR, GPU_ONE_MINUS_CONSTANT_ALPHA, GPU_CONSTANT_ALPHA);
	//I tried to use blending operations but the equation I used didn't like an alpha value of < 1 for a texture, and overwrote it entirely with the color. It looked gross.
	while(alpha <= 255)
	{
		alpha += 3;
		draw.framestart();
		draw.drawtexture(backgroundtop, 0, 0);
		int bannerx = 400/2 - banner.width/2;
		float bannery = 240/2 - banner.height/2;
		bannery += 6.0f*sinf(C3D_Angle(minusy)); //Wr're not moving it, but we are keeping it where it's at.
		draw.drawtexture(banner, bannerx, bannery);
		draw.drawrectangle(0, 0, 400, 240, RGBA8(rgbvalues[0], rgbvalues[1], rgbvalues[2], alpha)); //Overlay an increasingly covering rectangle for a fade effect
		draw.drawon(GFX_BOTTOM);
		draw.drawtexture(backgroundbot, 0, 0);
		draw.drawtexture(leftbutton, 0, 13);
		draw.drawtexture(rightbutton, 169, 13);
		draw.drawtexture(selector, 0, 159);
		draw.drawtextinrec(slotname.c_str(), 35, 180, 251, 1.4, 1.4);
		draw.drawrectangle(0, 0, 320, 240, RGBA8(rgbvalues[0], rgbvalues[1], rgbvalues[2], alpha));
		draw.frameend();
	}
	svcSignalEvent(event_fadefinished);
}

//https://www.linuxquestions.org/questions/linux-newbie-8/how-to-check-if-a-folder-is-empty-661934/
int countEntriesInDir(const char* dirname)
{
	int n = 0;
	dirent* d;
	DIR* dir = opendir(dirname);
	if (dir == NULL) return 0;
	while ((d = readdir(dir)) != NULL) n++;
	closedir(dir);
	return n;
}

void launch(){

	int rgb[3] = {0, 0, 0}; //Fade to black
	
	s32 mainthreadpriority;
	svcGetThreadPriority(&mainthreadpriority, CUR_THREAD_HANDLE);
	svcCreateEvent(&event_fadefinished, RESET_ONESHOT);
	threadCreate(threadfunc_fade, rgb, 8000, mainthreadpriority + 1, -2, true);
	config.u64multiwrite("ActiveTitleIDs", titleids, true);
	config.intmultiwrite("TitleIDSlots", slots);
	config.write("SelectedTitleIDPos", currenttidpos);
	config.flush();
	if(modsenabled)
	{
		string dest = issaltysdtitle() ? "/saltysd/smash" : "/luma/titles/" + currenttitleidstr;
		string src = modsfolder + currenttitleidstr + "/Slot_" + to_string(currentslot);
		attemptrename:
		if(rename(src.c_str(), dest.c_str()))
		{
			error("Failed to move slot file from\n" + modsfolder + '\n' + currenttitleidstr + "/Slot_" + to_string(currentslot) + "\nto /saltysd/smash!");
			error("Error code:\n" + to_string(errno));
			if ((unsigned int)errno == 0xC82044BE) //Destination already exists
			{
				if (countEntriesInDir(dest.c_str()) == 0)
				{
					rmdir(dest.c_str());
					error("ModMoon tackled this issue\nautomagically. Now isn't that\nnice? Retrying now...");
					goto attemptrename;
				}
			}
		}
	}
	svcWaitSynchronization(event_fadefinished, U64_MAX);
	draw.framestart(); //Prevent wonkiness with the app jump
	draw.drawrectangle(0, 0, 400, 240, 0); //Not actually drawn, just there to prevent Citro3D hang. Clear color does this for real
	draw.drawon(GFX_BOTTOM);
	draw.drawrectangle(0, 0, 320, 240, 0);
	draw.frameend();
	draw.cleanup();
	u8 param[0x300];
	u8 hmac[0x20];
	memset(param, 0, sizeof(param));
	memset(hmac, 0, sizeof(hmac));

	APT_PrepareToDoApplicationJump(0, currenttitleid, getSMDHdata()[currenttidpos].gametype);

	APT_DoApplicationJump(param, sizeof(param), hmac);
}
