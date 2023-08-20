#pragma once

#include "def.hpp"
#include "io_types.hpp"
#include "system_task.hpp"

namespace el1::dev::gpio
{
	using namespace io::types;
	using namespace system::task;

	struct IController;
	struct IPin;

	enum class EMode : u8_t
	{
		DISABLED,	// HIGHZ, drivers disabled, irq disabled, if possible: input disabled, pull-resistors disabled
		INPUT,		// HIGHZ, drivers disabled
		OUTPUT,
		ALT_FUNC
	};

	enum class ETrigger : u8_t
	{
		DISABLED,
		RISING_EDGE,
		FALLING_EDGE,
		BOTH_EDGES
	};

	enum class EPull : u8_t
	{
		DISABLED,
		UP,
		DOWN
	};

	struct IPin
	{
		virtual IController* Controller() const EL_GETTER = 0;
		virtual void AutoCommit(const bool) EL_SETTER = 0;

		virtual bool State() const = 0;
		virtual void State(const bool) EL_SETTER = 0;

		virtual EMode Mode() const = 0;
		virtual void Mode(const EMode) EL_SETTER = 0;

		virtual void AlternateFunction(const int func) EL_SETTER = 0;
		virtual int AlternateFunction() const EL_GETTER = 0;

		virtual ETrigger Trigger() const EL_GETTER = 0;
		virtual void Trigger(const ETrigger) EL_SETTER = 0;

		virtual EPull Pull() const EL_GETTER = 0;
		virtual void Pull(const EPull) EL_SETTER = 0;

		virtual IWaitable& OnInputTrigger() = 0;

		virtual void Commit();

		virtual ~IPin();
	};

	struct IController
	{
		virtual std::unique_ptr<IPin> ClaimPin(const usys_t index) = 0;

		// some controllers do not immediately update the hardware but instead need an explicit Commit() call
		// this allows the user to make many changes to GPIO pins and commit them all at once in one operation
		// this functions returns whether or not the controller needs explicit calls to Commit() to update the hardware
		// by default all GPIO pins always default to "auto-commit" mode - that is they call Commit() automatically
		// if it is required by the controller - only when explicitly set to AutoCommit(false) the user has to
		// make the call to Commit()
		virtual bool NeedCommit() const EL_GETTER { return false; }

		// commits any pending changes to the hardware and polls the state of all INPUT pins
		virtual void Commit() {}

		// if NeedCommit() is true a controller might not immediatelly receive updates from INPUT pins
		// in such cases Poll() can be used to request an immediate update
		virtual void Poll() {}
	};
}
