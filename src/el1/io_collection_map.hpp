#pragma once

#include "error.hpp"
#include "io_types.hpp"
#include "io_collection_list.hpp"

namespace el1::io::collection::map
{
	using namespace error;
	using namespace io::types;
	using namespace io::collection::list;

	// WARNING: TKey and TValue must be trivially moveable!

	template<typename TKey, typename TValue>
	class TSortedMap;

	template<typename TKey, typename TValue>
	class THashMap;

	struct TGenericKeyNotFoundException : IException
	{
		TString Message() const final override;
	};

	template<typename TKey>
	struct TKeyNotFoundException : TGenericKeyNotFoundException
	{
		const TKey key;

		IException* Clone() const override { return new TKeyNotFoundException(*this); }

		TKeyNotFoundException(TKey key) : key(key) {}
	};

	struct TGenericKeyAlreadyExistsException : IException
	{
		TString Message() const final override;
	};

	template<typename TKey>
	struct TKeyAlreadyExistsException : TGenericKeyAlreadyExistsException
	{
		const TKey key;

		IException* Clone() const override { return new TKeyAlreadyExistsException(*this); }

		TKeyAlreadyExistsException(TKey key) : key(key) {}
	};

	/*****************************************************************************/

	template<typename TKey, typename TValue>
	class TSortedMap<TKey, const TValue>
	{
		public:
			using kv_pair_t = kv_pair_tt<TKey, TValue>;
			typedef int (*sorter_function_t)(const TKey&, const TKey&);

		protected:
			TList<kv_pair_t> items;
			sorter_function_t sorter;

		public:
			const TList<kv_pair_t>& Items() const { return items; }
			sorter_function_t Sorter() const { return sorter; }

			// retreives the value associated with a key; throws if the key does not exist
			const TValue& operator[](const TKey& key) const;

			// receives the value associated with the specified key; return nullptr if the key does not exist
			const TValue* Get(const TKey& key) const EL_GETTER;

			bool Contains(const TKey& key) const EL_GETTER;

			// adds a new key/value pair to the map; if the key already exists it will throw an exception
			TValue& Add(TKey key, const TValue& value);
			TValue& Add(TKey key, TValue&& value);
			TValue& Add(kv_pair_t&& pair);

			TSortedMap(TSortedMap&& other) = default;
			TSortedMap(const TSortedMap& other) = default;
			TSortedMap(sorter_function_t sorter = &StdSorter<TKey>);
			TSortedMap(const TList< kv_pair_t>& items, sorter_function_t sorter = &StdSorter<TKey>);
	};

	template<typename TKey, typename TValue>
	class TSortedMap : public TSortedMap<TKey, const TValue>
	{
		public:
			using typename TSortedMap<TKey, const TValue>::kv_pair_t;
			using typename TSortedMap<TKey, const TValue>::sorter_function_t;

			using TSortedMap<TKey, const TValue>::Items;
			array_t<kv_pair_t> Items() { return this->items; }

			// retreives the value associated with a key; throws if the key does not exist
			TValue& operator[](const TKey& key) const;

			// retreivs the value associated with the specified key; return nullptr if the key does not exist
			TValue* Get(const TKey& key) const EL_GETTER;

			// updates the value asociated with a key; calls Add() if the key does not exist yet
			TValue& Set(const TKey& key, const TValue& value);
			TValue& Set(const TKey& key, TValue&& value);
			TValue& Set(kv_pair_t&& pair);

			// adds a _default value if the key does not exist yet, otherwise the existing value is returned
			TValue& Get(const TKey& key, const TValue& _default);

			// removes the specified key (along with its value) from the map; return false if the key did not exist; true otherwise
			bool Remove(const TKey& key);

			TSortedMap(TSortedMap&& other) = default;
			TSortedMap(const TSortedMap& other) = default;
			TSortedMap(const TSortedMap<TKey, const TValue>& other);
			TSortedMap(sorter_function_t sorter = &StdSorter<TKey>);
			TSortedMap(const TList< kv_pair_t>& items, sorter_function_t sorter = &StdSorter<TKey>);
	};

	/*****************************************************************************/

	template<typename TKey, typename TValue>
	class THashMap<TKey, const TValue>
	{
		public:
			using kv_pair_t = kv_pair_tt<TKey, TValue>;

		protected:
			TList<kv_pair_t> items;
// 			TView<kv_pair_t> view;	// TODO
			usys_t (*hasher)(TKey);

		public:
			const TValue& operator[](TKey key) const;

			// receives the value associated with the specified key; return nullptr if the key does not exist
			const TValue* Get(TKey key) const EL_GETTER;

			// adds a new key/value pair to the map; if the key already exists it will throw an exception
			TValue& Add(TKey key, const TValue& value);
	};

	template<typename TKey, typename TValue>
	class THashMap : public TSortedMap<TKey, const TValue>
	{
		public:
			TValue& operator[](TKey key) const;

			// retreivs the value associated with the specified key; return nullptr if the key does not exist
			TValue* Get(TKey key) const EL_GETTER;

			// updates the value asociated with a key; calls Add() if the key does not exist yet
			TValue& Set(TKey key, const TValue& value);

			// removes the specified key (along with its value) from the map; return false if the key did not exist; true otherwise
			bool Remove(TKey key);
	};

	/*****************************************************************************/

	template<typename TKey, typename TValue>
	TSortedMap<TKey, const TValue>::TSortedMap(const TList< kv_pair_t>& items, sorter_function_t sorter) : items(items), sorter(sorter)
	{
	}

	template<typename TKey, typename TValue>
	TSortedMap<TKey, const TValue>::TSortedMap(sorter_function_t sorter) : sorter(sorter)
	{
	}

	template<typename TKey, typename TValue>
	const TValue& TSortedMap<TKey, const TValue>::operator[](const TKey& key) const
	{
		const TValue* const value = this->Get(key);
		EL_ERROR(value == nullptr, TKeyNotFoundException<TKey>, key);
		return *value;
	}

	template<typename TKey, typename TValue>
	const TValue* TSortedMap<TKey, const TValue>::Get(const TKey& key) const
	{
		const usys_t index = this->items.BinarySearch([&](const kv_pair_t& item) {
			return this->sorter(item.key, key);
		}, false);

		if(index == NEG1)
			return nullptr;
		else
			return &this->items[index].value;
	}

	template<typename TKey, typename TValue>
	bool TSortedMap<TKey, const TValue>::Contains(const TKey& key) const
	{
		return this->Get(key) != nullptr;
	}

	template<typename TKey, typename TValue>
	TValue& TSortedMap<TKey, const TValue>::Add(TKey key, const TValue& value)
	{
		const usys_t index = this->items.BinarySearch([&](const kv_pair_t& item) {
			return this->sorter(item.key, key);
		}, true);

		EL_ERROR(index != NEG1 && this->items[index].key == key, TKeyAlreadyExistsException<TKey>, key);

		if(index == NEG1)
		{
			return this->items.Append({ key, value }).value;
		}
		else if(this->sorter(this->items[index].key, key) > 0)
		{
			return this->items.Insert(index, { key, value }).value;
		}
		else
		{
			return this->items.Insert(index + 1, { key, value }).value;
		}
	}

	template<typename TKey, typename TValue>
	TValue& TSortedMap<TKey, const TValue>::Add(TKey key, TValue&& value)
	{
		const usys_t index = this->items.BinarySearch([&](const kv_pair_t& item) {
			return this->sorter(item.key, key);
		}, true);

		EL_ERROR(index != NEG1 && this->items[index].key == key, TKeyAlreadyExistsException<TKey>, key);

		if(index == NEG1)
		{
			return this->items.MoveAppend({ key, std::move(value) }).value;
		}
		else if(this->sorter(this->items[index].key, key) > 0)
		{
			return this->items.MoveInsert(index, { key, std::move(value) }).value;
		}
		else
		{
			return this->items.MoveInsert(index + 1, { key, std::move(value) }).value;
		}
	}

	/*****************************************************************************/

	template<typename TKey, typename TValue>
	TSortedMap<TKey, TValue>::TSortedMap(const TSortedMap<TKey, const TValue>& other) : TSortedMap<TKey, const TValue>(other.Items(), other.Sorter())
	{
	}

	template<typename TKey, typename TValue>
	TSortedMap<TKey, TValue>::TSortedMap(sorter_function_t sorter) : TSortedMap<TKey, const TValue>(sorter)
	{
	}

	template<typename TKey, typename TValue>
	TSortedMap<TKey, TValue>::TSortedMap(const TList< kv_pair_t>& items, sorter_function_t sorter) : TSortedMap<TKey, const TValue>(items, sorter)
	{
	}

	template<typename TKey, typename TValue>
	TValue& TSortedMap<TKey, TValue>::operator[](const TKey& key) const
	{
		return const_cast<TValue&>(static_cast<const TSortedMap<TKey, const TValue>*>(this)->operator[](key));
	}

	template<typename TKey, typename TValue>
	TValue* TSortedMap<TKey, TValue>::Get(const TKey& key) const
	{
		return const_cast<TValue*>(static_cast<const TSortedMap<TKey, const TValue>*>(this)->Get(key));
	}

	template<typename TKey, typename TValue>
	TValue& TSortedMap<TKey, TValue>::Set(const TKey& key, const TValue& value)
	{
		const usys_t index = this->items.BinarySearch([&](const kv_pair_t& item) {
			return this->sorter(item.key, key);
		}, true);

		if(index == NEG1)
		{
			return this->items.Append({ key, value }).value;
		}
		else if(this->sorter(this->items[index].key, key) == 0)
		{
			return this->items[index].value = value;
		}
		else if(this->sorter(this->items[index].key, key) > 0)
		{
			return this->items.Insert(index, { key, value }).value;
		}
		else
		{
			return this->items.Insert(index + 1, { key, value }).value;
		}
	}

	template<typename TKey, typename TValue>
	TValue& TSortedMap<TKey, TValue>::Set(const TKey& key, TValue&& value)
	{
		const usys_t index = this->items.BinarySearch([&](const kv_pair_t& item) {
			return this->sorter(item.key, key);
		}, true);

		if(index == NEG1)
		{
			return this->items.MoveAppend({ key, std::move(value) }).value;
		}
		else if(this->sorter(this->items[index].key, key) == 0)
		{
			return this->items[index].value = std::move(value);
		}
		else if(this->sorter(this->items[index].key, key) > 0)
		{
			return this->items.MoveInsert(index, { key, std::move(value) }).value;
		}
		else
		{
			return this->items.MoveInsert(index + 1, { key, std::move(value) }).value;
		}
	}

	template<typename TKey, typename TValue>
	TValue& TSortedMap<TKey, TValue>::Get(const TKey& key, const TValue& _default)
	{
		TValue* value = this->Get(key);
		if(value == nullptr)
			value = &this->Add(key, _default);
		return *value;
	}

	template<typename TKey, typename TValue>
	bool TSortedMap<TKey, TValue>::Remove(const TKey& key)
	{
		const usys_t index = this->items.BinarySearch([&](const kv_pair_t& item) {
			return this->sorter(item.key, key);
		}, false);

		if(index == NEG1)
			return false;

		this->items.Remove(index, 1);
		return true;
	}
}
