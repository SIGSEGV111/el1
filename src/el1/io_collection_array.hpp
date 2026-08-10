#pragma once

#include "io_types.hpp"
#include "io_stream.hpp"
#include "error.hpp"
#include "util.hpp"
#include "system_memory.hpp"

#include <type_traits>
#include <utility>

namespace el1::io::collection::list
{
	template<typename T>
	class TList;
}

namespace el1::io::collection::array
{
	using namespace io::types;
	using list::TList;

	template<typename T>
	class array_t;

	template<typename T>
	class TArrayPipe;

	template<typename T>
	struct TArraySource;

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
	class EL_LIFETIME_POINTER TIterator
	{
		template<typename U>
		friend class TIterator;

		protected:
			T* ptr;

		public:
			constexpr explicit TIterator(T* const ptr = nullptr) noexcept : ptr(ptr) {}

			template<typename U>
			requires std::is_convertible_v<U*, T*>
			constexpr TIterator(const TIterator<U>& other) noexcept : ptr(other.ptr) {}

			constexpr TIterator& operator++() noexcept
			{
				ptr++;
				return *this;
			}

			constexpr TIterator operator++(int) noexcept
			{
				TIterator retval = *this;
				++(*this);
				return retval;
			}

			constexpr bool operator==(const TIterator& other) const noexcept { return ptr == other.ptr; }
			constexpr bool operator!=(const TIterator& other) const noexcept { return ptr != other.ptr; }

			constexpr T& operator*() const noexcept { return *ptr; }
			constexpr T* operator->() const noexcept { return ptr; }
	};

	/**
	 * Non-owning contiguous array view.
	 *
	 * array_t never allocates, frees, constructs or destructs elements. Copying an
	 * array_t only copies the pointer and item count. The referenced storage must
	 * outlive all views, iterators, pipes and sources created from it.
	 *
	 * Constness follows normal C++ container semantics:
	 *  - array_t<T> can modify T through a non-const view object.
	 *  - const array_t<T> only exposes const T.
	 *  - array_t<const T> never exposes mutable T, even when the view itself is mutable.
	 */
	template<typename T>
	class EL_LIFETIME_POINTER array_t
	{
		template<typename U>
		friend class array_t;

		protected:
			T* arr_items;
			usys_t n_items;

		private:
			struct TRawPointerTag {};
			constexpr array_t(T* const arr_items, const usys_t n_items, TRawPointerTag) noexcept : arr_items(arr_items), n_items(n_items) {}

		protected:
			template<typename C>
			usys_t BinarySearch(C comparator, const bool closest_match, usys_t idx_start, usys_t n_items_search) const EL_GETTER;

		public:
			using element_t = T;
			using value_t = std::remove_const_t<T>;
			using pointer_t = T*;
			using const_pointer_t = const value_t*;
			using reference_t = T&;
			using const_reference_t = const value_t&;
			using iterator_t = TIterator<T>;
			using const_iterator_t = TIterator<const value_t>;

			constexpr array_t() noexcept : arr_items(nullptr), n_items(0) {}

			// Explicit escape hatch for storage whose lifetime cannot be expressed through
			// an owner, C array or another view. The caller owns the lifetime contract.
			static constexpr array_t FromUnsafePointer(T* const arr_items EL_LIFETIME_BOUND, const usys_t n_items) noexcept
			{
				return array_t(arr_items, n_items, TRawPointerTag{});
			}

			template<usys_t N>
			constexpr array_t(T (&arr_items EL_LIFETIME_BOUND)[N]) noexcept : arr_items(arr_items), n_items(N) {}

			template<typename U>
			requires std::is_convertible_v<U(*)[], T(*)[]>
			constexpr array_t(const array_t<U>& other EL_LIFETIME_BOUND) noexcept : arr_items(other.arr_items), n_items(other.n_items) {}

			template<typename U>
			requires std::is_convertible_v<U*, T*>
			array_t(TList<U>& owner EL_LIFETIME_BOUND) noexcept;

			template<typename U>
			requires std::is_convertible_v<const U*, T*>
			array_t(const TList<U>& owner EL_LIFETIME_BOUND) noexcept;

			constexpr array_t(const array_t& arr_items EL_LIFETIME_BOUND, const usys_t n_items_max) noexcept : arr_items(arr_items.arr_items), n_items(util::Min(n_items_max, arr_items.n_items)) {}
			constexpr array_t(const array_t&) noexcept = default;
			constexpr array_t(array_t&&) noexcept = default;
			constexpr array_t& operator=(const array_t&) noexcept = default;
			constexpr array_t& operator=(array_t&&) noexcept = default;
			~array_t() = default;

			constexpr bool IsEmpty() const noexcept EL_GETTER { return n_items == 0; }
			constexpr usys_t Count() const noexcept EL_GETTER { return n_items; }

			constexpr T* Data() noexcept EL_LIFETIME_BOUND { return arr_items; }
			constexpr const value_t* Data() const noexcept EL_LIFETIME_BOUND { return arr_items; }

			T* ItemPtr(const usys_t index) noexcept EL_LIFETIME_BOUND EL_GETTER;
			const value_t* ItemPtr(const usys_t index) const noexcept EL_LIFETIME_BOUND EL_GETTER;

			T& operator[](const ssys_t index) EL_LIFETIME_BOUND EL_GETTER { return arr_items[AbsoluteIndex(index, false)]; }
			const value_t& operator[](const ssys_t index) const EL_LIFETIME_BOUND EL_GETTER { return arr_items[AbsoluteIndex(index, false)]; }

			T& First() EL_LIFETIME_BOUND EL_GETTER { return (*this)[0]; }
			const value_t& First() const EL_LIFETIME_BOUND EL_GETTER { return (*this)[0]; }
			T& Last() EL_LIFETIME_BOUND EL_GETTER { return (*this)[-1]; }
			const value_t& Last() const EL_LIFETIME_BOUND EL_GETTER { return (*this)[-1]; }

			constexpr iterator_t begin() noexcept EL_LIFETIME_BOUND { return iterator_t(arr_items); }
			constexpr iterator_t end() noexcept EL_LIFETIME_BOUND { return iterator_t(n_items == 0 ? arr_items : arr_items + n_items); }
			constexpr const_iterator_t begin() const noexcept EL_LIFETIME_BOUND { return const_iterator_t(arr_items); }
			constexpr const_iterator_t end() const noexcept EL_LIFETIME_BOUND { return const_iterator_t(n_items == 0 ? arr_items : arr_items + n_items); }
			constexpr const_iterator_t cbegin() const noexcept EL_LIFETIME_BOUND { return begin(); }
			constexpr const_iterator_t cend() const noexcept EL_LIFETIME_BOUND { return end(); }

			constexpr array_t<const value_t> AsConst() const noexcept EL_LIFETIME_BOUND { return array_t<const value_t>::FromUnsafePointer(arr_items, n_items); }

			usys_t AbsoluteIndex(const ssys_t rel_index, const bool allow_tail) const;

			array_t Slice(const ssys_t index, const usys_t count = NEG1) EL_LIFETIME_BOUND;
			array_t<const value_t> Slice(const ssys_t index, const usys_t count = NEG1) const EL_LIFETIME_BOUND;
			array_t Head(const usys_t count) EL_LIFETIME_BOUND;
			array_t<const value_t> Head(const usys_t count) const EL_LIFETIME_BOUND;
			array_t Tail(const usys_t count) EL_LIFETIME_BOUND;
			array_t<const value_t> Tail(const usys_t count) const EL_LIFETIME_BOUND;
			std::pair<array_t, array_t> SplitAt(const ssys_t index) EL_LIFETIME_BOUND;
			std::pair<array_t<const value_t>, array_t<const value_t>> SplitAt(const ssys_t index) const EL_LIFETIME_BOUND;

			// Rebinds this view to skip the first n items. It never moves array contents.
			void Shift(const usys_t n_items);

			void Reverse() requires (!std::is_const_v<T>);

			template<typename N, typename C = decltype(EqualsComparator<value_t>)>
			bool Contains(const N& needle, C comparator = EqualsComparator<value_t>) const EL_GETTER;

			template<typename C = decltype(EqualsComparator<value_t>)>
			usys_t FindFirst(const value_t& needle, C comparator = EqualsComparator<value_t>) const EL_GETTER;

			// Legacy allocating helper. Its definition lives in io_collection_list.hpp.
			template<typename C = decltype(EqualsComparator<value_t>)>
			TList<range_t> Find(const value_t& needle, const usys_t n_max = NEG1, C comparator = EqualsComparator<value_t>) const EL_GETTER;

			template<typename C>
			usys_t BinarySearch(C comparator, const bool closest_match = false) const EL_GETTER;

			template<typename L>
			void ForEach(L lambda) const;

			template<typename L>
			void Apply(L lambda) requires (!std::is_const_v<T>);

			template<typename S = decltype(StdSorter<value_t>)>
			void Sort(const ESortOrder order = ESortOrder::ASCENDING, S sorter = StdSorter<value_t>) requires (!std::is_const_v<T>);

			TArrayPipe<T> Pipe() noexcept EL_LIFETIME_BOUND;
			TArrayPipe<const value_t> Pipe() const noexcept EL_LIFETIME_BOUND;
			TArraySource<value_t> Source() const noexcept EL_LIFETIME_BOUND;

			system::waitable::TMemoryWaitable<usys_t> MakeItemCountWaitable(const usys_t* const n_expected_count) const;

	};

	template<typename T, usys_t N>
	array_t(T (&)[N]) -> array_t<T>;

	template<typename T>
	class EL_LIFETIME_POINTER TArrayPipe : public stream::IPipe<TArrayPipe<T>, T>
	{
		protected:
			T* const arr_items;
			const usys_t n_items;
			usys_t index;

		public:
			using TOut = T;
			using TIn = void;

			TOut* NextItem() final override
			{
				if(index < n_items)
					return arr_items + index++;
				return nullptr;
			}

			constexpr TArrayPipe(T* const arr_items EL_LIFETIME_BOUND, const usys_t n_items) noexcept : arr_items(arr_items), n_items(n_items), index(0) {}
		};

	template<typename T>
	struct EL_LIFETIME_POINTER TArraySource : stream::ISource<T>
	{
		const array_t<const T> array;
		usys_t pos;

		usys_t Remaining() const EL_GETTER
		{
			return array.Count() - pos;
		}

		usys_t Read(T* const arr_items, const usys_t n_items_max) final override EL_WARN_UNUSED_RESULT
		{
			const usys_t n = util::Min(Remaining(), n_items_max);
			for(usys_t i = 0; i < n; i++)
				arr_items[i] = array[pos++];
			return n;
		}

		iosize_t WriteOut(stream::ISink<T>& sink, const iosize_t n_items_max, const bool) final override
		{
			const iosize_t n_want = util::Min<iosize_t>(Remaining(), n_items_max);
			if(n_want == 0)
				return 0;

			const iosize_t n_wrote = sink.Write(array.Data() + pos, n_want);
			pos += n_wrote;
			return n_wrote;
		}

		void Discard(const usys_t n_items) final override
		{
			EL_ERROR(Remaining() < n_items, stream::TStreamDryException);
			pos += n_items;
		}

		TArraySource(const TArraySource&) = delete;
		constexpr TArraySource(const array_t<const T> array EL_LIFETIME_BOUND) noexcept : array(array), pos(0) {}
	};

	template<typename T>
	system::waitable::TMemoryWaitable<usys_t> array_t<T>::MakeItemCountWaitable(const usys_t* const n_expected_count) const
	{
		return system::waitable::TMemoryWaitable<usys_t>(&n_items, n_expected_count, io::types::NEG1);
	}

	template<typename T>
	TArrayPipe<T> array_t<T>::Pipe() noexcept
	{
		return TArrayPipe<T>(arr_items, n_items);
	}

	template<typename T>
	TArrayPipe<const typename array_t<T>::value_t> array_t<T>::Pipe() const noexcept
	{
		return TArrayPipe<const value_t>(arr_items, n_items);
	}

	template<typename T>
	TArraySource<typename array_t<T>::value_t> array_t<T>::Source() const noexcept
	{
		return TArraySource<value_t>(AsConst());
	}

	template<typename T>
	void array_t<T>::Shift(const usys_t n_shift)
	{
		EL_ERROR(n_shift > n_items, TIndexOutOfBoundsException, 0, n_items, n_shift);
		arr_items += n_shift;
		n_items -= n_shift;
	}

	template<typename T>
	usys_t array_t<T>::AbsoluteIndex(const ssys_t rel_index, const bool allow_tail) const
	{
		if(rel_index >= 0)
		{
			EL_ERROR(rel_index > (ssys_t)n_items - (allow_tail ? 0 : 1), TIndexOutOfBoundsException, -(ssys_t)n_items, (ssys_t)n_items - 1, rel_index);
			return (usys_t)rel_index;
		}

		EL_ERROR(-rel_index > (ssys_t)n_items + (allow_tail ? 1 : 0), TIndexOutOfBoundsException, -(ssys_t)n_items, (ssys_t)n_items - 1, rel_index);

		if(-rel_index > (ssys_t)n_items && allow_tail)
			return n_items + rel_index + 1;
		return n_items + rel_index;
	}

	template<typename T>
	array_t<T> array_t<T>::Slice(const ssys_t index, const usys_t count) EL_LIFETIME_BOUND
	{
		const usys_t idx = AbsoluteIndex(index, true);
		const usys_t n_available = n_items - idx;
		const usys_t n_slice = count == NEG1 ? n_available : count;
		EL_ERROR(n_slice > n_available, TIndexOutOfBoundsException, 0, n_available, n_slice);
		return array_t<T>::FromUnsafePointer(idx == 0 ? arr_items : arr_items + idx, n_slice);
	}

	template<typename T>
	array_t<const typename array_t<T>::value_t> array_t<T>::Slice(const ssys_t index, const usys_t count) const EL_LIFETIME_BOUND
	{
		const usys_t idx = AbsoluteIndex(index, true);
		const usys_t n_available = n_items - idx;
		const usys_t n_slice = count == NEG1 ? n_available : count;
		EL_ERROR(n_slice > n_available, TIndexOutOfBoundsException, 0, n_available, n_slice);
		return array_t<const value_t>::FromUnsafePointer(idx == 0 ? arr_items : arr_items + idx, n_slice);
	}

	template<typename T>
	array_t<T> array_t<T>::Head(const usys_t count) EL_LIFETIME_BOUND
	{
		EL_ERROR(count > n_items, TIndexOutOfBoundsException, 0, n_items, count);
		return array_t<T>::FromUnsafePointer(arr_items, count);
	}

	template<typename T>
	array_t<const typename array_t<T>::value_t> array_t<T>::Head(const usys_t count) const EL_LIFETIME_BOUND
	{
		EL_ERROR(count > n_items, TIndexOutOfBoundsException, 0, n_items, count);
		return array_t<const value_t>::FromUnsafePointer(arr_items, count);
	}

	template<typename T>
	array_t<T> array_t<T>::Tail(const usys_t count) EL_LIFETIME_BOUND
	{
		EL_ERROR(count > n_items, TIndexOutOfBoundsException, 0, n_items, count);
		const usys_t idx = n_items - count;
		return array_t<T>::FromUnsafePointer(idx == 0 ? arr_items : arr_items + idx, count);
	}

	template<typename T>
	array_t<const typename array_t<T>::value_t> array_t<T>::Tail(const usys_t count) const EL_LIFETIME_BOUND
	{
		EL_ERROR(count > n_items, TIndexOutOfBoundsException, 0, n_items, count);
		const usys_t idx = n_items - count;
		return array_t<const value_t>::FromUnsafePointer(idx == 0 ? arr_items : arr_items + idx, count);
	}

	template<typename T>
	std::pair<array_t<T>, array_t<T>> array_t<T>::SplitAt(const ssys_t index) EL_LIFETIME_BOUND
	{
		const usys_t idx = AbsoluteIndex(index, true);
		return { array_t<T>::FromUnsafePointer(arr_items, idx), array_t<T>::FromUnsafePointer(idx == 0 ? arr_items : arr_items + idx, n_items - idx) };
	}

	template<typename T>
	std::pair<array_t<const typename array_t<T>::value_t>, array_t<const typename array_t<T>::value_t>> array_t<T>::SplitAt(const ssys_t index) const EL_LIFETIME_BOUND
	{
		const usys_t idx = AbsoluteIndex(index, true);
		return { array_t<const value_t>::FromUnsafePointer(arr_items, idx), array_t<const value_t>::FromUnsafePointer(idx == 0 ? arr_items : arr_items + idx, n_items - idx) };
	}

	template<typename T>
	template<typename L>
	void array_t<T>::ForEach(L lambda) const
	{
		for(usys_t i = 0; i < n_items; i++)
			lambda((const value_t&)arr_items[i]);
	}

	template<typename T>
	template<typename N, typename C>
	bool array_t<T>::Contains(const N& needle, C comparator) const
	{
		for(usys_t i = 0; i < n_items; i++)
			if(comparator(needle, arr_items[i]))
				return true;
		return false;
	}

	template<typename T>
	template<typename C>
	usys_t array_t<T>::FindFirst(const value_t& needle, C comparator) const
	{
		for(usys_t i = 0; i < n_items; i++)
			if(comparator(needle, arr_items[i]))
				return i;
		return NEG1;
	}

	template<typename T>
	template<typename C>
	usys_t array_t<T>::BinarySearch(C comparator, const bool closest_match, usys_t idx_start, usys_t n_items_search) const
	{
		usys_t idx_pivot = NEG1;

		while(n_items_search != 0)
		{
			idx_pivot = idx_start + n_items_search / 2;

			const value_t& item = arr_items[idx_pivot];
			const int comp_result = comparator(item);

			if(comp_result == 0)
				return idx_pivot;

			if(comp_result < 0)
			{
				idx_start = idx_pivot + 1;
				n_items_search--;
				n_items_search /= 2;
			}
			else
			{
				n_items_search /= 2;
			}
		}

		return closest_match ? idx_pivot : NEG1;
	}

	template<typename T>
	template<typename C>
	usys_t array_t<T>::BinarySearch(C comparator, const bool closest_match) const
	{
		return BinarySearch(comparator, closest_match, 0, n_items);
	}

	template<typename T>
	void array_t<T>::Reverse() requires (!std::is_const_v<T>)
	{
		if(n_items <= 1)
			return;

		for(usys_t i = 0; i < n_items / 2; i++)
			util::Swap(arr_items[i], arr_items[n_items - 1 - i]);
	}

	template<typename T>
	template<typename L>
	void array_t<T>::Apply(L lambda) requires (!std::is_const_v<T>)
	{
		for(usys_t i = 0; i < n_items; i++)
			lambda(arr_items[i]);
	}

	template<typename T>
	template<typename S>
	void array_t<T>::Sort(const ESortOrder order, S sorter) requires (!std::is_const_v<T>)
	{
		if(n_items <= 1)
			return;

		auto item_should_follow = [order, sorter](const value_t& a, const value_t& b) -> bool
		{
			const int result = sorter(a, b);
			return order == ESortOrder::DESCENDING ? result < 0 : result > 0;
		};

		auto sift_down = [this, item_should_follow](usys_t idx_root, const usys_t idx_end)
		{
			while(true)
			{
				const usys_t idx_child_left = idx_root * 2 + 1;
				if(idx_child_left >= idx_end)
					break;

				usys_t idx_swap = idx_root;
				if(item_should_follow(arr_items[idx_child_left], arr_items[idx_swap]))
					idx_swap = idx_child_left;

				const usys_t idx_child_right = idx_child_left + 1;
				if(idx_child_right < idx_end && item_should_follow(arr_items[idx_child_right], arr_items[idx_swap]))
					idx_swap = idx_child_right;

				if(idx_swap == idx_root)
					break;

				util::Swap(arr_items[idx_root], arr_items[idx_swap]);
				idx_root = idx_swap;
			}
		};

		for(usys_t idx_start = n_items / 2; idx_start > 0; idx_start--)
			sift_down(idx_start - 1, n_items);

		for(usys_t idx_end = n_items; idx_end > 1; idx_end--)
		{
			util::Swap(arr_items[0], arr_items[idx_end - 1]);
			sift_down(0, idx_end - 1);
		}
	}

	template<typename T>
	T* array_t<T>::ItemPtr(const usys_t index) noexcept
	{
		return index < n_items ? arr_items + index : nullptr;
	}

	template<typename T>
	const typename array_t<T>::value_t* array_t<T>::ItemPtr(const usys_t index) const noexcept
	{
		return index < n_items ? arr_items + index : nullptr;
	}
}


// Compatibility: list historically exported array_t and its related helpers.
// Keep old qualified names available while the canonical namespace is array.
namespace el1::io::collection::list
{
	using namespace io::collection::array;
}
