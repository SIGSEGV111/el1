#pragma once

#include "io_types.hpp"
#include "io_stream.hpp"
#include "error.hpp"
#include "util.hpp"
#include "system_memory.hpp"

#include <utility>
#include <new>
#include <malloc.h>
#include <string.h>

namespace el1::io::collection::list
{
	using namespace io::types;

	// WARNING: T must be trivially moveable!

	template<typename T>
	class array_t;

	template<typename T>
	class TList;

	template<typename T>
	static bool EqualsComparator(const T& a, const T& b) EL_GETTER;

	template<typename T>
	static int StdSorter(const T& a, const T& b) EL_GETTER;

	template<typename T>
	static bool EqualsComparator(const T& a, const T& b)
	{
		return a == b;
	}

	template<typename T>
	static int StdSorter(const T& a, const T& b)
	{
		if(a == b)
			return 0;
		if(a > b)
			return 1;
		else
			return -1;
	}

	enum class ESortOrder : u8_t
	{
		ASCENDING = 1,
		DESCENDING = 2
	};

	struct range_t
	{
		usys_t index;
		usys_t count;
	};

	template<typename T>
	class TIterator
	{
		protected:
			array_t<T>* const array;
			usys_t index;

		public:
			explicit TIterator(array_t<T>* const array, const usys_t index = 0) : array(array), index(index) {}

			TIterator& operator++()
			{
				index++;
				return *this;
			}

			TIterator operator++(int)
			{
				TIterator retval = *this;
				++(*this);
				return retval;
			}

			bool operator==(TIterator other) const { return this->array == other.array && this->index == other.index; }
			bool operator!=(TIterator other) const { return this->array != other.array || this->index != other.index; }

			T& operator*() const
			{
				return (*array)[index];
			}
	};

	template<typename T>
	class TArrayPipe;

	// const array_t => everything is const, but you can just create a copy of the array_t and then everything is writeable
	// array_t<const T> => array-pointers can be modified, but data cannot (this is the only safe option, since an array_t can be copied)

	template<typename T>
	class array_t
	{
		protected:
			T* arr_items;
			usys_t n_items;

			template<typename C>
			usys_t BinarySearch(C comparator, const bool closest_match, usys_t idx_start, usys_t n_items_search) const EL_GETTER;

		public:
			// FIXME: never use Shift() on a TList !
			void Shift(const usys_t n_items);

			usys_t AbsoluteIndex(const ssys_t rel_index, const bool allow_tail) const;
			usys_t Count() const noexcept EL_GETTER { return n_items; }
			void Reverse();

			template<typename N, typename C = decltype(EqualsComparator<T>)>
			bool Contains(const N needle, C comparator = EqualsComparator<T>) const EL_GETTER; // TODO: binary search vs. linear search

			template<typename C = decltype(EqualsComparator<T>)>
			TList<range_t> Find(const T& needle, const usys_t n_max = NEG1, C comparator = EqualsComparator<T>) const EL_GETTER; // TODO: binary search vs. linear search

			template<typename C>
			usys_t BinarySearch(C comparator, const bool closest_match = false) const EL_GETTER; // TODO: => generic Find()

			template<typename L>
			void ForEach(L lambda) const;

			template<typename L>
			void Apply(L lambda);

			template<typename S = decltype(StdSorter<T>)>
			void Sort(const ESortOrder order, S sorter = StdSorter<T>);

			TArrayPipe<T> Pipe() const;

			TIterator<T> begin() { return TIterator<T>(this, 0); }
			TIterator<T> end()   { return TIterator<T>(this, this->n_items); }
			TIterator<const T> begin() const { return TIterator<const T>(this->operator array_t<const T>*(), 0); }
			TIterator<const T> end()   const { return TIterator<const T>(this->operator array_t<const T>*(), this->n_items); }

			T* ItemPtr(const usys_t index) EL_GETTER;
			const T* ItemPtr(const usys_t index) const EL_GETTER;

			T& operator[](const ssys_t index) EL_GETTER { return this->arr_items[this->AbsoluteIndex(index, false)]; }
			const T& operator[](const ssys_t index) const EL_GETTER { return const_cast<array_t&>(*this)[index]; }

			constexpr operator const array_t<const T>&() const    { return reinterpret_cast<array_t<const T>&>(const_cast<array_t&>(*this)); }
			constexpr explicit operator array_t<const T>*() const { return reinterpret_cast<array_t<const T>*>(const_cast<array_t*>( this)); }

			constexpr array_t& operator=(array_t&&) = default;
			constexpr array_t& operator=(const array_t&) = default;

			constexpr array_t() noexcept : arr_items(nullptr), n_items(0) {}
			constexpr array_t(T* const arr_items, const usys_t n_items) noexcept : arr_items(arr_items), n_items(n_items) {}
			constexpr array_t(const array_t&) = default;
			constexpr array_t(array_t&& other) = default;
			constexpr ~array_t() = default;
	};

	/*****************************************************************************/

	template<typename T, bool is_copyable>
	struct TList_Insert_Impl;

	template<typename T>
	struct TList_Insert_Impl<T, false>
	{
		T* MoveInsert(const ssys_t index, const array_t<T> array);
		T* MoveInsert(const ssys_t index, T* arr_items_insert, const usys_t n_items_insert);
		T& MoveInsert(const ssys_t index, T&& item);

		T* MoveAppend(const array_t<T> array);
		T* MoveAppend(T* arr_items_append, const usys_t n_items_append);
		T& MoveAppend(T&& item);
	};

	template<typename T>
	struct TList_Insert_Impl<T, true> : TList_Insert_Impl<T, false>
	{
		T* Insert(const ssys_t index, const array_t<T> array);
		T* Insert(const ssys_t index, const T* arr_items_insert, const usys_t n_items_insert);
		T& Insert(const ssys_t index, const T& item);
		T& Insert(const ssys_t index, T&& item);

		T* Append(const array_t<T> array);
		T* Append(const T* arr_items_append, const usys_t n_items_append);
		T& Append(const T& item);
		T& Append(T&& item);

		T* FillInsert(const ssys_t index, const T& item, const usys_t count);
	};

	/*****************************************************************************/

	template<typename T>
	class TList : public TList_Insert_Impl<T, std::is_copy_constructible<T>::value>, public array_t<T>
	{
		friend struct TList_Insert_Impl<T, std::is_copy_constructible<T>::value>;
		protected:
			void Prealloc(const usys_t n_items_need);
			void Shrink(const usys_t n_items_shrink);
			void CopyConstruct(const T* arr_items_source, const usys_t n_items_source, const usys_t n_prealloc = 0);
			void MoveItems(const usys_t idx_to, const usys_t idx_from, const usys_t n_items_move);
			void DestructItems(const usys_t index, const usys_t n_items_destruct);

		public:
			void Clear() noexcept;
			void Clear(const usys_t n_prealloc); // n_prealloc == NEG1 => keep existing buffer
			usys_t CountPreallocated() const EL_GETTER;

			template<typename G>
			T* GeneratorInsert(const ssys_t index, G generator, const usys_t n_items_insert);

			T* Inflate(const usys_t n_items_inflate, const T& templ)
			{
				return GeneratorInsert(this->n_items, [&templ](const usys_t){ return templ; }, n_items_inflate);
			}

			T* Claim() EL_WARN_UNUSED_RESULT;

			void Cut(const usys_t n_start, const usys_t n_end);
			void Remove(const ssys_t index, const usys_t n_items_remove);
			void Remove(const ssys_t index);

			template<typename C = decltype(EqualsComparator<T>)>
			usys_t RemoveItem(const T& needle, const usys_t n_max = 1U, C comparator = EqualsComparator<T>);

			TList& operator=(const TList& rhs);
			TList& operator=(TList&& rhs);

			constexpr TList() noexcept {}
			explicit TList(const usys_t n_prealloc);
			TList(const T* const arr_items, const usys_t n_items, const usys_t n_prealloc = 0);
			TList(T* const arr_items, const usys_t n_items, const bool claim, const usys_t n_prealloc = 0); // if claim is true, then n_prealloc is ignored
			TList(const array_t<const T>& other);
			TList(const array_t<T>& other);
			TList(const TList<T>& other);
			constexpr TList(TList&& other) noexcept;
			TList(std::initializer_list<T> list);
			~TList();
	};

	template<typename T>
	class TArrayPipe : public stream::IPipe<TArrayPipe<typename std::remove_const<T>::type>, typename std::remove_const<T>::type>
	{
		protected:
			const array_t<T>* const array;
			usys_t index;

		public:
			using TOut = T;
			using TIn = void;

			const TOut* NextItem() final override
			{
				if(index < array->Count())
					return &((*array)[index++]);
				else
					return nullptr;
			}

			TArrayPipe(const array_t<T>* array) : array(array), index(0) {}
	};

	template<typename T>
	struct TListSink : stream::ISink<T>
	{
		TList<T>* list;

		usys_t Write(const T* const arr_items, const usys_t n_items_max) final override EL_WARN_UNUSED_RESULT
		{
			this->list->Append(arr_items, n_items_max);
			return n_items_max;
		}

		constexpr TListSink(TList<T>* list) : list(list) {}
	};

	/*****************************************************************************/

	template<typename T>
	TArrayPipe<T> array_t<T>::Pipe() const
	{
		return TArrayPipe<T>(this);
	}

	template<typename T>
	void array_t<T>::Shift(const usys_t n_shift)
	{
		EL_ERROR(n_shift > this->n_items, TIndexOutOfBoundsException, 0, this->n_items, n_shift);
		this->arr_items += n_shift;
		this->n_items -= n_shift;
	}

	template<typename T>
	usys_t array_t<T>::AbsoluteIndex(const ssys_t rel_index, const bool allow_tail) const
	{
		if(rel_index >= 0)
		{
			EL_ERROR(rel_index > (ssys_t)n_items - (allow_tail ? 0 : 1), TIndexOutOfBoundsException, -n_items, n_items - 1, rel_index);
			return rel_index;
		}
		else
		{
			EL_ERROR(-rel_index > (ssys_t)n_items + (allow_tail ? 1 : 0), TIndexOutOfBoundsException, -n_items, n_items - 1, rel_index);

			if(-rel_index > (ssys_t)n_items && allow_tail)
				return n_items + rel_index + 1;
			else
				return n_items + rel_index;
		}
	}

	template<typename T>
	template<typename L>
	void array_t<T>::ForEach(L lambda) const
	{
		for(usys_t i = 0; i < n_items; i++)
			lambda((const T&)arr_items[i]);
	}

	template<typename T>
	template<typename N, typename C>
	bool array_t<T>::Contains(const N needle, C comparator) const
	{
		for(usys_t i = 0; i < n_items; i++)
			if(comparator(needle, arr_items[i]))
				return true;
		return false;
	}

	template<typename T>
	template<typename C>
	TList<range_t> array_t<T>::Find(const T& needle, const usys_t n_max, C comparator) const
	{
		TList<range_t> indices;
		usys_t n_left_to_find = n_max;

		if(n_max > 0)
			for(usys_t i = 0; i < n_items; i++)
				if(comparator(needle, arr_items[i]))
				{
					const usys_t start = i;
					for(i++; i < n_items && comparator(needle, arr_items[i]); i++);
					const usys_t end = i;

					range_t range = { start, end - start };

					if(range.count > n_left_to_find)
						range.count = n_left_to_find;
					n_left_to_find -= range.count;

					indices.Append(range);

					if(n_left_to_find == 0)
						break;
				}

		return indices;
	}

	template<typename T>
	template<typename C>
	usys_t array_t<T>::BinarySearch(C comparator, const bool closest_match, usys_t idx_start, usys_t n_items_search) const
	{
		usys_t idx_pivot = NEG1;

		while(n_items_search != 0)
		{
			idx_pivot = idx_start + n_items_search / 2;	// TODO: add some small amount of randomness

			const T& item = this->arr_items[idx_pivot];
			const int comp_result = comparator(item);

			// 0 exact match
			// >0 item is bigger than reference
			// <0 item is smaller than reference

			if(comp_result == 0)
				return idx_pivot;

			if(comp_result < 0)	// item was to small, go to upper half
			{
				idx_start = idx_pivot + 1;
				n_items_search--;
				n_items_search /= 2;
			}
			else //if(comp_result > 0)	// item was too big, go to lower half
			{
				n_items_search /= 2;
			}
		}

		if(closest_match)
			return idx_pivot;
		else
			return NEG1;
	}

	template<typename T>
	template<typename C>
	usys_t array_t<T>::BinarySearch(C comparator, const bool closest_match) const
	{
		return this->BinarySearch(comparator, closest_match, 0, this->n_items);
	}

	template<typename T>
	void array_t<T>::Reverse()
	{
		if(this->n_items <= 1) return;

		for(usys_t i = 0; i < this->n_items / 2; i++)
			util::Swap(this->arr_items[i], this->arr_items[this->n_items - 1 - i]);
	}


	template<typename T>
	template<typename L>
	void array_t<T>::Apply(L lambda)
	{
		for(usys_t i = 0; i < this->n_items; i++)
			lambda(this->arr_items[i]);
	}

	template<typename T>
	template<typename S>
	void array_t<T>::Sort(const ESortOrder order, S sorter)
	{
		EL_NOT_IMPLEMENTED;
	}

	template<typename T>
	T* array_t<T>::ItemPtr(const usys_t index)
	{
		if(index >= n_items)
			return nullptr;
		else
			return arr_items + index;
	}

	template<typename T>
	const T* array_t<T>::ItemPtr(const usys_t index) const
	{
		if(index >= n_items)
			return nullptr;
		else
			return arr_items + index;
	}

	/*****************************************************************************/

	template<typename T>
	T* TList_Insert_Impl<T, false>::MoveInsert(const ssys_t index, const array_t<T> array)
	{
		if(array.Count() > 0)
			return MoveInsert(index, &array[0], array.Count());
		else
			return nullptr;
	}

	template<typename T>
	T* TList_Insert_Impl<T, false>::MoveInsert(const ssys_t index, T* const arr_items_insert, const usys_t n_items_insert)
	{
		TList<T>* list = static_cast<TList<T>*>(this);
		return list->GeneratorInsert(index, [arr_items_insert](const usys_t i) { return std::move(arr_items_insert[i]); }, n_items_insert);
	}

	template<typename T>
	T& TList_Insert_Impl<T, false>::MoveInsert(const ssys_t index, T&& item)
	{
		return *MoveInsert(index, &item, 1);
	}

	template<typename T>
	T* TList_Insert_Impl<T, false>::MoveAppend(const array_t<T> array)
	{
		TList<T>* list = static_cast<TList<T>*>(this);
		return MoveInsert(list->Count(), array);
	}

	template<typename T>
	T* TList_Insert_Impl<T, false>::MoveAppend(T* const arr_items_append, const usys_t n_items_append)
	{
		TList<T>* list = static_cast<TList<T>*>(this);
		return MoveInsert(list->Count(), arr_items_append, n_items_append);
	}

	template<typename T>
	T& TList_Insert_Impl<T, false>::MoveAppend(T&& item)
	{
		TList<T>* list = static_cast<TList<T>*>(this);
		return MoveInsert(list->Count(), std::move(item));
	}

	/*****************************************************************************/

	template<typename T>
	T* TList_Insert_Impl<T, true>::Insert(const ssys_t index, const array_t<T> array)
	{
		if(array.Count() > 0)
			return Insert(index, &array[0], array.Count());
		else
			return nullptr;
	}

	template<typename T>
	T* TList_Insert_Impl<T, true>::Insert(const ssys_t index, const T* const arr_items_insert, const usys_t n_items_insert)
	{
		TList<T>* list = static_cast<TList<T>*>(this);
		return list->GeneratorInsert(index, [arr_items_insert](const usys_t i) { return arr_items_insert[i]; }, n_items_insert);
	}

	template<typename T>
	T& TList_Insert_Impl<T, true>::Insert(const ssys_t index, const T& item)
	{
		return *Insert(index, &item, 1);
	}

	template<typename T>
	T& TList_Insert_Impl<T, true>::Insert(const ssys_t index, T&& item)
	{
		TList<T>* list = static_cast<TList<T>*>(this);
		return *list->GeneratorInsert(index, [&item](const usys_t i) { return std::move(item); }, 1);
	}

	template<typename T>
	T* TList_Insert_Impl<T, true>::FillInsert(const ssys_t index, const T& item, const usys_t count)
	{
		TList<T>* list = static_cast<TList<T>*>(this);
		return list->GeneratorInsert(index, [&](const usys_t) { return item; }, count);
	}

	template<typename T>
	T* TList_Insert_Impl<T, true>::Append(const array_t<T> array)
	{
		TList<T>* list = static_cast<TList<T>*>(this);
		return Insert(list->n_items, array);
	}

	template<typename T>
	T* TList_Insert_Impl<T, true>::Append(const T* const arr_items_append, const usys_t n_items_append)
	{
		TList<T>* list = static_cast<TList<T>*>(this);
		return Insert(list->n_items, arr_items_append, n_items_append);
	}

	template<typename T>
	T& TList_Insert_Impl<T, true>::Append(const T& item)
	{
		TList<T>* list = static_cast<TList<T>*>(this);
		return Insert(list->n_items, item);
	}

	template<typename T>
	T& TList_Insert_Impl<T, true>::Append(T&& item)
	{
		TList<T>* list = static_cast<TList<T>*>(this);
		return Insert(list->n_items, std::move(item));
	}

	/*****************************************************************************/

	template<typename T>
	usys_t TList<T>::CountPreallocated() const
	{
		return (system::memory::UseableSize(this->arr_items) / sizeof(T)) - this->n_items;
	}

	template<typename T>
	void TList<T>::Prealloc(const usys_t n_items_need)
	{
		const usys_t n_items_preallocated = this->CountPreallocated();
		const usys_t n_items_prealloc_ideal = util::Max(n_items_need, this->n_items / 16, 128 / sizeof(T));

		if(n_items_preallocated < n_items_prealloc_ideal)
		{
			const usys_t n_bytes_realloc = sizeof(T) * (this->n_items + n_items_prealloc_ideal);
			T* const arr_items_new = (T*)realloc((void*)this->arr_items, n_bytes_realloc);
			EL_ERROR(n_bytes_realloc > 0 && arr_items_new == nullptr, TOutOfMemoryException, n_bytes_realloc);
			this->arr_items = arr_items_new;
		}
		else if(n_items_preallocated > n_items_prealloc_ideal * 2)
		{
			const usys_t n_bytes_realloc = sizeof(T) * (this->n_items + n_items_prealloc_ideal);
			T* const arr_items_new = (T*)realloc((void*)this->arr_items, n_bytes_realloc);
			EL_ERROR(n_bytes_realloc > 0 && arr_items_new == nullptr, TOutOfMemoryException, n_bytes_realloc - (this->n_items + n_items_preallocated) * sizeof(T));
			this->arr_items = arr_items_new;
		}
	}

	template<typename T>
	void TList<T>::Shrink(const usys_t n_items_shrink)
	{
		this->n_items -= n_items_shrink;
		Prealloc(0);
	}

	template<typename T>
	void TList<T>::CopyConstruct(const T* arr_items_source, const usys_t n_items_source, const usys_t n_prealloc)
	{
		try
		{
			Prealloc(n_items_source + n_prealloc);

			for(usys_t i = 0; i < n_items_source; i++)
			{
				new (this->arr_items + this->n_items) T(arr_items_source[i]);
				this->n_items++;
			}
		}
		catch(...)
		{
			Clear();
			throw;
		}
	}

	template<typename T>
	void TList<T>::MoveItems(const usys_t idx_to, const usys_t idx_from, const usys_t n_items_move)
	{
		if(n_items_move != 0)
		{
			::memmove(reinterpret_cast<void*>(this->arr_items + idx_to), reinterpret_cast<void*>(this->arr_items + idx_from), n_items_move * sizeof(T));
		}
	}

	template<typename T>
	void TList<T>::DestructItems(const usys_t index, const usys_t n_items_destruct)
	{
		for(usys_t i = 0; i < n_items_destruct; i++)
			this->arr_items[index + i].~T();
	}

	template<typename T>
	template<typename G>
	T* TList<T>::GeneratorInsert(const ssys_t index, G generator, const usys_t n_items_insert)
	{
		// TList<T>* this = static_cast<TList<T>*>(this);

		const usys_t abs_index = this->AbsoluteIndex(index, true);
		const usys_t n_items_tail = this->n_items - abs_index;

		this->Prealloc(n_items_insert);
		this->MoveItems(abs_index + n_items_insert, abs_index, n_items_tail);

		this->n_items += n_items_insert;

		for(usys_t i = 0; i < n_items_insert; i++)
		{
			try
			{
				new (this->arr_items + abs_index + i) T(std::move(generator(i)));
			}
			catch(...)
			{
				const usys_t n_items_actually_inserted = i;
				const usys_t n_items_not_inserted = n_items_insert - n_items_actually_inserted;

				this->MoveItems(abs_index + n_items_actually_inserted + 1, abs_index + n_items_insert, n_items_tail);
				this->n_items -= n_items_not_inserted;

				throw;
			}
		}

		return this->ItemPtr(abs_index);
	}

	template<typename T>
	void TList<T>::Clear() noexcept
	{
		for(usys_t i = 0; i < this->n_items; i++)
			this->arr_items[i].~T();

		free(this->arr_items);
		this->arr_items = nullptr;
		this->n_items = 0;
	}

	template<typename T>
	void TList<T>::Clear(const usys_t n_prealloc)
	{
		for(usys_t i = 0; i < this->n_items; i++)
			this->arr_items[i].~T();
		this->n_items = 0;

		if(n_prealloc == 0)
		{
			free(this->arr_items);
			this->arr_items = nullptr;
		}
		else
		{
			if(n_prealloc != NEG1)
				this->Prealloc(n_prealloc);
		}
	}

	template<typename T>
	TList<T>& TList<T>::operator=(const TList& rhs)
	{
		EL_ERROR(this == &rhs, TLogicException);
		EL_ERROR(this->arr_items == rhs.arr_items, TLogicException);

		this->Clear();
		this->CopyConstruct(rhs.arr_items, rhs.n_items);

		return *this;
	}

	template<typename T>
	TList<T>& TList<T>::operator=(TList&& rhs)
	{
		EL_ERROR(this == &rhs, TLogicException);
		EL_ERROR(this->arr_items != nullptr && this->arr_items == rhs.arr_items, TLogicException);

		this->Clear();
		this->arr_items = rhs.arr_items;
		this->n_items = rhs.n_items;
		rhs.arr_items = nullptr;
		rhs.n_items = 0;

		return *this;
	}

	template<typename T>
	TList<T>::TList(const usys_t n_prealloc)
	{
		Prealloc(n_prealloc);
	}

	template<typename T>
	TList<T>::TList(const T* const arr_items, const usys_t n_items, const usys_t n_prealloc)
	{
		CopyConstruct(arr_items, n_items, n_prealloc);
	}

	template<typename T>
	TList<T>::TList(T* const arr_items, const usys_t n_items, const bool claim, const usys_t n_prealloc)
	{
		if(claim)
		{
			this->arr_items = arr_items;
			this->n_items = n_items;
		}
		else
		{
			CopyConstruct(arr_items, n_items, n_prealloc);
		}
	}

	template<typename T>
	TList<T>::TList(const array_t<const T>& other) : array_t<T>()
	{
		if(other.Count() > 0)
			CopyConstruct(&other[0], other.Count());
	}

	template<typename T>
	TList<T>::TList(const array_t<T>& other) : array_t<T>()
	{
		if(other.Count() > 0)
			CopyConstruct(&other[0], other.Count());
	}

	template<typename T>
	TList<T>::TList(const TList<T>& other) : array_t<T>()
	{
		CopyConstruct(other.arr_items, other.n_items);
	}

	template<typename T>
	constexpr TList<T>::TList(TList&& other) noexcept : array_t<T>()
	{
		this->arr_items = other.arr_items;
		this->n_items = other.n_items;
		other.arr_items = nullptr;
		other.n_items = 0;
	}

	template<typename T>
	TList<T>::TList(std::initializer_list<T> list)
	{
		CopyConstruct(list.begin(), list.end() - list.begin());
	}

	template<typename T>
	TList<T>::~TList()
	{
		Clear();
	};

	template<typename T>
	void TList<T>::Remove(const ssys_t index_start, const usys_t n_items_remove)
	{
		const usys_t index = this->AbsoluteIndex(index_start, false);
		EL_ERROR(index + n_items_remove > this->n_items, TIndexOutOfBoundsException, -this->n_items, this->n_items - 1, index + n_items_remove);
		const usys_t n_items_tail = this->n_items - index - n_items_remove;

		this->DestructItems(index, n_items_remove);
		this->MoveItems(index, index + n_items_remove, n_items_tail);
		this->Shrink(n_items_remove);
	}

	template<typename T>
	void TList<T>::Remove(const ssys_t index)
	{
		Remove(index, 1);
	}

	template<typename T>
	void TList<T>::Cut(const usys_t n_start, const usys_t n_end)
	{
		const usys_t n_cut = n_start + n_end;
		EL_ERROR(n_cut > this->n_items, TIndexOutOfBoundsException, -this->n_items, this->n_items - 1, n_cut);
		const usys_t n_remaining = this->n_items - n_cut;

		this->DestructItems(0, n_start);
		this->DestructItems(this->n_items - n_end, n_end);
		this->MoveItems(0, n_start, n_remaining);
		this->Shrink(n_cut);
	}

	template<typename T>
	template<typename C>
	usys_t TList<T>::RemoveItem(const T& needle, const usys_t n_max, C comparator)
	{
		const TList<range_t> indices = this->Find(needle, n_max, comparator);
		usys_t n_removed = 0;

		for(usys_t i = 0; i < indices.Count(); i++)
		{
			const range_t& range = indices[i];
			const usys_t idx_from = range.index + range.count;
			const usys_t idx_to = range.index - n_removed;
			const usys_t n_move = (i + 1 < indices.Count())
								? ( indices[i+1].index - range.index - range.count )
								: ( this->n_items - range.index - range.count );

			this->DestructItems(range.index, range.count);
			this->MoveItems(idx_to, idx_from, n_move);
			n_removed += range.count;
		}

		this->Shrink(n_removed);
		return n_removed;
	}

	template<typename T>
	T* TList<T>::Claim()
	{
		T* tmp = this->arr_items;
		this->arr_items = nullptr;
		this->n_items = 0;
		return tmp;
	}
}

namespace el1::io::stream
{
	template<typename TStream, typename TOut>
	collection::list::TList<TOut> IPipe<TStream, TOut>::Collect()
	{
		collection::list::TList<TOut> list;
		TStream* source = static_cast<TStream*>(this);

		for(const TOut* item = source->NextItem(); item != nullptr; item = source->NextItem())
			list.Append(std::move(*item));

		return list;
	}
}
