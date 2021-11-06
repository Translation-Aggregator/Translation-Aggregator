#include <Shared/Shrink.h>

#include "LocalWindows/UntranslatedWindow.h"
#include "LocalWindows/AtlasWindow.h"
#include "LocalWindows/LECWindow.h"

#include "HttpWindows/BabelfishWindow.h"
#include "HttpWindows/BabylonWindow.h"
#include "HttpWindows/BaiduWindow.h"
#include "HttpWindows/BingWindow.h"
#include "HttpWindows/ExciteWindow.h"
//#include "HttpWindows/FreeTranslationWindow.h"
#include "HttpWindows/GoogleWindow.h"
#include "HttpWindows/DeepLWindow.h"
#include "HttpWindows/HonyakuWindow.h"
#include "HttpWindows/InfoseekWindow.h"
#include "HttpWindows/LECOnlineWindow.h"
#include "HttpWindows/SDLWindow.h"
#include "HttpWindows/SysTranWindow.h"
#include "HttpWindows/YandexWindow.h"

#include "HttpWindows/JdicWindow.h"

#include "LocalWindows/MecabWindow.h"
#include "LocalWindows/JParseWindow.h"

int MakeTranslationWindows(TranslationWindow** &windows, TranslationWindow* &srcWindow)
{
	int numWindows = 0;
	windows = (TranslationWindow**) malloc(sizeof(TranslationWindow*) * 18);
	windows[numWindows++] = srcWindow = new UntranslatedWindow();
	windows[numWindows++] = new AtlasWindow();
	windows[numWindows++] = new LECWindow();

	windows[numWindows++] = new ExciteWindow();
	windows[numWindows++] = new LECOnlineWindow();
	windows[numWindows++] = new GoogleWindow();
	windows[numWindows++] = new BingWindow();
	windows[numWindows++] = new DeepLWindow();
	windows[numWindows++] = new YandexWindow();
//	windows[numWindows++] = new HonyakuWindow(); // Service shutdown
	windows[numWindows++] = new SysTranWindow();
	windows[numWindows++] = new SDLWindow();
	windows[numWindows++] = new InfoseekWindow();
	windows[numWindows++] = new BaiduWindow();
	windows[numWindows++] = new BabylonWindow();
	windows[numWindows++] = new BabelfishWindow();
	//windows[numWindows++] = new FreeTranslationWindow();

	windows[numWindows++] = new JdicWindow();
	windows[numWindows++] = new MecabWindow();
	windows[numWindows++] = new JParseWindow();
	return numWindows;
}
