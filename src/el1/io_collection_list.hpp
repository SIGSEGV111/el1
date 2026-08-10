#pragma once

#include "io_collection_array.hpp"
#include "error.hpp"
#include "util.hpp"
#include "system_memory.hpp"

#include <utility>
#include <new>
#include <malloc.h>
#include <string.h>
#include <type_traits>

namespace el1::io::collection::list
{
	using namespace io::types;

	template<typename T>
	struct TListSink;

	template<typename T, bool is_copyable>
	struct TList_Insert_Impl;

	template<typename T>
	struct TList_Insert_Impl<T, false>
	{
		T* MoveInsert(const ssys_t index, array_t<T> array);
		T* MoveInsert(const ssys_t index, T* arr_items_insert, const usys_t n_items_insert);
		T& MoveInsert(const ssys_t index, T&& item);

		T* MoveAppend(const array_t<T> array);
		T* MoveAppend(T* arr_items_append, const usys_t n_items_append);
		T& MoveAppend(T&& item);
	};

	template<typename T>
	struct TList_Insert_Impl<T, true> : TList_Insert_Impl<T, false>
	{
		void SetCount(const usys_t new_count);

		T* Insert(const ssys_t index, const array_t<const T> array);
		T* Insert(const ssys_t index, const T* arr_items_insert, const usys_t n_items_insert);
		T& Insert(const ssys_t index, const T& item);
		T& Insert(const ssys_t index, T&& item);

		T* Append(const array_t<const T> array);
		T* Append(const T* arr_items_append, const usys_t n_items_append);
		T& Append(const T& item);
		T& Append(T&& item);

		T* FillInsert(const ssys_t index, const T& item, const usys_t count);
	};

	/*****************************************************************************/

	template<typename T>
	class EL_LIFETIME_OWNER TList : public TList_Insert_Impl<T, std::is_copy_constructible<T>::value>, public array_t<T>
	{
		static_assert(!std::is_const_v<T>, "TList owns mutable objects; use array_t<const T> for a read-only view");
		friend struct TList_Insert_Impl<T, std::is_copy_constructible<T>::value>;
		friend struct TListSink<T>;
		protected:
			using array_t<T>::Shift;

			void Shrink(const usys_t n_items_shrink);
			void CopyConstruct(const T* arr_items_source, const usys_t n_items_source, const usys_t n_prealloc = 0);
			void MoveItems(const usys_t idx_to, const usys_t idx_from, const usys_t n_items_move);
			void DestructItems(const usys_t index, const usys_t n_items_destruct);

		public:
			array_t<T> View() & noexcept EL_LIFETIME_BOUND { return array_t<T>::FromUnsafePointer(this->arr_items, this->n_items); }
			array_t<const T> View() const & noexcept EL_LIFETIME_BOUND { return array_t<const T>::FromUnsafePointer(this->arr_items, this->n_items); }
			array_t<T> View() && = delete;
			array_t<const T> View() const && = delete;

			void Prealloc(const usys_t n_items_need);
			void Truncate() noexcept;
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

			// removes and returns the first entry in the list
			T PopHead();

			template<typename C = decltype(EqualsComparator<T>)>
			usys_t RemoveItem(const T& needle, const usys_t n_max = 1U, C comparator = EqualsComparator<T>);

			template<typename C = decltype(EqualsComparator<T>)>
			usys_t RemoveItems(array_t<const T> items_remove, C comparator = EqualsComparator<T>);

			template<typename C = decltype(EqualsComparator<T>)>
			usys_t RemoveDuplicateItems(const bool is_sorted, C comparator = EqualsComparator<T>);

			TList& operator-=(array_t<const T> items_remove)
			{
				RemoveItems(items_remove);
				return *this;
			}

			TList operator-(array_t<const T> items_remove)
			{
				TList l = *this;
				l -= items_remove;
				return l;
			}

			TList& operator+=(array_t<const T> items_append)
			{
				Append(items_append);
				return *this;
			}

			TList operator+(array_t<const T> items_append)
			{
				TList l = *this;
				l += items_append;
				return l;
			}

			TList& operator=(const TList& rhs);
			TList& operator=(TList&& rhs);


			constexpr TList() noexcept {}
			explicit TList(const usys_t n_prealloc);
			TList(const T* const arr_items, const usys_t n_items, const usys_t n_prealloc = 0);
			TList(T* const arr_items, const usys_t n_items, const bool claim, const usys_t n_prealloc = 0); // if claim is true, then n_prealloc is ignored
			TList(const array_t<const T>& other);
			// TList(const array_t<T>& other);
			TList(const TList<T>& other);
			constexpr TList(TList&& other) noexcept;
			TList(std::initializer_list<T> list);
			~TList();
	};

	template<typename T>
	struct TListSink : stream::ISink<T>
	{
		TList<T>* list;
		usys_t n_items_prealloc;

		usys_t Write(const T* const arr_items, const usys_t n_items_max) final override EL_WARN_UNUSED_RESULT
		{
			this->list->Append(arr_items, n_items_max);
			return n_items_max;
		}

		iosize_t ReadIn(stream::ISource<T>& source, const iosize_t n_items_max = (iosize_t)-1, const bool = true) final override
		{
			const usys_t n_prealloc_req = util::Min<iosize_t>(n_items_max, n_items_prealloc);
			usys_t n_preallocated = list->CountPreallocated();
			T* p_append = list->arr_items + list->n_items;

			usys_t n_read = 0;
			while(n_read < n_items_max)
			{
				const usys_t n_remaining = n_items_max == (iosize_t)-1 ? NEG1 : (usys_t)n_items_max - n_read;

				if(n_preallocated == 0)
				{
					list->Prealloc(n_prealloc_req);
					n_preallocated = list->CountPreallocated();
					p_append = list->arr_items + list->n_items;
				}

				usys_t r = util::Min(n_remaining, n_preallocated);
				r = source.Read(p_append, r);
				if(r == 0)
					break;

				p_append += r;
				n_read += r;
				n_preallocated -= r;
				list->n_items += r;
			}
			return n_read;
		}

		constexpr TListSink(TList<T>* list, const usys_t n_items_prealloc = util::Max<iosize_t>(1, 4096U / sizeof(T))) : list(list), n_items_prealloc(n_items_prealloc) {}
	};

	template<typename T>
	struct TListSource : stream::ISource<T>
	{
		TList<T> list;
		usys_t pos;

		usys_t Remaining() const EL_GETTER
		{
			return list.Count() - pos;
		}

		usys_t Read(T* const arr_items, const usys_t n_items_max) final override EL_WARN_UNUSED_RESULT
		{
			const usys_t n = util::Min(Remaining(), n_items_max);
			for(usys_t i = 0; i < n; i++)
				arr_items[i] = list[pos++];
			return n;
		}

		iosize_t WriteOut(stream::ISink<T>& sink, const iosize_t n_items_max, const bool) final override
		{
			const usys_t n = n_items_max == (iosize_t)-1 ? Remaining() : util::Min<usys_t>(Remaining(), n_items_max);
			if(n == 0)
				return 0;

			const usys_t n_written = sink.Write(list.ItemPtr(pos), n);
			pos += n_written;
			return n_written;
		}

		TListSource(const TListSource&) = delete;
		TListSource(TListSource&&) = default;
		explicit TListSource(TList<T> list) : list(std::move(list)), pos(0) {}
	};
}

namespace el1::io::collection::array
{
	template<typename T>
	template<typename C>
	list::TList<range_t> array_t<T>::Find(const value_t& needle, const usys_t n_max, C comparator) const
	{
		list::TList<range_t> indices;
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
}

namespace el1::io::collection::list
{

	template<typename T>
	T* TList_Insert_Impl<T, false>::MoveInsert(const ssys_t index, array_t<T> array)
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
	void TList_Insert_Impl<T, true>::SetCount(const usys_t new_count)
	{
		TList<T>* list = static_cast<TList<T>*>(this);
		if(new_count < list->n_items)
		{
			list->Cut(0, list->n_items - new_count);
		}
		else
		{
			const usys_t n_add = new_count - list->n_items;
			list->Prealloc(new_count);
			for(usys_t i = 0; i < n_add; i++)
				try { new (list->arr_items + list->n_items + i) T(); }
				catch(...)
				{
					list->n_items += i;
					throw;
				}

			list->n_items = new_count;
		}
	}


	template<typename T>
	T* TList_Insert_Impl<T, true>::Insert(const ssys_t index, const array_t<const T> array)
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
		return *list->GeneratorInsert(index, [&item](const usys_t) { return std::move(item); }, 1);
	}

	template<typename T>
	T* TList_Insert_Impl<T, true>::FillInsert(const ssys_t index, const T& item, const usys_t count)
	{
		TList<T>* list = static_cast<TList<T>*>(this);
		return list->GeneratorInsert(index, [&](const usys_t) { return item; }, count);
	}

	template<typename T>
	T* TList_Insert_Impl<T, true>::Append(const array_t<const T> array)
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
		return (system::memory::UseableSize((void*)this->arr_items) / sizeof(T)) - this->n_items;
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
				new ((void*)(this->arr_items + this->n_items)) T(arr_items_source[i]);
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
	void TList<T>::Truncate() noexcept
	{
		for(usys_t i = 0; i < this->n_items; i++)
			this->arr_items[i].~T();
		this->n_items = 0;
	}

	template<typename T>
	void TList<T>::Clear() noexcept
	{
		this->Truncate();
		free((void*)this->arr_items);
		this->arr_items = nullptr;
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
		EL_ERROR(this->arr_items == rhs.arr_items && rhs.arr_items != nullptr, TLogicException);

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

	// template<typename T>
	// TList<T>::TList(const array_t<T>& other) : array_t<T>()
	// {
	// 	if(other.Count() > 0)
	// 		CopyConstruct(&other[0], other.Count());
	// }

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
	T TList<T>::PopHead()
	{
		EL_ERROR(this->n_items == 0, TException, "list is empty");
		T tmp = std::move(this->arr_items[0]);
		this->Remove(0,1);
		return tmp;
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
	template<typename C>
	usys_t TList<T>::RemoveItems(array_t<const T> items_remove, C comparator)
	{
		usys_t n = 0;
		for(usys_t i = 0; i < this->n_items; i++)
			for(auto& j : items_remove)
				if(comparator(this->arr_items[i], j))
				{
					Remove(i);
					i--;
					n++;
					break;
				}
		return n;
	}

	template<typename T>
	template<typename C>
	usys_t TList<T>::RemoveDuplicateItems(const bool is_sorted, C comparator)
	{
		usys_t n_removed = 0;

		if(is_sorted)
		{
			for(usys_t i = 1; i < this->n_items; i++)
			{
				usys_t j = i;
				for(; j < this->n_items; j++)
					if(!comparator(this->arr_items[i - 1], this->arr_items[j]))
						break;
				this->Remove(i, j - i);
				n_removed += j - i;
			}
		}
		else
		{
			for(usys_t i = 1; i < this->n_items; i++)
			{
				usys_t j = i;
				for(; j < this->n_items; j++)
					if(comparator(this->arr_items[i - 1], this->arr_items[j]))
					{
						this->Remove(j);
						j--;
						n_removed++;
					}
			}
		}

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

namespace el1::io::collection::array
{
	template<typename T>
	template<typename U>
	requires std::is_convertible_v<U*, T*>
	array_t<T>::array_t(list::TList<U>& owner EL_LIFETIME_BOUND) noexcept : arr_items(owner.Data()), n_items(owner.Count()) {}

	template<typename T>
	template<typename U>
	requires std::is_convertible_v<const U*, T*>
	array_t<T>::array_t(const list::TList<U>& owner EL_LIFETIME_BOUND) noexcept : arr_items(owner.Data()), n_items(owner.Count()) {}
}

namespace el1::io::stream
{
	template<typename TStream, typename TOut>
	collection::list::TList<std::remove_const_t<TOut>> IPipe<TStream, TOut>::Collect(const usys_t n_prealloc)
	{
		collection::list::TList<std::remove_const_t<TOut>> list;
		TStream* source = static_cast<TStream*>(this);

		for(TOut* item = source->NextItem(); item != nullptr; item = source->NextItem())
		{
			if constexpr(std::is_const_v<TOut>)
				list.Append(*item);
			else
				list.MoveAppend(std::move(*item));
		}

		return list;
	}
}
