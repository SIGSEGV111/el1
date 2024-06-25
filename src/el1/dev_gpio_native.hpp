#pragma once

#include "system_task.hpp"
#include "system_handle.hpp"
#include "system_waitable.hpp"
#include "io_file.hpp"
#include "dev_gpio.hpp"

namespace el1::dev::gpio::native
{
	using namespace system::task;
	using namespace system::handle;
	using namespace io::file;

	class TNativeGpioController;
	class TNativeGpioPin;

	class TNativeGpioPin : public IPin
	{
		private:
			TNativeGpioController* const controller;
			THandleWaitable on_input_trigger;
			EMode mode;
			ETrigger trigger;
			EPull pull;
			u32_t debounce_us;

		public:
			void Configure(const EMode new_mode, const ETrigger new_trigger, const EPull new_pull, const u32_t new_debounce_us);

			IController* Controller() const final override EL_GETTER;
			void AutoCommit(const bool) final override EL_SETTER;
			void Commit() final override;

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

			THandleWaitable& OnInputTrigger() final override { return on_input_trigger; }

			TNativeGpioPin(TNativeGpioController* const controller, const usys_t index);
			virtual ~TNativeGpioPin();
	};

	class TNativeGpioController : public IController
	{
		friend class TNativeGpioPin;
		private:
			TFile io;
			usys_t n_pins;

		public:
			usys_t CountPins() const EL_GETTER { return n_pins; }
			std::unique_ptr<IPin> ClaimPin(const usys_t index) final override;

			TNativeGpioController(TFile);
			virtual ~TNativeGpioController() {}
	};
}
