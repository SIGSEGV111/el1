#include "dev_i2c_mpu6050.hpp"
#include "io_text_string.hpp"
#include "system_task.hpp"
#include <endian.h>
#include <math.h>

namespace el1::dev::i2c::mpu6050
{
	using namespace error;
	using namespace io::text::string;
	using namespace system::task;

	static const u32_t MPU6050_MAX_CLOCK = 400000;

	static const u8_t REG_SMPRT_DIV = 0x19;
	static const u8_t REG_CONFIG = 0x1a;
	static const u8_t REG_GYRO_CONFIG = 0x1b;
	static const u8_t REG_ACCEL_CONFIG = 0x1c;
	static const u8_t REG_FIFO_EN = 0x23;
	static const u8_t REG_I2C_MST_CTRL = 0x24;

	static const u8_t REG_I2C_SLV0_ADDR = 0x25;
	static const u8_t REG_I2C_SLV0_REG  = 0x26;
	static const u8_t REG_I2C_SLV0_CTRL = 0x27;

	static const u8_t REG_I2C_SLV1_ADDR = 0x28;
	static const u8_t REG_I2C_SLV1_REG  = 0x29;
	static const u8_t REG_I2C_SLV1_CTRL = 0x2A;

	static const u8_t REG_I2C_SLV2_ADDR = 0x2B;
	static const u8_t REG_I2C_SLV2_REG  = 0x2C;
	static const u8_t REG_I2C_SLV2_CTRL = 0x2D;

	static const u8_t REG_I2C_SLV3_ADDR = 0x2E;
	static const u8_t REG_I2C_SLV3_REG  = 0x2F;
	static const u8_t REG_I2C_SLV3_CTRL = 0x30;

	static const u8_t REG_I2C_SLV4_ADDR = 0x31;
	static const u8_t REG_I2C_SLV4_REG  = 0x32;
	static const u8_t REG_I2C_SLV4_DO   = 0x33;
	static const u8_t REG_I2C_SLV4_CTRL = 0x34;
	static const u8_t REG_I2C_SLV4_DI   = 0x35;

	static const u8_t REG_INT_PIN_CFG   = 0x37;
	static const u8_t REG_INT_ENABLE    = 0x38;
	static const u8_t REG_INT_STATUS    = 0x3A;
	static const u8_t REG_USER_CTRL     = 0x6A;
	static const u8_t REG_PWR_MGMT      = 0x6B;
	static const u8_t REG_PWR_MGMT_2    = 0x6C;
	static const u8_t REG_SIGNAL_PATH_RESET = 0x68;

	static const u8_t REG_FIFO_COUNT_H  = 0x72;
	static const u8_t REG_FIFO_COUNT_L  = 0x73;
	static const u8_t REG_FIFO_R_W      = 0x74;

	TString TFifoOverflowException::Message() const
	{
		return L"MPU6050 on-chip FIFO overflow";
	}

	IException* TFifoOverflowException::Clone() const
	{
		return new TFifoOverflowException(*this);
	}

	void TMPU6050::GyroSampleRateDivider(const u8_t v)
	{
		this->device->WriteByteRegister(REG_SMPRT_DIV, v);
	}

	u8_t TMPU6050::GyroSampleRateDivider() const
	{
		return this->device->ReadByteRegister(REG_SMPRT_DIV);
	}

	void TMPU6050::LowPassFilter(const u8_t v)
	{
		this->device->WriteByteRegister(REG_CONFIG, v & 0b00000111);
	}

	u8_t TMPU6050::LowPassFilter() const
	{
		return this->device->ReadByteRegister(REG_CONFIG) & 0b00000111;
	}

	void TMPU6050::GyroFullScaleRange(const EGyroFSR v)
	{
		this->device->WriteByteRegister(REG_GYRO_CONFIG, ((u8_t)v & 0b00000011) << 3);
		fsr_gyro = 1 << (u8_t)v;
	}

	EGyroFSR TMPU6050::GyroFullScaleRange() const
	{
		return (EGyroFSR)((this->device->ReadByteRegister(REG_GYRO_CONFIG) & 0b00011000) >> 3);
	}

	void TMPU6050::AccelFullScaleRange(const EAccelFSR v)
	{
		this->device->WriteByteRegister(REG_ACCEL_CONFIG, ((u8_t)v & 0b00000011) << 3);
		fsr_accel = (u16_t)16384 >> (u8_t)v;
	}

	EAccelFSR TMPU6050::AccelFullScaleRange() const
	{
		return (EAccelFSR)((this->device->ReadByteRegister(REG_ACCEL_CONFIG) & 0b00011000) >> 3);
	}

	u16_t TMPU6050::ReadFifoCount() const
	{
		const u16_t high = this->device->ReadByteRegister(REG_FIFO_COUNT_H);
		const u16_t low  = this->device->ReadByteRegister(REG_FIFO_COUNT_L);
		return (high << 8) | low;
	}

	u16_t TMPU6050::UpdateStatus()
	{
		const u8_t irq_status = this->device->ReadByteRegister(REG_INT_STATUS);
		this->overflow = (irq_status & 0b00010000) ? 1 : 0;

		if(irq_status & 0b00000001) // data ready
			return ReadFifoCount();
		else
			return 0;
	}

	void TMPU6050::ResetFifo()
	{
		this->device->WriteByteRegister(REG_USER_CTRL, 0b00000111);	// reset FIFO, data reg, i2c master
		this->device->WriteByteRegister(REG_USER_CTRL, 0b01000000);	// enable FIFO, disable i2c master
		this->irq->State();	// clear IRQ
		this->overflow = 0;
	}

	float TMPU6050::ConvertAccel(const s16_t raw) const noexcept
	{
		return ((float)raw / (float)fsr_accel) * G;
	}

	float TMPU6050::ConvertGyro(const s16_t raw) const noexcept
	{
		return (float)raw / (131.0f / (float)fsr_gyro);
	}

	float TMPU6050::ConvertTemp(const s16_t raw) const noexcept
	{
		return (float)raw / 340.0f + 36.53f;
	}

	usys_t TMPU6050::Read(data_t* const arr_items, const usys_t n_items_max)
	{
		this->irq->State(); // clear IRQ
		const u16_t n_fifo_bytes = UpdateStatus();
		EL_ERROR(overflow, TFifoOverflowException);

		const usys_t n_wpr = this->temp + this->accel * 3 + this->gx + this->gy + this->gz;
		const usys_t n_items_available = n_fifo_bytes / (n_wpr*2);
		const usys_t n_read_items = util::Min(n_items_max, n_items_available);

		u16_t buffer[n_read_items * n_wpr];
		this->device->ReadRegisterArray(REG_FIFO_R_W, n_read_items * (n_wpr*2), (byte_t*)buffer);

		for(usys_t i = 0; i < n_read_items; i++)
		{
			unsigned w = 0;
			if(this->accel)
			{
				arr_items[i].accel.v[0] = ConvertAccel(be16toh(buffer[i * n_wpr + w++]));
				arr_items[i].accel.v[1] = ConvertAccel(be16toh(buffer[i * n_wpr + w++]));
				arr_items[i].accel.v[2] = ConvertAccel(be16toh(buffer[i * n_wpr + w++]));
			}
			else
			{
				arr_items[i].accel.v[0] = NAN;
				arr_items[i].accel.v[1] = NAN;
				arr_items[i].accel.v[2] = NAN;
			}

			arr_items[i].temp       = this->temp ? ConvertTemp (be16toh(buffer[i * n_wpr + w++])) : NAN;
			arr_items[i].gyro.v[0]  = this->gx   ? ConvertGyro (be16toh(buffer[i * n_wpr + w++])) : NAN;
			arr_items[i].gyro.v[1]  = this->gy   ? ConvertGyro (be16toh(buffer[i * n_wpr + w++])) : NAN;
			arr_items[i].gyro.v[2]  = this->gz   ? ConvertGyro (be16toh(buffer[i * n_wpr + w++])) : NAN;
		}

		return n_read_items;
	}

	void TMPU6050::EnabledSensors(const bool gyro_x, const bool gyro_y, const bool gyro_z, const bool accel, const bool temp)
	{
		const u8_t mask_temp  = 0b10000000;
		const u8_t mask_gx    = 0b01000000;
		const u8_t mask_gy    = 0b00100000;
		const u8_t mask_gz    = 0b00010000;
		const u8_t mask_accel = 0b00001000;
		const u8_t v = (gyro_x ? mask_gx : 0)
					 | (gyro_y ? mask_gy : 0)
					 | (gyro_z ? mask_gz : 0)
					 | (accel  ? mask_accel : 0)
					 | (temp   ? mask_temp : 0);

		this->device->WriteByteRegister(REG_FIFO_EN, v);
		this->accel = accel ? 1 : 0;
		this->temp = temp ? 1 : 0;
		this->gx = gyro_x ? 1 : 0;
		this->gy = gyro_y ? 1 : 0;
		this->gz = gyro_z ? 1 : 0;
	}

	TMPU6050::TMPU6050(std::unique_ptr<i2c::II2CDevice> device, std::unique_ptr<gpio::IPin> irq, const EIRQDrive irq_drive, const EIRQActive irq_active)
		: device(std::move(device)), irq(std::move(irq)), overflow(1), fsr_gyro(0), fsr_accel(0)
	{
		this->irq->Mode(gpio::EMode::INPUT);
		this->device->WriteByteRegister(REG_PWR_MGMT, 0b10000000);	// device reset, internal clock
		for(unsigned i = 0; i < 100 && (this->device->ReadByteRegister(REG_PWR_MGMT) & 0b10000000 ) != 0; i++)
			TFiber::Sleep(0.01); // 10ms

		this->device->WriteByteRegister(REG_SIGNAL_PATH_RESET, 0b00000111);
		TFiber::Sleep(0.1);

		// this->device->WriteByteRegister(REG_PWR_MGMT, 0b00000000);	// internal clock
		this->device->WriteByteRegister(REG_PWR_MGMT, 0b00000001);	// select X-gyro as clock-cource

		this->device->WriteByteRegister(REG_INT_PIN_CFG,
			(irq_active == EIRQActive::LOW ?      0b10000000 : 0) |
			(irq_drive == EIRQDrive::OPEN_DRAIN ? 0b01000000 : 0) |
			0b00100010 // LATCH_INT_EN=1, INT_RD_CLEAR=0, I2C_BYPASS_EN=1
		);

		GyroSampleRateDivider(7);
		LowPassFilter(0);
		GyroFullScaleRange(EGyroFSR::DPS_2000);
		AccelFullScaleRange(EAccelFSR::G16);

		this->device->WriteByteRegister(REG_INT_ENABLE, 0b00010001); // FIFO_OFLOW_EN=1, DATA_RDY_EN=1
		EnabledSensors(true, true, true, true, true);

		ResetFifo();
		UpdateStatus();

		this->irq->Trigger(irq_active == EIRQActive::LOW ? gpio::ETrigger::FALLING_EDGE : gpio::ETrigger::RISING_EDGE);
		this->irq->State(); // clear IRQ

		/*
		this->device->WriteByteRegister(REG_I2C_MST_CTRL, 0x00);

		// slave 0
		this->device->WriteByteRegister(REG_I2C_SLV0_CTRL, 0x00);
		this->device->WriteByteRegister(REG_I2C_SLV0_ADDR, 0x00);
		this->device->WriteByteRegister(REG_I2C_SLV0_REG,  0x00);

		// slave 1
		this->device->WriteByteRegister(REG_I2C_SLV1_CTRL, 0x00);
		this->device->WriteByteRegister(REG_I2C_SLV1_ADDR, 0x00);
		this->device->WriteByteRegister(REG_I2C_SLV1_REG,  0x00);

		// slave 2
		this->device->WriteByteRegister(REG_I2C_SLV2_CTRL, 0x00);
		this->device->WriteByteRegister(REG_I2C_SLV2_ADDR, 0x00);
		this->device->WriteByteRegister(REG_I2C_SLV2_REG,  0x00);

		// slave 3
		this->device->WriteByteRegister(REG_I2C_SLV3_CTRL, 0x00);
		this->device->WriteByteRegister(REG_I2C_SLV3_ADDR, 0x00);
		this->device->WriteByteRegister(REG_I2C_SLV3_REG,  0x00);

		// slave 4
		this->device->WriteByteRegister(REG_I2C_SLV4_CTRL, 0x00);
		this->device->WriteByteRegister(REG_I2C_SLV4_ADDR, 0x00);
		this->device->WriteByteRegister(REG_I2C_SLV4_REG,  0x00);
		this->device->WriteByteRegister(REG_I2C_SLV4_DO,   0x00);
		this->device->WriteByteRegister(REG_I2C_SLV4_DI,   0x00);
		*/
	}

	TMPU6050::~TMPU6050()
	{
		try { this->device->WriteByteRegister(REG_PWR_MGMT, 0b01000000); } catch(...) {}
	}
}
