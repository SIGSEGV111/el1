#include "dev_i2c.hpp"
#include <string.h>

namespace el1::dev::i2c
{
	u8_t  II2CDevice::ReadByteRegister(const u8_t regaddr)
	{
		WriteAll(&regaddr, 1);
		u8_t value;
		ReadAll((byte_t*)&value, sizeof(value));
		return value;
	}

	u16_t II2CDevice::ReadWordRegister(const u8_t regaddr)
	{
		WriteAll(&regaddr, 1);
		u16_t value;
		ReadAll((byte_t*)&value, sizeof(value));
		return value;
	}

	u32_t II2CDevice::ReadLongRegister(const u8_t regaddr)
	{
		WriteAll(&regaddr, 1);
		u32_t value;
		ReadAll((byte_t*)&value, sizeof(value));
		return value;
	}

	void  II2CDevice::ReadRegisterArray(const u8_t regaddr_start, const u8_t n_bytes, byte_t* const arr_bytes)
	{
		WriteAll(&regaddr_start, 1);
		ReadAll(arr_bytes, n_bytes);
	}

	void II2CDevice::WriteByteRegister(const u8_t regaddr, const u8_t  value)
	{
		byte_t arr[1 + sizeof(value)];
		arr[0] = regaddr;
		memcpy(arr + 1, &value, sizeof(value));
		WriteAll(arr, sizeof(arr));
	}

	void II2CDevice::WriteWordRegister(const u8_t regaddr, const u16_t value)
	{
		byte_t arr[1 + sizeof(value)];
		arr[0] = regaddr;
		memcpy(arr + 1, &value, sizeof(value));
		WriteAll(arr, sizeof(arr));
	}

	void II2CDevice::WriteLongRegister(const u8_t regaddr, const u32_t value)
	{
		byte_t arr[1 + sizeof(value)];
		arr[0] = regaddr;
		memcpy(arr + 1, &value, sizeof(value));
		WriteAll(arr, sizeof(arr));
	}

	void II2CDevice::WriteRegisterArray(const u8_t regaddr_start, const u8_t n_bytes, const byte_t* const arr_bytes)
	{
		byte_t arr[1 + n_bytes];
		arr[0] = regaddr_start;
		memcpy(arr + 1, arr_bytes, n_bytes);
		WriteAll(arr, 1 + n_bytes);
	}
}
