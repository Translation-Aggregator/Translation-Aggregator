cmake_minimum_required(VERSION 3.0...3.10)
project(TranslationAggregator)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if((NOT DEFINED CUR_GIT_TAG) OR (NOT CUR_GIT_TAG) OR (CUR_GIT_TAG STREQUAL ""))
	set(CUR_GIT_TAG Unknown)
	find_package(Git)
	if(GIT_FOUND)
		if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.git)
			execute_process(COMMAND ${GIT_EXECUTABLE} describe --abbrev=0 --tags
			WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
			OUTPUT_VARIABLE TMP_GIT_TAG
			RESULT_VARIABLE TMP_RES
			OUTPUT_STRIP_TRAILING_WHITESPACE)
			if(TMP_RES EQUAL "0")
				set(CUR_GIT_TAG ${TMP_GIT_TAG})
			endif()
		endif()
	endif()
endif()

include_directories(
	.
	exe
	3rdParty/json/single_include/nlohmann
)

add_executable(TranslationAggregator
	3rdParty/zlib/adler32.c
	3rdParty/zlib/crc32.c
	3rdParty/zlib/gzclose.c
	3rdParty/zlib/gzlib.c
	3rdParty/zlib/gzread.c
	3rdParty/zlib/inffast.c
	3rdParty/zlib/inflate.c
	3rdParty/zlib/inftrees.c
	3rdParty/zlib/zutil.c
	exe/BufferedSocket.cpp
	exe/Config.cpp
	exe/Context.cpp
	exe/Dialogs/InjectionDialog.cpp
	exe/Dialogs/MyToolTip.cpp
	exe/Filter.cpp
	exe/History/History.cpp
	exe/TranslationAggregator.cpp
	exe/TranslationWindows/HttpWindows/BabelfishWindow.cpp
	exe/TranslationWindows/HttpWindows/BabylonWindow.cpp
	exe/TranslationWindows/HttpWindows/BaiduWindow.cpp
	exe/TranslationWindows/HttpWindows/BingWindow.cpp
	exe/TranslationWindows/HttpWindows/DeepLWindow.cpp
	exe/TranslationWindows/HttpWindows/ExciteWindow.cpp
	exe/TranslationWindows/HttpWindows/FreeTranslationWindow.cpp
	exe/TranslationWindows/HttpWindows/GoogleWindow.cpp
	exe/TranslationWindows/HttpWindows/HonyakuWindow.cpp
	exe/TranslationWindows/HttpWindows/HttpWindow.cpp
	exe/TranslationWindows/HttpWindows/InfoseekWindow.cpp
	exe/TranslationWindows/HttpWindows/JdicWindow.cpp
	exe/TranslationWindows/HttpWindows/LECOnlineWindow.cpp
	exe/TranslationWindows/HttpWindows/SDLWindow.cpp
	exe/TranslationWindows/HttpWindows/SysTranWindow.cpp
	exe/TranslationWindows/HttpWindows/YandexWindow.cpp
	exe/TranslationWindows/LocalWindows/AtlasWindow.cpp
	exe/TranslationWindows/LocalWindows/FuriganaWindow.cpp
	exe/TranslationWindows/LocalWindows/JParseWindow.cpp
	exe/TranslationWindows/LocalWindows/LECWindow.cpp
	exe/TranslationWindows/LocalWindows/MecabWindow.cpp
	exe/TranslationWindows/LocalWindows/UntranslatedWindow.cpp
	exe/TranslationWindows/TranslationWindow.cpp
	exe/TranslationWindows/TranslationWindowFactory.cpp
	exe/util/Dictionary.cpp
	exe/util/DictionaryUtil.cpp
	exe/util/HttpUtil.cpp
	exe/util/Injector.cpp
	exe/util/Mecab.cpp
	exe/util/Regexp.cpp
	exe/util/Value.cpp
	exe/TranslationAggregator.res.rc
	Shared/Atlas.cpp
	Shared/DllInjection.cpp
	Shared/EUCJPtoUTF16.cpp
	Shared/HookEval.cpp
	Shared/Lock.cpp
	Shared/ProcessUtil.cpp
	Shared/ReadWriteLock.cpp
	Shared/Shrink.cpp
	Shared/StringUtil.cpp
	Shared/TextHookParser.cpp
	Shared/TextRuleSet.cpp
	Shared/Thread.cpp
)

set_target_properties(TranslationAggregator PROPERTIES C_STANDARD 11)
set_target_properties(TranslationAggregator PROPERTIES CXX_STANDARD 11)

if(WIN32)
	target_compile_definitions(TranslationAggregator PUBLIC
		NDEBUG
		NO_GZCOMPRESS
		WIN32
		_WINDOWS
		HAVE_CONFIG_H
		PCRE_STATIC
		MINGW_HAS_SECURE_API
		UNICODE
		_UNICODE
		GIT_TAG=L"${CUR_GIT_TAG}"
	)
	target_link_libraries(TranslationAggregator
		winmm
		ws2_32
		comctl32
		gdi32
		winhttp
		psapi
		user32
		comdlg32
		ole32
		shell32
		advapi32
	)
	target_link_options(TranslationAggregator PUBLIC
		-static
		-static-libgcc
		-static-libstdc++
		-municode
		-Wl,--subsystem,windows
	)
endif()

if((NOT DEFINED MY_ARCHIVE_NAME) OR (NOT MY_ARCHIVE_NAME) OR (MY_ARCHIVE_NAME STREQUAL ""))
if((NOT DEFINED USE_ARCHIVE_HAS_GIT_TAG) OR (NOT USE_ARCHIVE_HAS_GIT_TAG) OR (USE_ARCHIVE_HAS_GIT_TAG STREQUAL ""))
set(USE_ARCHIVE_HAS_GIT_TAG FALSE)
endif()
if(USE_ARCHIVE_HAS_GIT_TAG)
set(MY_ARCHIVE_NAME TranslationAggregator-${CUR_GIT_TAG}-win32.7z)
else()
set(MY_ARCHIVE_NAME TranslationAggregator-win32.7z)
endif()
endif()
add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${MY_ARCHIVE_NAME}
	COMMAND rm -f ${CMAKE_CURRENT_BINARY_DIR}/${MY_ARCHIVE_NAME} && 7z a ${CMAKE_CURRENT_BINARY_DIR}/${MY_ARCHIVE_NAME} $<TARGET_FILE:TranslationAggregator> ${CMAKE_CURRENT_SOURCE_DIR}/docs ${CMAKE_CURRENT_SOURCE_DIR}/dictionaries ${CMAKE_CURRENT_SOURCE_DIR}/Plugins
	DEPENDS TranslationAggregator
)
add_custom_target(archive
	DEPENDS ${MY_ARCHIVE_NAME}
)
