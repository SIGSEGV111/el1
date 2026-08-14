#include "dev_motor.hpp"
#include "system_task.hpp"
#include <cmath>

namespace el1::dev::motor
{
	#define WriteDebug(...) do {} while(false)
	#define WriteWarning(...) do {} while(false)
	using namespace gpio;
	using namespace io::collection::list;
	using namespace util;
	using namespace system::time;
	using namespace system::task;

	EMotorDirection InvertDirection(EMotorDirection dir)
	{
		switch(dir)
		{
			case EMotorDirection::FORWARD: return EMotorDirection::REVERSE;
			case EMotorDirection::REVERSE: return EMotorDirection::FORWARD;
		}
		EL_THROW(TLogicException);
	}

	bool IsEncoderMoving(const s64_t pos_now, const s64_t pos_prev, const EMotorDirection dir)
	{
		bool v;
		switch(dir)
		{
			case EMotorDirection::FORWARD: v = pos_now > pos_prev; break;
			case EMotorDirection::REVERSE: v = pos_now < pos_prev; break;
			default: EL_THROW(TLogicException);
		}
		return v;
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////

	float IRotaryEncoder::AngleDifference(const float a1, const float a2)
	{
		EL_ERROR(!std::isfinite(a1) || a1 < 0.0f || a1 >= 1.0f, TInvalidArgumentException, "a1", "a1 must be in range [0..1)");
		EL_ERROR(!std::isfinite(a2) || a2 < 0.0f || a2 >= 1.0f, TInvalidArgumentException, "a2", "a2 must be in range [0..1)");

		const float HALF_TURN = 0.5f;
		const float FULL_TURN = 1.0f;

		float diff = a1 - a2;

		if(diff > HALF_TURN)
		{
			diff -= FULL_TURN;
		}
		else if(diff < -HALF_TURN)
		{
			diff += FULL_TURN;
		}

		return diff;
	}

	float IRotaryEncoder::AngleDifference(const float from, const float to, const EMotorDirection dir)
	{
		EL_ERROR(!std::isfinite(from) || from < 0.0f || from >= 1.0f, TInvalidArgumentException, "from", "from must be in range [0..1)");
		EL_ERROR(!std::isfinite(to) || to < 0.0f || to >= 1.0f, TInvalidArgumentException, "to", "to must be in range [0..1)");

		const float FULL_TURN = 1.0f;
		float diff = to - from;

		switch(dir)
		{
			case EMotorDirection::FORWARD:
			{
				if(diff < 0.0f)
					diff += FULL_TURN;
				return diff;
			}

			case EMotorDirection::REVERSE:
			{
				if(diff > 0.0f)
					diff -= FULL_TURN;
				return diff;
			}
		}

		EL_THROW(TInvalidArgumentException, "dir", "invalid motor direction");
	}


	void IStepperDriver::Steps(const u32_t n_step, TTime t_step, const bool quick)
	{
		const TTime t_min = MinimumStepPulseLength();
		t_step = util::Max(t_min, t_step);

		for(u32_t i = 0; i < n_step; i++)
		{
			Step(true);
			TFiber::Sleep(t_min);
			Step(false);
			TFiber::Sleep(quick && i + 1 == n_step ? t_min : t_step);
		}
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////

	bool IStepperGroup::Enabled(const bool state)
	{
		usys_t i = 0;
		for(IStepperDriver* m = GroupMotor(0); m != nullptr; m = GroupMotor(++i))
			(void)m->Enabled(state);
		return state;
	}

	bool IStepperGroup::Enabled() const
	{
		if(GroupMotor(0) == nullptr)
			return false;
		return GroupMotor(0)->Enabled();
	}

	drive_current_t IStepperGroup::Amperage(drive_current_t c)
	{
		drive_current_t r = c;
		usys_t i = 0;
		for(IStepperDriver* m = GroupMotor(0); m != nullptr; m = GroupMotor(++i))
			r = m->Amperage(c);
		return r;
	}

	drive_current_t IStepperGroup::Amperage() const
	{
		if(GroupMotor(0) == nullptr)
			return {0,0,0,0};
		return GroupMotor(0)->Amperage();
	}

	const IPowerMeter* IStepperGroup::PowerMeter() const
	{
		if(GroupMotor(0) == nullptr)
			return nullptr;
		return GroupMotor(0)->PowerMeter();
	}

	EDriverState IStepperGroup::State() const
	{
		IStepperDriver* const first = GroupMotor(0);
		if(first == nullptr)
			return EDriverState::DISABLED;

		const EDriverState state = first->State();
		for(usys_t i = 1; IStepperDriver* const motor = GroupMotor(i); i++)
			if(motor->State() != state)
				return EDriverState::UNKNOWN;
		return state;
	}

	IStallDetector* IStepperGroup::StallDetector()
	{
		return const_cast<IStallDetector*>(static_cast<const IStepperGroup*>(this)->StallDetector());
	}

	const IStallDetector* IStepperGroup::StallDetector() const
	{
		if(GroupMotor(0) == nullptr)
			return nullptr;

		for(usys_t i = 0; IStepperDriver* const motor = GroupMotor(i); i++)
			if(motor->StallDetector() == nullptr)
				return nullptr;
		return &group_stall_detector;
	}

	bool IStepperGroup::TGroupStallDetector::Enabled(const bool state)
	{
		for(usys_t i = 0; IStepperDriver* const motor = parent->GroupMotor(i); i++)
		{
			IStallDetector* const detector = motor->StallDetector();
			EL_ERROR(detector == nullptr, TLogicException);
			(void)detector->Enabled(state);
		}
		return Enabled();
	}

	bool IStepperGroup::TGroupStallDetector::Enabled() const
	{
		if(parent->GroupMotor(0) == nullptr)
			return false;

		for(usys_t i = 0; IStepperDriver* const motor = parent->GroupMotor(i); i++)
		{
			const IStallDetector* const detector = motor->StallDetector();
			if(detector == nullptr || !detector->Enabled())
				return false;
		}
		return true;
	}

	EStallState IStepperGroup::TGroupStallDetector::State() const
	{
		IStepperDriver* const first_motor = parent->GroupMotor(0);
		if(first_motor == nullptr)
			return EStallState::DISABLED;

		const IStallDetector* const first_detector = first_motor->StallDetector();
		EL_ERROR(first_detector == nullptr, TLogicException);
		EStallState aggregate = first_detector->State();
		if(aggregate == EStallState::STALLED)
			return aggregate;

		for(usys_t i = 1; IStepperDriver* const motor = parent->GroupMotor(i); i++)
		{
			const IStallDetector* const detector = motor->StallDetector();
			EL_ERROR(detector == nullptr, TLogicException);
			const EStallState state = detector->State();
			if(state == EStallState::STALLED)
				return state;
			if(state != aggregate)
				aggregate = EStallState::UNKNOWN;
		}
		return aggregate;
	}

	void IStepperGroup::TGroupStallDetector::Reset() const
	{
		for(usys_t i = 0; IStepperDriver* const motor = parent->GroupMotor(i); i++)
		{
			const IStallDetector* const detector = motor->StallDetector();
			EL_ERROR(detector == nullptr, TLogicException);
			detector->Reset();
		}
	}

	IServo* IStepperGroup::Servo()
	{
		return nullptr;
	}

	const IServo* IStepperGroup::Servo() const
	{
		return nullptr;
	}

	bool IStepperGroup::StepDirectionAvailable() const
	{
		if(GroupMotor(0) == nullptr)
			return false;

		for(usys_t i = 0; IStepperDriver* const motor = GroupMotor(i); i++)
			if(!motor->StepDirectionAvailable())
				return false;
		return true;
	}

	void IStepperGroup::Step(const bool state)
	{
		usys_t i = 0;
		for(IStepperDriver* m = GroupMotor(0); m != nullptr; m = GroupMotor(++i))
			m->Step(state);
	}

	void IStepperGroup::Direction(const EMotorDirection dir)
	{
		usys_t i = 0;
		for(IStepperDriver* m = GroupMotor(0); m != nullptr; m = GroupMotor(++i))
			m->Direction(dir);
	}

	EMotorDirection IStepperGroup::Direction() const
	{
		if(GroupMotor(0) == nullptr)
			return EMotorDirection::FORWARD;
		return GroupMotor(0)->Direction();
	}

	TTime IStepperGroup::MinimumStepPulseLength() const
	{
		TTime t = 0;
		usys_t i = 0;
		for(IStepperDriver* m = GroupMotor(0); m != nullptr; m = GroupMotor(++i))
			if(m->MinimumStepPulseLength() > t)
				t = m->MinimumStepPulseLength();
		return t;
	}

	u32_t IStepperGroup::FullStepResolution() const
	{
		u32_t r = 0;
		usys_t i = 0;
		for(IStepperDriver* m = GroupMotor(0); m != nullptr; m = GroupMotor(++i))
			if(m->FullStepResolution() > r)
				r = m->FullStepResolution();
		return r;
	}

	u32_t IStepperGroup::Microsteps(u32_t _divider)
	{
		const u32_t virtual_fsr = FullStepResolution();
		const double divider = _divider;

		usys_t i = 0;
		for(IStepperDriver* m = GroupMotor(0); m != nullptr; m = GroupMotor(++i))
		{
			const double this_fsr = m->FullStepResolution();
			const double ideal_divider = (virtual_fsr / this_fsr) * divider;
			const u32_t this_divider = (u32_t)ideal_divider;
			EL_ERROR(ideal_divider != (double)this_divider, TInvalidArgumentException, "divider", "unable to find an even divider for this motor");
			EL_ERROR(this_divider != m->Microsteps(this_divider), TInvalidArgumentException, "divider", "motor did not accept the requested microstep divider");
		}

		return _divider;
	}

	u32_t IStepperGroup::Microsteps() const
	{
		if(GroupMotor(0) == nullptr)
			return 1;

		const u32_t virtual_fsr = FullStepResolution();
		usys_t i = 0;
		for(IStepperDriver* m = GroupMotor(0); m != nullptr; m = GroupMotor(++i))
			if(m->FullStepResolution() == virtual_fsr)
				return m->Microsteps();

		EL_THROW(TLogicException);
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////

	static IServo* ValidateStepperServo(IServo* const servo)
	{
		EL_ERROR(servo == nullptr, TInvalidArgumentException, "servo", "servo must not be null");
		return servo;
	}

	static u64_t GetRotaryEncoderStepsPerTurn(IServo* const servo)
	{
		IServo* const validated_servo = ValidateStepperServo(servo);
		IRotaryEncoder* const encoder = dynamic_cast<IRotaryEncoder*>(validated_servo->Encoder());
		EL_ERROR(encoder == nullptr, TInvalidArgumentException, "servo", "servo requires an IRotaryEncoder or an explicit encoder_steps_per_turn value");
		return encoder->StepsPerTurn();
	}

	static s64_t ValidateEncoderStepsPerTurn(const u64_t encoder_steps_per_turn)
	{
		EL_ERROR(encoder_steps_per_turn == 0 || encoder_steps_per_turn > (u64_t)INT64_MAX, TInvalidArgumentException, "encoder_steps_per_turn", "encoder steps per turn must fit into a positive signed 64-bit value");
		return (s64_t)encoder_steps_per_turn;
	}

	static u32_t ValidateFullStepResolution(const u32_t full_step_resolution)
	{
		EL_ERROR(full_step_resolution == 0, TInvalidArgumentException, "full_step_resolution", "full-step resolution must be greater than zero");
		return full_step_resolution;
	}

	static s64_t ValidateStepDenominator(const u32_t full_step_resolution, const u32_t microsteps, const s64_t encoder_steps_per_turn, const char* const argument_name)
	{
		EL_ERROR(microsteps == 0, TInvalidArgumentException, argument_name, "microstep divider must be greater than zero");

		// The product of two u32_t values always fits into u64_t. Convert to s64_t
		// only after checking the signed range used by Step().
		const u64_t denominator_u = (u64_t)full_step_resolution * (u64_t)microsteps;
		EL_ERROR(denominator_u > (u64_t)INT64_MAX, TInvalidArgumentException, argument_name, "full-step resolution multiplied by microstep divider exceeds the signed 64-bit range");
		const s64_t denominator = (s64_t)denominator_u;

		// Step() maintains abs(step_remainder) < denominator. Therefore the
		// largest possible accumulator magnitude is
		// encoder_steps_per_turn + denominator - 1. Reject configurations for
		// which that value would not fit in s64_t.
		EL_ERROR(encoder_steps_per_turn > INT64_MAX - (denominator - 1), TInvalidArgumentException, argument_name, "step geometry requires more than signed 64-bit intermediate precision");
		return denominator;
	}

	TStepperEmulation::TStepperEmulation(IServo* const servo, const u32_t full_step_resolution) :
		TStepperEmulation(servo, full_step_resolution, GetRotaryEncoderStepsPerTurn(servo))
	{
	}

	TStepperEmulation::TStepperEmulation(IServo* const servo_, const u32_t full_step_resolution_, const u64_t encoder_steps_per_turn_) :
		servo(ValidateStepperServo(servo_)),
		driver(&servo->Driver()),
		encoder_steps_per_turn(ValidateEncoderStepsPerTurn(encoder_steps_per_turn_)),
		full_step_resolution(ValidateFullStepResolution(full_step_resolution_)),
		microsteps(1),
		step_denominator(ValidateStepDenominator(full_step_resolution, microsteps, encoder_steps_per_turn, "full_step_resolution")),
		direction(EMotorDirection::FORWARD),
		step_state(false),
		enabled(false),
		target_position(servo->TargetPosition()),
		step_remainder(0),
		fib_control_loop()
	{
		enabled = driver->Enabled() && servo->Enabled();
	}

	void TStepperEmulation::ControlLoop()
	{
		while(true)
		{
			const s64_t new_target = target_position;
			servo->TargetPosition(new_target);

			if(new_target == target_position)
				fib_control_loop.Stop();
		}
	}

	void TStepperEmulation::WakeControlLoop()
	{
		switch(fib_control_loop.State())
		{
			case EFiberState::CONSTRUCTED:
				fib_control_loop.Start(TFunction<void>(this, &TStepperEmulation::ControlLoop));
				break;

			case EFiberState::STOPPED:
				fib_control_loop.Resume();
				break;

			case EFiberState::READY:
			case EFiberState::ACTIVE:
			case EFiberState::BLOCKED:
				break;

			case EFiberState::FINISHED:
			case EFiberState::CRASHED:
			case EFiberState::KILLED:
				EL_THROW(TLogicException);
		}
	}

	bool TStepperEmulation::Enabled(const bool state)
	{
		if(state == enabled)
			return Enabled();

		if(!state)
		{
			enabled = false;
			step_state = false;

			switch(fib_control_loop.State())
			{
				case EFiberState::READY:
				case EFiberState::BLOCKED:
					fib_control_loop.Stop();
					break;

				default:
					break;
			}

			(void)servo->Enabled(false);
			(void)driver->Enabled(false);

			// Enabling a STEP/DIR driver must not cause a position jump. Forget any
			// queued emulation target and re-anchor the virtual step position at the
			// actual servo position whenever an encoder is available.
			if(servo->Encoder() != nullptr)
				target_position = servo->Encoder()->Position();
			else
				target_position = servo->TargetPosition();
			step_remainder = 0;
			servo->TargetPosition(target_position);
			return false;
		}

		if(servo->Encoder() != nullptr)
			target_position = servo->Encoder()->Position();
		else
			target_position = servo->TargetPosition();
		step_remainder = 0;
		step_state = false;
		servo->TargetPosition(target_position);

		if(!driver->Enabled(true))
			return false;

		if(!servo->Enabled(true))
		{
			(void)driver->Enabled(false);
			return false;
		}

		enabled = true;
		return Enabled();
	}

	bool TStepperEmulation::Enabled() const
	{
		return enabled && driver->Enabled() && servo->Enabled();
	}

	drive_current_t TStepperEmulation::Amperage(drive_current_t current)
	{
		return driver->Amperage(current);
	}

	drive_current_t TStepperEmulation::Amperage() const
	{
		return driver->Amperage();
	}

	const IPowerMeter* TStepperEmulation::PowerMeter() const
	{
		return driver->PowerMeter();
	}

	EDriverState TStepperEmulation::State() const
	{
		if(fib_control_loop.State() == EFiberState::CRASHED)
			return EDriverState::IO_ERROR;
		return Enabled() ? driver->State() : EDriverState::DISABLED;
	}

	IStallDetector* TStepperEmulation::StallDetector()
	{
		return driver->StallDetector();
	}

	const IStallDetector* TStepperEmulation::StallDetector() const
	{
		return driver->StallDetector();
	}

	IServo* TStepperEmulation::Servo()
	{
		return servo;
	}

	const IServo* TStepperEmulation::Servo() const
	{
		return servo;
	}

	void TStepperEmulation::Step(const bool state)
	{
		if(step_state == state)
			return;

		step_state = state;
		if(!state || !enabled)
			return;

		const s64_t step_delta = direction == EMotorDirection::FORWARD ? encoder_steps_per_turn : -encoder_steps_per_turn;
		const s64_t accumulator = step_remainder + step_delta;
		const s64_t delta = accumulator / step_denominator;

		if(delta > 0)
			EL_ERROR(target_position > INT64_MAX - delta, TException, "step target exceeds the signed 64-bit servo coordinate range");
		else if(delta < 0)
			EL_ERROR(target_position < INT64_MIN - delta, TException, "step target exceeds the signed 64-bit servo coordinate range");

		step_remainder = accumulator % step_denominator;
		target_position += delta;
		WakeControlLoop();
	}

	void TStepperEmulation::Direction(const EMotorDirection dir)
	{
		direction = dir;
	}

	EMotorDirection TStepperEmulation::Direction() const
	{
		return direction;
	}

	system::time::TTime TStepperEmulation::MinimumStepPulseLength() const
	{
		return 0;
	}

	u32_t TStepperEmulation::FullStepResolution() const
	{
		return full_step_resolution;
	}

	u32_t TStepperEmulation::Microsteps(u32_t divider)
	{
		const s64_t new_denominator = ValidateStepDenominator(full_step_resolution, divider, encoder_steps_per_turn, "divider");

		if(step_remainder != 0 && step_denominator != new_denominator)
		{
			const s64_t abs_remainder = step_remainder < 0 ? -step_remainder : step_remainder;
			EL_ERROR(abs_remainder > INT64_MAX / new_denominator, TInvalidArgumentException, "divider", "preserving the current fractional step phase would require more than signed 64-bit intermediate precision");
			step_remainder = step_remainder * new_denominator / step_denominator;
		}

		microsteps = divider;
		step_denominator = new_denominator;
		return microsteps;
	}

	u32_t TStepperEmulation::Microsteps() const
	{
		return microsteps;
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////

	u32_t FindHomeEndstop(IStepperDriver& motor, gpio::IPin& endstop, const EMotorDirection seek_dir)
	{
		endstop.Mode(gpio::EMode::INPUT);
		(void)motor.Microsteps(1);

		if(endstop.State())
		{
			// fast clear endstop
			motor.Direction(InvertDirection(seek_dir));
			while(endstop.State())
				motor.Step();
		}
		else
		{
			// fast seek endstop
			motor.Direction(seek_dir);
			while(!endstop.State())
				motor.Step();
		}

		// precision seek endstop
		TFiber::Sleep(0.2);
		while(!endstop.State())
		{
			motor.Step();
			TFiber::Sleep(0.2);
		}

		// precision clear endstop
		u32_t n = 0;
		motor.Direction(InvertDirection(seek_dir));
		while(endstop.State())
		{
			n++;
			motor.Step();
			TFiber::Sleep(0.2);
		}

		// number of steps to clear endstop - should be 1 in an ideal world
		return n;
	}

	void TGantry::AddMotor(IStepperDriver* const stepper, const bool inverted)
	{
		EL_ERROR(stepper == nullptr, TInvalidArgumentException, "stepper", "stepper must not be null");
		motors.Append({
			.stepper = stepper,
			.servo = stepper->Servo(),
			.encoder = stepper->Servo() ? stepper->Servo()->Encoder() : nullptr,
			.inverted = inverted,
			.delete_stepper = false,
			.enabled = true
		});
	}

	void TGantry::AddMotor(IServo* const servo, const bool inverted)
	{
		EL_ERROR(servo == nullptr, TInvalidArgumentException, "servo", "servo must not be null");
		motor_info_t& mi = motors.Append({
			.stepper = dynamic_cast<IStepperDriver*>(&servo->Driver()),
			.servo = servo,
			.encoder = servo->Encoder(),
			.inverted = inverted,
			.delete_stepper = false,
			.enabled = true
		});

		if(mi.stepper == nullptr || !mi.stepper->StepDirectionAvailable())
		{
			mi.stepper = new TStepperEmulation(servo, emulated_fsr);
			mi.delete_stepper = true;
		}
	}

	IStepperDriver* TGantry::GroupMotor(const usys_t index) const
	{
		return motors.Count() > index ? motors[index].stepper : nullptr;
	}

	void TGantry::Direction(const EMotorDirection gantry_dir)
	{
		for(motor_info_t& mi : motors)
			mi.stepper->Direction(mi.inverted ? InvertDirection(gantry_dir) : gantry_dir);
	}

	EMotorDirection TGantry::Direction() const
	{
		if(motors.IsEmpty())
			return EMotorDirection::FORWARD;
		return motors[0].inverted ? InvertDirection(motors[0].stepper->Direction()) : motors[0].stepper->Direction();
	}

	template<typename L>
	void TGantry::MoveWhile(const TTime& t_pulse, const TTime& t_delay, L lambda)
	{
		bool c = lambda(0);
		for(u32_t i = 0; i < axis_length && c; i++)
		{
			for(usys_t i = 0; i < motors.Count(); i++)
				if(motors[i].enabled)
					motors[i].stepper->Step(true);

			TFiber::Sleep(t_pulse);

			for(usys_t i = 0; i < motors.Count(); i++)
				if(motors[i].enabled)
					motors[i].stepper->Step(false);

			const TTime t_start = TTime::Now(EClock::MONOTONIC);
			c = lambda(i+1);
			const TTime t_end = TTime::Now(EClock::MONOTONIC);
			const TTime t_lambda = t_end - t_start;
			if(t_delay > t_lambda)
				TFiber::Sleep(t_delay - t_lambda);
		}

		EL_ERROR(c, TException, "reached end of axis ");
	}

	void TGantry::AlignSquare(const double speed)
	{
		WriteDebug(U"TGantry::AlignSquare(speed=%d)", speed);
		EL_ERROR(!std::isfinite(speed) || speed == 0.0, TInvalidArgumentException, "speed", "speed must be finite and non-zero");
		EL_ERROR(motors.IsEmpty(), TException, "gantry has no motors");
		EL_ERROR(axis_length == 0, TException, "gantry axis length is zero");
		for(const motor_info_t& mi : motors)
		{
			const IStallDetector* const stall_detector = mi.stepper->StallDetector();
			EL_ERROR(mi.encoder == nullptr && (stall_detector == nullptr || !stall_detector->Enabled()), TException, "each gantry motor requires an enabled stall detector or an encoder");
		}
		const TTime t_pulse = motors.Pipe().Aggregate([](TTime& r, const motor_info_t& it) { r = Max(r, it.stepper->MinimumStepPulseLength()); }, TTime());
		WriteDebug(U"t_pulse=%dµs", t_pulse.ConvertToF(EUnit::MICROSECONDS));
		const TTime t_delay = TTime(1.0 / Abs(speed)) - t_pulse;
		WriteDebug(U"t_delay=%dµs", t_delay.ConvertToF(EUnit::MICROSECONDS));
		EL_ERROR(t_delay < t_pulse, TInvalidArgumentException, "speed", "speed to high for allowed MinimumStepPulseLength()");
		const EMotorDirection gantry_dir = speed > 0 ? EMotorDirection::FORWARD : EMotorDirection::REVERSE;
		WriteDebug(U"gantry_dir=%d", (int)gantry_dir);

		usys_t n_remaining = motors.Count();
		TList<s64_t> arr_encoder_pos(motors.Count());
		TList<u32_t> arr_encoder_last_motion_step(motors.Count());
		TList<bool> arr_stalled(motors.Count());
		arr_encoder_pos.SetCount(motors.Count());
		arr_encoder_last_motion_step.SetCount(motors.Count());
		arr_stalled.SetCount(motors.Count());
		const double encoder_stall_window = Min((double)UINT32_MAX, ceil(Abs(speed) * 0.05));
		const u32_t encoder_stall_window_steps = Max(8U, (u32_t)encoder_stall_window);

		WriteDebug(U"have %d motors", motors.Count());
		for(usys_t i = 0; i < motors.Count(); i++)
		{
			motor_info_t& mi = motors[i];
			const EMotorDirection motor_dir = mi.inverted ? InvertDirection(gantry_dir) : gantry_dir;
			arr_encoder_pos[i] = mi.encoder ? mi.encoder->Position() : 0;
			WriteDebug(U"setting up motor %d (inverted=%d, motor_dir=%d, @encoder=%d, has-encoder=%B)", i, (int)mi.inverted, (int)motor_dir, arr_encoder_pos[i], mi.encoder != nullptr);
			arr_encoder_last_motion_step[i] = 0;
			arr_stalled[i] = false;
			mi.enabled = true;
			EL_ERROR(mi.stepper->Enabled(true) != true, TException, "unable to enable stepper");
			if(mi.servo && !mi.delete_stepper)
				EL_ERROR(mi.servo->Enabled(false) != false, TException, "unable to disable servo");
			mi.stepper->Direction(motor_dir);
			IStallDetector* const stall_detector = mi.stepper->StallDetector();
			if(stall_detector != nullptr && stall_detector->Enabled())
				stall_detector->Reset();
		}

		WriteDebug(U"moving until all motors have stalled ...");
		MoveWhile(t_pulse, t_delay,
			[&](const u32_t i_step)
			{
				if(i_step < 10)
					return true;

				if((i_step % 64) == 0) WriteDebug(U"i_step=%d, n_remaining=%d", i_step, n_remaining);
				for(usys_t i = 0; i < motors.Count(); i++)
				{
					motor_info_t& mi = motors[i];
					if(!arr_stalled[i])
					{
						const s64_t pos_now = mi.encoder ? mi.encoder->Position() : 0;
						const EMotorDirection motor_dir = mi.inverted ? InvertDirection(gantry_dir) : gantry_dir;
						const bool emov = mi.encoder ? IsEncoderMoving(pos_now, arr_encoder_pos[i], motor_dir) : false;
						arr_encoder_pos[i] = pos_now;
						if(emov)
							arr_encoder_last_motion_step[i] = i_step;
						const bool encoder_stalled = mi.encoder != nullptr && (i_step - arr_encoder_last_motion_step[i]) >= encoder_stall_window_steps;

						const IStallDetector* const stall_detector = mi.stepper->StallDetector();
						if((stall_detector != nullptr && stall_detector->Enabled() && stall_detector->Detected()) || encoder_stalled)
						{
							arr_stalled[i] = true;
							(void)mi.stepper->Enabled(false);
							mi.enabled = false;
							if(stall_detector != nullptr)
								stall_detector->Reset();
							n_remaining--;
							WriteDebug(U"motor %d stalled, pos_now=%d, n_remaining=%d", i, pos_now, n_remaining);
						}
					}
				}

				return n_remaining > 0;
			}
		);

		WriteDebug(U"TGantry::AlignSquare(): DONE");
	}

	void TGantry::FindHomeRotaryEncoderHardstop(const double speed, const float target_angle, const unsigned n_iter)
	{
		WriteDebug(U"TGantry::FindHomeRotaryEncoderHardstop(speed=%d, target_angle=%d)", speed, target_angle);
		EL_ERROR(!std::isfinite(speed) || speed == 0.0, TInvalidArgumentException, "speed", "speed must be finite and non-zero");
		EL_ERROR(!std::isfinite(target_angle) || target_angle < 0.0f || target_angle >= 1.0f, TInvalidArgumentException, "target_angle", "target_angle must be in range [0..1)");
		EL_ERROR(motors.IsEmpty(), TException, "gantry has no motors");
		const TTime t_pulse = motors.Pipe().Aggregate([](TTime& r, const motor_info_t& it) { r = Max(r, it.stepper->MinimumStepPulseLength()); }, TTime());
		WriteDebug(U"t_pulse=%dµs", t_pulse.ConvertToF(EUnit::MICROSECONDS));
		const TTime t_delay = TTime(1.0 / Abs(speed)) - t_pulse;
		WriteDebug(U"t_delay=%dµs", t_delay.ConvertToF(EUnit::MICROSECONDS));
		EL_ERROR(t_delay < t_pulse, TInvalidArgumentException, "speed", "speed to high for allowed MinimumStepPulseLength()");

		// we are moving AWAY from the hardstop!
		const EMotorDirection dir_motor_away_from_hardstop = speed < 0 ? EMotorDirection::FORWARD : EMotorDirection::REVERSE;

		IRotaryEncoder* encoder = nullptr;
		EMotorDirection dir_enc_away_from_hardstop;

		for(motor_info_t& mi : motors)
			if((encoder = dynamic_cast<IRotaryEncoder*>(mi.encoder)) != nullptr)
			{
				dir_enc_away_from_hardstop = mi.inverted ? InvertDirection(dir_motor_away_from_hardstop) : dir_motor_away_from_hardstop;
				break;
			}

		EL_ERROR(encoder == nullptr, TException, "no IRotaryEncoder found");

		// move to hardstop and align axis
		AlignSquare(speed);

		WriteDebug(U"re-enable drivers and optional also the servos ...");
		for(motor_info_t& mi : motors)
		{
			mi.stepper->Direction(mi.inverted ? InvertDirection(dir_motor_away_from_hardstop) : dir_motor_away_from_hardstop);
			EL_ERROR(mi.stepper->Enabled(true) != true, TException, "unable to enable stepper");
			mi.enabled = true;
			if(mi.servo)
			{
				// mi.servo->Stop();
				(void)mi.servo->Enabled(true);
			}
		}

		{
			const float current_angle = encoder->RotorAngle();
			const float d_angle = Abs(IRotaryEncoder::AngleDifference(current_angle, target_angle));
			WriteDebug(U"current_angle=%d°; Abs(d_angle)=%d°", current_angle * 360.0f, d_angle * 360.0f);
			if(d_angle < 0.1f)
				WriteWarning(U"target angle %d° is very close to detected hardstop position at %d°", target_angle * 360.0f, current_angle * 360.0f);
		}

		WriteDebug(U"moving away from hardstop until we are approaching the target angle within 1/4 turn ...");
		MoveWhile(t_pulse, t_delay,
			[&](const u32_t i_step)
			{
				const float current_angle = encoder->RotorAngle();
				const float d_angle = Abs(IRotaryEncoder::AngleDifference(current_angle, target_angle, dir_enc_away_from_hardstop));
				return d_angle >= 0.25f;
			}
		);
		WriteDebug(U"move done; new angle: %d°", encoder->RotorAngle() * 360.0f);

		WriteDebug(U"performing precision moves to target angle ...");
		TTime t_precision_delay = t_delay;
		for(unsigned i = 0; i < n_iter; i++)
		{
			// move until the sign changes (moved past target_angle)

			t_precision_delay *= 2LL;
			const float angle_init = encoder->RotorAngle();
			const int sign_init = ((IRotaryEncoder::AngleDifference(angle_init, target_angle) > 0.0f) - (IRotaryEncoder::AngleDifference(angle_init, target_angle) < 0.0f));
			const EMotorDirection dir = ((i % 2) == 0) ? dir_motor_away_from_hardstop : InvertDirection(dir_motor_away_from_hardstop);
			Direction(dir);

			WriteDebug(U"iteration %d, t_precision_delay=%dµs, direction=%d, sign_init=%d, angle_init=%d°", i, t_precision_delay.ConvertToF(EUnit::MICROSECONDS), (int)dir, sign_init, angle_init * 360.0f);

			MoveWhile(t_pulse, 0,
				[&](const u32_t i_step)
				{
					TFiber::Sleep(t_precision_delay);
					const float current_angle = encoder->RotorAngle();
					const float d_angle = IRotaryEncoder::AngleDifference(current_angle, target_angle);
					const int sign_now = ((d_angle > 0.0f) - (d_angle < 0.0f));
					WriteDebug(U"i_step=%d; current_angle=%d°; d_angle=%d°; sign_now=%d; sign_init=%d", i_step, current_angle * 360.0f, d_angle * 360.0f, sign_now, sign_init);
					return sign_now == sign_init;
				}
			);

			TFiber::Sleep(0.1);
		}

		WriteDebug(U"final angle: %d°", encoder->RotorAngle() * 360.0f);
		WriteDebug(U"TGantry::FindHomeRotaryEncoderHardstop(): DONE");
	}

	TGantry::~TGantry()
	{
		for(motor_info_t& mi : motors)
		{
			if(mi.delete_stepper)
				delete mi.stepper;
		}
	}
}
