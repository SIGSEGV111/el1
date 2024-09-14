#pragma once

#include "def.hpp"
#include "math_vector.hpp"
#include "dev_i2c.hpp"
#include "dev_gpio.hpp"
#include "error.hpp"

namespace el1::io::text::string
{
	class TString;
}

namespace el1::dev::i2c::mpu6050
{
	using namespace io::types;

	enum class EGyroFSR : u8_t
	{
		// DPS = °/s (degree per second)
		DPS_250 = 0,
		DPS_500 = 1,
		DPS_1000 = 2,
		DPS_2000 = 3
	};

	enum class EAccelFSR : u8_t
	{
		G2 = 0,
		G4 = 1,
		G8 = 2,
		G16 = 3
	};

	enum class EIRQDrive
	{
		OPEN_DRAIN,
		PUSH_PULL
	};

	enum class EIRQActive
	{
		LOW,
		HIGH
	};

	using v3f_t = math::vector::vector_t<float, 3>;

	struct data_t
	{
		v3f_t accel;// m/s²
		float temp;	// °C
		v3f_t gyro;	// °/s
	};

	static const float G = 9.80665f;	// 1G in m/s²

	// call ResetFifo() to recover
	struct TFifoOverflowException : error::IException
	{
		io::text::string::TString Message() const final override;
		error::IException* Clone() const override;
	};

	class TMPU6050 : public io::stream::ISource<data_t>
	{
		protected:
			std::unique_ptr<i2c::II2CDevice> device;
			std::unique_ptr<gpio::IPin> irq;
			u8_t	overflow : 1,
					accel : 1,
					gx : 1,
					gy : 1,
					gz : 1,
					temp : 1;
			u8_t fsr_gyro;
			u16_t fsr_accel;

			u16_t UpdateStatus();
			u16_t ReadFifoCount() const;
			float ConvertAccel(const s16_t raw) const noexcept;
			float ConvertGyro(const s16_t raw) const noexcept;
			float ConvertTemp(const s16_t raw) const noexcept;

		public:
			// gyro base clock without LowPassFilter: 8KHz
			// gyro base clock with    LowPassFilter: 1KHz
			// SAMPLE_RATE = BASE_CLOCK / (value + 1)
			void GyroSampleRateDivider(const u8_t value);
			u8_t GyroSampleRateDivider() const;

			// range: 0-6 (0 = disabled, all other values are enabled in increasing strength)
			// for details see "RS-MPU-6000A-00.pdf" page 13, "DLPF_CFG" register
			void LowPassFilter(const u8_t value);
			u8_t LowPassFilter() const;

			void GyroFullScaleRange(const EGyroFSR value);
			EGyroFSR GyroFullScaleRange() const;
			void AccelFullScaleRange(const EAccelFSR value);
			EAccelFSR AccelFullScaleRange() const;
			void EnabledSensors(const bool gyro_x, const bool gyro_y, const bool gyro_z, const bool accel, const bool temp);

			// call ResetFifo() to recover after catching TFifoOverflowException
			// and after configuration changes (SampleRateDivider, LowPassFilter, *FullScaleRange, EnabledSensors, etc.)
			void ResetFifo();

			usys_t Read(data_t* const arr_items, const usys_t n_items_max) final override EL_WARN_UNUSED_RESULT;
			const system::waitable::IWaitable* OnInputReady() const final override { return &irq->OnInputTrigger(); }

			TMPU6050(std::unique_ptr<i2c::II2CDevice> device, std::unique_ptr<gpio::IPin> irq, const EIRQDrive irq_drive, const EIRQActive irq_active);
			~TMPU6050();
	};
}
