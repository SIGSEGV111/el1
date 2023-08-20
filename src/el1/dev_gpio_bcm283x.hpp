#pragma once

#include "def.hpp"
#ifdef EL_OS_LINUX

#include "system_task.hpp"
#include "system_handle.hpp"
#include "io_file.hpp"
#include "dev_gpio.hpp"

/*
 * BCM283x GPIO driver
 *
 * This is a very fast interface to the BCM283x/BCM2711 GPIO subsystem.
 * It uses direct register access and circumvents the linux kernel drivers.
 * This has massive advantages when it comes to speed, however also has some disadvantages:
 * - it does not support interrupts/waitables (OnInputTrigger/WaitFor and friends)
 * - it requres permissions to access /dev/mem (usually only root can access this)
 * - there is no synchronisation/protection between this driver and the kernel driver
 *
 * supports Raspberry Pi Models 1 to 4 (including all their models and variants)
 * - Raspberry Pi 1 (all models)
 * - Raspberry Pi 2 (all models)
 * - Raspberry Pi 3 (all models)
 * - Raspberry Pi 4 (all variants)
 * - Raspberry Pi Zero (all variants)
 * - Raspberry Pi Zero 2 (all variants)
 * - Raspberry Pi Compute Module 1 (all variants)
 * - Raspberry Pi Compute Module 3 (all variants)
 * - Raspberry Pi Compute Module 3+ (all variants)
 * - Raspberry Pi Compute Module 4
 * - Raspberry Pi 400
*/

namespace el1::dev::gpio::bcm283x
{
	using namespace system::task;
	using namespace system::handle;
	using namespace io::file;

	class TBCM283X;

	enum class ECPU : u8_t
	{
		BCM283X = 0,
		BCM2711 = 1
	};

	struct model_config_t
	{
		const char* const name;
		const usys_t addr;
		ECPU cpu;
	};

	static const u8_t NUM_GPIO = 54;

	class TPin : public IPin
	{
		friend class TBCM283X;

		protected:
			TBCM283X* const controller;
			const u8_t index;
			EPull pull;

			TPin(TBCM283X* const controller, const u8_t index);

		public:
			IController* Controller() const final override EL_GETTER;
			void AutoCommit(const bool) final override EL_SETTER;

			bool State() const final override;
			void State(const bool) final override EL_SETTER;

			EMode Mode() const final override;
			void Mode(const EMode) final override EL_SETTER;

			void AlternateFunction(const int func) final override EL_SETTER;
			int AlternateFunction() const final override EL_GETTER;

			ETrigger Trigger() const final override EL_GETTER;
			void Trigger(const ETrigger) final override EL_SETTER;

			EPull Pull() const final override EL_GETTER;
			void Pull(const EPull) final override EL_SETTER;

			THandleWaitable& OnInputTrigger() final override;

			~TPin();
	};

	class TBCM283X : public IController
	{
		friend class TPin;

		protected:
			u64_t claimed_pins;
			const model_config_t* const model;
			TFile file;
			TMapping map;

			TBCM283X();

		public:
			const model_config_t& Model() const EL_GETTER { return *model; }

			std::unique_ptr<IPin> ClaimPin(const usys_t index) final override;

			// bypasses TPin logic, do not use unless you know what your are doing
			void _PullResistor(const unsigned index, const EPull pull);

			static TBCM283X* Instance();
	};
}

#endif
