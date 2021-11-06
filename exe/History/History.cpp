#include "Shared/Shrink.h"

#include "exe/History/History.h"

History history;

History::History() : length_(0), memory_(0), next_id_(0)
{
	last_entry_ = history_.end();
}

void History::ClearTranslations()
{
	for (TranslationHistory::iterator it = history_.begin(); it != history_.end(); ++it)
	{
		it->translations.clear();
		// Reassign ids so in-progress translations (Which my use other settings) aren't added.
		it->id = next_id_;
		++next_id_;
	}
}

void History::SetTranslation(int history_entry_id, int translator_id, const std::wstring& translation)
{
	TranslationHistoryEntry* entry = GetEntry(history_entry_id);
	if (!entry)
		return;
	entry->translations[translator_id] = translation;
}

void History::GetTranslation(int history_entry_id, int translator_id, std::wstring* translation)
{
	TranslationHistoryEntry* entry = GetEntry(history_entry_id);
	if (!entry)
	{
		translation->erase();
		return;
	}
	*translation = entry->translations[translator_id];
}

TranslationHistoryEntry* History::GetEntry(int history_entry_id)
{
	if (length_ <= 0)
		return NULL;
	if (last_entry_->id == history_entry_id)
		return &*last_entry_;
	for (TranslationHistory::reverse_iterator it = history_.rbegin(); it != history_.rend(); ++it)
	{
		if (it->id == history_entry_id)
			return &*it;
	}
	return NULL;
}

int History::AddEntry(const std::wstring& string)
{
	// Don't do anything when retranslating an identical string.
	if (length_ > 0 && last_entry_->original_string == string)
		return last_entry_->id;
	// Don't allow empty string entries.
	if (string.empty())
		return -1;

	history_.push_back(TranslationHistoryEntry(string, next_id_));
	++next_id_;
	last_entry_ = history_.end();
	--last_entry_;
	++length_;
	memory_ += GuessMemory(string);
	Trim();
	return last_entry_->id;
}

TranslationHistoryEntry* History::Forward()
{
	if (!length_)
	{
		return NULL;
	}
	if (++last_entry_ == history_.end())
	{
		--last_entry_;
		return NULL;
	}
	return &*last_entry_;
}

TranslationHistoryEntry* History::Back()
{
	if (last_entry_ == history_.begin())
	{
		return NULL;
	}
	--last_entry_;
	return &*last_entry_;
}

void History::Trim()
{
	while (true)
	{
		// Min entry count.
		if (length_ < 20)
			return;
		// Min memory count.
		if (memory_ < 20 * 1024 * 1024)
			return;
		// Safety check.  Should never fail, anyways.
		if (last_entry_ == history_.begin())
			return;

		--length_;
		memory_ -= GuessMemory(history_.begin()->original_string);
		history_.pop_front();
	}
}

int History::GuessMemory(const std::wstring& string) const
{
	return sizeof(std::wstring) + (1 + string.length()) * sizeof(wchar_t);
}
