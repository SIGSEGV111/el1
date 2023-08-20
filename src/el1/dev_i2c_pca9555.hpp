#pragma once

#include "def.hpp"
#include "dev_i2c.hpp"
#include "dev_gpio.hpp"
#include "system_task.hpp"
#include "system_waitable.hpp"

namespace el1::dev::i2c::pca9555
{
	using namespace io::types;

	class TPin;
	class TPCA9555;

	class TPin : public gpio::IPin
	{
		protected:
			TPCA9555* const controller;
			system::waitable::TMemoryWaitable<u8_t> waitable;
			const u8_t index;
			bool auto_commit;
			gpio::ETrigger trigger;
			mutable u8_t state_ref;

		public:
			gpio::IController* Controller() const final override EL_GETTER;

			// auto-commit is on by default, but should be disabled for performance reasons if
			// your application updates multiple pins in quick succession
			void AutoCommit(const bool) final override EL_SETTER;

			bool State() const final override EL_GETTER;
			void State(const bool) final override EL_SETTER;

			// only INPUT and OUTPUT is supported
			gpio::EMode Mode() const final override EL_GETTER;
			void Mode(const gpio::EMode) final override EL_SETTER;

			// PCA9555 has no pin alternate functions
			// these functions always throw
			void AlternateFunction(const int func) final override EL_SETTER;
			int AlternateFunction() const final override EL_GETTER;

			// OnInputTrigger() is supported when a IRQ pin is provided to PCA9555
			// trigger can be any value, however this is implemented as
			// a filter in software - the hardware always triggers on BOTH edges
			gpio::ETrigger Trigger() const final override EL_GETTER;
			void Trigger(const gpio::ETrigger) final override EL_SETTER;

			// PCA9555 has a hardwired 100 KOhm pull-up
			// so pull is always EPull::UP and no other value is allowed to be set
			gpio::EPull Pull() const final override EL_GETTER;
			void Pull(const gpio::EPull) final override EL_SETTER;

			// OnInputTrigger() is supported when a IRQ pin is provided to PCA9555
			system::waitable::TMemoryWaitable<u8_t>& OnInputTrigger() final override EL_GETTER;

			TPin(TPCA9555* const controller, const u8_t index);
			~TPin();
	};

	static const u8_t REG_INPUT = 0;
	static const u8_t REG_OUTPUT = 2;
	static const u8_t REG_POLARITY = 4;
	static const u8_t REG_CONFIG = 6;

	class TPCA9555 : public gpio::IController
	{
		friend class TPin;
		protected:
			std::unique_ptr<II2CDevice> device;
			std::unique_ptr<gpio::IPin> irq;

			system::task::TFiber fiber;

			struct registers_t
			{
				union
				{
					u8_t arr[8];

					struct
					{
						u16_t input;
						u16_t output;
						u16_t polarity;
						u16_t config;
					};

					struct
					{
						u8_t arr_input[2];
						u8_t arr_output[2];
						u8_t arr_polarity[2];
						u8_t arr_config[2];
					};
				};
			};

			struct write_reg_t
			{
				const u8_t id;
				const u16_t value;
			} EL_PACKED;

			registers_t reg_ref;
			registers_t reg_new;
			u16_t claimed;

			void WriteRegister(const write_reg_t wr);

			void IrqMain();

		public:
			// "irq" can be nullptr, but then IPin::OnInputTrigger() and Trigger() will not work and instead throw an exception
			// the GPIO pin provided as "irq" must support OnInputTrigger() with ETrigger::FALLING_EDGE and either have a
			// hardwired pull-up or support EPull::UP - this must be configured correctly before passing the GPIO to TPCA9555
			TPCA9555(std::unique_ptr<II2CDevice> device, std::unique_ptr<gpio::IPin> irq);

			std::unique_ptr<gpio::IPin> ClaimPin(const usys_t index) final override;

			void Commit() final override;
			void Poll() final override;
			bool NeedCommit() const final override EL_GETTER { return true; }
	};
}
