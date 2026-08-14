#pragma once

#include "def.hpp"
#include "io_types.hpp"
#include "dev_gpio.hpp"
#include "io_collection_list.hpp"

namespace el1::dev::motor
{
	using namespace io::types;
	using namespace system::time;
	struct IMotorDriver;
	struct IStallDetector;

	enum class EMotorDirection
	{
		FORWARD = 0,
		REVERSE = 1
	};

	EMotorDirection InvertDirection(EMotorDirection dir);
	bool IsEncoderMoving(const s64_t pos_now, const s64_t pos_prev, const EMotorDirection dir);

	enum class EDriverState
	{
		DISABLED,
		UNKNOWN,
		OK,
		OVERLOAD_SHUTDOWN,
		SHORT_CIRCUIT,
		POWER_FAILURE,
		IO_ERROR,
		OVER_TEMPERATURE,
		OVER_VOLTAGE,
		OVER_CURRENT
	};

	enum class EEncoderState
	{
		DISABLED,
		UNKNOWN,
		OK,
		NOT_HOMED,
		ERROR
	};

	enum class EServoState
	{
		DISABLED,
		UNKNOWN,
		HOLD,
		DRIVE_FORWARD,
		DRIVE_REVERSE,
		ERROR
	};

	enum class EMotionControllerState
	{
		DISABLED,
		UNKNOWN,
		IDLE,
		ACTIVE,
		ERROR
	};

	enum class EHBridgeMode
	{
		DRIVE_FORWARD,
		DRIVE_REVERSE,
		BRAKE,
		COAST
	};

	enum class EStallState
	{
		DISABLED,
		UNKNOWN,
		CLEAR,
		STALLED
	};

	struct drive_current_t
	{
		float run_min;
		float run_max;
		float hold_min;
		float hold_max;
	};

	// the encoder must move in the same direction as the motor (FORWARD => increment encoder, REVERSE => decrement encoder)
	struct IEncoder
	{
		virtual ~IEncoder() {}

		virtual EEncoderState State() const EL_GETTER = 0;
		virtual s64_t Position() const EL_GETTER = 0;
	};

	struct IRotaryEncoder : IEncoder
	{
		virtual u64_t StepsPerTurn() const EL_GETTER = 0;

		// range [0..1)
		virtual float RotorAngle() const EL_GETTER = 0;

		// shortest path between two positions
		static float AngleDifference(const float a1, const float a2) EL_GETTER;

		// path between two positions in given direction
		static float AngleDifference(const float from, const float to, const EMotorDirection dir) EL_GETTER;
	};

	struct IPowerMeter
	{
		virtual ~IPowerMeter() {}

		virtual float Amperage() const EL_GETTER = 0;
		virtual float Voltage() const EL_GETTER = 0;
		inline float Power() const { return Amperage() * Voltage(); }
		virtual float Consumption() const EL_GETTER = 0;
	};

	struct ILimitSwitch
	{
		virtual ~ILimitSwitch() {}

		virtual bool State() const EL_GETTER = 0;
		virtual const system::waitable::IWaitable* OnTrigger() const EL_GETTER = 0;
	};

	// Optional motor capability. Hardware may implement this using load measurement,
	// encoder following error, a DIAG pin, or a controller-internal locked-rotor detector.
	// Enabled() arms/disarms the detector. On hardware where detection is coupled to
	// automatic stall protection, arming may also enable that protection.
	struct IStallDetector
	{
		virtual ~IStallDetector() {}

		virtual bool Enabled(const bool state) EL_WARN_UNUSED_RESULT = 0;
		virtual bool Enabled() const EL_GETTER = 0;
		virtual EStallState State() const EL_GETTER = 0;
		inline bool Detected() const EL_GETTER { return State() == EStallState::STALLED; }
		virtual void Reset() const = 0;

		// Optional event source. Polling-only implementations return nullptr.
		virtual const system::waitable::IWaitable* OnStall() const EL_GETTER { return nullptr; }
	};

	struct IServo
	{
		virtual ~IServo() {}

		virtual bool Enabled(const bool new_state) EL_WARN_UNUSED_RESULT = 0;
		virtual bool Enabled() const EL_GETTER = 0;

		virtual EServoState State() const EL_GETTER = 0;
		virtual const ILimitSwitch* LimitSwitch(const EMotorDirection dir) const EL_GETTER = 0;

		virtual IEncoder* Encoder() EL_GETTER = 0;
		virtual const IEncoder* Encoder() const EL_GETTER = 0;

		// the functions must not block/sleep
		// all servo position are reported and set in encoder units (even for stepper motors)
		// if the servo is disabled, the TargetPosition() is stored and applied when the servo is enabled
		virtual s64_t TargetPosition() const EL_GETTER = 0;
		virtual void TargetPosition(const s64_t pos) = 0;
		inline void Move(const s64_t distance) { TargetPosition(TargetPosition() + distance); }
		virtual void Stop() { TargetPosition(CurrentPosition()); }

		// only supported with an encoder
		inline s64_t CurrentPosition() const EL_GETTER { EL_ERROR(Encoder() == nullptr, TNotImplementedException); return Encoder()->Position(); }
		inline s64_t FollowError() const EL_GETTER { return TargetPosition() - CurrentPosition(); }

		virtual IMotorDriver& Driver() = 0;
		virtual const IMotorDriver& Driver() const EL_GETTER = 0;
	};

	struct IMotorDriver
	{
		virtual ~IMotorDriver() {}

		virtual bool Enabled(const bool state) EL_WARN_UNUSED_RESULT = 0;
		virtual bool Enabled() const EL_GETTER = 0;

		virtual drive_current_t Amperage(drive_current_t) = 0;
		virtual drive_current_t Amperage() const EL_GETTER = 0;
		inline drive_current_t SimpleAmperage(const float a) { return Amperage({a,a,a,a}); }

		virtual const IPowerMeter* PowerMeter() const EL_GETTER = 0;

		virtual EDriverState State() const EL_GETTER = 0;

		virtual IStallDetector* StallDetector() { return nullptr; }
		virtual const IStallDetector* StallDetector() const EL_GETTER { return nullptr; }

		// Compatibility helpers for older users of the motor API.
		inline bool HasStallDetection() const EL_GETTER { return StallDetector() != nullptr; }
		inline bool StallDetected() const EL_GETTER { const IStallDetector* const detector = StallDetector(); return detector != nullptr && detector->Detected(); }
		inline void ResetStallDetection() const { const IStallDetector* const detector = StallDetector(); if(detector != nullptr) detector->Reset(); }

		virtual IServo* Servo() = 0;
		virtual const IServo* Servo() const EL_GETTER = 0;
	};

	struct IStepperDriver : IMotorDriver
	{
		virtual ~IStepperDriver() {}

		// Some combined servo/stepper devices expose an IStepperDriver object even when
		// no physical STEP/DIR interface is connected. Callers which can fall back to
		// another control path should check this capability first.
		virtual bool StepDirectionAvailable() const EL_GETTER { return true; }

		virtual void Step(const bool state) = 0;
		virtual void Direction(const EMotorDirection dir) = 0;
		virtual EMotorDirection Direction() const EL_GETTER = 0;
		virtual system::time::TTime MinimumStepPulseLength() const EL_GETTER = 0;

		virtual u32_t FullStepResolution() const EL_GETTER = 0;
		virtual u32_t Microsteps(u32_t divider) EL_WARN_UNUSED_RESULT = 0;
		virtual u32_t Microsteps() const EL_GETTER = 0;

		inline u64_t StepsPerTurn() const { return (u64_t)FullStepResolution() * (u64_t)Microsteps(); }

		void Steps(const u32_t n_step, system::time::TTime t_step, const bool quick = false);
		inline void Step() { Steps(1, 0, false); }
	};

	struct IHBridgeDriver : IMotorDriver
	{
		virtual ~IHBridgeDriver() {}

		// negative power_factor => reverse
		virtual float Drive(const float power_factor) = 0;
		virtual void Brake() = 0;
		virtual void Coast() = 0;
		virtual EHBridgeMode Mode() const EL_GETTER = 0;
	};

	struct IMotionController
	{
		virtual ~IMotionController() {}

		virtual const IMotorDriver& Driver() const EL_GETTER = 0;
		virtual IMotorDriver& Driver() EL_GETTER = 0;

		virtual IEncoder* Encoder() EL_GETTER = 0;
		virtual const IEncoder* Encoder() const EL_GETTER = 0;

		virtual EMotionControllerState State() const EL_GETTER = 0;

		// positions in motors units (e.g. steps for a IStepperDriver, or encoder units for a IServo)
		// the functions must not block/sleep
		virtual void Goto(const s64_t absolute_position, const float accel, const float max_speed) = 0;
		virtual void Move(const s64_t relative_position, const float accel, const float max_speed) = 0;
		virtual void Stop(const float accel) = 0;
		virtual void GoHome() = 0;
		virtual void Run(const EMotorDirection dir, const float accel, const float max_speed) = 0;
	};

	class TSoftServo : public IServo
	{
		protected:
			system::task::TFiber fib_control_loop;

		public:
			TSoftServo(IMotorDriver* const driver, IEncoder* const encoder);
	};

	class TStepperEmulation : public IStepperDriver
	{
		protected:
			IServo* const servo;
			IMotorDriver* const driver;
			const s64_t encoder_steps_per_turn;
			const u32_t full_step_resolution;
			u32_t microsteps;
			s64_t step_denominator;
			EMotorDirection direction;
			bool step_state;
			bool enabled;
			s64_t target_position;
			s64_t step_remainder;

		public:
			// The first overload derives the servo coordinate scale from IRotaryEncoder.
			TStepperEmulation(IServo* const servo, const u32_t full_step_resolution);
			TStepperEmulation(IServo* const servo, const u32_t full_step_resolution, const u64_t encoder_steps_per_turn);

			bool Enabled(const bool state) final override;
			bool Enabled() const final override EL_GETTER;
			drive_current_t Amperage(drive_current_t) final override;
			drive_current_t Amperage() const final override EL_GETTER;
			const IPowerMeter* PowerMeter() const final override EL_GETTER;
			EDriverState State() const final override EL_GETTER;
			IStallDetector* StallDetector() final override;
			const IStallDetector* StallDetector() const final override EL_GETTER;
			IServo* Servo() final override;
			const IServo* Servo() const final override EL_GETTER;
			void Step(const bool state) final override;
			void Direction(const EMotorDirection dir) final override;
			EMotorDirection Direction() const final override EL_GETTER;
			system::time::TTime MinimumStepPulseLength() const final override EL_GETTER;
			u32_t FullStepResolution() const final override EL_GETTER;
			u32_t Microsteps(u32_t divider) final override EL_WARN_UNUSED_RESULT;
			u32_t Microsteps() const final override EL_GETTER;
	};

	class IStepperGroup : public IStepperDriver
	{
		protected:
			class TGroupStallDetector : public IStallDetector
			{
				protected:
					IStepperGroup* const parent;

				public:
					constexpr explicit TGroupStallDetector(IStepperGroup* const parent) : parent(parent) {}
					bool Enabled(const bool state) final override;
					bool Enabled() const final override EL_GETTER;
					EStallState State() const final override EL_GETTER;
					void Reset() const final override;
			};

			TGroupStallDetector group_stall_detector;
			virtual IStepperDriver* GroupMotor(const usys_t index) const = 0;

		public:
			constexpr IStepperGroup() : group_stall_detector(this) {}
			bool Enabled(const bool state) override EL_WARN_UNUSED_RESULT;
			bool Enabled() const override EL_GETTER;
			drive_current_t Amperage(drive_current_t) override;
			drive_current_t Amperage() const override EL_GETTER;
			const IPowerMeter* PowerMeter() const override EL_GETTER;
			EDriverState State() const override EL_GETTER;
			IStallDetector* StallDetector() override;
			const IStallDetector* StallDetector() const override EL_GETTER;
			IServo* Servo() override;
			const IServo* Servo() const override EL_GETTER;
			bool StepDirectionAvailable() const override EL_GETTER;
			void Step(const bool state) override;
			void Direction(const EMotorDirection dir) override;
			EMotorDirection Direction() const override EL_GETTER;
			system::time::TTime MinimumStepPulseLength() const override EL_GETTER;
			u32_t FullStepResolution() const override EL_GETTER;
			u32_t Microsteps(u32_t divider) override EL_WARN_UNUSED_RESULT;
			u32_t Microsteps() const override EL_GETTER;
	};

	class TGantry : public IStepperGroup
	{
		protected:
			struct motor_info_t
			{
				IStepperDriver* stepper;
				IServo* servo;
				IEncoder* encoder;
				bool inverted;
				bool delete_stepper;
				bool enabled;
			};

			io::collection::list::TList<motor_info_t> motors;

			template<typename L>
			void MoveWhile(const TTime& t_pulse, const TTime& t_delay, L lambda);

			IStepperDriver* GroupMotor(const usys_t index) const final override;

		public:
			u32_t axis_length;
			u32_t emulated_fsr;

			constexpr TGantry(const u32_t axis_length_steps, const u32_t emulated_full_step_resolution = 200) : axis_length(axis_length_steps), emulated_fsr(emulated_full_step_resolution) {}
			TGantry(TGantry&&) = default;
			TGantry(const TGantry&) = delete;
			~TGantry();

			// order in which motors are added does have effect on algoithms used
			// changing order can affect which encoder is choosen as reference!
			void AddMotor(IStepperDriver* const stepper, const bool inverted);
			void AddMotor(IServo* const servo, const bool inverted);

			// sets all motors direction taking any inverted flags into account
			void Direction(const EMotorDirection gantry_dir) final override;
			EMotorDirection Direction() const final override EL_GETTER;

			/**
			* @brief Squares the gantry by referencing all driven sides against a common mechanical hard stop.
			*
			* @details
			* @par Why this is required
			* A gantry axis driven by multiple independent motors can rack (become skewed) due to missed steps,
			* asymmetric loads, manual motion during power-down, or compliance in belts/couplers/structure.
			* Even if identical motion is commanded, each side may end up at a different absolute position.
			* This procedure removes the relative offset so the bridge becomes orthogonal to the linear guides again.
			*
			* @par How it works
			* All drives are commanded synchronously toward a rigid hard stop through a STEP/DIR abstraction.
			* Native steppers are driven directly; servos without a usable STEP/DIR interface are adapted through
			* @c TStepperEmulation and therefore remain closed-loop while following the emulated step position.
			* When a drive reaches the stop it stalls; on stall detection that drive is disabled so it no longer pushes
			* and cannot further twist the gantry. Remaining drives continue until they also contact the stop and stall.
			* Once all drives have stalled, all sides are physically constrained to the same reference line and the gantry
			* is squared.
			*
			* @par Notes
			* This method depends on a repeatable, sufficiently stiff hard stop and reliable stall detection.
			* It intentionally uses the mechanics as the reference because motor position alone cannot guarantee squareness
			* on a multi-drive axis.
			*
			* @param speed Signed speed in steps per second. The sign selects the approach direction. Must be non-zero.
			*/
			void AlignSquare(const double speed);

			/**
			* @brief Finds a repeatable home position using a hard stop and the motor's rotary encoder angle.
			*
			* @details
			* The axis is first driven into a rigid mechanical hard stop to force a deterministic mechanical reference
			* (eliminating backlash, compliance, and lost-step ambiguity). It then reverses away from the stop while
			* monitoring the motor shaft angle reported by the first rotary encoder. Motion stops when the measured shaft angle
			* passes @p target_angle, and that position is taken as home.
			*
			* @par Why this is useful
			* Rotary encoders often provide a consistent shaft angle across power cycles even when the multi-turn
			* position is unknown. The stall at the hard stop yields a known turn position and backing off
			* to a specific encoder angle yields a very repeatable home offset. This is often more repeatable than backing off
			* by a fixed distance because it compensates for variation in stall point and drivetrain elasticity.
			*
			* @par How it works (high level)
			* - Move toward the hard stop at @p speed until a stall/hard-stop condition is detected.
			* - Reverse direction and step out of the stop.
			* - Monitor encoder angle during reverse motion; stop when the angle passes @p target_angle.
			*
			* @param speed Signed speed in steps per second. The sign selects the approach direction. Must be non-zero.
			* @param target_angle Desired encoder shaft angle at the final home position.
			*/
			void FindHomeRotaryEncoderHardstop(const double speed, const float target_angle = 0.0f, const unsigned n_iter = 5);

	};

	u32_t FindHomeEndstop(IStepperDriver& motor, gpio::IPin& endstop, const EMotorDirection seek_dir);
}
