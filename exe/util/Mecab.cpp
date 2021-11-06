#include <Shared/Shrink.h>
#include <Shared/StringUtil.h>
#include <Shared/Lock.h>
#include "Mecab.h"

namespace
{

// Used to allow use of MeCab on any thread.
// Unfortunately, MeCab is not threadsafe.
Lock mecab_init_lock_;

typedef struct mecab_t mecab_t;
HMODULE libmecab = 0;

struct mecab_dictionary_info_t
{
	const char                     *filename;
	const char                     *charset;
	unsigned int                    size;
	int                             type;
	unsigned int                    lsize;
	unsigned int                    rsize;
	unsigned short                  version;
	struct mecab_dictionary_info_t *next;
};

typedef mecab_t* __cdecl mecab_new2_type(const char *arg);
mecab_new2_type *mecab_new2 = 0;

typedef void __cdecl mecab_destroy_type(mecab_t *mecab);
mecab_destroy_type *mecab_destroy = 0;

typedef const char* __cdecl mecab_sparse_tostr_type(mecab_t *mecab, const char *str);
mecab_sparse_tostr_type *mecab_sparse_tostr = 0;

typedef const mecab_dictionary_info_t* __cdecl mecab_dictionary_info_type(mecab_t *mecab);
mecab_dictionary_info_type *mecab_dictionary_info = 0;

// Should only be called on quit, or by InitMecab.
void UninitMecab()
{
	if (libmecab)
	{
		FreeLibrary(libmecab);
		libmecab = 0;
	}
}

}  // namespace

int InitMecab()
{
	AutoLock lock(mecab_init_lock_);
	if (libmecab) return 1;
	wchar_t newPath[MAX_PATH*2];
	for (int i=0; i<4; i++)
	{
		if (!i)
			newPath[0] = 0;
		else if (i < 3)
		{
			const HKEY base[2] = {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE};
			HKEY hKey = 0;
			if (ERROR_SUCCESS != RegOpenKey(base[i-1], L"Software\\Mecab", &hKey))
				continue;
			DWORD type;
			DWORD size = MAX_PATH;
			wchar_t *name;
			int res = RegQueryValueExW(hKey, L"mecabrc", 0, &type, (BYTE*)newPath, &size);
			RegCloseKey(hKey);
			if (ERROR_SUCCESS != res || type != REG_SZ || !(name = wcsrchr(newPath, '\\')) || name-newPath < 4 || wcsnicmp(name-4, L"\\etc\\", 5))
				continue;
			wcscpy(name-3, L"bin\\");
		}
		else
		{
			if (S_OK != SHGetFolderPathW(0, CSIDL_PROGRAM_FILES, 0, SHGFP_TYPE_CURRENT, newPath)) continue;
			wcscat(newPath, L"\\Mecab\\bin\\");
		}
		//wcscpy(mecab_path, newPath);
		wcscat(newPath, L"libmecab.dll");
		libmecab = LoadLibraryW(newPath);
		if (libmecab)
		{
			if ((mecab_new2 = (mecab_new2_type*) GetProcAddress(libmecab, "mecab_new2")) &&
				(mecab_destroy = (mecab_destroy_type*) GetProcAddress(libmecab, "mecab_destroy")) &&
				(mecab_sparse_tostr = (mecab_sparse_tostr_type*) GetProcAddress(libmecab, "mecab_sparse_tostr")) &&
				(mecab_dictionary_info = (mecab_dictionary_info_type*) GetProcAddress(libmecab, "mecab_dictionary_info")))
					return 1;
			UninitMecab();
		}
	}
	return 0;
}

wchar_t *MecabParseString(const wchar_t *string, int len, wchar_t **error)
{
	// Just to let me not have to check everywhere if I want errors
	// returned.
	wchar_t *local_error;
	if (!error)
		error = &local_error;

	*error = 0;
	mecab_t* mecab;
	AutoLock lock(mecab_init_lock_);
	if (!InitMecab() || !(mecab = mecab_new2("")))
	{
		*error = L"Failed to initialize MeCab.";
		return 0;
	}

	int charSet = 0;

	const mecab_dictionary_info_t *info = mecab_dictionary_info(mecab);
	if (info && info->charset)
		if (!stricmp(info->charset, "SHIFT-JIS"))
			charSet = 932;
		else if (!stricmp(info->charset, "EUC-JP"))
			charSet = 20932;
		else if (!stricmp(info->charset, "UTF-8"))
			charSet = CP_UTF8;
		else if (!stricmp(info->charset, "UTF-16"))
			*error = L"MeCab dll does not seem to work with UTF-16 MeCab dictionaries.  You must change their format for MeCab support to work.";
		else
			*error = L"MeCab is using an unrecognized character encoding.  Only Shift-JIS, UTF8, and EUC-JP are currently supported.";
	else
		*error = L"MeCab failed to provide character set information.";

	if (*error) return 0;

	int len2 = len+1;
	char *text = ToMultiByte(string, charSet, len2);
	wchar_t *out = 0;
	if (text)
	{
		char *temp = text;
		while (*temp)
		{
			if (temp[0] == '\n')
				temp[0] = '\r';
			temp++;
		}
		const char *res;
		if (res = mecab_sparse_tostr(mecab, text))
		{
			int len = -1;
			out = ToWideChar(res, charSet, len);
		}
		free(text);
	}
	mecab_destroy(mecab);
	if (!out)
		*error = L"Problem using MeCab";

	return out;
}

// Auto cleanup mecab on quit.
class MecabCleaner
{
public:
	~MecabCleaner()
	{
		// Not really needed.
		AutoLock lock(mecab_init_lock_);
		UninitMecab();
	}
};
static MecabCleaner MecabCleaner;
