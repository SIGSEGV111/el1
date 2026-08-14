#pragma once
#include "dev_motor.hpp"
#include "dev_modbus.hpp"

namespace el1::dev::motor::servo42d
{
	using namespace system::waitable;
	using namespace system::time;

	// vFOC = vector field-oriented control: uses encoder angle to align phase currents
	// for smooth, efficient torque control instead of simple step table driving.
	enum class EWorkMode : u8_t
	{
		SD_OPEN = 0,   // step/dir, open loop
		SD_CLOSE = 1,  // step/dir, closed loop driver, step table
		SD_VFOC = 2,   // step/dir, closed loop driver, vFOC

		MC_OPEN = 3,   // motion controller, open loop driver
		MC_CLOSE = 4,  // motion controller, closed loop driver, step table
		MC_VFOC = 5    // motion controller, closed loop driver, vFOC
	};

	// Selects how the external EN pin is interpreted by the Servo42D.
	enum class EEnableMode : u16_t
	{
		EN_LOW = 0,   // driver enabled when EN input is low (active-low)
		EN_HIGH = 1,  // driver enabled when EN input is high (active-high)
		ALWAYS = 2    // driver always enabled, EN input ignored
	};

	class TServo42D
	{
		friend class TLimitSwitch;
		friend class TEncoder;
		friend class TStepperDriver;
		friend class TStallDetector;
		friend class TServo;
		friend class TMotionController;

		protected:
			std::unique_ptr<modbus::TDevice> mbdev;
			std::unique_ptr<gpio::IPin> dir;
			std::unique_ptr<gpio::IPin> step;
			std::unique_ptr<gpio::IPin> en;
			s64_t servo_offset; // offset between encoder- and servo coordinate systems in encoder ticks
			s64_t servo_target;
			drive_current_t drive_current;
			const u16_t full_step_resolution;
			u16_t microsteps : 9;
			bool servo_enabled : 1;
			bool mc_enabled : 1;
			bool leave_enabled : 1;
			bool dir_inverted : 1;
			bool vfoc_enabled : 1;
			bool inverted : 1;
			bool stall_detection_enabled : 1;

			// Change underlying firmware work mode (SD_*/MC_* + open/closed/vFOC).
			void WorkMode(const EWorkMode new_mode);

			// Re-apply work mode after changes to flags (servo/motion/vFOC/enable).
			void UpdateWorkMode();
			void RebootFirmware();

		public:
			class helper_t
			{
				protected:
					TServo42D& parent;

					helper_t& operator=(const helper_t&) = delete;
					helper_t(helper_t&&) = delete;
					helper_t(const helper_t&) = delete;
					constexpr helper_t(TServo42D* const parent) : parent(*parent) {}
			};

			// Limit-switch wrapper around the Servo42D's limit / stop handling.
			class TLimitSwitch : public ILimitSwitch, helper_t
			{
				friend class TServo42D;
				TLimitSwitch(TServo42D* const parent) : helper_t(parent) {}

				public:
					// Enable limit switch handling for motion in the given direction.
					// A triggered switch in that direction will stop motion.
					void Enable(const EMotorDirection dir);

					// Disable limit switch.
					void Disable();

					// Read current logical state of the limit input (true = active).
					bool State() const final override EL_GETTER;

					// Event-based trigger is not implemented, polling only.
					const IWaitable* OnTrigger() const final override { return nullptr; }
			};

			// High-resolution absolute encoder adapter for the Servo42D.
			class TEncoder : public IRotaryEncoder, helper_t
			{
				friend class TServo42D;
				TEncoder(TServo42D* const parent) : helper_t(parent) {}

				public:
					// Return basic encoder health state (Servo42D reports only OK here).
					EEncoderState State() const final override { return EEncoderState::OK; }

					// Logical position in motor steps, including multi-turn, relative to home.
					s64_t Position() const final override EL_GETTER;

					// Encoder resolution per mechanical revolution (fixed 16384 counts).
					u64_t StepsPerTurn() const final override { return 16384; }

					// High-resolution rotor angle within one mechanical revolution.
					// Unit and range follow IRotaryEncoder convention (0..1).
					float RotorAngle() const final override EL_GETTER;
			};


			// Hardware locked-rotor detector exposed by the Servo42D firmware.
			class TStallDetector : public IStallDetector, helper_t
			{
				friend class TServo42D;
				TStallDetector(TServo42D* const parent) : helper_t(parent) {}

				public:
					// The Servo42D couples locked-rotor detection to its protection feature;
					// enabling this detector therefore also enables firmware stall protection.
					bool Enabled(const bool state) final override;
					bool Enabled() const final override { return parent.stall_detection_enabled; }
					EStallState State() const final override EL_GETTER;
					void Reset() const final override;
			};

			// Low-level stepper driver abstraction mapped onto the Servo42D.
			class TStepperDriver : public IStepperDriver, helper_t
			{
				friend class TServo42D;
				TStepperDriver(TServo42D* const parent) : helper_t(parent) {}

				public:
					void Invert(const bool state);
					bool Invert() const { return parent.inverted; }

					// Enable or disable the power stage (coils energized or not).
					bool Enabled(const bool state) final override;

					// Return whether the power stage is currently enabled.
					bool Enabled() const final override EL_GETTER;

					// Set the motor phase current in ampere.
					// Returns the effective current that will be used.
					drive_current_t Amperage(drive_current_t) final override;

					// Get the last configured motor phase current.
					drive_current_t Amperage() const final override { return parent.drive_current; }

					// The Servo42D does not expose power metering here.
					const IPowerMeter* PowerMeter() const final override { return nullptr; }

					// Read current driver state (enabled/disabled/fault) as seen by the wrapper.
					EDriverState State() const final override EL_GETTER;

					IStallDetector* StallDetector() final override { return &parent.stall_detector; }
					const IStallDetector* StallDetector() const final override { return &parent.stall_detector; }

					// Access the high-level servo abstraction associated with this driver.
					IServo* Servo() final override { return &parent.servo; }
					const IServo* Servo() const final override  { return &parent.servo; }

					bool StepDirectionAvailable() const final override { return parent.dir != nullptr && parent.step != nullptr; }

					// Generate a logical step signal (edge-driven). Caller toggles state.
					// Used in SD_* work modes.
					void Step(const bool state) final override;

					// Set the logical motion direction for subsequent Step() pulses.
					void Direction(const EMotorDirection dir) final override;

					// Read last commanded direction.
					EMotorDirection Direction() const final override EL_GETTER;

					// Minimum reliable high or low time for a STEP pulse.
					TTime MinimumStepPulseLength() const final override { return 0.00001; }

					// Native full-step resolution of the motor (e.g. 200 steps/rev).
					u32_t FullStepResolution() const final override { return parent.full_step_resolution; }

					// Configure microstep subdivision (e.g. 1, 2, 4, ...).
					// Returns the effective microstep value clamped to what the Servo42D supports.
					u32_t Microsteps(u32_t divider) final override EL_WARN_UNUSED_RESULT;

					// Return current microstep subdivision.
					u32_t Microsteps() const final override { return parent.microsteps; }
			};

			// High-level closed-loop servo wrapper around the Servo42D.
			class TServo : public IServo, helper_t
			{
				friend class TServo42D;
				TServo(TServo42D* const parent) : helper_t(parent) {}

				public:
					// Sets the zero-point of the internal coordinate system of the servo to the current encoder position.
					// This has no effect on this interface. This is only relevant for other programs that will be
					// using the servo afterwards and rely on the motion controller commands 0xF5 (mode4) and 0xFE (mode2).
					void SetHome();

					// Globally enable or disable the Servo42D servo function.
					// When disabled, the driver operates in open-loop mode.
					bool Enabled(const bool state) final override;

					// Return whether the servo is currently enabled.
					bool Enabled() const final override EL_GETTER;

					// Return coarse servo state; currently only DISABLED/UNKNOWN are used, since the Servo42D does not
					// report a status of the servo function.
					EServoState State() const final override EL_GETTER;

					// Return the limit switch for motion in the given direction, if available.
					// May return nullptr if no switch is configured.
					const ILimitSwitch* LimitSwitch(const EMotorDirection dir) const final override EL_GETTER;

					// Access the encoder used by the servo for position feedback.
					IEncoder* Encoder() final override { return &parent.encoder; }
					const IEncoder* Encoder() const final override { return &parent.encoder; }

					// Returns the currently set target position for the servo motor in encoder ticks
					s64_t TargetPosition() const final override { return parent.servo_target; }

					// Set a new closed-loop target position in encoder ticks.
					void TargetPosition(const s64_t pos) final override;

					// Access the associated motor driver abstraction.
					IMotorDriver& Driver() final override { return parent.driver; }
					const IMotorDriver& Driver() const final override { return parent.driver; }
			};

			// Wrapper around the Servo42D on-board motion controller.
			class TMotionController : public IMotionController, helper_t
			{
				friend class TServo42D;
				TMotionController(TServo42D* const parent) : helper_t(parent) {}

				public:
					// Enable or disable use of the internal trajectory generator (MC_* modes).
					// Must be enabled before calling Goto/Move/Run/Stop/GoHome.
					void Enabled(const bool state);

					// Return whether the internal motion controller is enabled.
					bool Enabled() const EL_GETTER;

					// Access the underlying driver used by the motion controller.
					const IMotorDriver& Driver() const final override { return parent.driver; }
					IMotorDriver& Driver() final override { return parent.driver; }

					// The built-in motion controller is "blind": it does not use encoder feedback
					// in its trajectory planner.
					IEncoder* Encoder() final override { return nullptr; }
					const IEncoder* Encoder() const final override { return nullptr; }

					// Return current motion controller state (idle, busy, error, ...).
					EMotionControllerState State() const final override EL_GETTER;

					// Plan and start a move to an absolute step position using the internal planner.
					// accel and max_speed follow IMotionController units (steps/s², steps/s).
					void Goto(const s64_t absolute_position, const float accel, const float max_speed) final override;

					// Plan and start a move relative to the current position in steps.
					void Move(const s64_t relative_position, const float accel, const float max_speed) final override;

					// Start a continuous run in the given direction with acceleration ramp.
					// The move continues until Stop() or Enabled(false) is requested.
					void Run(const EMotorDirection dir, const float accel, const float max_speed) final override;

					// Request a controlled stop using the given deceleration.
					void Stop(const float accel) final override;

					// Move to logical home (step 0) using the internal motion controller.
					// This uses the stored home_position; it does not perform a homing search.
					void GoHome() final override;
			};

			// Construct a Servo42D wrapper.
			// mbdev must remain valid for the lifetime of this object.
			// dir/step/en are the GPIO pins driving the Servo42D's external interface.
			// full_step_resolution is the motor's native steps per revolution.
			TServo42D(
				std::unique_ptr<modbus::TDevice> mbdev,
				std::unique_ptr<gpio::IPin> dir,
				std::unique_ptr<gpio::IPin> step,
				std::unique_ptr<gpio::IPin> en,
				const u16_t full_step_resolution = 200
			);

			// Destructor; stops motion, disables driver where possible and releases resources.
			~TServo42D();

			// Enable or disable vFOC (field-oriented control) modes.
			// When enabled, appropriate *_VFOC work modes are selected instead of step-table modes.
			void VFOC(const bool state);

			// Return true if vFOC is requested and the servo is enabled.
			bool VFOC() const { return vfoc_enabled && servo_enabled; }

			// Control whether the driver remains energized after ~TServo42D().
			void LeaveEnabled(const bool new_state) { leave_enabled = new_state; }

			// Configure how the external EN pin maps to driver enable (see EEnableMode).
			void EnableMode(const EEnableMode em);

			s32_t NumberOfPulses() const EL_GETTER;

			modbus::TDevice& ModbusDevice() EL_GETTER { return *mbdev; }

			// Public access to encoder, stall detector, driver, servo and motion-controller adapters.
			TEncoder encoder;
			TStallDetector stall_detector;
			TStepperDriver driver;
			TServo servo;
			TMotionController motion;
	};
}
