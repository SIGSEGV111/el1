#include "def.hpp"
#ifdef EL_OS_LINUX

#include "dev_gpio_native.hpp"

#include <stdio.h>
#include "system_task.hpp"

namespace el1::dev::gpio::native
{
	static const char* const SYSFS_GPIO_BASE = "/sys/class/gpio";
	static std::unique_ptr<TNativeGpioController> controller = nullptr;

	TPath TNativeGpioPin::Directory()
	{
		const TString str = "%s/gpio%d";
		return TString::Format(str, SYSFS_GPIO_BASE, controller->base + id);
	}

	IController* TNativeGpioPin::Controller() const
	{
		return TNativeGpioController::Instance();
	}

	void TNativeGpioPin::AutoCommit(const bool)
	{
		// noting to do
		// auto-commit is always on
	}

	bool TNativeGpioPin::State() const
	{
		byte_t buffer;
		state.Offset(0);
		state.Read(&buffer, 1);
		return buffer != '0';
	}

	void TNativeGpioPin::State(const bool new_state)
	{
		EL_ERROR(mode != EMode::OUTPUT, TException, "pin must be in OUTPUT mode before setting its state");
		state.Offset(0);
		state.Write((const byte_t*)(new_state ? "1" : "0"), 1);
	}

	EMode TNativeGpioPin::Mode() const
	{
		return mode;
	}

	void TNativeGpioPin::Mode(const EMode new_mode)
	{
		if(mode == new_mode) return;

		TFile direction(Directory() + "direction", TAccess::RW);

		switch(new_mode)
		{
			case EMode::DISABLED:
			case EMode::INPUT:
				direction.Write((const byte_t*)"in", 2);
				break;

			case EMode::OUTPUT:
				direction.Write((const byte_t*)"out", 3);
				break;

			case EMode::ALT_FUNC:
				EL_THROW(TInvalidArgumentException, "new_mode", "ALT_FUNC mode cannot be set through Mode() - it has to be set through AlternateFunction()");

			default:
				EL_THROW(TLogicException);
		}

		mode = new_mode;
	}

	ETrigger TNativeGpioPin::Trigger() const
	{
		return trigger;
	}

	void TNativeGpioPin::Trigger(const ETrigger new_trigger)
	{
		if(trigger == new_trigger) return;

		TFile edge(Directory() + "edge", TAccess::RW);

		switch(new_trigger)
		{
			case ETrigger::DISABLED:
				edge.Write((const byte_t*)"none", 4);
				break;

			case ETrigger::RISING_EDGE:
				edge.Write((const byte_t*)"rising", 6);
				break;

			case ETrigger::FALLING_EDGE:
				edge.Write((const byte_t*)"falling", 7);
				break;

			case ETrigger::BOTH_EDGES:
				edge.Write((const byte_t*)"both", 4);
				break;

			default:
				EL_THROW(TLogicException);
		}

		trigger = new_trigger;
	}

	void TNativeGpioPin::AlternateFunction(const int func)
	{
		EL_NOT_IMPLEMENTED;
	}

	int TNativeGpioPin::AlternateFunction() const
	{
		EL_NOT_IMPLEMENTED;
	}

	EPull TNativeGpioPin::Pull() const
	{
		return EPull::DISABLED;
	}

	void TNativeGpioPin::Pull(const EPull new_pull)
	{
		EL_ERROR(new_pull != EPull::DISABLED, TNotImplementedException);
	}

	TNativeGpioPin::TNativeGpioPin(const unsigned id) : id(id), mode(EMode::INPUT), trigger(ETrigger::DISABLED), state(Directory() + "value", TAccess::RW), on_input_trigger({.read = false, .write = false, .other = true}, state.Handle())
	{
	}

	TNativeGpioPin::~TNativeGpioPin()
	{
		controller->UnexportPin(id);
	}

	void TNativeGpioController::ExportPin(const unsigned id)
	{
		TFile _export(TString(SYSFS_GPIO_BASE) + "/export", TAccess::WO);

		char buffer[16] = {};
		const int len = snprintf(buffer, sizeof(buffer), "%d", base + id);
		_export.Write((const byte_t*)buffer, len);
	}

	void TNativeGpioController::UnexportPin(const unsigned id)
	{
		TFile _unexport(TString(SYSFS_GPIO_BASE) + "/unexport", TAccess::WO);

		char buffer[16] = {};
		const int len = snprintf(buffer, sizeof(buffer), "%d", base + id);
		_unexport.Write((const byte_t*)buffer, len);
	}

	std::unique_ptr<IPin> TNativeGpioController::ClaimPin(const usys_t id)
	{
		ExportPin(id);
		try
		{
			return std::unique_ptr<IPin>(new TNativeGpioPin(id));
		}
		catch(...)
		{
			try { UnexportPin(id); } catch(...) {}
			throw;
		}
	}

	TNativeGpioController* TNativeGpioController::Instance()
	{
		if(controller == nullptr)
			controller = std::unique_ptr<TNativeGpioController>(new TNativeGpioController());
		return controller.get();
	}

	TNativeGpioController::TNativeGpioController() : base(0)
	{
	}
}

#endif
