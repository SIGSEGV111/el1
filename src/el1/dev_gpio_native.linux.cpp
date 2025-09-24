#include "def.hpp"
#ifdef EL_OS_LINUX

#include "dev_gpio_native.hpp"
#include <linux/gpio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

namespace el1::dev::gpio::native
{
	void TNativeGpioPin::Configure(const EMode new_mode, const ETrigger new_trigger, const EPull new_pull, const u32_t new_debounce_us)
	{
		struct gpio_v2_line_config config = {};
		config.flags = GPIO_V2_LINE_FLAG_EVENT_CLOCK_HTE
			| (new_mode == EMode::INPUT  ? GPIO_V2_LINE_FLAG_INPUT  : 0)
			| (new_mode == EMode::OUTPUT ? GPIO_V2_LINE_FLAG_OUTPUT : 0)
			| ((new_mode != EMode::DISABLED && (new_trigger == ETrigger::RISING_EDGE  || new_trigger == ETrigger::BOTH_EDGES)) ? GPIO_V2_LINE_FLAG_EDGE_RISING  : 0)
			| ((new_mode != EMode::DISABLED && (new_trigger == ETrigger::FALLING_EDGE || new_trigger == ETrigger::BOTH_EDGES)) ? GPIO_V2_LINE_FLAG_EDGE_FALLING : 0)
			| ((new_mode != EMode::DISABLED && new_pull == EPull::UP       ) ? GPIO_V2_LINE_FLAG_BIAS_PULL_UP   : 0)
			| ((new_mode != EMode::DISABLED && new_pull == EPull::DOWN     ) ? GPIO_V2_LINE_FLAG_BIAS_PULL_DOWN : 0)
			| ((new_mode != EMode::DISABLED || new_pull == EPull::DISABLED ) ? GPIO_V2_LINE_FLAG_BIAS_DISABLED  : 0);

		if(new_mode == EMode::INPUT)
		{
			config.num_attrs = 1;
			config.attrs[0].attr.id = GPIO_V2_LINE_ATTR_ID_DEBOUNCE;
			config.attrs[0].attr.debounce_period_us = new_debounce_us;
			config.attrs[0].mask = 1;
		}

		EL_SYSERR(ioctl(on_input_trigger.Handle(), GPIO_V2_LINE_SET_CONFIG_IOCTL, &config));
		this->mode = new_mode;
		this->trigger = new_trigger;
		this->pull = new_pull;
		this->debounce_us = new_debounce_us;
	}

	IController* TNativeGpioPin::Controller() const
	{
		return controller;
	}

	void TNativeGpioPin::AutoCommit(const bool)
	{
		// no effect
	}

	void TNativeGpioPin::Commit()
	{
		// no effect
	}

	bool TNativeGpioPin::State() const
	{
		struct gpio_v2_line_values values = { .bits = 0, .mask = 1 };
		EL_SYSERR(ioctl(on_input_trigger.Handle(), GPIO_V2_LINE_GET_VALUES_IOCTL, &values));
		return values.bits != 0;
	}

	void TNativeGpioPin::State(const bool new_state)
	{
		EL_ERROR(mode != EMode::OUTPUT, TException, "line must be in OUTPUT mode before you can change the state");
		struct gpio_v2_line_values values = { .bits = (new_state ? 1U : 0U), .mask = 1 };
		EL_SYSERR(ioctl(on_input_trigger.Handle(), GPIO_V2_LINE_SET_VALUES_IOCTL, &values));
	}

	EMode TNativeGpioPin::Mode() const
	{
		return this->mode;
	}

	void TNativeGpioPin::Mode(const EMode new_mode)
	{
		if(this->mode == new_mode)
			return;

		Configure(new_mode, this->trigger, this->pull, this->debounce_us);
	}

	void TNativeGpioPin::AlternateFunction(const int)
	{
		EL_NOT_IMPLEMENTED;
	}

	int TNativeGpioPin::AlternateFunction() const
	{
		EL_NOT_IMPLEMENTED;
	}

	ETrigger TNativeGpioPin::Trigger() const
	{
		return this->trigger;
	}

	void TNativeGpioPin::Trigger(const ETrigger new_trigger)
	{
		if(this->trigger == new_trigger)
			return;

		Configure(this->mode, new_trigger, this->pull, this->debounce_us);
	}

	EPull TNativeGpioPin::Pull() const
	{
		return this->pull;
	}

	void TNativeGpioPin::Pull(const EPull new_pull)
	{
		if(this->pull == new_pull)
			return;

		Configure(this->mode, this->trigger, new_pull, this->debounce_us);
	}

	usys_t TNativeGpioPin::Index() const
	{
		return index;
	}

	TNativeGpioPin::TNativeGpioPin(TNativeGpioController* const controller, const usys_t index) : index(index), controller(controller), on_input_trigger({ .read = true, .write = false, .other = false }), mode(EMode::INPUT), trigger(ETrigger::DISABLED), pull(EPull::DISABLED), debounce_us(0)
	{
		struct gpio_v2_line_request request = {};
		request.offsets[0] = index;
		strncpy(request.consumer, "el1", GPIO_MAX_NAME_SIZE);
		request.config.flags = GPIO_V2_LINE_FLAG_INPUT | GPIO_V2_LINE_FLAG_BIAS_DISABLED | GPIO_V2_LINE_FLAG_EVENT_CLOCK_HTE;
		request.config.num_attrs = 0;
		request.num_lines = 1;
		request.event_buffer_size = 16;
		EL_SYSERR(ioctl(controller->io.Handle(), GPIO_V2_GET_LINE_IOCTL, &request));
		EL_ERROR(request.fd == -1, TLogicException);
		on_input_trigger.Handle(request.fd);
		EL_SYSERR(fcntl(request.fd, F_SETFD, EL_SYSERR(fcntl(request.fd, F_GETFD)) | FD_CLOEXEC));
		EL_SYSERR(fcntl(request.fd, F_SETFL, EL_SYSERR(fcntl(request.fd, F_GETFL)) | O_NONBLOCK));
	}

	TNativeGpioPin::~TNativeGpioPin()
	{
		if(on_input_trigger.Handle() != -1)
			close(on_input_trigger.Handle());
	}

	/**********************************************************************************/

	std::unique_ptr<IPin> TNativeGpioController::ClaimPin(const usys_t index)
	{
		return New<TNativeGpioPin, IPin>(this, index);
	}

	TNativeGpioController::TNativeGpioController(TFile _io) : io(std::move(_io))
	{
		struct gpiochip_info info = {};
		EL_SYSERR(ioctl(io.Handle(), GPIO_GET_CHIPINFO_IOCTL, &info));
		n_pins = info.lines;
	}
}

#endif
