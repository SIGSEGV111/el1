#pragma once

#include "error.hpp"
#include "io_types.hpp"
#include "io_collection_list.hpp"
#include "io_text_string.hpp"
#include <type_traits>

namespace el1::io::collection::map
{
	using namespace error;
	using namespace io::types;
	using namespace io::collection::list;

	// WARNING: TKey and TValue must be trivially moveable!

	enum class EInputOrder : u8_t
	{
		UNSORTED,
		ASSUME_SORTED
	};

	template<typename TKey>
	struct TDefaultSorter
	{
		int operator()(const TKey& a, const TKey& b) const EL_GETTER
		{
			return StdSorter(a, b);
		}
	};

	template<typename TKey, typename TValue, auto SORTER = TDefaultSorter<TKey>{}>
	class TSortedMap;

	template<typename TKey, typename TValue>
	class THashMap;

	struct TGenericKeyNotFoundException : IException
	{
		io::text::string::TString Message() const override;
	};

	template<typename TKey>
	struct TKeyNotFoundException : TGenericKeyNotFoundException
	{
		const TKey key;

		io::text::string::TString Message() const final override
		{
			return TString::Format(U"the requested key %q was not found", strigify_t<TKey>::ToString(key, U"<non-text value>"));
		}

		IException* Clone() const override { return new TKeyNotFoundException(*this); }

		TKeyNotFoundException(TKey key) : key(key) {}
	};

	struct TGenericKeyAlreadyExistsException : IException
	{
		io::text::string::TString Message() const final override;
	};

	template<typename TKey>
	struct TKeyAlreadyExistsException : TGenericKeyAlreadyExistsException
	{
		const TKey key;

		IException* Clone() const override { return new TKeyAlreadyExistsException(*this); }

		TKeyAlreadyExistsException(TKey key) : key(key) {}
	};

	/*****************************************************************************/

	template<typename TKey, typename TValue, auto SORTER>
	class TSortedMap<TKey, const TValue, SORTER>
	{
		public:
			using kv_pair_t = kv_pair_tt<TKey, TValue>;
		protected:
			TList<kv_pair_t> items;

		public:
			void Clear() { items.Clear(); }

			const TList<kv_pair_t>& Items() const { return items; }
			array_t<kv_pair_t>& Items() { return items; }
			static constexpr auto Sorter() noexcept { return SORTER; }

			// retrieves the value associated with a key; throws if the key does not exist
			const TValue& operator[](const TKey& key) const;

			// receives the value associated with the specified key; return nullptr if the key does not exist
			template<typename TLookupKey>
			const TValue* Get(const TLookupKey& key) const EL_GETTER;

			// receives the value associated with the specified key; otherwise a default-value is returned, which is NOT inserted
			const TValue& GetWithDefault(const TKey& key, const TValue& _default) const EL_GETTER;

			template<typename TLookupKey>
			bool Contains(const TLookupKey& key) const EL_GETTER;

			// adds a new key/value pair to the map; if the key already exists it will throw an exception
			TValue& Add(TKey key, const TValue& value);
			TValue& Add(TKey key, TValue&& value);
			TValue& Add(kv_pair_t&& pair);

			// adds a default value if the key does not exist yet, otherwise the existing value is returned
			const TValue& GetOrInsertDefault(const TKey& key, const TValue& _default);

			// if key exists in the map the value associated value is returned, otherwise the _default value is returned
			const TValue& GetWithDefault(const TKey& key, const TValue& _default);

			TSortedMap& operator=(TSortedMap&& other) = default;
			TSortedMap& operator=(TSortedMap& other) = default;

			TSortedMap(TSortedMap&& other) = default;
			TSortedMap(const TSortedMap& other) = default;
			TSortedMap() = default;
			TSortedMap(TList<kv_pair_t>&& items, EInputOrder input_order = EInputOrder::UNSORTED);
			TSortedMap(const TList<kv_pair_t>& items);
			TSortedMap(std::initializer_list<kv_pair_t> list);
	};

	template<typename TKey, typename TValue, auto SORTER>
	class TSortedMap : public TSortedMap<TKey, const TValue, SORTER>
	{
		public:
			using typename TSortedMap<TKey, const TValue, SORTER>::kv_pair_t;

			using TSortedMap<TKey, const TValue, SORTER>::Items;

			// retrieves the value associated with a key; throws if the key does not exist
			TValue& operator[](const TKey& key) const;

			// retrieves the value associated with the specified key; return nullptr if the key does not exist
			template<typename TLookupKey>
			TValue* Get(const TLookupKey& key) const EL_GETTER;

			// updates the value asociated with a key; calls Add() if the key does not exist yet
			TValue& Set(const TKey& key, const TValue& value);
			TValue& Set(const TKey& key, TValue&& value);
			TValue& Set(kv_pair_t&& pair);

			// removes the specified key (along with its value) from the map; return false if the key did not exist; true otherwise
			template<typename TLookupKey>
			bool Remove(const TLookupKey& key);

			// adds a default value if the key does not exist yet, otherwise the existing value is returned
			TValue& GetOrInsertDefault(const TKey& key, const TValue& _default);

			TSortedMap& operator=(TSortedMap&& other) = default;
			TSortedMap& operator=(TSortedMap& other) = default;

			TSortedMap(TSortedMap&& other) = default;
			TSortedMap(const TSortedMap& other) = default;
			TSortedMap(const TSortedMap<TKey, const TValue, SORTER>& other);
			TSortedMap() = default;
			TSortedMap(TList<kv_pair_t>&& items, EInputOrder input_order = EInputOrder::UNSORTED);
			TSortedMap(const TList<kv_pair_t>& items);
			TSortedMap(std::initializer_list<kv_pair_t> list);
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

	template<typename TKey, typename TValue, auto SORTER>
	TSortedMap<TKey, const TValue, SORTER>::TSortedMap(TList<kv_pair_t>&& items, const EInputOrder input_order) : items(std::move(items))
	{
		if(input_order == EInputOrder::UNSORTED)
			this->items.Sort(ESortOrder::ASCENDING, [](const kv_pair_t& a, const kv_pair_t& b) { return SORTER(a.key, b.key); });

		for(usys_t i = 1; i < this->items.Count(); i++)
			EL_ERROR(SORTER(this->items[i - 1].key, this->items[i].key) == 0, TKeyAlreadyExistsException<TKey>, this->items[i].key);
	}

	template<typename TKey, typename TValue, auto SORTER>
	TSortedMap<TKey, const TValue, SORTER>::TSortedMap(const TList<kv_pair_t>& items) : TSortedMap(TList<kv_pair_t>(items), EInputOrder::UNSORTED)
	{
	}


	template<typename TKey, typename TValue, auto SORTER>
	TSortedMap<TKey, const TValue, SORTER>::TSortedMap(std::initializer_list<kv_pair_t> list) : TSortedMap(TList<kv_pair_t>(list), EInputOrder::UNSORTED)
	{
	}

	template<typename TKey, typename TValue, auto SORTER>
	const TValue& TSortedMap<TKey, const TValue, SORTER>::operator[](const TKey& key) const
	{
		const TValue* const value = this->Get(key);
		EL_ERROR(value == nullptr, TKeyNotFoundException<TKey>, key);
		return *value;
	}

	template<typename TKey, typename TValue, auto SORTER>
	template<typename TLookupKey>
	const TValue* TSortedMap<TKey, const TValue, SORTER>::Get(const TLookupKey& key) const
	{
		if constexpr(std::is_same_v<decltype(SORTER), TDefaultSorter<TKey>> && !std::is_same_v<std::remove_cvref_t<TLookupKey>, TKey>)
		{
			const TKey normalized_key(key);
			return Get(normalized_key);
		}
		else
		{
			const usys_t index = this->items.BinarySearch([&](const kv_pair_t& item) {
				return SORTER(item.key, key);
			}, false);

			if(index == NEG1)
				return nullptr;
			else
				return &this->items[index].value;
		}
	}

	template<typename TKey, typename TValue, auto SORTER>
	const TValue& TSortedMap<TKey, const TValue, SORTER>::GetWithDefault(const TKey& key, const TValue& _default) const
	{
		const TValue* const value = Get(key);
		return value == nullptr ? _default : *value;
	}

	template<typename TKey, typename TValue, auto SORTER>
	template<typename TLookupKey>
	bool TSortedMap<TKey, const TValue, SORTER>::Contains(const TLookupKey& key) const
	{
		return this->Get(key) != nullptr;
	}

	template<typename TKey, typename TValue, auto SORTER>
	TValue& TSortedMap<TKey, const TValue, SORTER>::Add(TKey key, const TValue& value)
	{
		const usys_t index = this->items.BinarySearch([&](const kv_pair_t& item) {
			return SORTER(item.key, key);
		}, true);

		EL_ERROR(index != NEG1 && SORTER(this->items[index].key, key) == 0, TKeyAlreadyExistsException<TKey>, key);

		if(index == NEG1)
		{
			return this->items.Append({ key, value }).value;
		}
		else if(SORTER(this->items[index].key, key) > 0)
		{
			return this->items.Insert(index, { key, value }).value;
		}
		else
		{
			return this->items.Insert(index + 1, { key, value }).value;
		}
	}

	template<typename TKey, typename TValue, auto SORTER>
	TValue& TSortedMap<TKey, const TValue, SORTER>::Add(TKey key, TValue&& value)
	{
		const usys_t index = this->items.BinarySearch([&](const kv_pair_t& item) {
			return SORTER(item.key, key);
		}, true);

		EL_ERROR(index != NEG1 && SORTER(this->items[index].key, key) == 0, TKeyAlreadyExistsException<TKey>, key);

		if(index == NEG1)
		{
			return this->items.MoveAppend({ key, std::move(value) }).value;
		}
		else if(SORTER(this->items[index].key, key) > 0)
		{
			return this->items.MoveInsert(index, { key, std::move(value) }).value;
		}
		else
		{
			return this->items.MoveInsert(index + 1, { key, std::move(value) }).value;
		}
	}

	template<typename TKey, typename TValue, auto SORTER>
	const TValue& TSortedMap<TKey, const TValue, SORTER>::GetOrInsertDefault(const TKey& key, const TValue& _default)
	{
		const TValue* value = this->Get(key);
		if(value == nullptr)
			value = &this->Add(key, _default);
		return *value;
	}

	template<typename TKey, typename TValue, auto SORTER>
	const TValue& TSortedMap<TKey, const TValue, SORTER>::GetWithDefault(const TKey& key, const TValue& _default)
	{
		const TValue* value = this->Get(key);
		return value == nullptr ? _default : *value;
	}

	/*****************************************************************************/

	template<typename TKey, typename TValue, auto SORTER>
	TSortedMap<TKey, TValue, SORTER>::TSortedMap(const TSortedMap<TKey, const TValue, SORTER>& other) : TSortedMap<TKey, const TValue, SORTER>(TList<kv_pair_t>(other.Items()), EInputOrder::ASSUME_SORTED)
	{
	}


	template<typename TKey, typename TValue, auto SORTER>
	TSortedMap<TKey, TValue, SORTER>::TSortedMap(TList<kv_pair_t>&& items, const EInputOrder input_order) : TSortedMap<TKey, const TValue, SORTER>(std::move(items), input_order)
	{
	}

	template<typename TKey, typename TValue, auto SORTER>
	TSortedMap<TKey, TValue, SORTER>::TSortedMap(const TList<kv_pair_t>& items) : TSortedMap<TKey, const TValue, SORTER>(items)
	{
	}

	template<typename TKey, typename TValue, auto SORTER>
	TSortedMap<TKey, TValue, SORTER>::TSortedMap(std::initializer_list<kv_pair_t> list) : TSortedMap<TKey, const TValue, SORTER>(list)
	{
	}

	template<typename TKey, typename TValue, auto SORTER>
	TValue& TSortedMap<TKey, TValue, SORTER>::operator[](const TKey& key) const
	{
		return const_cast<TValue&>(static_cast<const TSortedMap<TKey, const TValue, SORTER>*>(this)->operator[](key));
	}

	template<typename TKey, typename TValue, auto SORTER>
	template<typename TLookupKey>
	TValue* TSortedMap<TKey, TValue, SORTER>::Get(const TLookupKey& key) const
	{
		return const_cast<TValue*>(static_cast<const TSortedMap<TKey, const TValue, SORTER>*>(this)->Get(key));
	}

	template<typename TKey, typename TValue, auto SORTER>
	TValue& TSortedMap<TKey, TValue, SORTER>::Set(const TKey& key, const TValue& value)
	{
		const usys_t index = this->items.BinarySearch([&](const kv_pair_t& item) {
			return SORTER(item.key, key);
		}, true);

		if(index == NEG1)
		{
			return this->items.Append({ key, value }).value;
		}
		else if(SORTER(this->items[index].key, key) == 0)
		{
			return this->items[index].value = value;
		}
		else if(SORTER(this->items[index].key, key) > 0)
		{
			return this->items.Insert(index, { key, value }).value;
		}
		else
		{
			return this->items.Insert(index + 1, { key, value }).value;
		}
	}

	template<typename TKey, typename TValue, auto SORTER>
	TValue& TSortedMap<TKey, TValue, SORTER>::Set(const TKey& key, TValue&& value)
	{
		const usys_t index = this->items.BinarySearch([&](const kv_pair_t& item) {
			return SORTER(item.key, key);
		}, true);

		if(index == NEG1)
		{
			return this->items.MoveAppend({ key, std::move(value) }).value;
		}
		else if(SORTER(this->items[index].key, key) == 0)
		{
			return this->items[index].value = std::move(value);
		}
		else if(SORTER(this->items[index].key, key) > 0)
		{
			return this->items.MoveInsert(index, { key, std::move(value) }).value;
		}
		else
		{
			return this->items.MoveInsert(index + 1, { key, std::move(value) }).value;
		}
	}

	template<typename TKey, typename TValue, auto SORTER>
	template<typename TLookupKey>
	bool TSortedMap<TKey, TValue, SORTER>::Remove(const TLookupKey& key)
	{
		if constexpr(std::is_same_v<decltype(SORTER), TDefaultSorter<TKey>> && !std::is_same_v<std::remove_cvref_t<TLookupKey>, TKey>)
		{
			const TKey normalized_key(key);
			return Remove(normalized_key);
		}
		else
		{
			const usys_t index = this->items.BinarySearch([&](const kv_pair_t& item) {
				return SORTER(item.key, key);
			}, false);

			if(index == NEG1)
				return false;

			this->items.Remove(index, 1);
			return true;
		}
	}

	template<typename TKey, typename TValue, auto SORTER>
	TValue& TSortedMap<TKey, TValue, SORTER>::GetOrInsertDefault(const TKey& key, const TValue& _default)
	{
		TValue* value = this->Get(key);
		if(value == nullptr)
			value = &this->Add(key, _default);
		return *value;
	}
}
