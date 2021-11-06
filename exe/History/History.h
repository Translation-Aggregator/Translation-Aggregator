#pragma once

struct TranslationHistoryEntry
{
	std::wstring original_string;
	std::map<int, std::wstring> translations;
	int id;
	TranslationHistoryEntry(const std::wstring& string, int id) : original_string(string), id(id) {}
};
typedef std::list<TranslationHistoryEntry> TranslationHistory;

class History
{
public:
	History();
	int AddEntry(const std::wstring& string);

	void ClearTranslations();

	void SetTranslation(int history_entry_id, int translator_id, const std::wstring& translation);
	void GetTranslation(int history_entry_id, int translator_id, std::wstring* translation);
	// Note:  Not optimized.  Search backwards from last added.
	TranslationHistoryEntry* GetEntry(int history_entry_id);

	TranslationHistoryEntry* Forward();
	TranslationHistoryEntry* Back();

private:
	// Guestimates how much memory |string| takes up.
	int GuessMemory(const std::wstring& string) const;

	// Reduce number of entries from front, based on total memory usage and number of entries.
	void Trim();

	int next_id_;

	// Number of history entries.  Used primarily to determine when to add/remove entries.
	int length_;

	// Estimate of total memory used by history.  Used primarily to determine when to add/remove entries.
	int memory_;

	// Last accessed entry.  Just for navigating through the history.
	// Updated on adding a new entry as well.
	TranslationHistory::iterator last_entry_;

	TranslationHistory history_;
};

extern History history;
