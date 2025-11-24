#include "app.h"

# ifdef WIN
#include <windows.h>
/**
 * Windows will automatically open up a terminal to accompany this program if we do not
 * implement the main method as such.
 * The windows terminal takes about HALF A SECOND to open up and prevents our program
 * from running in the meantime...
 */
int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int show_cmd) {
	app_run();
	return 0;
}
#else
/**
 * On any other machine, this should be just fine. (TODO: verify by testing)
 */
int main() {
	app_run();
	return 0;
}
#endif