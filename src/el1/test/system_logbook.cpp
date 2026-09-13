#include <gtest/gtest.h>
#include <el1/system_logbook.hpp>

using namespace ::testing;

namespace logbook_test
{
	struct TObject
	{
		el1::io::types::u32_t id;
	};
}

namespace el1::system::logbook
{
	template<>
	obj_id_t GetObjectIdentity<logbook_test::TObject>(const logbook_test::TObject* const object)
	{
		return object->id;
	}
}

namespace
{
	using namespace el1::io::types;
	using namespace el1::io::collection::array;
	using namespace el1::io::collection::list;
	using namespace el1::io::text::string;
	using namespace el1::system::logbook;
	using namespace el1::system::task;

	struct TTestSink final : ILogSink
	{
		TList<byte_t> records;
		u64_t n_overwritten_events = 0;
		u64_t n_dropped_events = 0;
		u64_t n_write_calls = 0;
		const TThread* thread = nullptr;

		explicit TTestSink(const TLogFilter pass_through_filter = TLogFilter::None()) :
			ILogSink(pass_through_filter)
		{
		}

		void Write(const TThread* const thread, const array_t<const byte_t> records, const u64_t n_overwritten_events, const u64_t n_dropped_events) final override
		{
			this->thread = thread;
			this->records.Append(records);
			this->n_overwritten_events += n_overwritten_events;
			this->n_dropped_events += n_dropped_events;
			this->n_write_calls++;
		}

		const TLogRecord& Record(const usys_t index) const
		{
			usys_t offset = 0;
			for(usys_t i = 0; i <= index; i++)
			{
				EXPECT_LT(offset, records.Count());
				const auto* const record = reinterpret_cast<const TLogRecord*>(records.Data() + offset);
				if(i == index)
					return *record;
				offset += record->Size();
			}
			throw std::logic_error("unreachable");
		}
	};

	struct TSinkRegistration
	{
		ILogSink* const sink;
		explicit TSinkRegistration(ILogSink* const sink) : sink(sink) { TLogBook::RegisterSink(sink); }
		~TSinkRegistration() { TLogBook::UnregisterSink(sink); }
	};

	TEST(system_logbook, WriteLogFormatsSerializedArguments)
	{
		TTestSink sink;
		TSinkRegistration registration(&sink);
		TThread::Self()->FlightRecorder().Discard();

		WriteLog<ECategory::LIVENESS, EVerbosity::DEBUG, U"value=%d text=%q">(17, TString(U"hello"));
		TThread::Self()->FlightRecorder().Commit();

		ASSERT_GT(sink.records.Count(), 0U);
		const TLogRecord& record = sink.Record(0);
		EXPECT_EQ(record.site->category, ECategory::LIVENESS);
		EXPECT_EQ(record.site->verbosity, EVerbosity::DEBUG);
		EXPECT_NE(record.call_site, nullptr);
		EXPECT_EQ(record.object_id, 0U);
		EXPECT_EQ(record.site->FormatMessage(record), U"value=17 text='hello'");

		const auto args = record.site->Arguments(record);
		ASSERT_EQ(args.Count(), 2U);
		EXPECT_EQ(args[0].ToInteger<s64_t>(), 17);
		EXPECT_EQ(args[1].String(), U"hello");
	}

	TEST(system_logbook, ObjectIdentityIsCustomizable)
	{
		TTestSink sink;
		TSinkRegistration registration(&sink);
		TThread::Self()->FlightRecorder().Discard();
		const logbook_test::TObject object{123456U};

		WriteLog<ECategory::STATE_CHANGE, EVerbosity::OPERATIONAL, U"state=%d">(&object, 3);
		TThread::Self()->FlightRecorder().Commit();

		const TLogRecord& record = sink.Record(0);
		EXPECT_EQ(record.object_id, object.id);
		EXPECT_EQ(record.site->FormatMessage(record), U"[logbook_test::TObject@123456] state=3");
	}

	TEST(system_logbook, ReturnAddressDistinguishesCallSites)
	{
		TTestSink sink;
		TSinkRegistration registration(&sink);
		TThread::Self()->FlightRecorder().Discard();

		WriteLog<ECategory::PROGRESS, EVerbosity::DIAG, U"value=%d">(1);
		WriteLog<ECategory::PROGRESS, EVerbosity::DIAG, U"value=%d">(2);
		TThread::Self()->FlightRecorder().Commit();

		ASSERT_NE(sink.Record(0).call_site, nullptr);
		ASSERT_NE(sink.Record(1).call_site, nullptr);
		EXPECT_NE(sink.Record(0).call_site, sink.Record(1).call_site);
		EXPECT_EQ(sink.Record(0).site, sink.Record(1).site);
	}

	TEST(system_logbook, PassThroughFilterWritesImmediatelyAndCommitKeepsFullHistory)
	{
		TTestSink sink(TLogFilter::AtLeastVerbosity(EVerbosity::OPERATIONAL));
		TSinkRegistration registration(&sink);
		TThread::Self()->FlightRecorder().Discard();

		WriteLog<ECategory::LIVENESS, EVerbosity::DEBUG, U"debug=%d">(1);
		EXPECT_EQ(sink.n_write_calls, 0U);

		WriteLog<ECategory::STATE_CHANGE, EVerbosity::OPERATIONAL, U"operational=%d">(2);
		ASSERT_EQ(sink.n_write_calls, 1U);
		ASSERT_GT(sink.records.Count(), 0U);
		EXPECT_EQ(sink.Record(0).site->FormatMessage(sink.Record(0)), U"operational=2");

		TThread::Self()->FlightRecorder().Commit();

		ASSERT_EQ(sink.n_write_calls, 2U);
		ASSERT_EQ(sink.Record(1).site->FormatMessage(sink.Record(1)), U"debug=1");
		ASSERT_EQ(sink.Record(2).site->FormatMessage(sink.Record(2)), U"operational=2");
	}

	TEST(system_logbook, DiscardSuppressesCommit)
	{
		TTestSink sink;
		TSinkRegistration registration(&sink);
		using site_t = TLogSite<U"value=%d", void, s32_t>;
		static const site_t site(ECategory::LIVENESS, EVerbosity::TRACE);
		TFlightRecorder recorder(TThread::Self(), 256);

		recorder.Write(&site, reinterpret_cast<const void*>(0x1234), 0, 17);
		recorder.Discard();
		recorder.Commit();

		EXPECT_EQ(sink.records.Count(), 0U);
	}

	TEST(system_logbook, DestructorCommitsPendingRecords)
	{
		TTestSink sink;
		TSinkRegistration registration(&sink);
		using site_t = TLogSite<U"value=%d", void, s32_t>;
		static const site_t site(ECategory::LIVENESS, EVerbosity::TRACE);

		{
			TFlightRecorder recorder(TThread::Self(), 256);
			recorder.Write(&site, reinterpret_cast<const void*>(0x1234), 0, 17);
		}

		ASSERT_GT(sink.records.Count(), 0U);
		EXPECT_EQ(sink.Record(0).site->FormatMessage(sink.Record(0)), U"value=17");
	}

	TEST(system_logbook, RingBufferOverwritesCompleteOldRecords)
	{
		TTestSink sink;
		TSinkRegistration registration(&sink);
		using site_t = TLogSite<U"value=%d", void, s32_t>;
		static const site_t site(ECategory::LIVENESS, EVerbosity::TRACE);
		TFlightRecorder recorder(TThread::Self(), 128);

		for(s32_t i = 0; i < 20; i++)
			recorder.Write(&site, reinterpret_cast<const void*>(0x1234), 0, i);
		recorder.Commit();

		EXPECT_GT(sink.n_overwritten_events, 0U);
		EXPECT_GT(sink.records.Count(), 0U);
		usys_t offset = 0;
		while(offset < sink.records.Count())
		{
			const auto* const record = reinterpret_cast<const TLogRecord*>(sink.records.Data() + offset);
			ASSERT_LE(record->Size(), sink.records.Count() - offset);
			offset += record->Size();
		}
		EXPECT_EQ(offset, sink.records.Count());
	}
}
