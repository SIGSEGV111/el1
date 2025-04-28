#pragma once

#include "io_types.hpp"
#include "io_stream.hpp"
#include "system_handle.hpp"
#include "util.hpp"

namespace el1::system::random
{
	using namespace io::types;

	struct IRNG;
	class TCMWC;
	class TXorShift;

	struct IRNG : io::stream::ISource<byte_t>
	{
		// full range of return type
		template<typename T>
		T Integer();

		// full range of arg type
		template<typename T>
		void Integer(T&);

		template<typename T>
		T IntegerRange(const T upper);

		template<typename T>
		T IntegerRange(const T lower, const T upper);

		// range -1.0 to +1.0 (allow_negative=true)
		// or 0.0 to +1.0 (allow_negative=false)
		double Float(const bool allow_negative = false);
	};

	// secure random number generator - possibly slow
	class TSystemRandom : public IRNG
	{
		protected:
			io::stream::TKernelStream stream;
			TSystemRandom();

		public:
			virtual usys_t Read(byte_t* const arr_items, const usys_t n_items_max) final override;
			virtual usys_t BlockingRead(byte_t* const arr_items, const usys_t n_items_max, system::time::TTime timeout = -1, const bool absolute_time = false) final override;
			virtual void ReadAll(byte_t* const arr_items, const usys_t n_items) final override;

			static TSystemRandom& Instance();
	};

	class TCMWC : public IRNG, public io::stream::IPipe<TCMWC, u32_t>
	{
		protected:
			static const u32_t PHI = 0x9e3779b9;

			u32_t q[4096];
			u32_t c;
			u32_t i;
			u32_t tmp;

		public:
			virtual usys_t Read(byte_t* const arr_items, const usys_t n_items_max) final override;
			virtual usys_t BlockingRead(byte_t* const arr_items, const usys_t n_items_max, system::time::TTime timeout = -1, const bool absolute_time = false) final override;
			virtual void ReadAll(byte_t* const arr_items, const usys_t n_items) final override;

			using IPipe<TCMWC, u32_t>::Read;
			using IPipe<TCMWC, u32_t>::BlockingRead;
			using IPipe<TCMWC, u32_t>::ReadAll;

			u32_t* NextItem() final override;

			u32_t Generate();

			TCMWC(const u32_t seed);
			TCMWC(IRNG& init = TSystemRandom::Instance());
	};

	class TXorShift : public IRNG, public io::stream::IPipe<TXorShift, u64_t>
	{
		protected:
			u64_t x;
			u64_t y;
			u64_t z;
			u64_t tmp;

		public:
			virtual usys_t Read(byte_t* const arr_items, const usys_t n_items_max) final override;
			virtual usys_t BlockingRead(byte_t* const arr_items, const usys_t n_items_max, system::time::TTime timeout = -1, const bool absolute_time = false) final override;
			virtual void ReadAll(byte_t* const arr_items, const usys_t n_items) final override;

			using IPipe<TXorShift, u64_t>::Read;
			using IPipe<TXorShift, u64_t>::BlockingRead;
			using IPipe<TXorShift, u64_t>::ReadAll;

			u64_t* NextItem() final override;

			u64_t Generate();

			TXorShift(const u64_t seed);
			TXorShift(IRNG& init = TSystemRandom::Instance());
	};

	/******************************************************/

	template<typename T>
	T IRNG::Integer()
	{
		T v;
		this->ReadAll((byte_t*)&v, sizeof(v));
		return v;
	}

	template<typename T>
	void IRNG::Integer(T& v)
	{
		this->ReadAll((byte_t*)&v, sizeof(v));
	}

	template<typename T>
	T IRNG::IntegerRange(const T upper)
	{
		const int h = util::FindLastSet((u64_t)upper);
		const T mask = ((T)1 << h) - 1;

		T value;
		while((value = Integer<T>() & mask) > upper);

		return value;
	}

	template<typename T>
	T IRNG::IntegerRange(const T lower, const T upper)
	{
		EL_ERROR(upper < lower, TInvalidArgumentException, "upper", "upper must be bigger than lower");
		const T len = upper - lower;
		return IntegerRange<T>(len) + lower;
	}
}
