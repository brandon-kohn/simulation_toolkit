//
//! Copyright © 2018
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
//! Based on:
//! Lock-free Dynamically Resizable Arrays,
//! Damian Dechev, Peter Pirkelbauer, and Bjarne Stroustrup.
//!
//! Many ideas from "The Art of Multiprocessor Programming",
//! @book{Herlihy:2008 : AMP : 1734069,
//! author = { Herlihy, Maurice and Shavit, Nir },
//! title = { The Art of Multiprocessor Programming },
//! year = { 2008 },
//! isbn = { 0123705916, 9780123705914 },
//! publisher = { Morgan Kaufmann Publishers Inc. },
//! address = { San Francisco, CA, USA },
//}
#pragma once

#include <stk/container/atomic_stampable_ptr.hpp>
#include <stk/container/node_deletion_manager.hpp>
#include <memory>

namespace stk { namespace detail {

        inline std::uint8_t hibit(std::uint32_t val)
        {
            std::uint8_t k = 0;
            if (val > 0x0000FFFFu) { val >>= 16; k = 16; }
            if (val > 0x000000FFu) { val >>= 8;  k |= 8; }
            if (val > 0x0000000Fu) { val >>= 4;  k |= 4; }
            if (val > 0x00000003u) { val >>= 2;  k |= 2; }
            k |= (val & 2) >> 1;
            return k;
        }

        inline std::uint8_t hibit(std::uint64_t n)
        {
            std::uint32_t r = n >> 32;
            if (r)
                return hibit(r) + 32;
            return hibit(static_cast<std::uint32_t>(n));
        }

        template<typename T, typename Alloc>
        class concurrent_vector_base
        {
        protected:

            using allocator_type = Alloc;
            using value_type = T;

            struct node
            {
                template <typename Value>
                node(Value&& data)
                    : m_value(std::forward<Value>(data))
                {}

                template <typename Value>
                void             set_value(Value&& v) { m_value = std::forward<Value>(v); }
                value_type&      value() { return m_value; }

            private:

                value_type m_value;
            };

            using node_allocator = typename allocator_type::template rebind<node>::other;
            using node_manager = node_deletion_manager<node, node_allocator>;

            using node_ptr = node*;

            concurrent_vector_base(allocator_type al)
                : m_scopeManager(std::make_shared<node_manager>(al))
            {}

            template <typename Data>
            node_ptr create_node(Data&& v)
            {
                return m_scopeManager->create_node(v);
            }

            void register_node_for_deletion(node_ptr pNode)
            {
                m_scopeManager->register_node_to_delete(pNode);
            }

            void destroy_node(node_ptr pNode)
            {
                m_scopeManager->destroy_node(pNode);
            }

            std::shared_ptr<node_manager> get_scope_manager() const { return m_scopeManager; }

        private:
            std::shared_ptr<node_manager> m_scopeManager;
        };
    }//! namespace detail;

    struct reserve_arg_t {} reserve_arg;
    struct generator_arg_t {} generator_arg;

    //! NOT YET WORKING!!!!
    //! A lock-free vector which has geometric capacity growth.
    template <typename T, typename Alloc = std::allocator<T>>
    class concurrent_vector : public detail::concurrent_vector_base<T, Alloc>
    {
        using base_t = detail::concurrent_vector_base<T, Alloc>;
        using node_ptr = typename base_t::node_ptr;
        using node_manager = typename base_t::node_manager;
    public:

        using value_type = T;
        using reference = T&;
        using const_reference = T const&;
        using size_type = std::size_t;
        using allocator_type = Alloc;

    private:

        using atomic_t = std::atomic<node_ptr>;
        using bucket_ptr = atomic_t*;
        using bucket_array = bucket_ptr*;
        using bucket_size_t = typename atomic_stampable_ptr<bucket_ptr>::stamp_type;
        static const std::uint32_t first_bucket_size = 2;

        enum class flags : std::uint8_t
        {
            read
          , write_pending
          , write_complete
        };

        struct descriptor
        {
            descriptor(size_type s = 0)
                : size(s)
                , state(flags::read)
            {}

            descriptor(size_type s, node_ptr old, node_ptr nw, size_type loc)
                : size(s)
                , old_value(old)
                , new_value(nw)
                , location(loc)
                , state(flags::write_pending)
            {}

            descriptor(const descriptor& rhs)
                : size(rhs.size)
                , old_value(rhs.old_value)
                , new_value(rhs.new_value)
                , location(rhs.location)
                , state(rhs.state.load(std::memory_order_relaxed))
            {}

            descriptor& operator =(const descriptor& rhs)
            {
                size = rhs.size;
                old_value = rhs.old_value;
                new_value = rhs.new_value;
                location = rhs.location;
                state.store(rhs.state.load(std::memory_order_relaxed), std::memory_order_relaxed);
                return *this;
            }

            descriptor(descriptor&& rhs)
                : size(rhs.size)
                , old_value(rhs.old_value)
                , new_value(rhs.new_value)
                , location(rhs.location)
                , state(rhs.state.load(std::memory_order_relaxed))
            {}

            descriptor& operator =(descriptor&& rhs)
            {
                size = rhs.size;
                old_value = rhs.old_value;
                new_value = rhs.new_value;
                location = rhs.location;
                state.store(rhs.state.load(std::memory_order_relaxed), std::memory_order_relaxed);
                return *this;
            }

            flags get_state() const { return state.load(std::memory_order_relaxed); }
            void set_state(flags f) { state.store(f, std::memory_order_relaxed); }

            size_type size;
            node_ptr old_value;
            node_ptr new_value;
            size_type location;
            std::atomic<flags> state{false};
        };

        using desc_allocator = typename allocator_type::template rebind<descriptor>::other;
        using desc_manager = stk::node_deletion_manager<descriptor, desc_allocator>;

        template <typename T>
        class node_iterator;

        template <typename T>
        friend class node_iterator;

        template <typename T>
        class node_iterator
            : public boost::iterator_facade
            <
                node_iterator<T>
              , T
              , boost::bidirectional_traversal_tag
            >
        {
        public:

            friend class concurrent_vector<value_type, allocator_type>;

            node_iterator()
                : m_pNode()
                , m_index((std::numeric_limits<size_type>::max)())
            {}

            node_iterator(const node_iterator& other)
                : m_pNode(other.m_pNode)
#ifdef STK_DEBUG_CONCURRENT_VECTOR_ITERATORS
                , m_pNodeManager(other.m_pNodeManager)
#endif
                , m_pMyVector(other.m_pMyVector)
                , m_index(other.m_index)
            {
#ifdef STK_DEBUG_CONCURRENT_VECTOR_ITERATORS
                if (m_pNode)
                    acquire();
#endif
            }

            node_iterator& operator = (const node_iterator& rhs)
            {
#ifdef STK_DEBUG_CONCURRENT_VECTOR_ITERATORS
                if (m_pNode && !rhs.m_pNode)
                    release();
                m_pNodeManager = rhs.m_pNodeManager;
                if (!m_pNode && rhs.m_pNode)
                    acquire();
#endif
                m_pMyVector = rhs.m_pMyVector;
                m_pNode = rhs.m_pNode;
                m_index = other.m_index;
                return *this;
            }

            node_iterator(node_iterator&& other)
                : m_pNode(other.m_pNode)
#ifdef STK_DEBUG_CONCURRENT_VECTOR_ITERATORS
                , m_pNodeManager(std::move(other.m_pNodeManager))
#endif
                , m_pMyVector(other.m_pMyVector)
                , m_index(other.m_index)
            {
                other.m_pNode = nullptr;
                other.m_index = (std::numeric_limits<size_type>::max)();
            }

            node_iterator& operator = (node_iterator&& rhs)
            {
#ifdef STK_DEBUG_CONCURRENT_VECTOR_ITERATORS
                if (m_pNode)
                    release();
                m_pNodeManager = std::move(rhs.m_pNodeManager);
#endif
                m_pMyVector = rhs.m_pMyVector;
                m_pNode = rhs.m_pNode;
                rhs.m_pNode = nullptr;
                rhs.m_index = (std::numeric_limits<size_type>::max)();
                return *this;
            }

            explicit node_iterator(concurrent_vector<value_type, allocator_type>* myVector, node_ptr pNode, size_type index)
                : m_pNode(pNode)
#ifdef STK_DEBUG_CONCURRENT_VECTOR_ITERATORS
                , m_pNodeManager(myVector->get_scope_manager())
#endif
                , m_pMyVector(myVector)
                , m_index(index)
            {
#ifdef STK_DEBUG_CONCURRENT_VECTOR_ITERATORS
                if (m_pNode)
                    acquire();
#endif
            }

            ~node_iterator()
            {
#ifdef STK_DEBUG_CONCURRENT_VECTOR_ITERATORS
                if (m_pNode)
                    release();
#endif
            }

        private:

            template <typename U>
            BOOST_FORCEINLINE bool is_uninitialized(std::weak_ptr<U> const& weak)
            {
                using wt = std::weak_ptr<U>;
                return !weak.owner_before(wt{}) && !wt{}.owner_before(weak);
            }

            friend class boost::iterator_core_access;

#ifdef STK_DEBUG_CONCURRENT_VECTOR_ITERATORS
            void release()
            {
                GEOMETRIX_ASSERT(is_uninitialized(m_pNodeManager) || !m_pNodeManager.expired());
                auto pMgr = m_pNodeManager.lock();
                if (pMgr)
                    pMgr->remove_checkout();
            }

            void acquire()
            {
                GEOMETRIX_ASSERT(is_uninitialized(m_pNodeManager) || !m_pNodeManager.expired());
                auto pMgr = m_pNodeManager.lock();
                if (pMgr)
                    pMgr->add_checkout();
            }
#endif

            void increment()
            {
                if (m_pMyVector)
                {
                    bool hadNode = m_pNode != nullptr;

                    //! node deletion is disabled here. So even if the size changes between
                    //! the time of this acquisition; the node will be valid. Even if the
                    //! size is not.
                    auto p = m_pMyVector->at_impl(++m_index);
                    auto size = m_pMyVector->size();
                    GEOMETRIX_ASSERT(m_index <= size);//! should not increment past the end of the container.
                    m_pNode = (m_index < size && p) ? p->load() : nullptr;

#ifdef STK_DEBUG_CONCURRENT_VECTOR_ITERATORS
                    if (hadNode && !m_pNode)
                        release();
                    else if (!hadNode && m_pNode)
                        acquire();
#endif
                }
            }

            void decrement()
            {
                if (m_pMyVector)
                {
                    bool hadNode = m_pNode != nullptr;
                    //! node deletion is disabled here. So even if the size changes between
                    //! the time of this acquisition; the node will be valid. Even if the
                    //! size is not.
                    if (m_index > 0)
                    {
                        auto p = m_pMyVector->at_impl(--m_index);
                        m_pNode = (m_index-- >= 0 && p) ? p->load() : nullptr;
                    }
                    else
                    {
                        GEOMETRIX_ASSERT(false);//! Should not decrement an iterator pointing to the 0th index.
                        m_pNode = nullptr;
                        m_index = (std::numeric_limits<size_type>::max)();
                    }

#ifdef STK_DEBUG_CONCURRENT_VECTOR_ITERATORS
                    if (hadNode && !m_pNode)
                        release();
                    else if (!hadNode && m_pNode)
                        acquire();
#endif
                }
            }

            template <typename U>
            bool equal(node_iterator<U> const& other) const
            {
                return (m_index == other.m_index || m_pNode == other.m_pNode) && m_pMyVector == other.m_pMyVector;
            }

            //! Not thread safe.
            T& dereference() const
            {
                GEOMETRIX_ASSERT(m_pNode);
                return m_pNode->value();
            }

            node_ptr m_pNode{ nullptr };
#ifdef STK_DEBUG_CONCURRENT_VECTOR_ITERATORS
            std::weak_ptr<node_manager> m_pNodeManager;
#endif
            concurrent_vector<value_type, allocator_type>* m_pMyVector{ nullptr };
            size_type m_index;
        };

        template <typename U, typename Alloc>
        static U* allocate(Alloc& al, std::size_t n)
        {
            return typename std::allocator_traits<Alloc>::template rebind_alloc<U>(al).allocate(n);
        }

        template <typename U, typename Alloc>
        static void deallocate(Alloc& al, U* u, std::size_t n)
        {
            typename std::allocator_traits<Alloc>::template rebind_alloc<U>(al).deallocate(u, n);
        }

    public:

        struct const_iterator;

        struct iterator : node_iterator< value_type >
        {
            iterator()
                : node_iterator< value_type >()
            {}

            iterator(const iterator&) = default;
            iterator& operator=(const iterator&) = default;
            iterator(iterator&&) = default;
            iterator& operator=(iterator&&) = default;

            iterator(concurrent_vector<value_type, allocator_type>* myVector, node_ptr pNode, size_type index)
                : node_iterator< value_type >(myVector, pNode, index)
            {}

            template <typename U>
            bool operator ==(node_iterator<U> const& other) const
            {
                return equal(other);
            }

            template <typename U>
            bool operator !=(node_iterator<U> const& other) const
            {
                return !equal(other);// this->m_pNode != other.m_pNode;
            }
        };

        struct const_iterator : node_iterator<const value_type>
        {
            const_iterator()
                : node_iterator<const value_type>()
            {}

            const_iterator(const const_iterator&) = default;
            const_iterator& operator=(const const_iterator&) = default;
            const_iterator(const_iterator&&) = default;
            const_iterator& operator=(const_iterator&&) = default;

            const_iterator(const concurrent_vector<value_type, allocator_type>* myVector, node_ptr pNode, size_type index)
                : node_iterator<const value_type>(const_cast<concurrent_vector<value_type, allocator_type>*>(myVector), pNode, index)
            {}

            const_iterator(iterator const& other)
                : node_iterator< const value_type >(other)
            {}

            const_iterator operator =(iterator const& other)
            {
                this->m_pNode = other.m_pNode;
                return *this;
            }

            bool operator ==(iterator const& other) const
            {
                return equal(other);// this->m_pNode == other.m_pNode;
            }

            bool operator ==(const_iterator const& other) const
            {
                return equal(other);// this->m_pNode == other.m_pNode;
            }

            bool operator !=(iterator const& other) const
            {
                return !equal(other);// this->m_pNode != other.m_pNode;
            }

            bool operator !=(const_iterator const& other) const
            {
                return !equal(other);// this->m_pNode != other.m_pNode;
            }
        };

        concurrent_vector()
            : concurrent_vector(allocator_type())
        {}

        explicit concurrent_vector(const allocator_type& al)
            : base_t(al)
            , m_desc_manager(al)
            , m_descriptor(m_desc_manager.create_node(0))
            , m_array(allocate<bucket_ptr>(al, 1), 1)
            , m_alloc(al)
        {
            m_array.get_ptr()[0] = allocate<atomic_t>(m_alloc, first_bucket_size);
        }

        explicit concurrent_vector(reserve_arg_t, size_type s, const allocator_type& al = allocator_type())
            : base_t(al)
            , m_desc_manager(al)
            , m_descriptor(m_desc_manager.create_node(0))
            , m_array(allocate<bucket_ptr>(al, 1), 1)
            , m_alloc(al)
        {
            m_array.get_ptr()[0] = allocate<atomic_t>(m_alloc, first_bucket_size);
            reserve(s);
        }

        template <typename Generator>
        explicit concurrent_vector(generator_arg_t, size_type s, Generator&& gen, const allocator_type& al = allocator_type())
            : base_t(al)
            , m_desc_manager(al)
            , m_descriptor(m_desc_manager.create_node(0))
            , m_array(allocate<bucket_ptr>(al, 1), 1)
            , m_alloc(al)
        {
            m_array.get_ptr()[0] = allocate<atomic_t>(m_alloc, first_bucket_size);
            generate_impl(s, std::forward<Generator>(gen));
        }

        explicit concurrent_vector(size_type s, const allocator_type& al = allocator_type())
            : concurrent_vector(generator_arg, s, []() -> T { return T(); }, al)
        {}

        explicit concurrent_vector(size_type s, const T& t, const allocator_type& al = allocator_type())
            : concurrent_vector(generator_arg, s, [&t]() -> const T& { return t; }, al)
        {}

        template <typename InputIt>
        concurrent_vector(InputIt first, InputIt last, const allocator_type& al = allocator_type())
            : concurrent_vector(generator_arg, std::distance(first, last), [&first, last]() { return *(first++); }, al)
        {}

        concurrent_vector(std::initializer_list<T> init, const allocator_type& al = allocator_type())
            : concurrent_vector(init.begin(), init.end(), al)
        {}

        //! The destructor is not thread safe.
        ~concurrent_vector() noexcept
        {
            bucket_array pArray;
            bucket_size_t aSize;
            std::tie(pArray, aSize) = m_array.load(std::memory_order_relaxed);
            for (auto i = 0UL; i < aSize; ++i)
            {
                auto sizeI = get_bucket_size(i);
                deallocate<atomic_t>(m_alloc, pArray[i], sizeI);
            }

            deallocate<bucket_ptr>(m_alloc, pArray, aSize);
        }

        //! The reference returned is only safe if nodes are not being deleted concurrently.
        reference operator[](size_type i)
        {
            auto pNode = at_impl(i);
            GEOMETRIX_ASSERT(pNode);
            auto v = pNode->load();
            return v->value();
        }

        const_reference operator[](size_type i) const
        {
            auto pNode = at_impl(i);
            GEOMETRIX_ASSERT(pNode);
            auto v = pNode->load();
            return v->value();
        }

        //! The reference returned is only safe if nodes are not being deleted concurrently.
        reference at(size_type i)
        {
            auto pDesc = get_descriptor();
            if (i < pDesc->size)
            {
                auto pNode = at_impl(i);
                GEOMETRIX_ASSERT(pNode);
                auto v = pNode->load();
                return v->value();
            }

            throw std::out_of_range("index out of bounds");
        }

        const_reference at(size_type i) const
        {
            auto pDesc = get_descriptor();
            if (i < pDesc->size)
            {
                auto pNode = at_impl(i);
                GEOMETRIX_ASSERT(pNode);
                auto v = pNode->load();
                return v->value();
            }

            throw std::out_of_range("index out of bounds");
        }

        reference front()
        {
            GEOMETRIX_ASSERT(!empty());
            return this->operator[](0);
        }

        const_reference front() const
        {
            GEOMETRIX_ASSERT(!empty());
            return this->operator[](0);
        }

        reference back()
        {
            auto pCurr = get_descriptor();
            GEOMETRIX_ASSERT(pCurr->size > 0);
            return *at_impl(pCurr->size - 1)->load();
        }

        const_reference back() const
        {
            auto pCurr = get_descriptor();
            GEOMETRIX_ASSERT(pCurr->size > 0);
            return *at_impl(pCurr->size - 1)->load();
        }

        template <typename... Args>
        void emplace_back(Args&&... a)
        {
            descriptor* pCurr = get_descriptor();
            auto deleter = [this](descriptor* pDesc) { m_desc_manager.destroy_node(pDesc); };
            std::unique_ptr<descriptor, decltype(deleter)> newDesc{ m_desc_manager.create_node(), deleter };
            auto pNode = create_node(std::forward<Args>(a)...);
            auto spincount = 0UL;
            do
            {
                //pCurr = get_descriptor();
                complete_write(*pCurr);
                auto bucket = detail::hibit(pCurr->size + first_bucket_size) - detail::hibit(first_bucket_size);
                bucket_array oldArray;
                bucket_size_t oldSize;
                std::tie(oldArray, oldSize) = m_array.load(std::memory_order_relaxed);
                if(oldSize <= bucket)
                    allocate_bucket(oldArray, oldSize);
                *newDesc = descriptor(pCurr->size + 1, at_impl(pCurr->size)->load(), pNode, pCurr->size);
                if (++spincount > 100)
                {
                    auto backoff = spincount * 10;
                    while (--backoff > 0)
                        std::this_thread::yield();
                }
            }
            while(!m_descriptor.compare_exchange_weak(pCurr, newDesc.get()));
            m_desc_manager.register_node_to_delete(pCurr);
            complete_write(*newDesc);
            newDesc.release();
        }

        template <typename U>
        void push_back(U&& u)
        {
            emplace_back(u);
        }

        bool pop_back(T& value)
        {
            descriptor* pCurr = get_descriptor();
            auto deleter = [this](descriptor* pDesc) { m_desc_manager.destroy_node(pDesc); };
            std::unique_ptr<descriptor, decltype(deleter)> newDesc{ m_desc_manager.create_node(), deleter };
            node_ptr pNode;
            auto spincount = 0UL;
            do
            {
                //pCurr = get_descriptor();
                complete_write(*pCurr);
                if (pCurr->size > 0)
                {
                    pNode = at_impl(pCurr->size - 1)->load();
                    *newDesc = descriptor(pCurr->size - 1);
                }
                else
                    return false;

                if (++spincount > 100)
                {
                    auto backoff = spincount * 10;
                    while (--backoff > 0)
                        std::this_thread::yield();
                }
            }
            while(!m_descriptor.compare_exchange_weak(pCurr, newDesc.get()));
            value = std::move(pNode->value());
            m_desc_manager.register_node_to_delete(pCurr);
            newDesc.release();
            register_node_for_deletion(pNode);
            return true;
        }

        void pop_back()
        {
            descriptor* pCurr = get_descriptor();
            auto deleter = [this](descriptor* pDesc) { m_desc_manager.destroy_node(pDesc); };
            std::unique_ptr<descriptor, decltype(deleter)> newDesc{ m_desc_manager.create_node(), deleter };
            node_ptr pNode;
            auto spincount = 0UL;
            do
            {
                //pCurr = get_descriptor();
                complete_write(*pCurr);
                if (pCurr->size > 0)
                {
                    pNode = at_impl(pCurr->size - 1)->load();
                    *newDesc = descriptor(pCurr->size - 1);
                }
                else
                    return;

                if (++spincount > 100)
                {
                    auto backoff = spincount * 10;
                    while (--backoff > 0)
                        std::this_thread::yield();
                }
            }
            while(!m_descriptor.compare_exchange_weak(pCurr, newDesc.get()));
            register_node_for_deletion(pNode);
            newDesc.release();
            m_desc_manager.register_node_to_delete(pCurr);
        }

        //! This should not be called by multiple threads. It's likely safe, but will be contentious and do a lot of new/delete ping-ponging.
        void reserve(size_type s)
        {
            auto pCurr = get_descriptor();
            auto i = detail::hibit(pCurr->size + first_bucket_size - 1) - detail::hibit(first_bucket_size);
            if (i < 0)
                i = 0;
            auto limit = detail::hibit(s + first_bucket_size - 1) - detail::hibit(first_bucket_size);

            bucket_array oldArray;
            bucket_size_t oldSize;
            while (i++ < limit)
            {
                std::tie(oldArray, oldSize) = m_array.load(std::memory_order_relaxed);
                allocate_bucket(oldArray, oldSize);
            }
        }

        size_type size() const
        {
            auto pCurr = get_descriptor();
            auto s = pCurr->size;
            auto state = pCurr->get_state();
            if(state == flags::write_pending)
                --s;
            return s;
        }

        bool empty() const
        {
            return size() == 0;
        }

        iterator begin()
        {
            auto pNode = at_impl(0)->load(std::memory_order_relaxed);
            return iterator(this, pNode, 0);
        }

        const_iterator begin() const
        {
            auto pNode = at_impl(0)->load(std::memory_order_relaxed);
            return const_iterator(this, pNode, 0);
        }

        const_iterator cbegin() const
        {
            auto pNode = at_impl(0)->load(std::memory_order_relaxed);
            return const_iterator(this, pNode, 0);
        }

        iterator end()
        {
            return iterator(this, nullptr, size());
        }

        const_iterator end() const
        {
            return const_iterator(this, nullptr, size());
        }

        const_iterator cend() const
        {
            return const_iterator(this, nullptr, size());
        }

        size_type capacity() const
        {
            size_type s = 0;
            auto nBuckets = m_array.get_stamp();
            for (auto i = 0UL; i < nBuckets; ++i)
                s += get_bucket_size(i);
            return s;
        }

        void clear()
        {
            while (!empty())
                pop_back();
        }

        void quiesce()
        {
            get_scope_manager()->quiesce();
            m_desc_manager.quiesce();
        }

    private:

        //! Returns -1 if the index is the beginning of a new bucket. Return 1 if the index is that last of the bucket. Returns 0 otherwise.
        int is_at_bucket_boundary(size_type i) const
        {
            size_type bucket, idx;
            std::tie(bucket, idx) = get_bucket_index_and_offset(i);
            if (idx)
                return ((idx + 1) != get_bucket_size(bucket)) ? 0 : 1;

            return -1;
        }

        size_type get_bucket_size(size_type bidx) const
        {
            return first_bucket_size << bidx;
        }

        std::tuple<size_type, size_type> get_bucket_index_and_offset(size_type i) const
        {
            size_type pos = i + first_bucket_size;
            size_type hbit = detail::hibit(pos);
            size_type idx = pos ^ (1 << hbit);
            return std::make_tuple(hbit - detail::hibit(first_bucket_size), idx);
        }

        atomic_t* at_impl(size_type i) const
        {
            auto pos = i + first_bucket_size;
            auto hbit = detail::hibit(pos);
            auto idx = pos ^ (1 << hbit);
            return &m_array.get_ptr()[hbit - detail::hibit(first_bucket_size)][idx];
        }

        void complete_write(descriptor& desc)
        {
            if(desc.get_state() == flags::write_pending)
            {
                auto expected = desc.old_value;
                if(at_impl(desc.location)->compare_exchange_strong(expected, desc.new_value));
                    desc.set_state(flags::write_complete);
            }
        }

        void allocate_bucket(bucket_array oldArray, bucket_size_t oldSize)
        {
            //! Allocate a new array of bucket pointers.
            auto newArray = allocate<bucket_ptr>(m_alloc, oldSize + 1);

            //! Copy the old pointers over.
            std::copy(oldArray, oldArray + oldSize, newArray);

            //! Create and add the new bucket.
            auto newIndex = oldSize;
            auto bucket_size = get_bucket_size(newIndex);
            auto newBucket = allocate<atomic_t>(m_alloc, bucket_size);
            newArray[newIndex] = newBucket;
            if (!m_array.compare_exchange_strong(oldArray, oldSize, newArray, oldSize + 1))
            {
                deallocate<bucket_ptr>(m_alloc, newArray, oldSize + 1);
                deallocate<atomic_t>(m_alloc, newBucket, bucket_size);
            }
        }

        descriptor* get_descriptor() const
        {
            return m_descriptor.load(std::memory_order_relaxed);
        }

        template <typename Generator>
        void generate_impl(size_type s, Generator&& gen)
        {
            auto pCurr = get_descriptor();
            GEOMETRIX_ASSERT(pCurr->size == 0);//Only call this from ctor.
            auto i = detail::hibit(pCurr->size + first_bucket_size - 1) - detail::hibit(first_bucket_size);
            if (i < 0)
                i = 0;
            auto limit = detail::hibit(s + first_bucket_size - 1) - detail::hibit(first_bucket_size);

            bucket_array oldArray;
            bucket_size_t oldSize;
            while (i++ < limit)
            {
                std::tie(oldArray, oldSize) = m_array.load(std::memory_order_relaxed);
                allocate_bucket(oldArray, oldSize);
            }

            for(auto idx = 0UL; idx < s; ++idx)
            {
                at_impl(idx)->exchange(create_node(gen()));
            }

            pCurr->size = s;
        }

        mutable desc_manager                m_desc_manager;
        mutable std::atomic<descriptor*>    m_descriptor;
        atomic_stampable_ptr<bucket_ptr>    m_array;
        allocator_type                      m_alloc;

    };

}//! namespace stk;

