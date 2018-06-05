### STK Multi-threading

The stk now has the following facilities to support concurrent programming:

* [Concurrent hash-map via junction library.](#junction-hash)[<sup>[1]</sup>](https://github.com/preshing/junction)
* [Just about every other type of concurrent data structure via libcds](#libcds).[<sup>[2]</sup>](https://github.com/khizmax/libcds)
* [Active Objects](#active-objects)
* [Facilities to support multiple flavors of fiber based concurrency (based on boost fibers)](#fibers)
* [Thread-Pools](#thread-pools)
* [Tiny Spinlock](#tiny-spinlock)
* [Thread local data with object scoped lifetime](#thread-local)

#### Junction Hash

The Junction library contains lock-free hash-map implementations which work on integer sized key and value data. This means you can store integers or raw pointers in a junction hash-map. Documentation on the use of the library can be found [here](https://github.com/preshing/junction.git). Here is an example use case for a hash-grid:

```C++
#include <geometrix/numeric/number_comparison_policy.hpp>
#include <geometrix/algorithm/fast_voxel_grid_traversal.hpp>
#include <junction/ConcurrentMap_Leapfrog.h>
#include <stk/random/xorshift1024starphi_generator.hpp>
#include <stk/geometry/geometry_kernel.hpp>
#include <stk/utility/compressed_integer_pair.hpp>
#include <stk/utility/boost_unique_ptr.hpp>
#include <junction/Core.h>
#include <turf/Util.h>

namespace stk {
    struct compressed_integer_pair_key_traits
    {
        typedef std::uint64_t Key;
        typedef typename turf::util::BestFit<std::uint64_t>::Unsigned Hash;
        static const Key NullKey = (std::numeric_limits<Key>::max)();
        static const Hash NullHash = Hash((std::numeric_limits<Key>::max)());
        static Hash hash(Key key)
        {
            return turf::util::avalanche(Hash(key));
        }
        static Key dehash(Hash hash)
        {
            return (Key)turf::util::deavalanche(hash);
        }
    };

    template <typename T>
    struct pointer_value_traits
    {
        typedef T* Value;
        typedef typename turf::util::BestFit<T*>::Unsigned IntType;
        static const IntType NullValue = 0;
        static const IntType Redirect = 1;
    };

    template<typename Data, typename GridTraits>
    class concurrent_hash_grid_2d
    {
    public:

        typedef Data* data_ptr;
        typedef GridTraits traits_type;
        typedef stk::compressed_integer_pair key_type;
        typedef junction::ConcurrentMap_Leapfrog<std::uint64_t, Data*, stk::compressed_integer_pair_key_traits, stk::pointer_value_traits<Data>> grid_type;

        concurrent_hash_grid_2d( const GridTraits& traits )
            : m_gridTraits(traits)
            , m_grid()
        {}

        ~concurrent_hash_grid_2d()
        {
            quiesce();
            clear();
        }

        template <typename Point>
        data_ptr find_cell(const Point& point) const
        {
            BOOST_CONCEPT_ASSERT( (geometrix::Point2DConcept<Point>) );
            GEOMETRIX_ASSERT( is_contained( point ) );
            boost::uint32_t i = m_gridTraits.get_x_index(geometrix::get<0>(point));
            boost::uint32_t j = m_gridTraits.get_y_index(geometrix::get<1>(point));
            return find_cell(i,j);
        }

        data_ptr find_cell(boost::uint32_t i, boost::uint32_t j) const
        {
            stk::compressed_integer_pair p = { i, j };
            auto iter = m_grid.find(p.to_uint64());
            GEOMETRIX_ASSERT(iter.getValue() != (data_ptr)stk::pointer_value_traits<Data>::Redirect);
            return iter.getValue();
        }

        template <typename Point>
        Data& get_cell(const Point& point)
        {
            BOOST_CONCEPT_ASSERT( (geometrix::Point2DConcept<Point>) );
            GEOMETRIX_ASSERT( is_contained( point ) );
            boost::uint32_t i = m_gridTraits.get_x_index(geometrix::get<0>(point));
            boost::uint32_t j = m_gridTraits.get_y_index(geometrix::get<1>(point));
            return get_cell(i,j);
        }

        Data& get_cell(boost::uint32_t i, boost::uint32_t j)
        {
            stk::compressed_integer_pair p = { i, j };
            auto mutator = m_grid.insertOrFind(p.to_uint64());
            auto result = mutator.getValue();
            if (result == nullptr)
            {
                auto pNewData = boost::make_unique<Data>();
                result = pNewData.get();
                if (result != mutator.exchangeValue(result))
                    pNewData.release();
                result = mutator.getValue();
            }

            GEOMETRIX_ASSERT(result != nullptr);
            return *result;
        }

        const traits_type& get_traits() const { return m_gridTraits; }

        template <typename Point>
        bool is_contained( const Point& p ) const
        {
            BOOST_CONCEPT_ASSERT( (geometrix::Point2DConcept<Point>) );
            return m_gridTraits.is_contained( p );
        }

        void erase(boost::uint32_t i, boost::uint32_t j)
        {
            stk::compressed_integer_pair p = { i, j };
            auto iter = m_grid.find(p.to_uint64());
            if (iter.isValid())
                iter.eraseValue();
        }

        void clear()
        {
            auto it = typename grid_type::Iterator(m_grid);
            while(it.isValid())
            {
                auto pValue = it.getValue();
                delete pValue;
                m_grid.erase(it.getKey());
                it.next();
            };
        }

        //! This should be called when the grid is not being modified to allow threads to reclaim memory from deletions.
        void quiesce()
        {
            junction::DefaultQSBR.flush();
        }

    private:

        traits_type m_gridTraits;
        mutable grid_type m_grid;

    };
}//! namespace stk;
```

One particular note is that junction's hash-maps require memory reclamation to be manually scheduled during a point in the program where the hash-map is not accessed by multiple threads. The method of calling that can be seen in the `quiesce` method in the example above.

#### LibCDS

Documentation on the use of LibCDS can be found [here](http://libcds.sourceforge.net/doc/cds-api/index.html). I have not used libcds yet, but have created a test for its skiplist implementation. It's very fast. The thread initialization management makes it a bit difficult to use.

#### Active Objects

Active objects are objects (in the OOP sense) which additionally encapsulate the the compute units on which they run. This paradigm simplifies the synchronization of the state of the object, because it is only ever mutated by the compute unit which it also encapsulates. Active objects interact with other objects via message passing. For example:

```C++
#include <stk/thread/active_object.hpp>

template <typename Key, typename Data>
class active_map : stk::thread::active_object<>
{
public:

    std::future<boost::optional<Data>> find(const Key& k)
    {
        return send([k](){
            auto it = m_map.find(k);
            if(it != m_map.end())
                return it->second;
            return boost::none;
        });
    }

    std::future<bool> insert(const Key& key, Data&& p)
    {
        return send([key, p = std::move(p)](){ m_map.insert(std::make_pair(key, std::move(p))).second; });
    }

    //! And so on...

private:

    std::map<Key,Data> m_map;

};
```

The above example, while contrived, serves to illustrate how sychronization is achieved with active objects. The private data in the map is only ever accessed by the thread running in the active object base. Some care must be made with regards to the results returned by an active object. For example, in the above, returning iterators or references/pointers to internal data would not be thread safe.

#### Fibers
At the time of this writing I have not been able to verify any claim about the performance of fibers being superior to threads. The workloads I've used with fiber pools all perform better with threads. Therefore this section is more to explain the idea and show some of the stuff I tried.

Fibers are lightweight threads which use cooperative scheduling. As such they can be faster to use than OS threads in systems with high contention and large numbers of threads. The reason for this is that when a preemptive OS thread is blocked, nothing can happen on that thread. The OS must schedule a context switch to run a different ready thread. Context switches for OS threads are on the order of microseconds[<sup>[3]</sup>](https://www.usenix.org/legacy/events/expcs07/papers/2-li.pdf). To perform an OS context switch the following steps must be performed:
> From [<sup>[4]</sup>](https://blogs.msdn.microsoft.com/larryosterman/2005/01/05/why-does-win32-even-have-fibers/):
> 1. Enter Kernel Mode
> 2. Save all the old threads registers.
> 3. Acquire the dispatch spinlock.
> 4. Determine the next thread to run (if the next thread is in another process, this can get expensive)
> 5. Leave the dispatch spinlock.
> 6. Swap the old threads kernel state with the new threads kernel state.
> 7. Restore the new threads registers.
> 8. Leave Kernel Mode
>
> [Context switching for a fiber] effectively removes steps 1, 3, 5 and 8 from the context switch steps above.


In theory a good use of the fiber system would be via the use of fiber backed active-objects used to build a highly scalable actor-model implementation. Here is an example using active objects backed by fibers:

```C++
#include <stk/thread/fiber_thread_system.hpp>
#include <stk/thread/boost_fiber_traits.hpp>
#include <stk/thread/active_object.hpp>
#include <stk/geometry/geometry_kernel.hpp>
#include "pool_work_stealing.cpp"
namespace test {
    class vehicle : stk::thread::active_object<stk::thread::boost_fiber_traits>
    {
    public:

        vehicle()
            : stk::thread::active_object<
                stk::thread::boost_fiber_traits
            >(stk::thread::boost_fiber_creation_policy<>(boost::fibers::fixedsize_stack{ 4 * 1024 }))//! Create with 4k stack size.
        {}

        boost::fibers::future<stk::point2> move(stk::units::time dt)
        {
            return send([dt, this]() -> stk::point2 {
                m_p = m_p + dt * m_v + 0.5 * m_a * dt * dt;
                boost::this_fiber::yield();
                return m_p;
            });
        }

    private:

        stk::velocity2     m_v;
        stk::acceleration2 m_a;
        stk::point2        m_p;

    };

    class environment : stk::thread::active_object<stk::thread::boost_fiber_traits>
    {
    public:

        environment(int nCars)
            : stk::thread::active_object<
                stk::thread::boost_fiber_traits
            >(stk::thread::boost_fiber_creation_policy<>(boost::fibers::fixedsize_stack{ 1024 * 1024 }))//! Create with 1M stack size.
        {
            for (int i = 0; i < nCars; ++i) {
                m_vehicles.emplace_back(std::make_shared<vehicle>());
            }
        }

        boost::fibers::future<void> run_frame(stk::units::time dt)
        {
            return send([dt, this]() -> void {
                std::vector<boost::fibers::future<stk::point2>> fs;
                fs.reserve(m_vehicles.size());
                for (auto& v : m_vehicles)
                    fs.emplace_back(v->move(dt));

                for (auto& f : fs)
                    f.wait();
            });
        }

    private:

        std::vector<std::shared_ptr<vehicle>> m_vehicles;
    };
}//! namespace test;
TEST(actor_model, fibers_thread_system_with_actors)
{
    using namespace geometrix;
    using namespace ::testing;
    using namespace stk;
    using namespace stk::thread;
    auto salloc = boost::fibers::fixedsize_stack{ 64 * 1024 };
    std::vector<boost::intrusive_ptr<boost::fibers::algo::pool_work_stealing>> schedulers(std::thread::hardware_concurrency());

    auto schedulerPolicy = [&schedulers](unsigned int id)
    {
        boost::fibers::use_scheduling_algorithm<boost::fibers::algo::pool_work_stealing>(id, &schedulers, false);
    };
    fiber_thread_system<decltype(salloc)> fts(salloc, std::thread::hardware_concurrency(), schedulerPolicy);
    {
        using namespace test;

        //! Create an environment with 1 million cars. The environment and the vehicles are all fibers. You can't do that with OS threads with current tech.
        environment env(1000000);

        auto dt = 0.8 * units::si::seconds;
        for (auto t = 0.0 * units::si::seconds; t < 10.0 * units::si::seconds; t += dt) {
            env.run_frame(dt).wait();
        }
    }
}
```
The idea is to implement an actor model with a large number of actors made possible by using fibers. (As you can't get the same numbers for threads due to the large stack sizes threads usually have by default.) The above example works, but even on current hardware is far too slow to be viable. It does however present a compelling avenue to investigate with perhaps coarser levels of aggregation on the work supplied to the compute units.

This idea also suffers from a a number of drawbacks.

In no particular order:

* Fibers do not work with non-fiber synchronization. If you block a thread, you block all the fibers on it.
* Fibers tend to have very short call stacks. This makes it hard to debug the flow of control which results in an error.
* Fibers have very small stacks sizes allocated which can be a problem for deep recursion.
* Stack exhaustion for fibers usually results in a segmentation fault/access violation/bad stuff happening.
* Developers have to actively promote cooperative scheduling via calling `yield()`. This can be a bit of a dark art.
* Boost.Fiber seems to be fairly fragile to use in practice. I've had lots of difficulty getting tests working on all platforms we need.

The most glaring deficiency (IMO) is the lack of future composability in `boost::fiber` (which also exists in the current standard and won't be addressed until c++ 20.) The problem is that the futures implemented in `boost::fiber` do not provide for continuations (i.e. `future<T>::then(SomeWorkToDoAfterThePreviousFutureFinishes)`). The problem manifests when you try to create functions which use future results from multiple compute-units.

For example:

```C++
//! Let's say we want to call `m_pCollisionGrid->update(xxx)` from the move member function of `vehicle`.
//! `move` returns a `future<point2>`
//! `m_pCollisionGrid->update` calls into a different active-object and returns a `future<void>`.
boost::fibers::future<stk::point2> move(stk::units::time dt)
{
    return send([dt, this]() -> stk::point2 {
        auto oldPos = m_p;
        m_p = m_p + dt * m_v + 0.5 * m_a * dt * dt;
        //! We need some mechanism to wait for the future provided by this method call.
        //! Yet we don't want to block in this function.
        //! What we really want is serial composition.
        m_pCollisionGrid->update(this, oldPos, m_p);
        return m_p;
    });
}
```

Once continuations are in place (or if you are using the current Boost Thread library with `BOOST_THREAD_VERSION` defined to `4`) then the following becomes possible:
```C++
some_continuation_supporting::future<stk::point2> move(stk::units::time dt)
{
    auto oldPos = m_p;
    return send([dt, this]() -> stk::point2 {
        m_p = m_p + dt * m_v + 0.5 * m_a * dt * dt;
    }).then([oldPos, this](){
        m_pCollisionGrid->update(this, oldPos, m_p);
        return m_p;
    });
}
```

All the bad aside, the code to easily use Boost.Fibers is in the stk library, and can be used as specified above. Perhaps future versions will address some of the problems I found.

#### Thread-Pools

There are two implementations of thread-pools in the stk. Each evolved from initial implementations from Anthony Williams' book: ["C++ Concurrency in Action"](https://www.manning.com/books/c-plus-plus-concurrency-in-action) (followed by lots of experimentation and googling...) The implementations are:

* A single task queue shared among all threads in the pool
* A work stealing pool where each thread has its own task queue

The former is really a reference design. The latter tends to have better performance on real applications. The work-stealing implementation has less contention due to the multiple queues and will automatically load balance via work-stealing if threads run out of work. There are still some facets of the interface which I will likely want to change, but here is a provisional synopsis:
```C++
#include <stk/thread/work_stealing_thread_pool.hpp>
```

```C++
namespace stk { namespace thread {
    //! Note: The intention will be to provide singleton access to the thread pool
    //! via a plugin "job system". Details about the policy template parameters
    //! or the constructors should not be a concern of the users.
    template <typename QTraits, typename ThreadTraits = std_thread_traits>
    class work_stealing_thread_pool : boost::noncopyable
    {
    public:

        template <typename T>
        using future = typename ThreadTraits::template future_type<T>;

        //! Submit a single task whose signature is `R(void)`. Returns a future<R> instance.
        template <typename Task>
        future<decltype(std::declval<Task>()())> send(Task&& x);

        //! Submit a single task whose signature is `R(void)` to the task queue of the thread whose index is threadIndex. Returns a future<R> instance.
        template <typename Task>
        future<decltype(std::declval<Task>()())> send(std::uint32_t threadIndex, Task&& x);

        //! Submit a single task whose signature is `R(void)` and return void. Task completion is assumed
        //! to be tracked by the user within the supplied task.
        //! Precondition: Task must be noexcept.
        template <typename Task>
        void send_no_future(Task&& x);

        //! Submit a single task whose signature is `R(void)` to the task queue of the thread whose index is threadIndex. Returns void. Task completion is assumed
        //! to be tracked by the user within the supplied task.
        //! Precondition: Task must be noexcept.
        template <typename Task>
        void send_no_future(std::uint32_t threadIndex, Task&& x);

        //! Maps a task whose signature is `R(Range::value_type)` into each item in `range` and submits the tasks into the threadpool. The calling thread blocks until each tasks completes. Returns void.
        //! NOTE: Optimization available if Task is noexcept.
        template <typename Range, typename TaskFn>
        void parallel_for(Range&& range, TaskFn&& task);

        //! Maps a task whose signature is `R(Range::value_type)` into each item in `range` and submits the tasks into the threadpool at a granularity specified by npartitions. The calling thread blocks until each tasks completes. Returns void.
        //! NOTE: Optimization available if Task is noexcept.
        template <typename Range, typename TaskFn>
        void parallel_for(Range&& range, TaskFn&& task, std::size_t npartitions);

        //! Maps a task whose signature is `R(std::size_t i)` into calls which range over the interval from [0, count) and submits the tasks into the threadpool. The calling thread blocks until each tasks completes. Returns void.
        //! NOTE: Optimization available if Task is noexcept.
        template <typename TaskFn>
        void parallel_apply(std::ptrdiff_t count, TaskFn&& task);

        //! Maps a task whose signature is `R(std::size_t i)` into calls which range over the interval from [0, count) and submits the tasks into the threadpool at a granularity specified by npartitions. The calling thread blocks until each tasks completes. Returns void.
        //! NOTE: Optimization available if Task is noexcept.
        template <typename TaskFn>
        void parallel_apply(std::ptrdiff_t count, TaskFn&& task, std::size_t npartitions);

        //! Returns the number of threads. This can be used with `%` to specify the indices of thread queues for task submission.
        std::size_t number_threads() const;
    };

}}//! namespace stk::thread;
```

#### Tiny Spinlock

Sometimes you have to lock data which is both numerous and fine grained. For example the cells in a collision grid. Mutexes provided by the standard are often large:

```
//! On Visual Studio 2017 Win64:
sizeof(std::mutex) == 80
sizeof(boost::mutex) == 16
sizeof(boost::fibers::mutex) == 32
sizeof(stk::thread::tiny_atomic_spin_lock<>) == 1
```

For these cases use this:

```C++
#include <stk/thread/tiny_atomic_spin_lock.hpp>
```
Synopsis:
```C++
namespace stk { namespace thread {

    //! Conforms to the Lockable concept and can be used in std locks as a mutex. All with size of one byte.
    template <typename WaitStrategy = null_wait_strategy>
    class tiny_atomic_spin_lock
    {
    public:

        tiny_atomic_spin_lock() = default;

        void lock();

        bool try_lock();

        void unlock();

    };

}}//! namespace stk::thread;

//! Use:
using namespace stk::thread;
tiny_atomic_spin_lock mtx;
//! The lock will busy-wait while spinning. Suitable for very small chuncks of work with low contention.
auto lk = std::unique_lock<tiny_atomic_spin_lock<>>{mtx};
```

#### Thread-Local

The C++11 `thread_local` keyword is a great way to create variables which are scoped to whichever thread is running. Also it is very fast. One problem with `thread_local` data is that the scope is static to the lifetime of the thread. This means that you cannot easily have a data instances per thread which are also scoped to the lifetime of a class instance. If you must have per instance portable thread specific storage, then your options are `boost::thread_specific_ptr<T>`, or `stk::thread::thread_specific<T>`.

Pros and cons of each:

`boost::thread_specific_ptr<T>`:

Pros:
* Fairly easy to maintain.
* Works with C++03.

Cons:
* requires the use of a `boost::thread`
* and is slightly less performant than `stk::thread::thread_specific<T>`.

`stk::thread::thread_specific<T>`:

Pros:
* Slightly faster than boost's.
* Works with any threading system that works with `thread_local`.

Cons:
* Must register singleton instances of each type of `thread_specific<T>` to avoid violations of the One Definition Rule (ODR).
* Requires `thread_local` for internal stuff.

NOTE: In general, the performance benefits of `thread_local` are so great, its worthwhile to make it work for your use case if possible.

```C++
#include <stk/thread/thread_specific.hpp>
```
Synopsis:

```C++
namespace stk { namespace thread {

template <typename T>
class thread_specific
{
public:

    using reference = typename std::add_lvalue_reference<T>::type;
    using const_reference = typename std::add_const<reference>::type;

    thread_specific()
        : m_initializer([](){ return T();})
    {}

    thread_specific(const thread_specific&) = delete;
    thread_specific& operator=(const thread_specific&) = delete;
    thread_specific(thread_specific&&) = delete;
    thread_specific& operator=(thread_specific&&) = delete;

    template <typename Initializer>
    thread_specific(Initializer&& init);

    template <typename Initializer, typename Deinitializer>
    thread_specific(Initializer&& init, Deinitializer&& deinit);

    template <typename Value>
    thread_specific& operator = ( Value&& t );

    reference operator *();

    const_reference operator *() const;

    T* operator ->();

    T const* operator ->() const;

    bool has_value_on_calling_thread() const;

    explicit operator bool();
};

}}//! stk::thread.

//! This should be placed in a cpp file to avoid issues with singletons and the ODR.
//! There should be only one such definition for each type T specified.
#define STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(T)                  \
namespace stk { namespace thread {                                  \
template <>                                                         \
inline thread_specific<T>::instance_map& thread_specific<T>::hive() \
{                                                                   \
    static thread_local instance_map instance;                      \
    instance_map& m = instance;                                     \
    return m;                                                       \
}                                                                   \
}}                                                                  \
/***/
```

```C++
//! In one module that is used within a given application, there should be one instance of the following macro defined for each `T` used in a `thread_specific<T>`.
//! For example:
STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(int);
STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(std::unique_ptr<int>);

//! Usage:
using namespace stk::thread;
//! Construct the instance with a lambda which initializes the value for each thread.
thread_specific<int> sut{ []() { return 10; } };

//! Accessing the values uses ptr interface semantics as if the instance were a `T*`.
auto& localValue = *sut;
```

In cases where you know that the ODR will not be violated for a given `thread_specific<T>` instantiation, you can define `STK_DEFINE_THREAD_SPECIFIC_HIVE_INLINE` before the inclusion of the header to define the instance definitions inline. If you don't understand what this means, it probably isn't a good idea to do it :-).
