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

	enum class ESOC : u8_t
	{
		BCM283X = 0,
		BCM2711 = 1,
		BCM2712 = 2
	};

	enum class ECPU : u8_t
	{
		ARM1176JZF_S = 0,   // BCM2835: ARM11
		CortexA7     = 1,   // BCM2836: ARMv7
		CortexA53    = 2,   // BCM2837: ARMv8
		CortexA72    = 3,   // BCM2711: ARMv8
		CortexA76    = 4    // BCM2712: ARMv8.2
	};

	struct model_config_t
	{
		const char* const name;
		const u32_t peri_base;
		const ESOC soc;
		const ECPU cpu;
	};

	extern const model_config_t MODEL_CONFIG[];
	const model_config_t* DetectModel();	// may return nullptr
	const model_config_t* DetectModelThrow();	// returns a valid pointer or throws an exception

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
			// Bitmask tracking which GPIO pins have been claimed
			u64_t claimed_pins;

			// Pointer to board-specific peripheral base addresses and offsets
			const model_config_t* const model;

			// File descriptor wrapper for /dev/mem access
			TFile file;

			// Memory mapping for GPIO registers
			TMapping gpio_map;

			// Memory mapping for system timer registers
			TMapping timer_map;

			// Private constructor for singleton pattern
			TBCM283X();

		public:
			// Get the board model configuration
			const model_config_t& Model() const EL_GETTER { return *model; }

			// Claim exclusive access to a GPIO pin and return a pin interface
			std::unique_ptr<IPin> ClaimPin(const usys_t index) final override;

			// ----------------------------------------------------------------
			// Low-level register access methods (only use when you know what you are doing!)
			// These methods bypass TPin validation and state tracking.
			// ----------------------------------------------------------------

			// Configure pull-up/down resistor for a single GPIO pin
			// index: GPIO pin number
			// pull: desired pull state (DISABLED, UP, DOWN)
			void _PullResistor(const unsigned index, const EPull pull);

			// Get pointer to the GPSET register block for a given 32-bit bank.
			// idx: 0 for pins 0–31, 1 for pins 32–53.
			// Write a 1 bit to set a pin high.
			volatile u32_t* _Set(const u8_t idx) EL_SETTER;

			// Get pointer to the GPCLR register block for a given 32-bit bank.
			// idx: 0 for pins 0–31, 1 for pins 32–53.
			// Write a 1 bit to clear a pin (set low).
			volatile u32_t* _Clear(const u8_t idx) EL_SETTER;

			// Get pointer to the GPLEV register block for a given 32-bit bank.
			// idx: 0 for pins 0–31, 1 for pins 32–53.
			// Read bitmask of current pin levels.
			volatile const u32_t* _Level(const u8_t idx) const EL_GETTER;

			// ----------------------------------------------------------------
			// System timer accessors
			// ----------------------------------------------------------------

			// Get pointer to low 32 bits of the free-running microsecond timer
			volatile const u32_t* _GpioTimerLow() const EL_GETTER;

			// Get pointer to high 32 bits of the free-running microsecond timer
			volatile const u32_t* _GpioTimerHigh() const EL_GETTER;

			// Read and return GPIO-timer value
			TTime GpioTime() const EL_GETTER;

			// Return the singleton instance of this controller
			static TBCM283X* Instance();
			virtual ~TBCM283X();
	};
}

#endif
