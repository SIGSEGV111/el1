#include <gtest/gtest.h>
#include <el1/dev_motor.hpp>

using namespace ::testing;

namespace
{
	using namespace el1::dev::motor;
	using namespace el1::io::types;
	using namespace el1::system::task;

	class TTestStallDetector : public IStallDetector
	{
		public:
			bool enabled = false;
			EStallState state = EStallState::DISABLED;
			unsigned reset_count = 0;

			bool Enabled(const bool new_state) final override
			{
				enabled = new_state;
				state = enabled ? EStallState::CLEAR : EStallState::DISABLED;
				return enabled;
			}

			bool Enabled() const final override
			{
				return enabled;
			}

			EStallState State() const final override
			{
				return state;
			}

			void Reset() const final override
			{
				const_cast<TTestStallDetector*>(this)->reset_count++;
			}
	};

	class TTestServo;

	class TTestMotorDriver : public IStepperDriver
	{
		public:
			bool enabled = false;
			drive_current_t current = {1.0f, 2.0f, 3.0f, 4.0f};
			TTestStallDetector stall_detector;
			TTestServo* servo = nullptr;
			bool step_direction_available = true;
			bool step_state = false;
			EMotorDirection direction = EMotorDirection::FORWARD;
			u32_t microsteps = 1;

			bool Enabled(const bool new_state) final override
			{
				enabled = new_state;
				return enabled;
			}

			bool Enabled() const final override
			{
				return enabled;
			}

			drive_current_t Amperage(const drive_current_t new_current) final override
			{
				current = new_current;
				return current;
			}

			drive_current_t Amperage() const final override
			{
				return current;
			}

			const IPowerMeter* PowerMeter() const final override
			{
				return nullptr;
			}

			EDriverState State() const final override
			{
				return enabled ? EDriverState::OK : EDriverState::DISABLED;
			}

			IStallDetector* StallDetector() final override
			{
				return &stall_detector;
			}

			const IStallDetector* StallDetector() const final override
			{
				return &stall_detector;
			}

			IServo* Servo() final override;
			const IServo* Servo() const final override;

			bool StepDirectionAvailable() const final override
			{
				return step_direction_available;
			}

			void Step(const bool state) final override
			{
				step_state = state;
			}

			void Direction(const EMotorDirection new_direction) final override
			{
				direction = new_direction;
			}

			EMotorDirection Direction() const final override
			{
				return direction;
			}

			el1::system::time::TTime MinimumStepPulseLength() const final override
			{
				return 0;
			}

			u32_t FullStepResolution() const final override
			{
				return 200;
			}

			u32_t Microsteps(const u32_t divider) final override
			{
				microsteps = divider;
				return microsteps;
			}

			u32_t Microsteps() const final override
			{
				return microsteps;
			}
	};

	class TTestEncoder : public IRotaryEncoder
	{
		public:
			s64_t position = 0;
			const u64_t steps_per_turn;

			explicit TTestEncoder(const u64_t steps_per_turn) : steps_per_turn(steps_per_turn)
			{
			}

			EEncoderState State() const final override
			{
				return EEncoderState::OK;
			}

			s64_t Position() const final override
			{
				return position;
			}

			u64_t StepsPerTurn() const final override
			{
				return steps_per_turn;
			}

			float RotorAngle() const final override
			{
				const s64_t modulus = (s64_t)steps_per_turn;
				s64_t remainder = position % modulus;
				if(remainder < 0)
					remainder += modulus;
				return (float)((double)remainder / (double)steps_per_turn);
			}
	};

	class TTestServo : public IServo
	{
		public:
			TTestMotorDriver& driver;
			TTestEncoder encoder;
			bool enabled = false;
			s64_t target_position = 0;
			unsigned target_write_count = 0;

			TTestServo(TTestMotorDriver& driver, const u64_t encoder_steps_per_turn) : driver(driver), encoder(encoder_steps_per_turn)
			{
				driver.servo = this;
			}

			bool Enabled(const bool new_state) final override
			{
				enabled = new_state;
				return enabled;
			}

			bool Enabled() const final override
			{
				return enabled;
			}

			EServoState State() const final override
			{
				return enabled ? EServoState::HOLD : EServoState::DISABLED;
			}

			const ILimitSwitch* LimitSwitch(const EMotorDirection) const final override
			{
				return nullptr;
			}

			IEncoder* Encoder() final override
			{
				return &encoder;
			}

			const IEncoder* Encoder() const final override
			{
				return &encoder;
			}

			s64_t TargetPosition() const final override
			{
				return target_position;
			}

			void TargetPosition(const s64_t new_position) final override
			{
				target_position = new_position;
				target_write_count++;
			}

			IMotorDriver& Driver() final override
			{
				return driver;
			}

			const IMotorDriver& Driver() const final override
			{
				return driver;
			}
	};

	IServo* TTestMotorDriver::Servo()
	{
		return servo;
	}

	const IServo* TTestMotorDriver::Servo() const
	{
		return servo;
	}

	static void Pulse(TStepperEmulation& stepper, const unsigned count)
	{
		for(unsigned i = 0; i < count; i++)
		{
			stepper.Step(true);
			stepper.Step(false);
		}
	}

	TEST(TStepperEmulation, ConvertsFullTurnsWithoutDrift)
	{
		TTestMotorDriver driver;
		TTestServo servo(driver, 16384);
		TStepperEmulation stepper(&servo, 200);

		EXPECT_TRUE(stepper.Enabled(true));
		EXPECT_EQ(stepper.Microsteps(16), 16U);

		Pulse(stepper, 200U * 16U);
		EXPECT_EQ(servo.TargetPosition(), 0);
		TFiber::Yield();
		EXPECT_EQ(servo.TargetPosition(), 16384);

		stepper.Direction(EMotorDirection::REVERSE);
		Pulse(stepper, 200U * 16U);
		TFiber::Yield();
		EXPECT_EQ(servo.TargetPosition(), 0);
	}

	TEST(TStepperEmulation, CountsOnlyRisingEdges)
	{
		TTestMotorDriver driver;
		TTestServo servo(driver, 200);
		TStepperEmulation stepper(&servo, 200);
		ASSERT_TRUE(stepper.Enabled(true));

		stepper.Step(true);
		stepper.Step(true);
		stepper.Step(true);
		stepper.Step(false);
		TFiber::Yield();

		EXPECT_EQ(servo.TargetPosition(), 1);
	}

	TEST(TStepperEmulation, IgnoresPulsesWhileDisabled)
	{
		TTestMotorDriver driver;
		TTestServo servo(driver, 200);
		TStepperEmulation stepper(&servo, 200);

		Pulse(stepper, 10);
		EXPECT_TRUE(stepper.Enabled(true));
		EXPECT_EQ(servo.TargetPosition(), 0);
	}

	TEST(TStepperEmulation, PreservesFractionalPhaseAcrossMicrostepChanges)
	{
		TTestMotorDriver driver;
		TTestServo servo(driver, 16384);
		TStepperEmulation stepper(&servo, 200);
		ASSERT_TRUE(stepper.Enabled(true));

		ASSERT_EQ(stepper.Microsteps(16), 16U);
		Pulse(stepper, 1);
		ASSERT_EQ(stepper.Microsteps(32), 32U);
		Pulse(stepper, 2);
		TFiber::Yield();

		// 1/3200 + 2/6400 turns = 1/1600 turn exactly.
		EXPECT_EQ(servo.TargetPosition(), 10);
	}

	TEST(TStepperEmulation, DelegatesDriverCapabilities)
	{
		TTestMotorDriver driver;
		TTestServo servo(driver, 200);
		TStepperEmulation stepper(&servo, 200);

		EXPECT_EQ(stepper.StallDetector(), &driver.stall_detector);
		EXPECT_EQ(stepper.Servo(), &servo);
		EXPECT_EQ(stepper.FullStepResolution(), 200U);
		EXPECT_EQ(stepper.MinimumStepPulseLength(), el1::system::time::TTime(0));

		const drive_current_t current = {5.0f, 6.0f, 7.0f, 8.0f};
		const drive_current_t actual = stepper.Amperage(current);
		EXPECT_FLOAT_EQ(actual.run_min, 5.0f);
		EXPECT_FLOAT_EQ(stepper.Amperage().hold_max, 8.0f);
	}

	TEST(TStepperEmulation, DisableCancelsQueuedTargetAndReanchors)
	{
		TTestMotorDriver driver;
		TTestServo servo(driver, 200);
		TStepperEmulation stepper(&servo, 200);
		ASSERT_TRUE(stepper.Enabled(true));

		Pulse(stepper, 10);
		EXPECT_EQ(servo.TargetPosition(), 0);
		EXPECT_FALSE(stepper.Enabled(false));
		EXPECT_EQ(servo.TargetPosition(), 0);

		ASSERT_TRUE(stepper.Enabled(true));
		EXPECT_EQ(servo.TargetPosition(), 0);
	}

	TEST(TStepperEmulation, GantryFallsBackWhenNativeStepDirectionIsUnavailable)
	{
		TTestMotorDriver driver;
		driver.step_direction_available = false;
		TTestServo servo(driver, 16384);
		TGantry gantry(1000, 400);

		gantry.AddMotor(&servo, false);

		EXPECT_TRUE(gantry.StepDirectionAvailable());
		EXPECT_EQ(gantry.FullStepResolution(), 400U);
	}

	TEST(IStallDetector, GantryAggregatesMemberDetectors)
	{
		TTestMotorDriver driver_a;
		TTestMotorDriver driver_b;
		TTestServo servo_a(driver_a, 200);
		TTestServo servo_b(driver_b, 200);
		TGantry gantry(1000);
		gantry.AddMotor(&driver_a, false);
		gantry.AddMotor(&driver_b, false);

		IStallDetector* const detector = gantry.StallDetector();
		ASSERT_NE(detector, nullptr);
		EXPECT_TRUE(detector->Enabled(true));
		EXPECT_EQ(detector->State(), EStallState::CLEAR);

		driver_b.stall_detector.state = EStallState::STALLED;
		EXPECT_TRUE(detector->Detected());

		detector->Reset();
		EXPECT_EQ(driver_a.stall_detector.reset_count, 1U);
		EXPECT_EQ(driver_b.stall_detector.reset_count, 1U);
	}

	TEST(TStepperEmulation, RejectsInvalidGeometry)
	{
		TTestMotorDriver driver;
		TTestServo servo(driver, 16384);

		EXPECT_ANY_THROW(TStepperEmulation(&servo, 0));
		EXPECT_ANY_THROW(TStepperEmulation(&servo, 200, 0));
		EXPECT_ANY_THROW(TStepperEmulation(&servo, 2, (u64_t)INT64_MAX));

		TStepperEmulation stepper(&servo, 200);
		EXPECT_ANY_THROW((void)stepper.Microsteps(0));

		TStepperEmulation wide_stepper(&servo, 1, (u64_t)INT64_MAX);
		EXPECT_ANY_THROW((void)wide_stepper.Microsteps(2));
	}

	TEST(TStepperEmulation, RejectsMicrostepChangeRequiringWideIntermediate)
	{
		TTestMotorDriver driver;
		TTestServo servo(driver, 2);
		TStepperEmulation stepper(&servo, UINT32_MAX, 2);
		ASSERT_TRUE(stepper.Enabled(true));

		Pulse(stepper, 1);
		EXPECT_ANY_THROW((void)stepper.Microsteps(1500000000U));
		EXPECT_EQ(stepper.Microsteps(), 1U);
	}

	TEST(TStepperEmulation, RejectsTargetPositionOverflow)
	{
		TTestMotorDriver driver;
		TTestServo servo(driver, 1);
		TStepperEmulation stepper(&servo, 1);

		servo.encoder.position = INT64_MAX;
		ASSERT_TRUE(stepper.Enabled(true));
		EXPECT_ANY_THROW(stepper.Step(true));
	}
}
