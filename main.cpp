

using namespace std;

#include "light_display.h"

int main(int argc, char *argv[])
{
	const int CHANNELS_IN_USE=8;
	light_display Display(CHANNELS_IN_USE);
	if (argc>1)
		Display.runDisplay(arg[1]);
	else
		Display.runDisplay();
	return 0;
}