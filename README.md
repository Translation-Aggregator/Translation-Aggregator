# Translation Aggregator

![A sample window of Translation Aggregator](https://i.imgur.com/s8fHwUP.png)

Translation Aggregator is a program that machine translates texts through multiple sources and presents them in one conveient window.

The **archived** discussion thread for this program is available [here](https://web.archive.org/web/20190905051811/http://www.hongfire.com:80/forum/forum/hentai-lair/hentai-game-discussion/tools-and-tutorials/68499-translation-aggregator).

## Downloads

The following binaries are available:  
* [Win32 (Intel 32-bit GCC)](https://github.com/Translation-Aggregator/Translation-Aggregator/releases/latest/download/TranslationAggregator-win32.7z)  

## About

Translation Aggregator basically works like ATLAS, with support for using a number of website translators and ATLAS simultaneously. It was designed to replace ATLAS's interface as well as add support for getting translations from a few additional sources. Currently, it has support for getting translations from Atlas V13 or V14 (Don't need to have Atlas running), Google, Honyaku, Babel Fish, FreeTranslations.com, Excite, OCN, a word-by-word breakdown from WWWJDIC, [MeCab](http://taku910.github.io/mecab/), which converts Kanji to Katakana, and its own built-in Japanese parser (JParser). I picked websites based primarily on what I use and how easy it was to figure out their translation request format. I'm open to adding more, but some of the other sites (Like Word Lingo) seem to go to some effort to make this difficult.

JParser requires [edict2](http://www.edrdg.org/wiki/index.php/JMdict-EDICT_Dictionary_Project) (Or edict) in the dictionaries directory, and supports multiple dictionaries in there at once. It does not support jmdict. You can also stick enamdict in the directory and it'll detect some names as well, though the name list will be heavily filtered to avoid swamping out other hits. If you have MeCab installed, JParser can use it to significantly improve its results. TA can also look up definitions for MeCab output as well, if a dictionary is installed. In general, MeCab makes fewer mistakes, but JParser handles compound words better, and groups verb conjugations with the verb rather than treating them as separate words.

TA also includes the ability to launch Japanese apps with Japanese locale settings, automatically inject AGTH into them, and inject its own dll into Japanese apps. Its dll can also translate their menus and dialogs using the ATLAS module (Requires you have ATLAS installed, of course). Versions 0.4.0 and later also include a text hooking engine modeled after AGTH. The menu translation option attempts to translate Windows-managed in-game menus, and is AGTH compatible. The AGTH exe and dlls must be in the Translation Aggregator directory for it to be able to inject AGTH into a process. AGTH is included with the most recent versions of TA.

The interface is pretty simple, much like ATLAS: Just paste text into the upper left window, and either press the double arrow button to run it through all translators, or press the arrow buttons for individual translation apps. Each algorithm is only run once at a time, so if a window is busy when you tell it to translate something, it'll queue it up if it's a remote request, or stop and rerun it for local algorithms. If you have clipboard monitoring enabled (The untranslated text clipboard button disables it altogether), it'll run any clipboard text with Japanese characters copied from any other app through all translators with clipboard monitoring enabled. I won't automatically submit text with over 500 characters to any of the translation websites, so you can skip forward in agth without flooding servers, in theory. I still don't recommend automatic clipboard translation for the website translators, however.

To assign a hotkey to the current window layout, press shift-alt-#. Press alt-# to restore the layout. Bound hotkeys will automatically include the current transparency, window frame, and toobar states. If you don't want a bound hotkey to affect one or more of those states, then you can remove the first 1 to 3 entries in the associated line in the ini file. Only modify the ini yourself when the program isn't running. All other values in those lines are mandatory.

Pre-translation substitutions modify input text before it's sent to any translator. Currently applies to websites, ATLAS, Mecab, and JParser. There's a list of universal replacements ("\*") and replacements for every launch profile you've created. I pick which set(s) of substitutions to use based on currently running apps. Note that you do not need to be running AGTH or even have launched a game through TA's launch interface for the game to be detected, but you do need to create a launch profile. May allow you to just drag and drop exes onto the dialog in the future.

[MeCab](http://taku910.github.io/mecab/) is a free program that separates words and gives their pronunciation and part of speech. I use it to get the information needed to parse words and display furigana. If you have MeCab installed but I report I'm having trouble initializing it, you can try copying libmecab.dll to the same directory as this program. Do not install MeCab using a UTF16 dictionary, as I have no idea how to talk to it (UTF16 strings don't seem to work). Instead, configure MeCab to use UTF8, Shift-JIS, or EUC-JP. If you have both MeCab and edict/edict2 installed, you can view a word's translation in MeCab by hovering the mouse over it. Also, JParser can use MeCab to help in parsing sentences.

JParser tends to be a better choice for those who know almost no Japanese - it tells you how verbs are conjugated, handles some expressions, etc. MeCab may well be the better choice for those who know some Japanese, however.

Source in this repository is available under the **GPL v2**. Please read `LICENSE` for more details.

Thanks to (In alphabetical order, sorry if I'm leaving anyone out):  
**Hongfire Members**:  
**Freaka** for his innumerable feature suggestions and reported issues over the course of development.  
**Setsumi** for [TA Helper](https://github.com/setsumi/TAHelper) and for all his suggested improvements and reported issues, particularly with JParser.  
**Setx** for AGTH.  
**Stomp** for fixing the open file dialog not working properly on some systems and adding the tooltip font dialog, and fixing a bug that required admin privileges when certain other software was installed.  
Might sound like minor contributions, but feedback really drives the development of TA.  

**Non-members**:  
**KingMike** of KingMike's Translations, who is apparently the creator of the EUC-JP table I used to generate my own conversion table.  
**Nasser R. Rowhani** for his function hooking code.  
**Z0mbie** for writing the [opcode length detector/disassembler](http://hack-expo.void.ru/groups/blt/text/disasm.txt) I use for hooking. Apparently was intended for virus-related use, but works fine for other things, too.  
And the creators and maintainers of **edict**, **MeCab**, and **zlib**.  

You might also be interested in:
* **Setsumi**'s [TA Helper](https://github.com/setsumi/TAHelper) and [AGTHGrab](https://web.archive.org/web/20170509141602/http://www.hongfire.com/forum/forum/hentai-lair/hentai-game-discussion/tools-and-tutorials/68499-translation-aggregator/page13?postcount=195).
* **errotzol**'s [replacements script](https://web.archive.org/web/20190910085707/http://www.hongfire.com/forum/forum/hentai-lair/hentai-game-discussion/tools-and-tutorials/68499-translation-aggregator/page34#post2766594).
* **Devocalypse**'s [devOSD](https://web.archive.org/web/20190918173557/http://www.hongfire.com/forum/forum/hentai-lair/hentai-game-discussion/tools-and-tutorials/114249-devosd-a-japanese-galge-eroge-visual-novel-translation-helper?t=139063).
* **kaosu**'s [ITH](https://web.archive.org/web/20190921092912/http://www.hongfire.com/forum/forum/hentai-lair/hentai-game-discussion/tools-and-tutorials/185725-interactive-text-hooker-new-text-extraction-tool?t=208860) (Like AGTH. No direct TA support, due to lack of a command line interface, but definitely worth checking out).
* [MeCab](http://taku910.github.io/mecab/)
* [edict2](http://www.edrdg.org/wiki/index.php/JMdict-EDICT_Dictionary_Project)
