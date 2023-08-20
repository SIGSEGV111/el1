#pragma once

#include "system_task.hpp"
#include "system_handle.hpp"
#include "io_file.hpp"
#include "dev_gpio.hpp"

namespace el1::dev::gpio::native
{
	using namespace system::task;
	using namespace system::handle;
	using namespace io::file;

	class TNativeGpioController;

	class TNativeGpioPin : public IPin
	{
		friend class TNativeGpioController;

		protected:
			const unsigned id;
			EMode mode;
			ETrigger trigger;
			#ifdef EL_OS_LINUX
				mutable TFile state;
			#endif
			THandleWaitable on_input_trigger;

			TPath Directory();
			TNativeGpioPin(const unsigned id);

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

			THandleWaitable& OnInputTrigger() final override { return on_input_trigger; }

			~TNativeGpioPin();
	};

	class TNativeGpioController : public IController
	{
		friend class TNativeGpioPin;

		protected:
			unsigned base;
			TNativeGpioController();

			void ExportPin(const unsigned id);
			void UnexportPin(const unsigned id);

		public:
			std::unique_ptr<IPin> ClaimPin(const usys_t index) final override;

			static TNativeGpioController* Instance();

			virtual ~TNativeGpioController() {}
	};
}
