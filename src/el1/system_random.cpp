#include "system_random.hpp"
#include <string.h>

namespace el1::system::random
{
	static std::unique_ptr<TSystemRandom> sysrnd;

	TSystemRandom& TSystemRandom::Instance()
	{
		if(sysrnd == nullptr)
			sysrnd = std::unique_ptr<TSystemRandom>(new TSystemRandom());
		return *sysrnd;
	}

	usys_t TSystemRandom::Read(byte_t* const arr_items, const usys_t n_items_max)
	{
		return file.Read(arr_items, n_items_max);
	}

	usys_t TSystemRandom::BlockingRead(byte_t* const arr_items, const usys_t n_items_max)
	{
		return file.BlockingRead(arr_items, n_items_max);
	}

	void TSystemRandom::ReadAll(byte_t* const arr_items, const usys_t n_items)
	{
		file.ReadAll(arr_items, n_items);
	}

	/***************************************************/

	usys_t TCMWC::Read(byte_t* const arr_items, const usys_t n_items_max)
	{
		this->ReadAll(arr_items, n_items_max);
		return n_items_max;
	}

	usys_t TCMWC::BlockingRead(byte_t* const arr_items, const usys_t n_items_max)
	{
		this->ReadAll(arr_items, n_items_max);
		return n_items_max;
	}

	void TCMWC::ReadAll(byte_t* const arr_items, const usys_t n_items)
	{
		const usys_t n_blocks = n_items / 4U;
		for(usys_t i = 0; i < n_blocks; i++)
			reinterpret_cast<u32_t*>(arr_items)[i] = this->Generate();

		const usys_t n_remaining = n_items - n_blocks * 4U;
		if(n_remaining > 0)
		{
			const u32_t block = this->Generate();
			memcpy(arr_items + n_items - n_remaining, &block, n_remaining);
		}
	}

	u32_t TCMWC::Generate()
	{
		u64_t t, a = 18782LL;
		u32_t x, r = 0xfffffffe;
		i = (i + 1) & 4095;
		t = a * q[i] + c;
		c = (t >> 32);
		x = ((u32_t)t) + c;
		if (x < c)
		{
			x++;
			c++;
		}
		return (q[i] = r - x);
	}

	const u32_t* TCMWC::NextItem()
	{
		tmp = Generate();
		return &tmp;
	}

	TCMWC::TCMWC(const u32_t seed)
	{
		q[0] = seed;
		q[1] = seed + PHI;
		q[2] = seed + PHI + PHI;

		for(unsigned j = 3; j < 4096; j++)
			q[j] = q[j - 3] ^ q[j - 2] ^ PHI ^ j;

		c = 18781;
		i = 4095;

		for(unsigned i = 0; i < 4096; i++)
			Generate();
	}

	TCMWC::TCMWC(IRNG& init)
	{
		init.ReadAll((byte_t*)q, sizeof(q));
		c = 18781;
		i = 4095;
	}

	/***************************************************/

	usys_t TXorShift::Read(byte_t* const arr_items, const usys_t n_items_max)
	{
		this->ReadAll(arr_items, n_items_max);
		return n_items_max;
	}

	usys_t TXorShift::BlockingRead(byte_t* const arr_items, const usys_t n_items_max)
	{
		this->ReadAll(arr_items, n_items_max);
		return n_items_max;
	}

	void TXorShift::ReadAll(byte_t* const arr_items, const usys_t n_items)
	{
		const usys_t n_blocks = n_items / 8U;
		for(usys_t i = 0; i < n_blocks; i++)
			reinterpret_cast<u64_t*>(arr_items)[i] = this->Generate();

		const usys_t n_remaining = n_items - n_blocks * 8U;
		if(n_remaining > 0)
		{
			const u64_t block = this->Generate();
			memcpy(arr_items + n_items - n_remaining, &block, n_remaining);
		}
	}

	const u64_t* TXorShift::NextItem()
	{
		tmp = Generate();
		return &tmp;
	}

	u64_t TXorShift::Generate()
	{
		u64_t t;

		x ^= x << 16;
		x ^= x >> 5;
		x ^= x << 1;

		t = x;
		x = y;
		y = z;
		z = t ^ x ^ y;

		return z;
	}

	TXorShift::TXorShift(const u64_t seed)
	{
		x = seed;
		y = ~seed;
		z = x + y;
	}

	TXorShift::TXorShift(IRNG& init)
	{
		x = init.Integer<u64_t>();
		y = init.Integer<u64_t>();
		z = init.Integer<u64_t>();
	}
}
