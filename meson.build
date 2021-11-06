project(
	'TranslationAggregator',
	[
		'c',
		'cpp'
	],
	default_options: [
		'optimization=3',
		'debug=true',
		'warning_level=0'
	],
	version: 'unknown'
)

cxx = meson.get_compiler('cpp')
cc = meson.get_compiler('c')
host_system = host_machine.system()
windows = import('windows')
git_tag = run_command('git', '-C', meson.source_root(), 'describe', '--abbrev=0', '--tags').stdout().strip()

TranslationAggregator_cflags = []
TranslationAggregator_cppflags = []
TranslationAggregator_rcflags = []

TranslationAggregator_rcflags += [
	'-DGIT_TAG=L"' + git_tag + '"',
]

TranslationAggregator_cflags += TranslationAggregator_rcflags

TranslationAggregator_cflags += [
	'-DNDEBUG',
	'-DNO_GZCOMPRESS',
	'-DWIN32',
	'-D_WINDOWS',
	'-DHAVE_CONFIG_H',
	'-DPCRE_STATIC',
	'-DMINGW_HAS_SECURE_API',
	'-DUNICODE',
	'-D_UNICODE',
]

TranslationAggregator_cppflags += TranslationAggregator_cflags

TranslationAggregator_cppflags += [
	'-std=c++11',
]

TranslationAggregator_ldflags = []
if target_machine.system() == 'windows'
	TranslationAggregator_ldflags += [
		'-static',
		'-static-libstdc++',
		'-static-libgcc',
		'-municode',
		'-Wl,--subsystem,windows',
	]
endif

TranslationAggregator_ldflags += [
	'-lwinmm',
	'-lws2_32',
	'-lcomctl32',
	'-lgdi32',
	'-lwinhttp',
	'-lpsapi',
	'-luser32',
	'-lcomdlg32',
	'-lole32',
	'-lshell32',
	'-ladvapi32',
]

TranslationAggregator_src = [
	'3rdParty/zlib/adler32.c',
	'3rdParty/zlib/crc32.c',
	'3rdParty/zlib/gzclose.c',
	'3rdParty/zlib/gzlib.c',
	'3rdParty/zlib/gzread.c',
	'3rdParty/zlib/inffast.c',
	'3rdParty/zlib/inflate.c',
	'3rdParty/zlib/inftrees.c',
	'3rdParty/zlib/zutil.c',
	'exe/BufferedSocket.cpp',
	'exe/Config.cpp',
	'exe/Context.cpp',
	'exe/Dialogs/InjectionDialog.cpp',
	'exe/Dialogs/MyToolTip.cpp',
	'exe/Filter.cpp',
	'exe/History/History.cpp',
	'exe/TranslationAggregator.cpp',
	'exe/TranslationWindows/HttpWindows/BabelfishWindow.cpp',
	'exe/TranslationWindows/HttpWindows/BabylonWindow.cpp',
	'exe/TranslationWindows/HttpWindows/BaiduWindow.cpp',
	'exe/TranslationWindows/HttpWindows/BingWindow.cpp',
	'exe/TranslationWindows/HttpWindows/DeepLWindow.cpp',
	'exe/TranslationWindows/HttpWindows/ExciteWindow.cpp',
	'exe/TranslationWindows/HttpWindows/FreeTranslationWindow.cpp',
	'exe/TranslationWindows/HttpWindows/GoogleWindow.cpp',
	'exe/TranslationWindows/HttpWindows/HonyakuWindow.cpp',
	'exe/TranslationWindows/HttpWindows/HttpWindow.cpp',
	'exe/TranslationWindows/HttpWindows/InfoseekWindow.cpp',
	'exe/TranslationWindows/HttpWindows/JdicWindow.cpp',
	'exe/TranslationWindows/HttpWindows/LECOnlineWindow.cpp',
	'exe/TranslationWindows/HttpWindows/SDLWindow.cpp',
	'exe/TranslationWindows/HttpWindows/SysTranWindow.cpp',
	'exe/TranslationWindows/HttpWindows/YandexWindow.cpp',
	'exe/TranslationWindows/LocalWindows/AtlasWindow.cpp',
	'exe/TranslationWindows/LocalWindows/FuriganaWindow.cpp',
	'exe/TranslationWindows/LocalWindows/JParseWindow.cpp',
	'exe/TranslationWindows/LocalWindows/LECWindow.cpp',
	'exe/TranslationWindows/LocalWindows/MecabWindow.cpp',
	'exe/TranslationWindows/LocalWindows/UntranslatedWindow.cpp',
	'exe/TranslationWindows/TranslationWindow.cpp',
	'exe/TranslationWindows/TranslationWindowFactory.cpp',
	'exe/util/Dictionary.cpp',
	'exe/util/DictionaryUtil.cpp',
	'exe/util/HttpUtil.cpp',
	'exe/util/Injector.cpp',
	'exe/util/Mecab.cpp',
	'exe/util/Regexp.cpp',
	'exe/util/Value.cpp',
	'Shared/Atlas.cpp',
	'Shared/DllInjection.cpp',
	'Shared/EUCJPtoUTF16.cpp',
	'Shared/HookEval.cpp',
	'Shared/Lock.cpp',
	'Shared/ProcessUtil.cpp',
	'Shared/ReadWriteLock.cpp',
	'Shared/Shrink.cpp',
	'Shared/StringUtil.cpp',
	'Shared/TextHookParser.cpp',
	'Shared/TextRuleSet.cpp',
	'Shared/Thread.cpp',
]

TranslationAggregator_src_rc = [
	'exe/TranslationAggregator.res.rc',
]

TranslationAggregator_includes = [
	'exe',
	'3rdParty/json/single_include/nlohmann'
]

TranslationAggregator_src += windows.compile_resources(
	TranslationAggregator_src_rc,
	args: TranslationAggregator_rcflags,
	include_directories: include_directories(TranslationAggregator_includes)
)

TranslationAggregator = executable(
	'TranslationAggregator',
	sources: TranslationAggregator_src,
	include_directories: include_directories(TranslationAggregator_includes),
	c_args: TranslationAggregator_cflags,
	cpp_args: TranslationAggregator_cppflags,
	link_args: TranslationAggregator_ldflags,
)

if host_system == 'windows' and cc.get_id() != 'msvc' and cc.get_id() != 'clang-cl'
archive = custom_target(
	'archive',
	output: meson.project_name() + '-' + git_tag + '-win32.7z',
	input: [
		TranslationAggregator,
		'exe/TranslationAggregator.ini',
	],
	build_by_default: true,
	command: [
		find_program('7z'),
		'a',
		'@OUTPUT@',
		'@INPUT@',
		join_paths(meson.source_root(), 'docs'),
		join_paths(meson.source_root(), 'dictionaries'),
		join_paths(meson.source_root(), 'Plugins')
	]
)
endif
