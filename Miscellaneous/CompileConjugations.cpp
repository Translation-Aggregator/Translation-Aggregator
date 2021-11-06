int ParseOldConjugations()
{
	// Code to load and convery old conjugation format.  It's easier to
	// modify the old file and convert it, than to modify the new format.
	// New format is just for easy loading.

	wchar_t *data;
	int size;
	LoadFile(L"dictionaries\\Conjugations_old.txt", &data, &size);
	if (!data) return 0;
	wchar_t *d = data;

	ListValue* list_value = new ListValue();
	DictionaryValue* dict_value = NULL;
	ListValue* sub_list = NULL;;

	while (*d)
	{
		int endPos = wcscspn(d, L"\r\n");
		if (!endPos)
		{
			d++;
			continue;
		}
		if (d[endPos])
		{
			d[endPos] = 0;
			endPos++;
		}
		wchar_t *strings[5] = {0,0,0,0,0};
		strings[0] = d;
		int i = 1;
		while (i<5 && strings[i-1][0] && (strings[i] = wcschr(strings[i-1], '\t')))
		{
			strings[i][0] = 0;
			strings[i]++;
			i++;
		}
		for (int j=0; j<i; j++)
		{
			// Remove any extra whitespace.
			while (strings[j][0] == ' ') strings[j]++;
			wchar_t *q = wcschr(strings[j], 0);
			while (q > strings[j] && q[-1] == ' ')
			{
				q--;
				q[0] = 0;
			}
		}

		if (strings[0] && strings[0][0] && strings[0][0] != '/')
		{
			if (!wcscmp(strings[0], L"Verb") || !wcscmp(strings[0], L"Adj") && strings[1] && strings[1][0])
			{
				dict_value = new DictionaryValue();
				list_value->Append(dict_value);
				dict_value->SetStringAt(L"Part of Speech", strings[0]);
				dict_value->SetStringAt(L"Name", strings[1]);
				sub_list = new ListValue();
				dict_value->Set(L"Tenses", sub_list);
			}
			else if (dict_value)
			{
				std::wstring tense = strings[0];
				for (i=0; i<4; i++)
				{
					wchar_t *s = strings[i+1];
					if (s && s[0])
					{
						if (s[0] == ',')
							s++;
						if (s[0])
							s = mywcstok(s, L" ,");
						else
							// prevents crash on next mywcstok.
							mywcstok(s, L" ,");
						do
						{
							DictionaryValue* sub_dict = new DictionaryValue();
							sub_dict->SetStringAt(L"Tense", tense);
							wchar_t* q1 = wcschr(s, '(');
							wchar_t* q2 = wcschr(s, ')');
							if (q1 && q2)
								*q1 = *q2 = 0;
							sub_dict->SetStringAt(L"Suffix", s);
							if (q1 && q2)
							{
								sub_dict->SetStringAt(L"Next Type", q1+1);
								*q1 = '(';
								*q2 = ')';
							}
							sub_dict->SetBooleanAt(L"Formal", (i&1) == 1);
							sub_dict->SetBooleanAt(L"Negative", i>1);
							sub_list->Append(sub_dict);
						}
						while (s = mywcstok(0, L" ,"));
					}
				}
			}
		}
		d += endPos;
	}
	free(data);

	std::wstring test = list_value->ToString(true);
	FILE *out = fopen("Goatling.txt", "wb");
	fwrite("\xFF\xFE", 2, 1, out);
	fwrite(test.c_str(), 2, test.length(), out);
	fclose(out);
	delete list_value;
	return 1;
}
