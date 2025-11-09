#include <chrono>
#include <cstdint>

#if defined( _MSC_VER )
	#include <intrin.h>
	#define COMPILER_BARRIER() _ReadWriteBarrier()
	#define NOINLINE __declspec( noinline )
#else
	#define COMPILER_BARRIER() asm volatile( "" ::: "memory" )
	#define NOINLINE __attribute__( ( noinline ) )
#endif

namespace stk {
template <typename Duration>
NOINLINE void synthetic_work( const Duration& duration )
{
	// renamed to avoid conflict with time.h's clock_t
	using clock = std::chrono::high_resolution_clock;
	const auto start = clock::now();
	double     sink = 1.0;

	while( clock::now() - start < duration )
	{
		sink = sink * 1.0000001 + 1.0;
		COMPILER_BARRIER();
	}

	COMPILER_BARRIER();
}
}
