#include <Shared/Shrink.h>
#include <Shared/StringUtil.h>
#include <Shared/scoped_ptr.h>

#include "Value.h"

namespace
{

void ValueToString(Value* value, bool pretty_print, int indent, std::wstringstream& stream)
{
	switch (value->type())
	{
		case VALUE_NULL:
		{
			stream << L"null";
			break;
		}
		case VALUE_BOOLEAN:
		{
			bool boolean_value;
			value->AsBoolean(&boolean_value);
			if (boolean_value)
			{
				stream << L"true";
			}
			else
			{
				stream << L"false";
			}
			break;
		}
		case VALUE_INTEGER:
		{
			int int_value;
			value->AsInteger(&int_value);
			stream << int_value;
			break;
		}
		case VALUE_DOUBLE:
		{
			double double_value;
			value->AsDouble(&double_value);
			stream << double_value;
			break;
		}
		case VALUE_STRING:
		{
			std::wstring string_value;
			value->AsString(&string_value);
			stream << L'"';
			size_t size = string_value.length();
			for (size_t i = 0; i < size; ++i)
			{
				wchar_t char_value = string_value[i];
				switch (char_value)
				{
					case '\t':
						stream << "\\t";
						break;
					case '\r':
						stream << "\\r";
						break;
					case '\n':
						stream << "\\n";
						break;
					case '\\':
						stream << "\\\\";
						break;
					case '"':
						stream << "\\\"";
						break;
					case '/':
						stream << "\\/";
						break;
					case '\b':
						stream << "\\b";
						break;
					case '\f':
						stream << "\\f";
						break;
					default:
						if (char_value >= 0x20)
							stream << char_value;
						else
						{
							wchar_t temp[10];
							swprintf(temp, L"\\u%04X", char_value);
							stream << temp;
						}
						break;
				}
			}
			stream << L'"';
			break;
		}
		case VALUE_LIST:
		{
			ListValue* list_value;
			value->AsList(&list_value);
			size_t size = list_value->Size();
			stream << L"[";
			if (pretty_print)
				++indent;
			for (size_t i = 0; i < size; ++i)
			{
				if (i > 0)
					stream << L",";
				if (pretty_print)
				{
					stream << L"\r\n";
					for (int j = 0; j < indent; ++j)
						stream << L"  ";
				}
				Value* sub_value;
				list_value->Get(i, &sub_value);
				ValueToString(sub_value, pretty_print, indent, stream);
			}
			if (pretty_print)
			{
				stream << L"\r\n";
				for (int j = 1; j < indent; ++j)
					stream << L"  ";
			}
			stream << L"]";
			break;
		}
		case VALUE_DICTIONARY:
		{
			DictionaryValue* dict_value;
			value->AsDictionary(&dict_value);
			stream << L"{";
			if (pretty_print)
				++indent;
			DictionaryValue::const_iterator it = dict_value->begin();
			while (it != dict_value->end())
			{
				if (it != dict_value->begin())
					stream << L", ";
				if (pretty_print)
				{
					stream << L"\r\n";
					for (int j = 0; j < indent; ++j)
						stream << L"  ";
				}
				stream << L'"' << it->first << L"\" : ";
				ValueToString(it->second, pretty_print, indent, stream);
				++it;
			}
			if (pretty_print)
			{
				stream << L"\r\n";
				for (int j = 1; j < indent; ++j)
					stream << L"  ";
			}
			stream << L"}";
			break;
		}
	}
}

void EatWhitespace(const wchar_t* source, size_t length, size_t& offset)
{
	while (offset < length && MyIsWspace(source[offset]))
		++offset;
}

// Returns the next non-whitespace character.  Returns 0 if there isn't one.
wchar_t PeekNextCharacter(const wchar_t* source, size_t length, size_t& offset)
{
	EatWhitespace(source, length, offset);
	if (length <= offset)
		return 0;
	return source[offset];
}

// Returns the next non-whitespace character and increments offset.
// Returns 0 if there isn't one.
wchar_t EatNextCharacter(const wchar_t* source, size_t length, size_t& offset)
{
	EatWhitespace(source, length, offset);
	if (length <= offset)
		return 0;
	return source[offset++];
}

Value* ValueFromString(const wchar_t* source, size_t length, size_t& offset)
{
	switch (PeekNextCharacter(source, length, offset))
	{
		case 't':
		{
			if (length - offset < 4 || wcsncmp(source + offset, L"true", 4))
				return NULL;
			offset += 4;
			return new BooleanValue(true);
		}
		case 'f':
		{
			if (length - offset < 5 || wcsncmp(source + offset, L"false", 5))
				return NULL;
			offset += 5;
			return new BooleanValue(false);
		}
		case 'n':
		{
			if (length - offset < 4 || wcsncmp(source + offset, L"null", 4))
				return NULL;
			offset += 4;
			return new NullValue();
		}
		case '"':
		{
			++offset;
			std::wstring string_value;
			while (true)
			{
				if (offset >= length)
					return NULL;
				if (source[offset] == '"')
				{
					++offset;
					break;
				}
				if (source[offset] != '\\')
				{
					string_value.append(1, source[offset]);
					++offset;
				}
				else
				{
					++offset;
					if (offset >= length)
						return NULL;
					switch (source[offset])
					{
						case 't':
						{
							string_value.append(1, '\t');
							break;
						}
						case 'r':
						{
							string_value.append(1, '\r');
							break;
						}
						case 'n':
						{
							string_value.append(1, '\n');
							break;
						}
						case '\\':
						{
							string_value.append(1, '\\');
							break;
						}
						case '"':
						{
							string_value.append(1, '"');
							break;
						}
						case '/':
						{
							string_value.append(1, '/');
							break;
						}
						case 'b':
						{
							string_value.append(1, '\b');
							break;
						}
						case 'f':
						{
							string_value.append(1, '\f');
							break;
						}
						case 'u':
						{
							if (offset + 5 >= length ||
								!iswxdigit(source[offset+1]) ||
								!iswxdigit(source[offset+2]) ||
								!iswxdigit(source[offset+3]) ||
								!iswxdigit(source[offset+4]))
									return NULL;
							wchar_t char_value;
							if (!wscanf(L"%4x", source + offset + 1, &char_value) != 1)
								return NULL;
							string_value.append(1, char_value);
							offset += 4;
						}
						default:
						{
							return NULL;
						}
					}
					++offset;
				}
			}
			return new StringValue(string_value);
		}
		case '[':
		{
			scoped_ptr<ListValue> list_value(new ListValue());
			++offset;
			wchar_t next_char = PeekNextCharacter(source, length, offset);
			if (next_char == ']')
			{
				++offset;
				return list_value.release();
			}
			do
			{
				Value* value = ValueFromString(source, length, offset);
				if (!value)
					return NULL;
				list_value->Append(value);
				next_char = EatNextCharacter(source, length, offset);
				if (next_char == ']')
					return list_value.release();
			} while (next_char == ',');
			return NULL;
		}
		case '{':
		{
			scoped_ptr<DictionaryValue> dict_value(new DictionaryValue());
			++offset;
			wchar_t next_char = PeekNextCharacter(source, length, offset);
			if (next_char == '}')
			{
				++offset;
				return dict_value.release();
			}
			do
			{
				scoped_ptr<Value> key_value(ValueFromString(source, length, offset));
				std::wstring key_string;
				if (!key_value.get() || !key_value->AsString(&key_string))
					return NULL;
				if (EatNextCharacter(source, length, offset) != L':')
					return NULL;

				Value* value;
				//if (dict_value->Get(key_string, &value))
				//  return NULL;
				value = ValueFromString(source, length, offset);
				if (!value)
					return NULL;
				dict_value->Set(key_string, value);
				next_char = EatNextCharacter(source, length, offset);
				if (next_char == '}')
					return dict_value.release();
			} while (next_char == ',');
			return NULL;
		}
		default:
		{
			// Try to read as int or double.  If both work, take whichever reads more characters,
			// preferring integer if they parse the same number.
			// TODO:  Might want to not read as double based on some heuristic, if integer works.
			wchar_t* int_end;
			int int_value = wcstol(source + offset, &int_end, 10);
			wchar_t* double_end;
			double double_value = wcstod(source + offset, &double_end);
			if (int_end > source + offset && int_end >= double_end)
			{
				offset = int_end - source;
				return new IntegerValue(int_value);
			}
			else if (double_end > int_end)
			{
				offset = double_end - source;
				return new DoubleValue(double_value);
			}
			return NULL;
		}
	}
}

}  // namespace

std::wstring Value::ToString(bool pretty_print)
{
	std::wstringstream stream;
	ValueToString(this, pretty_print, 0, stream);
	return stream.str();
}

//static
Value* Value::FromString(const std::wstring& source)
{
	size_t offset = 0;
	size_t length = source.length();
	scoped_ptr<Value> value(ValueFromString(source.c_str(), length, offset));
	EatWhitespace(source.c_str(), length, offset);
	if (offset == length)
		return value.release();
	return NULL;
}

Value* Value::FromFile(const std::wstring& file_name)
{
	std::wstring file_data;
	if (!LoadFile(file_name, &file_data))
		return NULL;
	return FromString(file_data);
}

bool Value::AsString(std::string* value) const
{
	std::wstring wstring;
	if (!AsString(&wstring))
		return false;
	*value = ToMultiByteString(wstring);
	return true;
}

NullValue::NullValue() : Value(VALUE_NULL)
{
}

BooleanValue::BooleanValue(bool value) :
	Value(VALUE_BOOLEAN),
	value_(value)
{
}

bool BooleanValue::AsBoolean(bool* value) const
{
	*value = value_;
	return true;
}

bool BooleanValue::AsInteger(int* value) const
{
	*value = value_;
	return true;
}

IntegerValue::IntegerValue(int value) :
	Value(VALUE_INTEGER),
	value_(value)
{
}

bool IntegerValue::AsBoolean(bool* value) const
{
	*value = (value_ != 0);
	return true;
}

bool IntegerValue::AsInteger(int* value) const
{
	*value = value_;
	return true;
}

bool IntegerValue::AsDouble(double* value) const
{
	*value = value_;
	return true;
}

DoubleValue::DoubleValue(double value) :
	Value(VALUE_DOUBLE),
	value_(value)
{
}

bool DoubleValue::AsDouble(double* value) const
{
	*value = value_;
	return true;
}

StringValue::StringValue(const std::wstring& value) :
	Value(VALUE_STRING),
	value_(value)
{
}

StringValue::StringValue(const std::string& value) :
	Value(VALUE_STRING),
	value_(ToWstring(value))
{
}

bool StringValue::AsString(std::wstring* value) const
{
	*value = value_;
	return true;
}

ListValue::ListValue() :
	Value(VALUE_LIST)
{
}

ListValue::~ListValue()
{
	for (std::vector<Value*>::iterator it = value_.begin(); it != value_.end(); ++it)
		delete (*it);
}

bool ListValue::AsList(ListValue** value)
{
	*value = this;
	return true;
}

void ListValue::Set(size_t offset, Value* value)
{
	if (offset >= value_.size())
	{
		while (offset > value_.size())
			Append(new NullValue());
		Append(value);
	}
	else
	{
		delete value_[offset];
		value_[offset] = value;
	}
}

void ListValue::Append(Value* value)
{
	value_.push_back(value);
}

bool ListValue::Get(size_t offset, Value** value) const
{
	if (offset >= value_.size())
		return false;
	*value = value_[offset];
	return true;
}

bool ListValue::GetBooleanAt(size_t offset, bool* value) const
{
	Value *temp_value;
	if (!Get(offset, &temp_value))
		return false;
	return temp_value->AsBoolean(value);
}

bool ListValue::GetIntegerAt(size_t offset, int* value) const
{
	Value *temp_value;
	if (!Get(offset, &temp_value))
		return false;
	return temp_value->AsInteger(value);
}

bool ListValue::GetDoubleAt(size_t offset, double* value) const
{
	Value *temp_value;
	if (!Get(offset, &temp_value))
		return false;
	return temp_value->AsDouble(value);
}

bool ListValue::GetStringAt(size_t offset, std::wstring* value) const
{
	Value *temp_value;
	if (!Get(offset, &temp_value))
		return false;
	return temp_value->AsString(value);
}

bool ListValue::GetStringAt(size_t offset, std::string* value) const
{
	Value *temp_value;
	if (!Get(offset, &temp_value))
		return false;
	return temp_value->AsString(value);
}

bool ListValue::GetListAt(size_t offset, ListValue** value) const
{
	Value *temp_value;
	if (!Get(offset, &temp_value))
		return false;
	return temp_value->AsList(value);
}

bool ListValue::GetDictionaryAt(size_t offset, DictionaryValue** value) const
{
	Value *temp_value;
	if (!Get(offset, &temp_value))
		return false;
	return temp_value->AsDictionary(value);
}

size_t ListValue::Size() const
{
	return value_.size();
}

DictionaryValue::DictionaryValue() :
	Value(VALUE_DICTIONARY)
{
}

DictionaryValue::~DictionaryValue()
{
	for (ValueMap::iterator it = value_.begin(); it != value_.end(); ++it)
		delete (it->second);
}


bool DictionaryValue::AsDictionary(DictionaryValue** value)
{
	*value = this;
	return true;
}

void DictionaryValue::Set(const std::wstring& key, Value* value)
{
	ValueMap::iterator it = value_.find(key);
	if (it != value_.end())
		delete it->second;
	value_[key] = value;
}

bool DictionaryValue::Get(const std::wstring& key, Value** value) const
{
	ValueMap::const_iterator it = value_.find(key);
	if (it == value_.end())
		return false;
	*value = it->second;
	return true;
}

void DictionaryValue::SetBooleanAt(const std::wstring& key, bool value)
{
	Set(key, new BooleanValue(value));;
}

void DictionaryValue::SetIntegerAt(const std::wstring& key, int value)
{
	Set(key, new IntegerValue(value));;
}

void DictionaryValue::SetDoubleAt(const std::wstring& key, double value)
{
	Set(key, new DoubleValue(value));;
}

void DictionaryValue::SetStringAt(const std::wstring& key, const std::wstring& value)
{
	Set(key, new StringValue(value));;
}

bool DictionaryValue::GetBooleanAt(const std::wstring& key, bool* value) const
{
	Value *temp_value;
	if (!Get(key, &temp_value))
		return false;
	return temp_value->AsBoolean(value);
}

bool DictionaryValue::GetIntegerAt(const std::wstring& key, int* value) const
{
	Value *temp_value;
	if (!Get(key, &temp_value))
		return false;
	return temp_value->AsInteger(value);
}

bool DictionaryValue::GetDoubleAt(const std::wstring& key, double* value) const
{
	Value *temp_value;
	if (!Get(key, &temp_value))
		return false;
	return temp_value->AsDouble(value);
}

bool DictionaryValue::GetStringAt(const std::wstring& key, std::wstring* value) const
{
	Value *temp_value;
	if (!Get(key, &temp_value))
		return false;
	return temp_value->AsString(value);
}

bool DictionaryValue::GetStringAt(const std::wstring& key, std::string* value) const
{
	Value *temp_value;
	if (!Get(key, &temp_value))
		return false;
	return temp_value->AsString(value);
}

bool DictionaryValue::GetListAt(const std::wstring& key, ListValue** value) const
{
	Value *temp_value;
	if (!Get(key, &temp_value))
		return false;
	return temp_value->AsList(value);
}

bool DictionaryValue::GetDictionaryAt(const std::wstring& key, DictionaryValue** value) const
{
	Value *temp_value;
	if (!Get(key, &temp_value))
		return false;
	return temp_value->AsDictionary(value);
}

DictionaryValue::const_iterator DictionaryValue::begin()
{
	return value_.begin();
}

DictionaryValue::const_iterator DictionaryValue::end()
{
	return value_.end();
}
