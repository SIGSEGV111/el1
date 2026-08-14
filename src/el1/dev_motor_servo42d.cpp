#include "dev_motor_servo42d.hpp"
#include "system_task.hpp"
#include <cmath>
#include <exception>

namespace el1::dev::motor::servo42d
{
	template<typename F>
	class TScopeExit
	{
		private:
			F function;

		public:
			explicit TScopeExit(F function) : function(std::move(function))
			{
			}

			~TScopeExit() noexcept(false)
			{
				if(std::uncaught_exceptions() == 0)
				{
					function();
				}
				else
				{
					try
					{
						function();
					}
					catch(...)
					{
					}
				}
			}
	};

	#define WriteDebug(...) do {} while(false)
	#define WriteWarning(...) do {} while(false)
	using namespace gpio;
	using namespace modbus;
	using namespace util;
	using namespace system::task;

	// high-level state of the motion controller
	// the servo tries to follow that, but may lack behind
	// so this does NOT represent the actual motor motion
	enum class EInternalMotionControllerState : u16_t
	{
		INVALID = 0,
		STOPPED = 1,
		ACCERLERATING = 2,
		DECELERATING = 3,
		FULL_SPEED = 4,
		HOMING = 5,
		CALIBRATING = 6
	};

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	// TServo42D

	TServo42D::TServo42D(
		std::unique_ptr<modbus::TDevice> mbdev_,
		std::unique_ptr<gpio::IPin> _dir,
		std::unique_ptr<gpio::IPin> _step,
		std::unique_ptr<gpio::IPin> _en,
		const u16_t full_step_resolution
	) :
		mbdev(std::move(mbdev_)),
		dir(std::move(_dir)),
		step(std::move(_step)),
		en(std::move(_en)),
		servo_offset(0),
		servo_target(0),
		drive_current{0,0,0,0},
		full_step_resolution(full_step_resolution),
		microsteps(1),
		servo_enabled(false),
		mc_enabled(!(dir && step)),
		leave_enabled(false),
		dir_inverted(false),
		vfoc_enabled(true),
		inverted(true),
		stall_detection_enabled(false),
		fib_servo_control(TFunction<void>(this, &TServo42D::RunServoControlLoop), false),
		encoder(this),
		stall_detector(this),
		driver(this),
		servo(this),
		motion(this)
	{
		EL_ERROR(mbdev == nullptr, TInvalidArgumentException, "mbdev", "Modbus device must not be null");
		EL_ERROR(full_step_resolution == 0, TInvalidArgumentException, "full_step_resolution", "full-step resolution must be greater than zero");
		EL_ERROR((dir == nullptr) != (step == nullptr), TInvalidArgumentException, "dir/step", "DIR and STEP pins must either both be provided or both be omitted");

		if(dir && step)
		{
			dir->Mode(EMode::OUTPUT);
			dir->State(false);
			step->Mode(EMode::OUTPUT);
			step->State(false);
		}

		if(en)
		{
			en->Mode(EMode::OUTPUT);
			en->State(true);
			EnableMode(EEnableMode::EN_LOW);
		}

		RebootFirmware();
		if(motion.State() == EMotionControllerState::ACTIVE)
		{
			WriteDebug(U"stopping ... ");
				motion.Stop(100.0f);

			while(motion.State() == EMotionControllerState::ACTIVE)
				TFiber::Sleep(0.005);

			WriteDebug(U"stopped");
		}

		UpdateWorkMode();
		(void)driver.Microsteps(microsteps);

		// we can't detect it, so we set a defined state
		driver.Invert(false);

		// disable interpolation
		mbdev->WriteHoldingRegister(0x0089, 0);

		// disable port remap
		mbdev->WriteHoldingRegister(0x009E, 0);

		// disable screen auto-off
		mbdev->WriteHoldingRegister(0x0087, 0);

		// Keep locked-rotor protection opt-in; enabling it releases the motor on a detected stall.
		(void)stall_detector.Enabled(false);

		servo.SetHome();
		servo_target = servo_offset;

		if(en)
			en->State(false);
		else
			EnableMode(EEnableMode::ALWAYS);
	}

	TServo42D::~TServo42D()
	{
		try
		{
			if(fib_servo_control.State() != EFiberState::CONSTRUCTED)
			{
				(void)fib_servo_control.Shutdown();
				const std::unique_ptr<const IException> exception = fib_servo_control.Join();
				if(exception != nullptr)
					exception->Print("Servo42D control fiber terminated with exception");
			}
		}
		catch(...) {}

		try
		{
			if(en && !leave_enabled)
				driver.Enabled(false);
		}
		catch(...) {}
	}

	s32_t TServo42D::ComputeServoAxisPosition(const s64_t absolute_target) const
	{
		if(absolute_target >= servo_offset)
		{
			const u64_t distance = (u64_t)absolute_target - (u64_t)servo_offset;
			EL_ERROR(distance > (u64_t)INT32_MAX, TInvalidArgumentException, "absolute_target", "target position exceeds Servo42D 32-bit motion-controller range");
			return (s32_t)distance;
		}

		const u64_t distance = (u64_t)servo_offset - (u64_t)absolute_target;
		const u64_t max_negative_distance = (u64_t)INT32_MAX + 1U;
		EL_ERROR(distance > max_negative_distance, TInvalidArgumentException, "absolute_target", "target position exceeds Servo42D 32-bit motion-controller range");
		return distance == max_negative_distance ? INT32_MIN : -(s32_t)distance;
	}

	void TServo42D::SendServoTarget(const s64_t absolute_target)
	{
		const s32_t axis_position = ComputeServoAxisPosition(absolute_target);
		const u16_t acceleration = 255;
		const u16_t rpm = 3000;
		const u32_t raw_axis_position = (u32_t)axis_position;
		u16_t regs[4] =
		{
			acceleration,
			rpm,
			(u16_t)((raw_axis_position >> 16) & 0xFFFF),
			(u16_t)(raw_axis_position & 0xFFFF)
		};

		WriteDebug(U"sending servo target via motion-controller command 0x00F5: acc=%d; rpm=%d; axis=%d ...", acceleration, rpm, axis_position);
		mbdev->WriteHoldingRegisters(0x00F5, 4, regs);
	}

	void TServo42D::ProcessServoTargets()
	{
		const bool mc_prev = mc_enabled;
		TScopeExit restore_motion_controller([&]() { motion.Enabled(mc_prev); });
		motion.Enabled(true);

		while(servo_enabled)
		{
			const s64_t target = servo_target;
			const EMotionControllerState state_before_command = motion.State();
			EL_ERROR(state_before_command == EMotionControllerState::ERROR, TException, "Servo42D motion controller reported an error before applying servo target");
			if(state_before_command == EMotionControllerState::ACTIVE)
				motion.Stop(-1.0f);

			SendServoTarget(target);

			while(servo_enabled && target == servo_target)
			{
				const EMotionControllerState state = motion.State();
				if(state == EMotionControllerState::IDLE)
					return;
				EL_ERROR(state == EMotionControllerState::ERROR, TException, "Servo42D motion controller reported an error while applying servo target");
				EL_ERROR(state == EMotionControllerState::DISABLED, TException, "Servo42D motion controller became disabled while applying servo target");
				TFiber::Sleep(0.01);
			}

			if(!servo_enabled)
			{
				if(motion.State() == EMotionControllerState::ACTIVE)
					motion.Stop(-1.0f);
				return;
			}
		}
	}

	void TServo42D::RunServoControlLoop()
	{
		while(true)
		{
			if(servo_enabled)
				ProcessServoTargets();

			fib_servo_control.Stop();
		}
	}

	void TServo42D::WakeServoControlLoop()
	{
		if(!servo_enabled)
			return;

		switch(fib_servo_control.State())
		{
			case EFiberState::CONSTRUCTED:
				fib_servo_control.Start();
				break;

			case EFiberState::STOPPED:
				fib_servo_control.Resume();
				break;

			case EFiberState::READY:
			case EFiberState::ACTIVE:
			case EFiberState::BLOCKED:
				break;

			case EFiberState::FINISHED:
			case EFiberState::CRASHED:
			case EFiberState::KILLED:
				EL_THROW(TException, "Servo42D control fiber is no longer operational");
		}
	}

	void TServo42D::UpdateWorkMode()
	{
		EWorkMode m;
		if(mc_enabled)
		{
			if(servo_enabled)
			{
				if(vfoc_enabled)
				{
					m = EWorkMode::MC_VFOC;
				}
				else
				{
					m = EWorkMode::MC_CLOSE;
				}
			}
			else
			{
				m = EWorkMode::MC_OPEN;
			}
		}
		else
		{
			if(servo_enabled)
			{
				if(vfoc_enabled)
				{
					m = EWorkMode::SD_VFOC;
				}
				else
				{
					m = EWorkMode::SD_CLOSE;
				}
			}
			else
			{
				m = EWorkMode::SD_OPEN;
			}
		}
		WorkMode(m);
	}

	void TServo42D::EnableMode(const EEnableMode em)
	{
		const bool lax_response_prev = mbdev->bus->lax_response;
		mbdev->bus->lax_response = true;
		TScopeExit reset_lax_response([&]() { mbdev->bus->lax_response = lax_response_prev; });
		mbdev->WriteHoldingRegister(0x0085, (u16_t)em);
	}

	void TServo42D::WorkMode(const EWorkMode new_mode)
	{
		WriteDebug(U"TServo42D::WorkMode(new_mode=%d)", (int)new_mode);
		const bool lax_response_prev = mbdev->bus->lax_response;
		mbdev->bus->lax_response = true;
		TScopeExit reset_lax_response([&]() { mbdev->bus->lax_response = lax_response_prev; });
		mbdev->WriteHoldingRegister(0x0082, (u16_t)new_mode);
		driver.Invert(inverted);
	}

	void TServo42D::RebootFirmware()
	{
		WriteDebug(U"TServo42D::RebootFirmware()");
		mbdev->WriteHoldingRegister(0x0041, 1);
		TFiber::Sleep(2);
	}

	void TServo42D::VFOC(const bool state)
	{
		vfoc_enabled = state;
		UpdateWorkMode();
	}

	s32_t TServo42D::NumberOfPulses() const
	{
		u16_t regs[2];
		mbdev->ReadInputRegisters(0x0033, 2, regs);
		const u32_t raw = ((u32_t)regs[0] << 16) | (u32_t)regs[1];
		return (s32_t)raw;
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	// TLimitSwitch

	void TServo42D::TLimitSwitch::Enable(const EMotorDirection dir)
	{
		EL_NOT_IMPLEMENTED;
	}

	void TServo42D::TLimitSwitch::Disable()
	{
		EL_NOT_IMPLEMENTED;
	}

	bool TServo42D::TLimitSwitch::State() const
	{
		EL_NOT_IMPLEMENTED;
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	// TEncoder

	s64_t TServo42D::TEncoder::Position() const
	{
		u16_t regs[3];
		parent.mbdev->ReadInputRegisters(0x0035, 3, regs);

		u64_t raw = ((u64_t)regs[0] << 32) | ((u64_t)regs[1] << 16) | (u64_t)regs[2];
		if(raw & (1ULL << 47))
			raw |= 0xFFFF000000000000ULL;
		return -(s64_t)raw;
	}

	float TServo42D::TEncoder::RotorAngle() const
	{
		const s64_t steps_per_turn = StepsPerTurn();
		s64_t angle_steps = Position() % steps_per_turn;
		if(angle_steps < 0)
			angle_steps += steps_per_turn;
		return (float)angle_steps / (float)steps_per_turn;
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	// TStallDetector

	bool TServo42D::TStallDetector::Enabled(const bool state)
	{
		if(state)
			Reset();
		parent.mbdev->WriteHoldingRegister(0x0088, state ? 1 : 0);
		parent.stall_detection_enabled = state;
		if(!state)
			Reset();
		return parent.stall_detection_enabled;
	}

	EStallState TServo42D::TStallDetector::State() const
	{
		if(!parent.stall_detection_enabled)
			return EStallState::DISABLED;
		return (parent.mbdev->ReadInputRegister(0x003E) & 0x00FF) != 0 ? EStallState::STALLED : EStallState::CLEAR;
	}

	void TServo42D::TStallDetector::Reset() const
	{
		if((parent.mbdev->ReadInputRegister(0x003E) & 0x00FF) != 0)
			parent.mbdev->WriteHoldingRegister(0x003D, 1);
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	// TStepperDriver

	void TServo42D::TStepperDriver::Invert(const bool state)
	{
		parent.mbdev->WriteHoldingRegister(0x0086, state ? 1 : 0);
		parent.inverted = state;
	}

	bool TServo42D::TStepperDriver::Enabled(const bool state)
	{
		if(parent.en == nullptr)
		{
			WriteDebug(U"ignoring request to enable/disable driver since we have no EN pin assigned");
			return true;
		}
		parent.en->State(!state);
		return state;
	}

	bool TServo42D::TStepperDriver::Enabled() const
	{
		return parent.en ? !parent.en->State() : true;
	}

	static void ClampDriveAmperage(float& x, float max = 3.0)
	{
		x = Max(0.0f, Min(max, x));
	}

	drive_current_t TServo42D::TStepperDriver::Amperage(drive_current_t dc)
	{
		EL_ERROR(!std::isfinite(dc.run_max), TInvalidArgumentException, "dc.run_max", "run current must be finite");
		EL_ERROR(!std::isfinite(dc.hold_max), TInvalidArgumentException, "dc.hold_max", "hold current must be finite");
		ClampDriveAmperage(dc.run_max);
		const u16_t v_run = lroundf(dc.run_max * 1000.0f);
		const bool lax_response_prev = parent.mbdev->bus->lax_response;
		parent.mbdev->bus->lax_response = true;
		TScopeExit reset_lax_response([&]() { parent.mbdev->bus->lax_response = lax_response_prev; });
		parent.mbdev->WriteHoldingRegister(0x0083, v_run);
		dc.run_max = v_run / 1000.0f;

		if(!(parent.vfoc_enabled && parent.servo_enabled))
		{
			ClampDriveAmperage(dc.hold_max, dc.run_max * 0.9f);

			const u16_t idx_hold = dc.run_max > 0.0f ? (u16_t)Max(1L, Min(10L, lroundf(dc.hold_max / dc.run_max * 10.0f))) : 1;
			dc.hold_max = (float)idx_hold * dc.run_max / 10.0f;

			parent.mbdev->WriteHoldingRegister(0x009B, idx_hold - 1);
		}
		else
		{
			dc.hold_max = -1.0f;
		}

		dc.run_min = dc.run_max;
		dc.hold_min = dc.hold_max;
		parent.drive_current = dc;
		return dc;
	}

	EDriverState TServo42D::TStepperDriver::State() const
	{
		return parent.mbdev->ReadInputRegister(0x003A) ? EDriverState::OK : EDriverState::DISABLED;
	}

	void TServo42D::TStepperDriver::Step(const bool state)
	{
		EL_ERROR(parent.step == nullptr, TException, "no STEP pin was provided");
		EL_ERROR(parent.mc_enabled, TException, "Step/Dir interface is only available when the motion controller is disabled");
		parent.step->State(state);
	}

	void TServo42D::TStepperDriver::Direction(const EMotorDirection dir)
	{
		EL_ERROR(parent.dir == nullptr, TException, "no DIR pin was provided");
		EL_ERROR(parent.mc_enabled, TException, "Step/Dir interface is only available when the motion controller is disabled");

		if(parent.dir_inverted)
			parent.dir->State(dir == EMotorDirection::FORWARD);
		else
			parent.dir->State(dir == EMotorDirection::REVERSE);
	}

	EMotorDirection TServo42D::TStepperDriver::Direction() const
	{
		EL_ERROR(parent.dir == nullptr, TException, "no DIR pin was provided");
		EL_ERROR(parent.mc_enabled, TException, "Step/Dir interface is only available when the motion controller is disabled");
		return parent.dir->State() == parent.dir_inverted ? EMotorDirection::FORWARD : EMotorDirection::REVERSE;
	}

	u32_t TServo42D::TStepperDriver::Microsteps(u32_t divider)
	{
		divider = Max(1U, Min(256U, divider));
		WriteDebug(U"setting microstep divider to %d", divider);
		const bool lax_response_prev = parent.mbdev->bus->lax_response;
		parent.mbdev->bus->lax_response = true;
		TScopeExit reset_lax_response([&]() { parent.mbdev->bus->lax_response = lax_response_prev; });
		parent.mbdev->WriteHoldingRegister(0x0084, divider);
		parent.microsteps = divider;
		return divider;
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	// TServo

	bool TServo42D::TServo::Enabled(const bool state)
	{
		if(parent.servo_enabled != state)
		{
			WriteDebug(U"switching servo %B", state);
			parent.servo_enabled = state;
			parent.UpdateWorkMode();

			if(state)
				parent.WakeServoControlLoop();
		}
		return state;
	}

	bool TServo42D::TServo::Enabled() const
	{
		return parent.servo_enabled;
	}

	EServoState TServo42D::TServo::State() const
	{
		if(!parent.servo_enabled)
			return EServoState::DISABLED;
		switch(parent.fib_servo_control.State())
		{
			case EFiberState::FINISHED:
			case EFiberState::CRASHED:
			case EFiberState::KILLED:
				return EServoState::ERROR;

			default:
				break;
		}
		return EServoState::UNKNOWN;
	}

	const ILimitSwitch* TServo42D::TServo::LimitSwitch(const EMotorDirection dir) const
	{
		EL_NOT_IMPLEMENTED;
	}

	void TServo42D::TServo::TargetPosition(const s64_t abs_target)
	{
		// Validate synchronously so the non-blocking setter never accepts a target
		// which the asynchronous Servo42D motion controller cannot represent.
		(void)parent.ComputeServoAxisPosition(abs_target);
		parent.servo_target = abs_target;
		parent.WakeServoControlLoop();
	}

	void TServo42D::TServo::SetHome()
	{
		WriteDebug(U"TServo42D::TServo::SetHome()");

		// reset servo coordinate system
		// parent.mbdev->bus->lax_response = true;
		parent.mbdev->WriteHoldingRegister(0x0092, 1);
		// parent.mbdev->bus->lax_response = false;

		// read current encoder position
		parent.servo_offset = parent.encoder.Position();
		WriteDebug(U"set servo_offset=%d", parent.servo_offset);
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	// TMotionController

	void TServo42D::TMotionController::Enabled(const bool state)
	{
		if(parent.mc_enabled != state)
		{
			WriteDebug(U"switching motion-controller %B", state);
			parent.mc_enabled = state;
			parent.UpdateWorkMode();
		}
	}

	bool TServo42D::TMotionController::Enabled() const
	{
		return parent.mc_enabled;
	}

	EMotionControllerState TServo42D::TMotionController::State() const
	{
		if(!parent.mc_enabled)
			return EMotionControllerState::DISABLED;

		const u16_t s = parent.mbdev->ReadInputRegister(0x00F1);
		switch(s)
		{
			case 0: return EMotionControllerState::ERROR;
			case 1: return EMotionControllerState::IDLE;

			case 2:
			case 3:
			case 4:
			case 5:
			case 6: return EMotionControllerState::ACTIVE;

			default:
				 EL_THROW(TException, TString::Format(U"unknown motion controller state %d received", s));
		}
	}

	static u16_t GetRealMicrostepsUsedByMotionController(u16_t microsteps)
	{
		switch(microsteps)
		{
			case 1:
			case 256:
				microsteps = 24; break;
			case 16: break;
			case 32: break;
			case 64: break;
			default: microsteps = 16;	// yep the firmware really does just that
		}
		return microsteps;
	}

	static u16_t ComputeSpeedValue(const float max_speed, const u32_t steps_per_turn)
	{
		return Max(1L, Min(3000L, lround(max_speed * 60.0f / (float)steps_per_turn)));
	}

	static u8_t ComputeAccelerationValue(const float a_spss, const u32_t steps_per_turn)
	{
		// a_spss is in steps/s²
		if(a_spss <= 0.0)
			return 0;

		// "acc" determines the time difference between two speed increments
		// motion controller thinks in RPMs (not steps/s)
		// v(t2) = v(t1) + 1 (from the manual)
		// => Δv = 1 RPM = steps_per_turn / 60 steps/s
		// => Δt = t2 - t1 = time it takes to increment speed by 1RPM
		//
		// t2 - t1 = (256 - acc) * 50µs (from the manual)
		// Δt = (256 - acc) * 50µs
		// Δt / 50µs = 256 - acc
		// acc = 256 - (Δt / 50µs)
		// Δv = a * Δt (from 5th grade physics book)
		// => Δt = Δv / a

		// example (from the manual):
		// acc = 236
		// Δt = 1ms (according to the table in the manual)
		//
		// lets check that:
		// Δt = (256 - acc) * 50µs
		// Δt = (256 - 236) * 50µs
		// Δt = 20 * 50µs = 1000µs = 1ms
		// => example is correct

		// const double a = Max(1.307189542483660f, Min(333.3333333333333f, a_spss));
		const double delta_v = (double)steps_per_turn / 60.0;
		const double delta_t = delta_v / a_spss;
		const u8_t acc = Min(255L, Max(1L, 256L - lround(delta_t / 0.00005)));
		WriteDebug(U"ComputeAccelerationValue(a_spss = %d, steps_per_turn = %d):\tΔv = %d\tΔt = %d\t=> acc = %d", a_spss, steps_per_turn, delta_v, delta_t, acc);
		return acc;
	}

	static void SendRelativeMotionBySteps(TDevice& mbdev, const u8_t dir, const u8_t acc, const u16_t rpm, const u32_t pulses)
	{
		u16_t regs[4] =
		{
			(u16_t)(((u16_t)dir << 8) | acc),
			rpm,
			(u16_t)((pulses >> 16) & 0xFFFF),
			(u16_t)(pulses & 0xFFFF)
		};

		WriteDebug(U"SendRelativeMotionBySteps(dir = %d, acc = %d, rpm = %d, pos = %d)", dir, acc, rpm, pulses);
		mbdev.WriteHoldingRegisters(0x00FD, 4, regs);
	}

	static void SendAbsoluteMotionBySteps(TDevice& mbdev, const u16_t acc, const u16_t rpm, const s32_t pulses)
	{
		const u32_t raw_pulses = (u32_t)pulses;
		u16_t regs[4] =
		{
			acc,
			rpm,
			(u16_t)((raw_pulses >> 16) & 0xFFFF),
			(u16_t)(raw_pulses & 0xFFFF)
		};

		WriteDebug(U"SendAbsoluteMotionBySteps(acc = %d, rpm = %d, pos = %d)", acc, rpm, pulses);
		mbdev.WriteHoldingRegisters(0x00FE, 4, regs);
	}

	// static void SendRelativeMotionByEncoder(TDevice& mbdev, u16_t acc, u16_t rpm, s32_t rel_encoder)
	// {
	// 	union
	// 	{
	// 		u16_t regs[4];
	// 		struct
	// 		{
	// 			u16_t _acc;
	// 			u16_t _speed;
	// 			s32_t _axis;
	// 		};
	// 	};
 //
	// 	_acc = acc;
	// 	_speed = rpm;
	// 	_axis = rel_encoder;
	// 	Swap(regs[2], regs[3]);
 //
	// 	mbdev.WriteHoldingRegisters(0x00F4, 4, regs);
	// };
 //
	// static void SendRelativeMotionByEncoder(TDevice& mbdev, u16_t acc, u16_t rpm, s32_t abs_encoder)
	// {
	// 	union
	// 	{
	// 		u16_t regs[4];
	// 		struct
	// 		{
	// 			u16_t _acc;
	// 			u16_t _speed;
	// 			s32_t _axis;
	// 		};
	// 	};
 //
	// 	_acc = acc;
	// 	_speed = rpm;
	// 	_axis = abs_encoder;
	// 	Swap(regs[2], regs[3]);
 //
	// 	mbdev.WriteHoldingRegisters(0x00F5, 4, regs);
	// };

	void TServo42D::TMotionController::Goto(const s64_t absolute_position, const float accel, const float max_speed)
	{
		EL_ERROR(!parent.mc_enabled, TException, "motion controller is currently disabled");
		EL_ERROR(!std::isfinite(accel) || accel < 0.0f, TInvalidArgumentException, "accel", "acceleration must be finite and non-negative");
		EL_ERROR(!std::isfinite(max_speed) || max_speed <= 0.0f, TInvalidArgumentException, "max_speed", "max_speed must be finite and greater than zero");
		EL_ERROR(absolute_position != (s64_t)(s32_t)absolute_position, TInvalidArgumentException, "absolute_position", "value too large");

		const u32_t steps_per_turn = GetRealMicrostepsUsedByMotionController(parent.microsteps) * parent.full_step_resolution;

		SendAbsoluteMotionBySteps(
			*parent.mbdev,
			ComputeAccelerationValue(accel, steps_per_turn),
			ComputeSpeedValue(max_speed, steps_per_turn),
			absolute_position
		);
	}

	void TServo42D::TMotionController::Move(const s64_t relative_position, const float accel, const float max_speed)
	{
		EL_ERROR(!parent.mc_enabled, TException, "motion controller is currently disabled");
		EL_ERROR(!std::isfinite(accel) || accel < 0.0f, TInvalidArgumentException, "accel", "acceleration must be finite and non-negative");
		EL_ERROR(!std::isfinite(max_speed) || max_speed <= 0.0f, TInvalidArgumentException, "max_speed", "max_speed must be finite and greater than zero");
		EL_ERROR(relative_position < -0xFFFFFFFFLL || relative_position > 0xFFFFFFFFLL, TInvalidArgumentException, "relative_position", "value too large");

		const u32_t steps_per_turn = GetRealMicrostepsUsedByMotionController(parent.microsteps) * parent.full_step_resolution;

		SendRelativeMotionBySteps(
			*parent.mbdev,
			relative_position > 0 ? 1 : 0,
			ComputeAccelerationValue(accel, steps_per_turn),
			ComputeSpeedValue(max_speed, steps_per_turn),
			relative_position < 0 ? (u32_t)(-relative_position) : (u32_t)relative_position
		);
	}

	void TServo42D::TMotionController::Stop(const float accel)
	{
		if(parent.mc_enabled)
		{
			Enabled(false);
			Enabled(true);
		}
	}

	void TServo42D::TMotionController::GoHome()
	{
		EL_ERROR(!parent.mc_enabled, TException, "motion controller is currently disabled");
		EL_NOT_IMPLEMENTED;
	}

	void TServo42D::TMotionController::Run(const EMotorDirection _dir, const float accel, const float max_speed)
	{
		EL_ERROR(!parent.mc_enabled, TException, "motion controller is currently disabled");
		EL_ERROR(!std::isfinite(accel) || accel < 0.0f, TInvalidArgumentException, "accel", "acceleration must be finite and non-negative");
		EL_ERROR(!std::isfinite(max_speed) || max_speed <= 0.0f, TInvalidArgumentException, "max_speed", "max_speed must be finite and greater than zero");
		const u32_t steps_per_turn = parent.driver.StepsPerTurn();
		const u8_t dir = _dir == EMotorDirection::FORWARD ? 1 : 0;
		const u8_t acc = ComputeAccelerationValue(accel, steps_per_turn);
		const u16_t rpm = ComputeSpeedValue(max_speed, steps_per_turn);
		u16_t regs[2] = { (u16_t)(((u16_t)dir << 8) | acc), rpm };
		parent.mbdev->WriteHoldingRegisters(0x00F6, 2, regs);
	}
}
