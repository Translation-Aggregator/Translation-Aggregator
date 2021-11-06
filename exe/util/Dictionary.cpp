#include "Shared/Shrink.h"

#include "Dictionary.h"
#include "Shared/scoped_ptr.h"
#include "Shared/StringUtil.h"
#include "Mecab.h"

#include "exe/util/Value.h"

#define DICT_MAGIC 'TCD\00'
#define DICT_VERSION 0x000C

#define DICT_FLAG_NAMES    0x0001

ReadWriteLock dictionary_read_write_lock;
/*
struct DictionaryEntry
{
	unsigned char numJStrings;
	unsigned char reserved[3];
	// Each entry is actually just a set of JapStrings
	// followed by a single EngString.  Currently,
	// all strings are only 2-byte aligned, so
	// only "entryIndex" crossed a byte boundary.
	unsigned int firstString;
};
//*/

struct FileSig
{
	__int64 size;
	__int64 modTime;
	__int64 createTime;
};

struct DictionaryHeader
{
	int magic;
	int version;
	FileSig srcSig;
	FileSig conjSig;

	//int numEntries;
	int numJStrings;

	unsigned int flags;
};

struct Dictionary
{
	HANDLE hFile;
	DictionaryHeader *header;

	// int numEntries;
	int numJStrings;

	// DictionaryEntry *entries;

	// jStrings are a pre-sorted list of Japanese strings.
	unsigned int *jStrings;

	char *strings;
	wchar_t fileName[MAX_PATH];
};

void CleanupDict(Dictionary *dict)
{
	UnmapViewOfFile(dict->header);
	CloseHandle(dict->hFile);
	free(dict);
}
struct VerbConjugation
{
	// Index of string in tenses array.
	unsigned int tenseID;
	// If can be conjugated multiple times
	unsigned short next_verb_type_id;
	// Affirmative/negative/singular/plural
	unsigned char form;

	std::wstring suffix;
	std::string next_type;
};

struct VerbType
{
	std::string type;
	// 1 if adjective.
	unsigned char is_adjective;
	// Tense with suffix to remove.  Either TENSE_REMOVE or TENSE_NON_PAST.
	unsigned int remove_tense;
	std::vector<VerbConjugation> conjugations;
};

struct ConjugationTable
{
	std::vector<VerbType> verb_types;
	std::vector<std::wstring> tense_names;
	FileSig sig;
};

#define TENSE_REMOVE 0
#define TENSE_NON_PAST 1
#define TENSE_STEM 2
#define TENSE_POTENTIAL 3

const static wchar_t *staticTenses[] =
{
	L"Remove",
	L"Non-past",
	L"Stem",
	L"Potential",
	L"Past",
	L"Te-form",
	L"Conditional",
	L"Provisional",
	L"Passive",
	L"Causative",
	L"Caus-Pass",
	L"Volitional",
	L"Conjectural",
	L"Adverbal",
	L"Alternative",
	L"Imperative",
	L"Imperfective",
	L"Continuative",
	L"Hypothetical",
	L"Prenominal",
	// L"Attributive",
	// to do accidentally/to finish completely
};

int numDicts = 0;
Dictionary **dicts = 0;

scoped_ptr<ConjugationTable> conjugation_table;

Dictionary *LoadDict(wchar_t *path)
{
	HANDLE hFile = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	if (hFile == INVALID_HANDLE_VALUE) return 0;
	DWORD high;
	DWORD low = GetFileSize(hFile, &high);
	if ((low == INVALID_FILE_SIZE && GetLastError() != NO_ERROR) || (!high && low < sizeof(DictionaryHeader)))
	{
		CloseHandle(hFile);
		return 0;
	}
	HANDLE hMapping = CreateFileMapping(hFile, 0, PAGE_READONLY, high, low, 0);
	DictionaryHeader *d;
	size_t size = (size_t)(low + (((__int64)high)<<32));
	if (!hMapping || !(d = (DictionaryHeader*)MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, size)))
	{
		CloseHandle(hFile);
		return 0;
	}
	// Should be safe.
	CloseHandle(hMapping);
	Dictionary *dict = (Dictionary*) calloc(1, sizeof(Dictionary));
	dict->hFile = hFile;
	dict->header = d;
	if (d->magic != DICT_MAGIC || d->version != DICT_VERSION)
	{
		CleanupDict(dict);
		return 0;
	}
	wcscpy(dict->fileName, path);

	// dict->numEntries = dict->header->numEntries;
	dict->numJStrings = dict->header->numJStrings;

	// dict->entries = (DictionaryEntry*) (dict->header+1);
	dict->jStrings = (unsigned int*)(dict->header+1);
	dict->strings = (char*)(dict->jStrings + dict->numJStrings);
	void *start = (d+1);
	void *end = ((char*)d)+size;
	// Very minimal sanity check.
	if (/*dict->numEntries <= 0 ||*/ dict->numJStrings <= 0 ||
		/*dict->entries < start || dict->entries > end ||*/
		dict->jStrings < start || dict->jStrings > end ||
		dict->strings < start || dict->strings > end)
	{
			CleanupDict(dict);
			return 0;
	}
	return dict;
}

int __cdecl CompareWcharJap(const void* s1, const void* s2)
{
	wchar_t *w1 = *(wchar_t**)s1;
	wchar_t *w2 = *(wchar_t**)s2;
	return wcsijcmp(w1, w2);
}

// Like wcstok, only more evil and stupid.
__forceinline wchar_t* __cdecl NextEvilString(wchar_t *s1, const wchar_t *delim)
{
	static wchar_t *temp;
	if (s1) temp = s1;
	size_t len;
	while (1)
	{
		len = 0;
		while (1)
		{
			if (wcschr(delim, temp[len])) break;
			else if (temp[len] == '(')
				while (temp[len] && temp[len]!=')')
					len++;
			len++;
		}
		if (!len)
		{
			if (!*temp) return 0;
			temp++;
			continue;
		}

		wchar_t *out = temp;
		if (temp[len])
		{
			temp[len] = 0;
			temp += len+1;
		}
		else temp+=len;
		return out;
	}
}

void RemoveEntLJunk(wchar_t *line)
{
	while (line = wcsstr(line, L"/EntL"))
	{
		wchar_t *e = wcschr(line+5, '/');
		if (!e)
			break;
		/*if (e-line < 12 || e-line > 15) {
			line++;
			continue;
		}//*/
		wcscpy(line, e);
	}
}

int CreateDict(wchar_t *inPath, FileSig &srcSig, wchar_t *path)
{
	wchar_t *data;
	int len;
	LoadFile(inPath, &data, &len);
	if (!data) return 0;
	int num_verb_types = conjugation_table->verb_types.size();
	if (len >= 4)
	{
		int i;
		for (i=0; i<4; i++)
			if (data[i] != L'\xFF1F' && data[i] != L'\x3000')
				break;
		if (i == 4 || 1)
		{
			//DictionaryEntry *entries = 0;
			//int numEntries = 0;
			//int maxEntries = 0;

			int *jStrings = 0;
			int numJStrings = 0;
			int maxJStrings = 0;

			char *strings = 0;
			int stringLen = 0;
			int maxStringLen = 0;

			wchar_t *line = data;
			//if (line) line++;
			while (1)
			{
				int lineLen = wcscspn(line, L"\r\n");
				if (!lineLen) break;
				wchar_t *next = line+lineLen;
				while (next[0] == '\n' || next[0] == '\r')
				{
					*next = 0;
					next++;
				}

				unsigned char flags = JAP_WORD_PRIMARY;
#if 0
				unsigned char primaryFlags = 0;
#endif

				RemoveEntLJunk(line);

				wchar_t *p = wcsstr(line, L"/(P)");
				if (p)
#if 0
					if (p+1 == wcsstr(line, L"(P)"))
						flags |= JAP_WORD_COMMON;
					else
						primaryFlags |= JAP_WORD_COMMON;
#else
					flags |= JAP_WORD_COMMON_LINE;
#endif
#ifdef SETSUMI_CHANGES
				//hack - set custom word top priority flag (1)
				if (wcsstr(line, L"(T)")) {
					flags |= JAP_WORD_TOP;
					primaryFlags |= JAP_WORD_TOP;
				}
				//hackend
#endif
				/*
				if (numEntries == maxEntries)
				{
					maxEntries += maxEntries + 1024;
					entries = (DictionaryEntry*) realloc(entries, sizeof(*entries) * maxEntries);
				}//*/
				if (lineLen * 160 + stringLen >= maxStringLen)
				{
					maxStringLen += maxStringLen + 8096 + lineLen*160;
					strings = (char*) realloc(strings, maxStringLen);
				}

				wchar_t *jap = line;
				wchar_t *eng = wcschr(line, '/');
				if (jap && eng && jap[0] !=0x3000 && jap[0] != 0xFF1F)
				{
					*eng = 0;
					eng++;

					int engLen = wcslen(eng);
					if ((wcsstr(eng, L"(ctr)") || wcsstr(eng, L"(suf)")) && !wcsstr(eng, L"(arch)"))
						flags |= JAP_WORD_COUNTER;

					// Need the open bracket.
					wchar_t *japString = NextEvilString(jap, L"; ]");
					if (engLen > 1 && japString)
					{
						if (eng[engLen-1] == '/')
							eng[--engLen] = 0;
						/*DictionaryEntry *entry = entries + numEntries;
						entry->firstString = stringLen;
						entry->numJStrings = 0;
						//*/

						WideCharToMultiByte(CP_UTF8, 0, eng, -1, strings+stringLen, maxStringLen-stringLen, 0, 0);
						POSData POS;
						GetPartsOfSpeech(strings+stringLen, &POS);
						if (POS.pos[POS_PART])
							flags |= JAP_WORD_PART;

						int firstJStringPos = stringLen;
						int firstJString = numJStrings;
						unsigned int *lastFlags = 0;
						do
						{
							if (japString[0] == '[')
							{
								flags |= JAP_WORD_PRONOUNCE;
								flags &= ~JAP_WORD_PRIMARY;
								japString++;
							}
							if (numJStrings+20 >= maxJStrings)
							{
								maxJStrings += maxJStrings + 1024;
								jStrings = (int*) realloc(jStrings, sizeof(int) * maxJStrings);
							}
							JapString * jStringData = (JapString*) (strings+stringLen);
							jStrings[numJStrings++] = stringLen;
							//jStringData->entryIndex = numEntries;
							jStringData->startOffset = stringLen - firstJStringPos;
							jStringData->flags = flags;
#if 0
							if (flags & JAP_WORD_PRIMARY)
								jStringData->flags |= primaryFlags;
#endif
							flags &= ~JAP_WORD_PRIMARY;
							lastFlags = &jStringData->flags;
							jStringData->verbType = 0;

							// Remove extra annotations.
							if (wchar_t *p = wcschr(japString, '('))
							{
								if (wcsstr(p, L"(P)"))
									jStringData->flags |= JAP_WORD_COMMON;
#ifdef SETSUMI_CHANGES
								//hack - set custom word top priority flag (2)
								if (wcsstr(japString+w, L"(T)"))
									jStringData->flags |= JAP_WORD_TOP;
								//hackend
#endif
								*p = 0;
							}

							wcscpy(jStringData->jstring, japString);
							int len = (int) wcslen(japString);
							stringLen += (len+1)*sizeof(wchar_t) + sizeof(JapString) - sizeof(jStringData->jstring);
							// entry->numJStrings ++;

							// Hack for the copula
							if (!POS.numVerbTypes && wcsstr(eng, L"plain copula") && !wcscmp(japString, L"\x3060"))
							{
								for (int i = 0; i < num_verb_types; ++i)
								{
									if (!stricmp(conjugation_table->verb_types[i].type.c_str(), "copula"))
									{
										POS.numVerbTypes ++;
										POS.verbTypes[0] = i;
										break;
									}
								}
							}

							// Really nasty verb conjugation stuff.
							for (int vindex = 0; vindex < POS.numVerbTypes; ++vindex)
							{
								const char *typeString = conjugation_table->verb_types[POS.verbTypes[vindex]].type.c_str();
								//for (int vt = POS.verbTypes[vindex]; vt<conjTable->numVerbTypes; vt++) {
								// Slightly slower, but needed for fix below.
								for (int vt = 0; vt < num_verb_types; ++vt)
								{
									VerbType *type = &conjugation_table->verb_types[vt];
									if (strcmp(type->type.c_str(), typeString))
									{
										// Fix a couple dozen incorrectly annotated verbs.  Doesn't get them all, but gets a lot.
										// Seems to be only one verb I still have trouble with after this hack.
										if (strncmp(typeString, "v5", 2) || strncmp(type->type.c_str(), "v5", 2) ||
											strlen(typeString) != strlen(type->type.c_str()))
												continue;
									}
									int match = -1;
									for (unsigned int cj=0; cj<type->conjugations.size(); cj++)
									{
										VerbConjugation* conj = &type->conjugations[cj];
										if (conj->tenseID != type->remove_tense || conj->form != 0)
											continue;
										int sufLen = (int)conj->suffix.length();
										if (sufLen > len)
											continue;
										if (wcsijcmp(conj->suffix.c_str(), japString+len-sufLen))
											continue;
										match = cj;
										break;
									}
									if (match < 0)
										continue;

									VerbConjugation * conj = &type->conjugations[match];
									int sufLen = conj->suffix.length();
									int js;
									JapString* jStringData2;
									for (js=firstJString; js<numJStrings; js++)
									{
										jStringData2 = (JapString*)(strings+js);
										// Sometimes two different initial strings result in identical conjugations.  Not too common.
										if (jStringData2->verbType != vt+1 || wcslen(jStringData2->jstring) != len-sufLen || !wcsnijcmp(jStringData2->jstring, japString, len-sufLen)) continue;
									}
									if (js < numJStrings) break;

									jStringData2 = (JapString*) (strings+stringLen);
									jStrings[numJStrings++] = stringLen;

									*jStringData2 = *jStringData;
									jStringData2->verbType = vt+1;
									wcscpy(jStringData2->jstring, japString);
									jStringData2->jstring[len-sufLen] = 0;
									jStringData2->startOffset = stringLen - firstJStringPos;

									stringLen += (len+1-sufLen)*sizeof(wchar_t) + sizeof(JapString) - sizeof(jStringData2->jstring);
									// entry->numJStrings ++;

									lastFlags = &jStringData2->flags;

									break;
								}
							}
						}
						while (japString = NextEvilString(0, L"; ]"));
						lastFlags[0] |= JAP_WORD_FINAL;
						int elen = WideCharToMultiByte(CP_UTF8, 0, eng, -1, strings+stringLen, maxStringLen-stringLen, 0, 0);

						stringLen += (elen+1)&~1;
					}
				}
				line = next;
				// numEntries++;
			}
			free(data);

			wchar_t **SortedStrings = (wchar_t**) malloc(sizeof(wchar_t*) * numJStrings);
			for (int i=0; i<numJStrings; i++)
				SortedStrings[i] = ((JapString*)(strings + jStrings[i]))->jstring;
			qsort(SortedStrings,numJStrings,sizeof(wchar_t*),CompareWcharJap);
			// Needed for sizeof operator.
			JapString * jStringData = 0;
			for (int i=0; i<numJStrings; i++)
				jStrings[i] = (int) (((char*) SortedStrings[i])-strings) - (sizeof(JapString) - sizeof(jStringData->jstring));
			free(SortedStrings);

			DictionaryHeader header;
			header.magic = DICT_MAGIC;
			header.version = DICT_VERSION;
			header.srcSig = srcSig;
			header.conjSig = conjugation_table->sig;
			// header.numEntries = numEntries;
			header.numJStrings = numJStrings;
			header.flags = 0;
			wchar_t *fname = wcsrchr(inPath, '\\');

			if (fname) fname++;
			else fname = inPath;

			if (!wcsnicmp(fname, L"enam", 4))
				header.flags |= DICT_FLAG_NAMES;

			HANDLE hFile = CreateFile(path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
			if (hFile != INVALID_HANDLE_VALUE)
			{
				DWORD junk;
				WriteFile(hFile, &header, sizeof(header), &junk, 0);
				//WriteFile(hFile, entries, sizeof(entries[0]) * numEntries, &junk, 0);
				WriteFile(hFile, jStrings, sizeof(jStrings[0]) * numJStrings, &junk, 0);
				WriteFile(hFile, strings, stringLen, &junk, 0);
				CloseHandle(hFile);
			}

			// free(entries);
			free(strings);
			free(jStrings);
			return 1;
		}
	}
	free(data);
	return 0;
}

void DictCheck()
{
	CreateDirectoryW(L"dictionaries",0);
	WIN32_FIND_DATAW data;
	HANDLE hFind = FindFirstFileW(L"dictionaries\\*", &data);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
			wchar_t *ext = wcschr(data.cFileName, '.');
			if (ext && wcsicmp(ext, L".gz")) continue;
			wchar_t srcPath[MAX_PATH*2], dstPath[MAX_PATH*2];
			wsprintfW(srcPath, L"dictionaries\\%s", data.cFileName);
			wsprintfW(dstPath, L"dictionaries\\%s.bin", data.cFileName);

			FileSig srcSig;
			srcSig.createTime = *(__int64*)&data.ftCreationTime;
			srcSig.modTime = *(__int64*)&data.ftLastWriteTime;
			srcSig.size = data.nFileSizeLow + (((__int64)data.nFileSizeHigh)<<32);

			int i;
			for (i=0; i<numDicts; i++)
				if (!wcsicmp(dicts[i]->fileName, dstPath)) break;
			if (i < numDicts)
			{
				Dictionary *dict = dicts[i];
				if (srcSig.createTime == dict->header->srcSig.createTime &&
					srcSig.modTime == dict->header->srcSig.modTime &&
					srcSig.size == dict->header->srcSig.size &&
					conjugation_table->sig.createTime == dict->header->conjSig.createTime &&
					conjugation_table->sig.modTime == dict->header->conjSig.modTime &&
					conjugation_table->sig.size == dict->header->conjSig.size)
						// Up to date.
						continue;
				CleanupDict(dict);
				dicts[i] = dicts[--numDicts];
			}
			CreateDict(srcPath, srcSig, dstPath);
			dicts = (Dictionary**) realloc(dicts, sizeof(Dictionary*) * (numDicts+1));
			if (dicts[numDicts] = LoadDict(dstPath))
				numDicts++;
		}
		while (FindNextFileW(hFind, &data));
		FindClose(hFind);
	}
}

bool LoadConjugationTable()
{
	scoped_ptr<Value> root_value(Value::FromFile(L"dictionaries\\Conjugations.txt"));
	ListValue* conjugation_list;
	if (!root_value.get() || !root_value->AsList(&conjugation_list))
		return false;

	WIN32_FILE_ATTRIBUTE_DATA fileInfo;
	GetFileAttributesExW(L"dictionaries\\Conjugations.txt", GetFileExInfoStandard, &fileInfo);

	conjugation_table.reset();

	scoped_ptr<ConjugationTable> table(new ConjugationTable());
	table->sig.createTime = *(__int64*)&fileInfo.ftCreationTime;
	table->sig.modTime = *(__int64*)&fileInfo.ftLastWriteTime;
	table->sig.size = fileInfo.nFileSizeLow + (((__int64)fileInfo.nFileSizeHigh)<<32);

	for (int i=0; i<sizeof(staticTenses)/sizeof(staticTenses[0]); ++i)
		table->tense_names.push_back(staticTenses[i]);

	for (unsigned int i = 0; i < conjugation_list->Size(); ++i)
	{
		table->verb_types.push_back(VerbType());
		VerbType* verb_type = &table->verb_types.back();
		verb_type->remove_tense = TENSE_NON_PAST;

		DictionaryValue* dict_value;
		ListValue* tense_list;
		std::wstring part_of_speech;

		bool success = conjugation_list->GetDictionaryAt(i, &dict_value) &&
			dict_value->GetStringAt(L"Name", &verb_type->type) &&
			dict_value->GetStringAt(L"Part of Speech", &part_of_speech) &&
			dict_value->GetListAt(L"Tenses", &tense_list);
		verb_type->is_adjective = (part_of_speech == L"Adj");
		if (!success || (!verb_type->is_adjective && part_of_speech != L"Verb"))
			return false;

		for (unsigned int j = 0; j < tense_list->Size(); ++j)
		{
			verb_type->conjugations.push_back(VerbConjugation());
			VerbConjugation* verb_conjugation = &verb_type->conjugations.back();
			verb_conjugation->next_verb_type_id = 0;

			bool formal;
			bool negative;
			std::wstring tense;
			bool success = tense_list->GetDictionaryAt(j, &dict_value) &&
				dict_value->GetBooleanAt(L"Formal", &formal) &&
				dict_value->GetBooleanAt(L"Negative", &negative) &&
				dict_value->GetStringAt(L"Suffix", &verb_conjugation->suffix) &&
				dict_value->GetStringAt(L"Tense", &tense);
			if (!success)
				return false;
			dict_value->GetStringAt(L"Next Type", &verb_conjugation->next_type);
			verb_conjugation->form = static_cast<int>(formal) + static_cast<int>(negative) * 2;
			unsigned int k;
			for (k = 0; k < table->tense_names.size(); ++k)
				if (table->tense_names[k] == tense)
					break;
			if (k == table->tense_names.size())
				table->tense_names.push_back(tense);
			verb_conjugation->tenseID = k;
			if (k == TENSE_REMOVE)
				verb_type->remove_tense = k;
		}
	}

	// Handle stacking verb forms.
	for (unsigned int vt = 0; vt < table->verb_types.size(); ++vt)
	{
		VerbType *type = &table->verb_types[vt];
		for (unsigned int ct = 0; ct < type->conjugations.size(); ++ct)
		{
			VerbConjugation* conj = &type->conjugations[ct];
			if (conj->next_type.empty())
				continue;
			bool found = false;
			int len = conj->suffix.length();
			for (unsigned int vt2 = 0; vt2 < table->verb_types.size() && !found; ++vt2)
			{
				VerbType *type2 = &table->verb_types[vt2];
				if (type2->type != conj->next_type)
					continue;
				for (unsigned int ct2 = 0; ct2 < type2->conjugations.size(); ++ct2)
				{
					VerbConjugation* conj2 = &type2->conjugations[ct2];
					if (conj2->tenseID != type2->remove_tense || conj2->form != 0)
						continue;
					int len2 = conj2->suffix.length();
					if (len2 > len) continue;
					if (!wcsnijcmp(conj->suffix.c_str()+len-len2, conj2->suffix.c_str(), len2))
					{
						conj->suffix.resize(len - len2);
						found = true;
						conj->next_verb_type_id = vt2 + 1;
						break;
					}
				}
			}
			if (!found)
				return false;
		}
	}

	conjugation_table.reset(table.release());
	return true;
}

void LoadDicts()
{
	if (!numDicts)
	{
		// Not really needed, but frees up old conjugation table.
		CleanupDicts();
		if (!LoadConjugationTable()) return;

		WIN32_FIND_DATAW data;
		HANDLE hFind = FindFirstFileW(L"dictionaries\\*.bin", &data);
		if (hFind != INVALID_HANDLE_VALUE)
		{
			do
			{
				if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
				wchar_t path[MAX_PATH*2];
				wsprintfW(path, L"dictionaries\\%s", data.cFileName);
				Dictionary *dict = LoadDict(path);
				if (!dict) continue;
				dicts = (Dictionary**) realloc(dicts, sizeof(Dictionary*) * (numDicts+1));
				dicts[numDicts++] = dict;
			}
			while (FindNextFileW(hFind, &data));
			FindClose(hFind);
		}
	}
	DictCheck();
}

void CleanupDicts()
{
	conjugation_table.reset();

	for (int i=0; i<numDicts; i++)
		CleanupDict(dicts[i]);
	free(dicts);
	dicts = 0;
	numDicts = 0;
}

JapString *GetJapString(Dictionary *dict, int mid)
{
	return (JapString*)(dict->strings + dict->jStrings[mid]);
}

int FindVerbMatches(const wchar_t *string, int slen, int vtype, Match* &matches, int &numMatches, int depth, int inexact)
{
	VerbType *type = &conjugation_table->verb_types[vtype-1];
	int added = 0;
	int num_conjugations = type->conjugations.size();
	for (int cj = 0; cj < num_conjugations; ++cj)
	{
		VerbConjugation *conj = &type->conjugations[cj];
		if (conj->tenseID == TENSE_REMOVE) continue;
		int suffixLen = conj->suffix.length();
		if (!wcsnijcmp(conj->suffix.c_str(), string+slen, suffixLen))
		{
			int inexact2 = inexact;
			if (wcsnicmp(conj->suffix.c_str(), string+slen, suffixLen))
				inexact2 = 1;
			if (!conj->next_verb_type_id)
			{
				if (numMatches % 32 == 0)
					matches = (Match*) realloc(matches, sizeof(Match) * (numMatches + 32));
				memset(matches[numMatches].conj, 0, sizeof(matches[numMatches].conj));
				matches[numMatches].conj[depth].verbType = vtype;
				matches[numMatches].conj[depth].verbForm = conj->form;
				matches[numMatches].conj[depth].verbTense = conj->tenseID;
				matches[numMatches].conj[depth].verbConj = cj;
				matches[numMatches].inexactMatch = inexact2;
				matches[numMatches].len = slen + suffixLen;

				numMatches++;
				added++;
			}
			else
			{
				// If not first, don't count stems towards limit, and don't add to list.
				if (depth && conj->tenseID == TENSE_STEM && !conj->form)
					added += FindVerbMatches(string, slen+suffixLen, conj->next_verb_type_id, matches, numMatches, depth, inexact2);
				else
				{
					if (depth < MAX_CONJ_DEPTH-1)
					{
						int newlyAdded = FindVerbMatches(string, slen+suffixLen, conj->next_verb_type_id, matches, numMatches, depth+1, inexact2);
						for (int m = numMatches-newlyAdded; m<numMatches; m++)
						{
							if (matches[m].conj[depth+1].verbTense == conj->tenseID)
							{
								// Remove the lovely "Potential Potential" results.
								if (conj->tenseID == TENSE_POTENTIAL)
								{
									matches[m] = matches[numMatches-1];
									newlyAdded--;
									numMatches--;
									m--;

									continue;
								}
							}
							matches[m].conj[depth].verbType = vtype;
							matches[m].conj[depth].verbForm = conj->form;
							matches[m].conj[depth].verbConj = cj;
							matches[m].conj[depth].verbTense = conj->tenseID;
						}
						added += newlyAdded;
					}
				}
			}
		}
	}
	return added;
}

void FindMatches(const wchar_t *string, int stringLen, Match *&matches, int &numMatches)
{
	matches = 0;
	numMatches = 0;
	for (int i=0; i<numDicts; i++)
	{
		Dictionary *dict = dicts[i];
		int first = 0;
		int last = dict->numJStrings-1;
		// len 0 is for verbs which have 0 characters after removing the suffix.
		int len = 0;

		int min = first;
		int max = last;
		JapString *jap;
		// Find the last possible match.
		// Makes searches a bit faster, as I only have to look at one character at a time and
		// have to search less stuff at each pass.
		while (min < max)
		{
			int mid = (min+max+1)/2;
			jap = GetJapString(dict, mid);
			int cmp = wcsijcmp(string, jap->jstring);
			if (cmp >= 0) min = mid;
			else if (cmp < 0) max = mid-1;
		}
		last = max;
		first = 0;

		while (1)
		{
			if (len > stringLen) break;

			if (len)
			{
				int min = first;
				int max = last;
				JapString *jap;
				while (min < max)
				{
					int mid = (min+max)/2;
					jap = GetJapString(dict, mid);
					// All magically long enough for this and first len-1 characters all match.
					int cmp = wcsnijcmp(string+len-1, jap->jstring+len-1, 1);
					if (cmp > 0) min = mid+1;
					else if (cmp <= 0) max = mid;
				}
				first = min;
			}

			while (first <= last)
			{
				jap = GetJapString(dict, first);
				int slen = (int) wcslen(jap->jstring);
				int cmp = wcsnijcmp(string, jap->jstring, slen);
				first++;
				if (!cmp)
				{
					int inexactMatch = wcsnicmp(string, jap->jstring, slen);

					if (!jap->verbType)
					{
						if (numMatches % 32 == 0)
							matches = (Match*) realloc(matches, sizeof(Match) * (numMatches + 32));
						memset(matches[numMatches].conj, 0, sizeof(matches[numMatches].conj));
						matches[numMatches].jString = jap;
						matches[numMatches].firstJString = (JapString*)(((char*)jap) - jap->startOffset);
						matches[numMatches].inexactMatch = inexactMatch;
						matches[numMatches].srcLen = matches[numMatches].len = slen;
						matches[numMatches].start = 0;
						matches[numMatches].japFlags = jap->flags;
						matches[numMatches].dictIndex = i;
						matches[numMatches].matchFlags = 0;
						if (dict->header->flags & DICT_FLAG_NAMES)
							matches[numMatches].matchFlags |= MATCH_IS_NAME;
						int j;
						for (j=0; j<numMatches; j++)
						{
							if (!memcmp(matches+numMatches, matches+j, sizeof(Match)-sizeof(int)))
							{
MessageBoxA(NULL, "Please report text to developer", "TEST_CASE_2", MB_ICONWARNING);
								// If have exact and inexact matches, exact overwrite inexact.
								if (!matches[numMatches].inexactMatch)
									matches[j].inexactMatch = 0;
								break;
							}
						}
						if (j == numMatches && (!inexactMatch || !(dict->header->flags & DICT_FLAG_NAMES)))
							numMatches++;
					}
					else
					{
						int addedMatches = FindVerbMatches(string, slen, jap->verbType, matches, numMatches, 0, inexactMatch);
						if (addedMatches)
						{
							for (int m = numMatches-addedMatches; m<numMatches; m++)
							{
								matches[m].jString = jap;
								matches[m].firstJString = (JapString*)(((char*)jap) - jap->startOffset);
								matches[m].inexactMatch |= inexactMatch;
								matches[m].srcLen = slen;
								matches[m].start = 0;
								matches[m].japFlags = jap->flags;
								matches[m].dictIndex = i;
								matches[m].matchFlags = 0;

								for (int j=0; j<m; j++)
								{
									if (!memcmp(matches+m, matches+j, sizeof(Match)-sizeof(int)))
									{
MessageBoxA(NULL, "Please report text to developer", "TEST_CASE_1", MB_ICONWARNING);
										// If have exact and inexact matches, exact overwrite inexact.
										if (!matches[m].inexactMatch)
											matches[j].inexactMatch = 0;
										numMatches--;
										m--;
										break;
									}
								}
							}
						}
					}
				}
				else if (cmp > 0)
					break;
				else
				{
					// Minor optimization.
					while (len < slen && len < stringLen && !wcsnijcmp(string+len, jap->jstring+len, 2))
						len++;
					break;
				}
			}
			if (first > last) break;
			len++;
		}
	}
}

void FindAllMatches(const wchar_t *string, int len, Match *&matches, int &numMatches)
{
	matches = 0;
	numMatches = 0;
	int maxMatches = 0;
	for (int i=0; i<len; i++)
	{
		Match *matches2;
		int numMatches2;
		FindMatches(string+i, len-i, matches2, numMatches2);
		for (int j=0; j<numMatches2; j++)
			matches2[j].start = i;
		if (numMatches + numMatches2 > maxMatches)
		{
			maxMatches = (numMatches + numMatches2)*2 + 1024;
			matches = (Match*)realloc(matches, sizeof(Match)*maxMatches);
		}
		memcpy(matches+numMatches, matches2, numMatches2*sizeof(Match));
		numMatches += numMatches2;
		free(matches2);
	}
}

struct BestMatchInfo
{
	// -2 means no match/failure, should only have it on first position.  -1 means that
	// residue matches nothing, go to previous.  Could just use the same value for both...
	int matchIndex;
	// redundant, so not really needed.
	int matchLen;
	int score;
};

// Used to sync up inexactMatch value of different matches to same word.
// should only be an issue with verb conjugations.
int __cdecl CompareIdenticalMatches(const void *v1, const void *v2)
{
	Match *m1 = (Match*) v1;
	Match *m2 = (Match*) v2;
	if (m1->start != m2->start)
		return m1->start - m2->start;
	if (m1->dictIndex != m2->dictIndex)
		return m1->dictIndex - m2->dictIndex;
	if (m1->firstJString != m2->firstJString)
		return m1->firstJString - m2->firstJString;
	return (m1->conj[0].verbType - (int)m2->conj[0].verbType) * (1<<16) + (m1->conj[0].verbTense - (int)m2->conj[0].verbTense) * 4 + m1->conj[0].verbForm - m2->conj[0].verbForm;
}

int __cdecl CompareMatches(const void *v1, const void *v2)
{
	Match *m1 = (Match*) v1;
	Match *m2 = (Match*) v2;
	if (m1->start != m2->start)
		return m1->start - m2->start;
	if (m1->inexactMatch != m2->inexactMatch)
	{
		// Could theoretically have inexact and exact matches for the same verb or adjective, unless fixed first.
		return m1->inexactMatch - m2->inexactMatch;
	}
	int name = (m1->matchFlags & MATCH_IS_NAME) - (m2->matchFlags & MATCH_IS_NAME);
	if (name) return name;

#ifdef SETSUMI_CHANGES
	//hack - ???
	int mask = JAP_WORD_TOP | JAP_WORD_COUNTER | JAP_WORD_PART | JAP_WORD_COMMON | JAP_WORD_COMMON_LINE | JAP_WORD_PRIMARY;
	//hackend
#else
	int mask = JAP_WORD_COUNTER | JAP_WORD_PART | JAP_WORD_COMMON | JAP_WORD_COMMON_LINE | JAP_WORD_PRIMARY;
#endif
	int flagDiff = (m2->japFlags & mask) - (m1->japFlags & mask);
	if (flagDiff) return flagDiff;
	// May do something better later.
	if (m1->dictIndex != m2->dictIndex)
		return m2->dictIndex - m1->dictIndex;
	if (m1->firstJString != m2->firstJString)
		return m2->firstJString - m1->firstJString;
	return m1->conj[0].verbForm - m2->conj[0].verbForm;
}

void SortMatches(Match *&matches, int &numMatches)
{
	qsort(matches, numMatches, sizeof(Match), CompareIdenticalMatches);
	int d = numMatches > 0;
	for (int i=1; i<numMatches; i++)
	{
		int j = i, k = d - 1;
		while (matches[j].dictIndex    == matches[k].dictIndex &&
		       matches[j].firstJString == matches[k].firstJString &&
		       matches[j].start        == matches[k].start &&
		       matches[j].len          == matches[k].len &&
		       matches[j].inexactMatch != matches[k].inexactMatch)
		{
			matches[j].inexactMatch = matches[k].inexactMatch = 0;
			j = k;
			if (--k < 0)
				break;
		}
		// If have multiple matches using the same conjugation of different base verb forms,
		// remove duplicates.  Not sure how likely this is.  If any adj-na's have both
		// purely hiragana and katakana entries, they could result in this.
		if (!memcmp(&matches[i], &matches[d-1], sizeof(Match)))
			continue;
		if (matches[i].dictIndex    == matches[d-1].dictIndex &&
			matches[i].firstJString == matches[d-1].firstJString &&
			matches[i].start        == matches[d-1].start &&
			matches[i].len          == matches[d-1].len &&
			matches[i].conj[0].verbType && !matches[d-1].conj[0].verbType &&
			matches[i].conj[0].verbForm == 0 && matches[i].conj[0].verbTense == TENSE_NON_PAST &&
				(i+1 == numMatches ||
				matches[i+1].dictIndex    != matches[i].dictIndex ||
				matches[i+1].firstJString != matches[i].firstJString ||
				matches[i+1].len          != matches[i].len ||
				matches[i+1].start        != matches[i].start))
			continue;
		matches[d++] = matches[i];
	}
	numMatches = d;
	qsort(matches, numMatches, sizeof(Match), CompareMatches);
}

#define MECAB_BAD_END   0x01
#define MECAB_BAD_START 0x02

bool IsDigit(wchar_t c)
{
	return c - '0' <= 9u || c - L'０' <= 9u || c == L'一' || c == L'二' || c == L'三' || c == L'四' || c == L'五' ||
		c == L'六' || c == L'七' || c == L'八' || c == L'九' || c == L'十' || c == L'百' || c == L'千' || c == L'万';
}

void FindBestMatches(const wchar_t *string, int len, Match *&matches, int &numMatches, int useMecab, JParserCallback* callback)
{
	if (callback)
		callback(SEARCHING_DICTIONARY);
	FindAllMatches(string, len, matches, numMatches);
	if (!numMatches) return;
	BestMatchInfo *best = (BestMatchInfo*) malloc(sizeof(BestMatchInfo) * (len+1) + sizeof(char) * len);

	unsigned char *posFlags = (unsigned char*) (best + len + 1);
	memset(posFlags, 0, sizeof(char) * len);
	wchar_t *mecab;
	if (useMecab)
	{
		if (callback)
			callback(RUNNING_MECAB);

		if ((mecab = MecabParseString(string, len, 0)))
		{
			int pos = 0;
			wchar_t *line = wcstok(mecab, L"\r\n");
			while (line)
			{
				if (wcscmp(line, L"EOS"))
				{
					wchar_t *word = line, *end;
					if (word && (end = wcschr(word, '\t')))
					{
						*end = 0;
						int matchLen = 0;
						int oldPos = pos;
						while (string[pos])
						{
							if (!word[matchLen]) break;
							if (string[pos] == word[matchLen])
								matchLen++;
							pos++;
						}
						if (!word[matchLen] && !wcsnijcmp(&string[pos-matchLen], word, matchLen))
						{
							wchar_t *srcWord = end+1;
							// If katakana is '*' or does not exist, not real word, so don't penalize.
							for (int i=0; i<7; i++)
							{
								if (srcWord) srcWord = wcschr(srcWord, ',');
								if (!srcWord) break;
								srcWord++;
							}
							if (srcWord && srcWord[0] != '*' && srcWord[0] != ',')
								for (int i=0; i<matchLen-1; i++)
								{
									posFlags[pos-matchLen + i] |= MECAB_BAD_END;
									posFlags[pos-matchLen + i + 1] |= MECAB_BAD_START;
								}
						}
						else
							// I don't trust mecab all that much.
							pos = oldPos;
					}
				}
				line = wcstok(0, L"\r\n");
			}
			free(mecab);
		}
	}

	if (callback)
		callback(FINDING_BEST_WORDS);

	// Note:  High score is bad, low is good.  Currently doesn't have to be signed,
	// but if I add enough bonuses, that could change.
	best[0].score = 0;
	best[0].matchLen = 0;
	best[0].matchIndex = -2;
	for (int i=1; i <= len; i++)
	{
		// Worst possible score.
		best[i].score = 0x7FFFFFFF;
		// Don't really have to be initialized, but can't hurt.
		best[i].matchLen = 0;
		best[i].matchIndex = -2;
	}
	int pos = 0;
	int matchIndex = 0;
	while (pos < len)
	{
		// Calculate score if current character is not in a match.
		// Note that I store the score up to and including the pos character
		// in best[pos+1].  As matches are sorted by first, not last, character,
		// I do things out of order, so need the first check.
		int score = best[pos].score + 100;
		// Kanji.
		if (string[pos] >= 0x4E00 && string[pos] <= 0x9FBF)
			score += 400;
		int nextPos = pos+1;
		if (best[nextPos].score > score)
		{
			best[nextPos].score = score;
			best[nextPos].matchIndex = -1;
			best[nextPos].matchLen = 0;
		}

		while (matchIndex < numMatches && matches[matchIndex].start <= pos)
		{
			Match *match = matches + matchIndex;
			int score = best[pos].score + 10;
			if (posFlags[pos] & MECAB_BAD_START)
				score += 10;
			if (posFlags[pos + matches[matchIndex].len-1] & MECAB_BAD_END)
				score += 10;
			// Favor breaking words around particles, though not as much as entries with combined particles
			// and neighboring words.
			if (match->japFlags & JAP_WORD_PART)
				score -= 2;
			// Hack to discourage things like matching hiragana "ku" alone...
			else if (match->len == 1)
				score += 1;
			// Don't break in the middle of numbers
			else if (pos > 0 && IsDigit(string[pos]) && IsDigit(string[pos - 1]))
				score += 100;
			// Slightly favor common words.  Bonus is so small because common word annotations don't seem to
			// be all that good.
			if (match->japFlags & (JAP_WORD_COMMON | JAP_WORD_COMMON_LINE))
				score -= 3;
			if (match->japFlags & JAP_WORD_COUNTER)
			{
				int i = matches[matchIndex].start - 1;
				while (i >= 0 && (string[i] == ' ' || string[i] == L'　'))
					i--;
				if (i >= 0 && IsDigit(string[i]))
					score -= 2; // boost counter words after numbers
				else
					match->japFlags &= ~JAP_WORD_COUNTER; // don't bring counter variants to top on sort later
			}
			// Penalize for inexact matches (Hiragana/Katakana matched to each other).
			if (match->inexactMatch)
				score += 10;
			if (dicts[match->dictIndex]->header->flags & DICT_FLAG_NAMES)
			{
				int mad = match->inexactMatch;
				if (!mad)
				{
					const wchar_t *base = string + matches[matchIndex].start;
					for (int i=0; i<matches[matchIndex].len; i++)
					{
						if (!IsKatakana(base[i]))
						{
							mad = 1;
							break;
						}
					}
					if (matches[matchIndex].start && IsKatakana(base[-1]))
						mad = 1;
					int last = matches[matchIndex].len;
					if (last+matches[matchIndex].start < len && IsKatakana(base[last]))
						mad = 1;
				}
				if (mad)
					score += 500 * match->len;
				else
					score += 5;
			}

#ifdef SETSUMI_CHANGES
			//hack - set custom word top priority score
			if (match->japFlags & JAP_WORD_TOP) {
				//Beep(500,50);
				score = -999999;
			}
			//hackend
#endif

			/*
			// Bonus for exactly matching a primary entry - basically discourage
			// Hiragana entries for entries with kanji, in favor of words with
			// no Kanji.  Helps with some long runs or hiragana.
			if (match->japFlags & JAP_WORD_PRIMARY)
				score -= 1;
			//*/

			int nextPos = pos + match->len;
			if (best[nextPos].score >= score)
			{
				best[nextPos].score = score;
				best[nextPos].matchLen = match->len;
				best[nextPos].matchIndex = matchIndex;
			}
			matchIndex++;
		}

		pos ++;
	}
#if 0
	static wchar_t buf[0x1000];
	*buf = 0;
	for (int i=0; i<numMatches; i++)
	{
		EntryData entry;
		GetDictEntry(matches[i], &entry);
		swprintf(buf + wcslen(buf), L"%2i: [%.*s] - <%hs>", i, matches[i].len, string + matches[i].start, entry.english);
		for (int j = 0; j < entry.numJap; j++)
			swprintf(buf + wcslen(buf), L" [%s]", entry.jap[j]->jstring);
		wcscat(buf, L"\n");
	}
	MessageBoxW(NULL, buf, L"dbg", 0);
#endif
	// Find all matches that align with the start/stop positions of the best scoring
	// set of matches, even if they didn't contribute to the score.
	int index = numMatches-1;
	int outIndex = index;
	while (pos > 0)
	{
		if (best[pos].matchIndex < 0)
		{
			while (pos > 0 && best[pos].matchIndex < 0)
				pos--;
			continue;
		}
		int start = pos-best[pos].matchLen;
		int len = best[pos].matchLen;
		while (index >= 0 && matches[index].start >= start)
		{
			if (matches[index].start == start && matches[index].len == len)
				matches[outIndex--] = matches[index];
			index--;
		}
		pos = start;
	}
	free(best);
	numMatches = numMatches-1 - outIndex;
	memmove(matches, matches+outIndex+1, sizeof(Match) * numMatches);
	matches = (Match*) realloc(matches, sizeof(Match) * numMatches);
}

bool DictsLoaded()
{
	if (dictionary_read_write_lock.TryGetWriteLock())
	{
		LoadDicts();
		bool return_value = numDicts > 0;
		dictionary_read_write_lock.Release();
		return return_value;
	}
	AutoLock lock(dictionary_read_write_lock);
	return numDicts > 0;
}

int GetDictEntry(Match &match, EntryData *out)
{
	if (!GetDictEntry(match.dictIndex, match.firstJString, out)) return 0;
	if (match.conj[0].verbType)
	{
		VerbType *type = &conjugation_table->verb_types[match.conj[0].verbType-1];
		VerbConjugation *conj = &type->conjugations[match.conj[0].verbConj];
		if (conj->suffix[0] >= 0x4E00 && conj->suffix[0] <= 0x9FBF)
		{
			int len = conj->suffix.length();
			for (unsigned int i=0; i<conjugation_table->verb_types.size(); ++i)
			{
				VerbType *type2 = &conjugation_table->verb_types[i];
				if (type2->type != type->type || type2 == type) continue;
				int best = 10000;
				const wchar_t *bestString = 0;
				for (unsigned int j=0; j<type2->conjugations.size(); j++)
				{
					VerbConjugation *conj2 = &type2->conjugations[j];
					if (conj->tenseID != conj2->tenseID || conj->form != conj2->form || conj->next_verb_type_id != conj2->next_verb_type_id) continue;
					int len2 = conj2->suffix.length();
					if (len2 < len || len2 >= best) continue;
					if (!wcsnijcmp(conj->suffix.c_str()+1, conj2->suffix.c_str() + len2-len+1, len-1))
					{
						best = len2;
						bestString = conj2->suffix.c_str();
					}
				}
				if (bestString)
				{
					int lenWant = wcslen(bestString)-len+1;
					if (lenWant <= 3)
					{
						memcpy(out->kuruHack, bestString, sizeof(wchar_t)*lenWant);
						out->kuruHack[lenWant] = 0;
						break;
					}
				}
			}
		}
	}
	return 1;
}

int GetDictEntry(int dictIndex, JapString *jap, EntryData *out)
{
	if (dictIndex >= numDicts || dictIndex<0) return 0;
	out->kuruHack[0] = 0;
	JapString *jap2 = (JapString*)(((char*)jap) - jap->startOffset);
	int i=0;
	while (1)
	{
		int flags = jap2->flags;
		if (i<256)
		{
			out->numJap = i+1;
			out->jap[i] = jap2;
		}
		i++;
		jap2 = (JapString*) (((char*)jap2) + sizeof(JapString) - sizeof(jap2->jstring) + sizeof(wchar_t) * (wcslen(jap2->jstring)+1));
		if (flags & JAP_WORD_FINAL) break;
	}
	out->english = (char*) jap2;
	return 1;
}

void FindExactMatches(wchar_t *string, int len, Match *&matches, int &numMatches)
{
	FindMatches(string, len, matches, numMatches);
	if (numMatches)
	{
		int p1 = 0;
		for (int p2=0; p2<numMatches; p2++)
			if (matches[p2].len == len)
				matches[p1++] = matches[p2];
		numMatches = p1;
		matches = (Match*) realloc(matches, numMatches*sizeof(Match));
	}
}

struct POSList
{
	char name[7];
	unsigned char pos;
};

void GetPartsOfSpeech(char *eng, POSData *pos)
{
	static const POSList posList[] =
	{
		"prt", POS_PART,
		"conj", POS_PART,
	};
	memset(pos, 0, sizeof(*pos));
	if (eng[0] == '(')
	{
		char *start = eng+1;
		while (1)
		{
			while (*start == ',' || *start==' ' || *start==';') start++;
			if (*start ==')') break;
			char *end = start;
			while (*end != ',' && *end!=' ' && *end!=';' && *end!=')' && *end) end++;
			int len = (int)(end-start);
			unsigned int i;
			for (i=0; i<sizeof(posList)/sizeof(posList[0]); i++)
				if (!strncmp(posList[i].name, start, len) && len == strlen(posList[i].name))
				{
					pos->pos[posList[i].pos] = 1;
					break;
				}
			if (i == sizeof(posList)/sizeof(posList[0]))
				for (i = 0; i<conjugation_table->verb_types.size(); i++)
					if (!strncmp(conjugation_table->verb_types[i].type.c_str(), start, len) && len == conjugation_table->verb_types[i].type.length())
					{
						if (!pos->pos[POS_LINEBREAK + 1 + i])
						{
							pos->pos[POS_LINEBREAK + 1 + i] = 1;
							if (pos->numVerbTypes < 10)
								pos->verbTypes[pos->numVerbTypes++] = i;
						}
						break;
					}
			start = end;
		}
	}
}

int GetConjString(wchar_t *temp, Match *match)
{
	*temp = 0;
	for (int i=0; i<MAX_CONJ_DEPTH && match->conj[i].verbType; i++)
	{
		if (match->conj[i].verbForm & 2)
			wcscat(temp, L"Negative ");
		if (match->conj[i].verbForm & 1)
			wcscat(temp, L"Formal ");
		if (i && match->conj[i].verbTense == TENSE_NON_PAST &&
			(i>1 || match->conj[0].verbTense != TENSE_STEM)) continue;
		if (match->conj[i].verbTense == TENSE_STEM) continue;
		wcscat(temp, conjugation_table->tense_names[match->conj[i].verbTense].c_str());
		wcscat(temp, L" ");
	}
	int q = wcslen(temp);
	if (q && temp[q-1]== ' ')
		temp[--q] = 0;
	return q;
}
