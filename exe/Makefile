
CC = i686-w64-mingw32-gcc
CXX = i686-w64-mingw32-g++
AR = i686-w64-mingw32-ar
WINDRES = i686-w64-mingw32-windres
#CFLAGS += -O0 -g
GIT_TAG := $(shell git describe --abbrev=0 --tags)
CFLAGS += -O3
CFLAGS += -std=c++17 -Wall -Wno-unused-value -Wno-format -I.. -DGIT_TAG=L\"$(GIT_TAG)\" -DNDEBUG -DNO_GZCOMPRESS -DWIN32 -D_WINDOWS -DHAVE_CONFIG_H -DPCRE_STATIC -DMINGW_HAS_SECURE_API -DUNICODE -D_UNICODE
LDFLAGS += -s -static -static-libstdc++ -static-libgcc -municode
LDLIBS += -lwinmm -lws2_32 -lcomctl32 -lgdi32 -lwinhttp -lpsapi -luser32 -lcomdlg32 -lole32 -lshell32 -ladvapi32 

%.o: %.c
	@printf '\t%s %s\n' CC $<
	$(CC) -c $(CFLAGS) -o $@ $<

%.o: %.cpp
	@printf '\t%s %s\n' CXX $<
	$(CXX) -c $(CFLAGS) -o $@ $<

%.o: %.rc
	@printf '\t%s %s\n' WINDRES $<
	$(WINDRES) $< $@

SOURCES := TranslationAggregator.res.rc ../shared/EUCJPtoUTF16.cpp BufferedSocket.cpp Config.cpp Context.cpp Filter.cpp TranslationAggregator.cpp Dialogs/InjectionDialog.cpp Dialogs/MyToolTip.cpp TranslationWindows/HttpWindows/BabylonWindow.cpp TranslationWindows/HttpWindows/BaiduWindow.cpp TranslationWindows/HttpWindows/FreeTranslationWindow.cpp TranslationWindows/HttpWindows/InfoseekWindow.cpp TranslationWindows/HttpWindows/LECOnlineWindow.cpp TranslationWindows/HttpWindows/SDLWindow.cpp TranslationWindows/HttpWindows/SysTranWindow.cpp TranslationWindows/HttpWindows/YandexWindow.cpp TranslationWindows/LocalWindows/LECWindow.cpp util/Dictionary.cpp util/DictionaryUtil.cpp util/HttpUtil.cpp util/Injector.cpp util/Mecab.cpp util/Regexp.cpp util/Value.cpp History/History.cpp TranslationWindows/TranslationWindow.cpp TranslationWindows/TranslationWindowFactory.cpp TranslationWindows/HttpWindows/BabelfishWindow.cpp TranslationWindows/HttpWindows/BingWindow.cpp TranslationWindows/HttpWindows/ExciteWindow.cpp TranslationWindows/HttpWindows/GoogleWindow.cpp TranslationWindows/HttpWindows/HonyakuWindow.cpp TranslationWindows/HttpWindows/HttpWindow.cpp TranslationWindows/HttpWindows/JdicWindow.cpp TranslationWindows/LocalWindows/AtlasWindow.cpp TranslationWindows/LocalWindows/FuriganaWindow.cpp TranslationWindows/LocalWindows/JParseWindow.cpp TranslationWindows/LocalWindows/MecabWindow.cpp TranslationWindows/LocalWindows/UntranslatedWindow.cpp ../shared/Shrink.cpp ../3rdParty/zlib/adler32.c ../3rdParty/zlib/crc32.c ../3rdParty/zlib/gzclose.c ../3rdParty/zlib/gzlib.c ../3rdParty/zlib/gzread.c ../3rdParty/zlib/inffast.c ../3rdParty/zlib/inflate.c ../3rdParty/zlib/inftrees.c ../3rdParty/zlib/zutil.c ../shared/Atlas.cpp ../shared/DllInjection.cpp ../shared/HookEval.cpp ../shared/Lock.cpp ../shared/ProcessUtil.cpp ../shared/ReadWriteLock.cpp ../shared/StringUtil.cpp ../shared/TextHookParser.cpp ../shared/TextRuleSet.cpp ../shared/Thread.cpp
OBJECTS := $(SOURCES:.c=.o)
OBJECTS := $(OBJECTS:.cpp=.o)
OBJECTS := $(OBJECTS:.rc=.o)

BINARY ?= TranslationAggregator.exe
ARCHIVE ?= TranslationAggregator.$(GIT_TAG).7z

all: $(BINARY)

archive: $(ARCHIVE)

clean:
	rm -f $(OBJECTS) $(BINARY) $(ARCHIVE)

$(ARCHIVE): $(BINARY) TranslationAggregator.ini ../docs ../dictionaries ../Plugins
	rm -f $(ARCHIVE)
	7z a $@ $^

$(BINARY): $(OBJECTS) 
	@printf '\t%s %s\n' LNK $@
	$(CXX) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)
