#pragma once

class DictionaryValue;
class ListValue;

enum ValueType
{
	VALUE_NULL,
	VALUE_BOOLEAN,
	VALUE_INTEGER,
	VALUE_DOUBLE,
	VALUE_STRING,
	VALUE_LIST,
	VALUE_DICTIONARY,
};

// Simple class to represent JSON strings.  Used to simplify saving/loading settings.
// Only lists and dictionaries are mutable.
class Value
{
public:
	// Reversible conversion.
	std::wstring ToString(bool pretty_print);
	static Value* FromString(const std::wstring& source);
	static Value* FromFile(const std::wstring& file_name);

	inline virtual bool AsBoolean(bool* value) const { return false; }
	inline virtual bool AsInteger(int* value) const { return false; }
	inline virtual bool AsDouble(double* value) const { return false; }
	inline virtual bool AsString(std::wstring* value) const { return false; }
	bool AsString(std::string* value) const;
	inline virtual bool AsList(ListValue** value) { return false; }
	inline virtual bool AsDictionary(DictionaryValue** value) { return false; }

	inline virtual ~Value() {}

	inline ValueType type() const { return type_; }

protected:
	inline Value(ValueType type) : type_(type) {}

	ValueType type_;
};

class NullValue : public Value
{
public:
	NullValue();
};

class BooleanValue : public Value
{
public:
	BooleanValue(bool value);
	virtual bool AsBoolean(bool* value) const;
	// Returns 0/1, for convenience.
	virtual bool AsInteger(int* value) const;

private:
	const bool value_;
};

class IntegerValue : public Value
{
public:
	IntegerValue(int value);
	virtual bool AsBoolean(bool* value) const;
	virtual bool AsInteger(int* value) const;
	virtual bool AsDouble(double* value) const;

private:
	const int value_;
};

class DoubleValue : public Value
{
public:
	DoubleValue(double value);
	virtual bool AsDouble(double* value) const;

private:
	const double value_;
};

class StringValue : public Value
{
public:
	StringValue(const std::wstring& value);
	StringValue(const std::string& value);
	virtual bool AsString(std::wstring* value) const;

private:
	const std::wstring value_;
};

class ListValue : public Value
{
public:
	ListValue();
	~ListValue();

	virtual bool AsList(ListValue** value);

	// |this| takes ownership of |value|.
	void Set(size_t offset, Value* value);
	void Append(Value* value);

	// |this| maintains ownership of |value|.
	bool Get(size_t offset, Value** value) const;

	bool GetBooleanAt(size_t offset, bool* value) const;
	bool GetIntegerAt(size_t offset, int* value) const;
	bool GetDoubleAt(size_t offset, double* value) const;
	bool GetStringAt(size_t offset, std::wstring* value) const;
	bool GetStringAt(size_t offset, std::string* value) const;
	bool GetListAt(size_t offset, ListValue** value) const;
	bool GetDictionaryAt(size_t offset, DictionaryValue** value) const;

	size_t Size() const;

private:
	std::vector<Value*> value_;
};

class DictionaryValue : public Value
{
public:
	typedef std::map<std::wstring, Value*> ValueMap;
	typedef ValueMap::const_iterator const_iterator;

	DictionaryValue();
	~DictionaryValue();

	virtual bool AsDictionary(DictionaryValue** value);

	// |this| takes ownership of |value|.
	void Set(const std::wstring& key, Value* value);

	// |this| maintains ownership of |value|.
	bool Get(const std::wstring& key, Value** value) const;


	void SetBooleanAt(const std::wstring& key, bool value);
	void SetIntegerAt(const std::wstring& key, int value);
	void SetDoubleAt(const std::wstring& key, double value);
	void SetStringAt(const std::wstring& key, const std::wstring& value);

	bool GetBooleanAt(const std::wstring& key, bool* value) const;
	bool GetIntegerAt(const std::wstring& key, int* value) const;
	bool GetDoubleAt(const std::wstring& key, double* value) const;
	bool GetStringAt(const std::wstring& key, std::wstring* value) const;
	bool GetStringAt(const std::wstring& key, std::string* value) const;
	bool GetListAt(const std::wstring& key, ListValue** value) const;
	bool GetDictionaryAt(const std::wstring& key, DictionaryValue** value) const;

	const_iterator begin();
	const_iterator end();

private:
	ValueMap value_;
};
