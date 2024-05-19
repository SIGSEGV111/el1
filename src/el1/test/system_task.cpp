#include <gtest/gtest.h>
#include <el1/system_task.hpp>
#include <el1/system_time_timer.hpp>
#include <el1/io_collection_map.hpp>
#include "util.hpp"

using namespace ::testing;

namespace
{
	using namespace el1::system::task;
	using namespace el1::system::time::timer;
	using namespace el1::util::function;
	using namespace el1::io::collection::map;
	using namespace el1::testing;
	using namespace el1::error;

	struct TInit
	{
		TInit()
		{
			// TFiber::DEBUG = true;
			TFiber::FIBER_DEFAULT_STACK_SIZE_BYTES = 16 * 1024;
			TProcess::WARN_NONZERO_EXIT_IN_DESTRUCTOR = false;
		}
	};

	TInit __init;

	struct TSlowThread
	{
		bool* status;
		TThread thread;

		void ThreadMain()
		{
			usleep(1000);
			*status = true;
		}

		TSlowThread(bool* status) : status(status), thread("test", TFunction(this, &TSlowThread::ThreadMain), false)
		{
			this->thread.Start();
		}

		~TSlowThread()
		{
			this->thread.Shutdown();
			auto e = this->thread.Join();
			if(e)
				e->Print("TSlowThread terminated with exception");
		}
	};

	struct TSlowConstructorThread
	{
		bool* status;
		TThread thread;

		void ThreadMain()
		{
		}

		TSlowConstructorThread(bool* status) : status(status), thread("test", TFunction(this, &TSlowConstructorThread::ThreadMain))
		{
			usleep(1000);
		}

		~TSlowConstructorThread()
		{
			auto exception = this->thread.Join();
			*status = (exception == nullptr);
		}
	};

	struct TThrowThread
	{
		bool* status;
		TThread thread;

		void ThreadMain()
		{
			EL_THROW(TDebugException, 17);
		}

		TThrowThread(bool* status) : status(status), thread("test", TFunction(this, &TThrowThread::ThreadMain))
		{
			usleep(1000);
		}

		~TThrowThread()
		{
			auto exception = this->thread.Join();
			if(exception != nullptr)
			{
				const TDebugException* const dbge = dynamic_cast<const TDebugException*>(exception.get());
				if(dbge)
				{
					*status = dbge->number == 17;
				}
			}
		}
	};

	struct TStopThread
	{
		TThread thread;

		void ThreadMain()
		{
			this->thread.Stop();
		}

		TStopThread() : thread("test", TFunction(this, &TStopThread::ThreadMain))
		{
		}

		~TStopThread()
		{
			auto e = this->thread.Join();
			if(e)
				e->Print("TStopThread terminated with exception");
		}
	};

	TEST(system_task, TThread_Construct)
	{
		bool status;

		{
			status = false;
			TSlowConstructorThread test_thread(&status);
		}
		EXPECT_TRUE(status);

		{
			status = false;
			TThrowThread test_thread(&status);
		}
		EXPECT_TRUE(status);

		{
			status = false;
			TSlowThread test_thread(&status);
		}
		EXPECT_TRUE(status);

		{
			status = false;
			TThread thread("test", [&](){
				status = true;
			});
		}
		EXPECT_TRUE(status);
	}

	TEST(system_task, TFiber_Construct)
	{
		bool status;
		{
			status = false;
			TFiber f1([&](){ status = true; });
		}
		EXPECT_TRUE(status);

		{
			status = false;
			TFiber f1([&](){ status = true; });
			TFiber f2([&](){ status = false; }, false);
		}
		EXPECT_TRUE(status);

		{
			status = false;
			TFiber f1([&](){ status = true; });
			TFiber f2([&](){ status = false; }, false);
			f2.Start();
			f1.SwitchTo();
			f2.SwitchTo();
		}
		EXPECT_FALSE(status);
	}

// 	TEST(system_task, TFiber_cascade_fibers)
// 	{
// 		bool status;
// 		{
// 			status = false;
//
// 			auto l3 = [&](){
// 				status = true;
// 			};
//
// 			auto l2 = [&](){
//
// 				TFiber f3(l3);
// 				f3.Join();
// 			};
//
// 			auto l1 = [&](){
//
// 				TFiber f2(l2);
// 				f2.Join();
// 			};
//
// 			TFiber f1(l1);
// 			f1.Join();
// 		}
// 		EXPECT_TRUE(status);
// 	}

	TEST(system_task, TFiber_libc_compat)
	{
		bool status = false;

		TFiber f1([&](){
			char* p1 = (char*)malloc(8);
			int* p2 = new int(5);

			strcpy(p1, "foobar");

			fprintf(stderr, "[libc_compat]: fprintf() on stderr (p1 = %s, p2 = %d)\n", p1, *p2);
			fprintf(stdout, "[libc_compat]: fprintf() on stdout (p1 = %s, p2 = %d)\n", p1, *p2);

			fprintf(stderr, "here 1\n");
			free(p1);
			fprintf(stderr, "here 2\n");
			delete p2;

			fprintf(stderr, "here 3\n");
			volatile byte_t large_buffer[4096];
			for(usys_t i = 0; i < sizeof(large_buffer); i++)
				large_buffer[i] = 0xff;

			fprintf(stderr, "here 4\n");
			status = (large_buffer[sizeof(large_buffer) - 1] == 0xff);
		});

		f1.SwitchTo();

		EXPECT_TRUE(status);
	}

	static void Checkpoint(unsigned& counter, const unsigned num)
	{
		counter++;
		EXPECT_EQ(counter, num);
	}

	TEST(system_task, TFiber_multiple)
	{
		unsigned counter = 0;

		TFiber a([&](){
// 			std::cerr<<"Fiber A: alive!\n";
			Checkpoint(counter, 6);
			TTimer timer(EClock::MONOTONIC, 0.2);
// 			std::cerr<<"Fiber A: waiting for tick...\n";
			Checkpoint(counter, 7);
			timer.OnTick().WaitFor();
// 			std::cerr<<"Fiber A: done with wait => shutting down\n";
			Checkpoint(counter, 10);
		});
// 		std::cerr<<"Fiber A started\n";
		Checkpoint(counter, 1);

		TFiber b([&](){
// 			std::cerr<<"Fiber B: alive!\n";
			Checkpoint(counter, 4);
			TTimer timer(EClock::MONOTONIC, 0.1);
// 			std::cerr<<"Fiber B: waiting for tick...\n";
			Checkpoint(counter, 5);
			timer.OnTick().WaitFor();
// 			std::cerr<<"Fiber B: done with wait => shutting down\n";
			Checkpoint(counter, 8);
		});
// 		std::cerr<<"Fiber B started\n";
		Checkpoint(counter, 2);

// 		std::cerr<<"joining with B\n";
		Checkpoint(counter, 3);
		if(auto e = b.Join())
			e->Print("fiber B terminated with exception");
// 		std::cerr<<"joining with A\n";
		Checkpoint(counter, 9);
		if(auto e = a.Join())
			e->Print("fiber A terminated with exception");
// 		std::cerr<<"main: all joined => returning\n";
		Checkpoint(counter, 11);
	}

	TEST(system_task, TFiber_shutdown)
	{
		TFiber a([](){
			TFiber::Self()->Stop();
		});

		a.SwitchTo();
	}

	TEST(system_task, EnvironmentVariables)
	{
		EXPECT_TRUE(EnvironmentVariables().Items().Count() > 0U);
		EXPECT_TRUE(EnvironmentVariables().Get("PATH") != nullptr);
		EXPECT_TRUE(EnvironmentVariables()["PATH"].chars.Count() > 0U);
	}

// 	TEST(system_task, TThread_StopResume)
// 	{
// 		TStopThread th;
//
// 		th.On
// 		th.Resume();
// 	}

	TEST(system_task, TThread_TaskState)
	{
		volatile int state = 0;
		TSimpleMutex mtx;
		mtx.Acquire();

		TThread t("test", [&](){
			mtx.Acquire();
			while(state == 0); // busy loop
		});

		TFiber::Sleep(0.1);
		EXPECT_EQ(t.TaskState(), ETaskState::BLOCKED);
		mtx.Release();
		TFiber::Sleep(0.1);
		EXPECT_EQ(t.TaskState(), ETaskState::RUNNING);
		state = 1;
		TFiber::Sleep(0.1);
		if(auto e = t.Join())
			throw e;
		EXPECT_EQ(t.TaskState(), ETaskState::NOT_CREATED);
	}

	TEST(system_task, TProcess_simple_invoke)
	{
		{
			TProcess proc1("/bin/echo", { "hello world!" } );
			EXPECT_EQ(proc1.Join(), 0);
		}

		{
			TProcess proc1("/bin/sleep", { "1h" } );
		}

		{
			TProcess proc1("/bin/sleep", { "1h" } );
			usleep(100000UL);
			proc1.Shutdown();
			EXPECT_TRUE(proc1.Join() < 0);
		}
	}

	TEST(system_task, TProcess_simple_pipe)
	{
		TSortedMap<fd_t, TProcess::EFDIO> stream_map;
		stream_map.Add(0, TProcess::EFDIO::PIPE_PARENT_TO_CHILD);
		stream_map.Add(1, TProcess::EFDIO::PIPE_CHILD_TO_PARENT);
		stream_map.Add(2, TProcess::EFDIO::INHERIT);

		TProcess proc1("/bin/tac", {}, stream_map);

		EXPECT_EQ(proc1.SendStream(0)->Write((const byte_t*)"hello world\nfoobar\n", 19U), 19U);
		proc1.SendStream(0)->Close();

		byte_t buffer[32] = {};
		byte_t* w = (byte_t*)buffer;

		while(w < buffer + sizeof(buffer) - 1)
		{
			const usys_t r = proc1.ReceiveStream(1)->Read(w, sizeof(buffer) - 1 - (w - buffer));
			if(r == 0)
			{
				if(proc1.ReceiveStream(1)->OnInputReady() == nullptr)
					break;

				proc1.ReceiveStream(1)->OnInputReady()->WaitFor();
				continue;
			}

			w += r;
		}

		EXPECT_EQ(TString("foobar\nhello world\n"), (char*)buffer);

		EXPECT_EQ(proc1.Join(), 0);
	}

	TEST(system_task, TProcess_FDIO)
	{
		{
			TSortedMap<fd_t, TProcess::EFDIO> stream_map;
			stream_map.Add(0, TProcess::EFDIO::ZERO);
			stream_map.Add(1, TProcess::EFDIO::DISCARD);
			stream_map.Add(2, TProcess::EFDIO::INHERIT);

			TProcess proc1("/bin/head", { "-c", "128" }, stream_map);
			EXPECT_EQ(proc1.Join(), 0);
		}

		{
			TSortedMap<fd_t, TProcess::EFDIO> stream_map;
			stream_map.Add(0, TProcess::EFDIO::RANDOM);
			stream_map.Add(1, TProcess::EFDIO::DISCARD);
			stream_map.Add(2, TProcess::EFDIO::INHERIT);

			TProcess proc1("/bin/head", { "-c", "128" }, stream_map);
			EXPECT_EQ(proc1.Join(), 0);
		}

		{
			TSortedMap<fd_t, TProcess::EFDIO> stream_map;
			stream_map.Add(0, TProcess::EFDIO::EMPTY);
			stream_map.Add(1, TProcess::EFDIO::DISCARD);
			stream_map.Add(2, TProcess::EFDIO::INHERIT);

			TProcess proc1("/bin/head", { "-c", "128" }, stream_map);
			EXPECT_EQ(proc1.Join(), 0);
		}
	}

	TEST(system_task, TProcess_stop_resume)
	{
		{
			TProcess proc1("/bin/sleep", { "1h" } );
			usleep(100000UL);
			proc1.Stop();
			proc1.Shutdown();
			EXPECT_TRUE(proc1.Join() < 0);
		}

		{
			TProcess proc1("/bin/sleep", { "1h" } );
			usleep(100000UL);
			proc1.Stop();
		}
	}

	TEST(system_task, TProcess_Execute)
	{
		{
			const TString stdout = TProcess::Execute("/bin/echo", { "hello world" });
			EXPECT_EQ(stdout, "hello world\n");
		}

		{
			TString stdin = "hello world foobar";
			const TString stdout = TProcess::Execute("/bin/cat", {}, &stdin, nullptr, 10);
			EXPECT_EQ(stdout, "hello world foobar");
		}

		{
			TString stdin = "hello world foobar";
			TString stderr;
			const TString stdout = TProcess::Execute("/bin/bash", { "-euc", "echo text_on_stderr 1>&2; echo text_on_stdout" }, &stdin, &stderr, 10);
			EXPECT_EQ(stdout, "text_on_stdout\n");
			EXPECT_EQ(stderr, "text_on_stderr\n");
		}

		{
			EXPECT_THROW(TProcess::Execute("/bin/sleep", { "1m" }, nullptr, nullptr, 1), TProcess::TTimeoutException);
			EXPECT_THROW(TProcess::Execute("/bin/false"), TProcess::TNonZeroExitException);
		}
	}

	TEST(system_task, TFiber_Sleep)
	{
		{
			const TTime t_sleep = TTime(0,100000000000000000ULL);
			const TTime ts_start = TTime::Now(EClock::MONOTONIC);
			TFiber::Sleep(t_sleep);
			const TTime ts_end = TTime::Now(EClock::MONOTONIC);
			const TTime delta_time = ts_end - ts_start;
			EXPECT_GE(delta_time, t_sleep);
			EXPECT_LE(delta_time, t_sleep * 2LL);
		}

		{
			const TTime t_sleep = TTime(0,500000000000000000ULL);
			const TTime ts_start = TTime::Now(EClock::MONOTONIC);
			TFiber::Sleep(t_sleep);
			const TTime ts_end = TTime::Now(EClock::MONOTONIC);
			const TTime delta_time = ts_end - ts_start;
			EXPECT_GE(delta_time, t_sleep);
			EXPECT_LE(delta_time, t_sleep * 2LL);
		}
	}
}
