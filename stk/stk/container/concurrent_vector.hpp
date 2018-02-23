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
#include <stk/container/ref_count_node_manager.hpp>
#include <memory>

namespace stk { namespace detail {

        inline constexpr std::uint8_t hibit(std::uint32_t val)
        {
			std::uint8_t k = 0;
			if (val > 0x0000FFFFu) { val >>= 16; k = 16; }
			if (val > 0x000000FFu) { val >>= 8;  k |= 8; }
			if (val > 0x0000000Fu) { val >>= 4;  k |= 4; }
			if (val > 0x00000003u) { val >>= 2;  k |= 2; }
			k |= (val & 2) >> 1;
			return k;
        }

        inline constexpr std::uint8_t hibit(std::uint64_t n)
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
			using node_manager = ref_count_node_manager<node, node_allocator>;

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
    	using alloc_type = Alloc;

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
			descriptor(size_type s)
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

			flags get_state() const { return state.load(std::memory_order_relaxed); }
			void set_state(flags f) { state.store(f, std::memory_order_relaxed); }

			size_type size;
			node_ptr old_value;
			node_ptr new_value;
            size_type location;
            std::atomic<flags> state{false};
        };

        using desc_allocator = typename allocator_type::template rebind<descriptor>::other;

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
			  , boost::forward_traversal_tag
			>
		{
		public:

			friend class concurrent_vector<value_type, alloc_type>;

			node_iterator()
				: m_pNode()
			{}

			node_iterator(const node_iterator& other)
				: m_pNode(other.m_pNode)
				, m_pNodeManager(other.m_pNodeManager)
				, m_pMyVector(other.m_pMyVector)
			{
				if (m_pNode)
					acquire();
			}

			node_iterator& operator = (const node_iterator& rhs)
			{
				if (m_pNode && !rhs.m_pNode)
					release();
				m_pNodeManager = rhs.m_pNodeManager;
				if (!m_pNode && rhs.m_pNode)
					acquire();
				m_pNode = rhs.m_pNode;
				return *this;
			}

			node_iterator(node_iterator&& other)
				: m_pNode(other.m_pNode)
				, m_pNodeManager(std::move(other.m_pNodeManager))
				, m_pMyVector(other.m_pMyVector)
			{
				other.m_pNode = nullptr;
				other.m_pMyVector = nullptr;
			}

			node_iterator& operator = (node_iterator&& rhs)
			{
				if (m_pNode)
					release();
				m_pNodeManager = std::move(rhs.m_pNodeManager);
				m_pMyVector = rhs.m_pMyVector;
				m_pNode = rhs.m_pNode;
				rhs.m_pNode = nullptr;
				rhs.m_pMyVector = nullptr;
				return *this;
			}

			explicit node_iterator(concurrent_vector<value_type, alloc_type>* myVector)
				: m_pNode(nullptr)
                , m_pMyVector(myVector)
			{}

			explicit node_iterator(concurrent_vector<value_type, alloc_type>* myVector, const std::shared_ptr<node_manager>& pNodeManager, node_ptr pNode, size_type index)
				: m_pNode(pNode)
				, m_pNodeManager(pNodeManager)
                , m_pMyVector(myVector)
				, m_index(index)
			{
				if (m_pNode)
					pNodeManager->add_checkout();
			}

			~node_iterator()
			{
				if (m_pNode)
					release();
			}

		private:

			template <typename U>
			BOOST_FORCEINLINE bool is_uninitialized(std::weak_ptr<U> const& weak)
			{
				using wt = std::weak_ptr<U>;
				return !weak.owner_before(wt{}) && !wt{}.owner_before(weak);
			}

			friend class boost::iterator_core_access;

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

			void increment()
			{
				if (m_pMyVector && m_pNode)
				{
					//! node deletion is disabled here. So even if the size changes between 
					//! the time of this acquisition; the node will be valid. Even if the
					//! size is not.
					auto p = m_pMyVector->at_impl(++m_index);
					m_pNode = (m_index < m_pMyVector->size() && p) ? p->load() : nullptr;
						
					if (!m_pNode)
						release();
				}
			}

			bool equal(node_iterator<T> const& other) const
			{
				return m_pNode == other.m_pNode;
			}

			//! Not thread safe.
			T& dereference() const
			{
				GEOMETRIX_ASSERT(m_pNode);
				return m_pNode->value();
			}

			node_ptr m_pNode{ nullptr };
			std::weak_ptr<node_manager> m_pNodeManager;
			concurrent_vector<value_type, alloc_type>* m_pMyVector{ nullptr };
			size_type m_index;
		};

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

			iterator(concurrent_vector<value_type, alloc_type>* myVector)
				: node_iterator< value_type >(myVector)
			{}

			iterator(concurrent_vector<value_type, alloc_type>* myVector, const std::shared_ptr<node_manager>& pMgr, node_ptr pNode, size_type index)
				: node_iterator< value_type >(myVector, pMgr, pNode, index)
			{}

			template <typename U>
			bool operator ==(node_iterator<U> const& other) const
			{
				return this->m_pNode == other.m_pNode;
			}

			template <typename U>
			bool operator !=(node_iterator<U> const& other) const
			{
				return this->m_pNode != other.m_pNode;
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

			const_iterator(const concurrent_vector<value_type, alloc_type>* myVector)
				: node_iterator<const value_type>(const_cast<concurrent_vector<value_type, alloc_type>*>(myVector))
			{}

			const_iterator(const concurrent_vector<value_type, alloc_type>* myVector, const std::shared_ptr<node_manager>& pMgr, node_ptr pNode, size_type index)
				: node_iterator<const value_type>(const_cast<concurrent_vector<value_type, alloc_type>*>(myVector), pMgr, pNode, index)
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
				return this->m_pNode == other.m_pNode;
			}

			bool operator ==(const_iterator const& other) const
			{
				return this->m_pNode == other.m_pNode;
			}

			bool operator !=(iterator const& other) const
			{
				return this->m_pNode != other.m_pNode;
			}

			bool operator !=(const_iterator const& other) const
			{
				return this->m_pNode != other.m_pNode;
			}
		};

		concurrent_vector() noexcept(noexcept(alloc_type()))
			: concurrent_vector(alloc_type())
		{}

		explicit concurrent_vector(const alloc_type& al)
			: base_t(al)
			, m_descriptor(std::allocate_shared<descriptor>(al, 0))
            , m_array(new bucket_ptr[1](), 1)
			, m_alloc(al)
        {
			m_array.get_ptr()[0] = new atomic_t[first_bucket_size]();
        }

        explicit concurrent_vector(reserve_arg_t, size_type s, const alloc_type& al = alloc_type())
			: base_t(al)
			, m_descriptor(std::allocate_shared<descriptor>(al, 0))
            , m_array(new bucket_ptr[1](), 1)
			, m_alloc(al)
        {
			m_array.get_ptr()[0] = new atomic_t[first_bucket_size]();
			reserve(s);
        }

		template <typename Generator>
		explicit concurrent_vector(generator_arg_t, size_type s, Generator&& gen, const alloc_type& al = alloc_type())
			: base_t(al)
			, m_descriptor(std::allocate_shared<descriptor>(al, 0))
            , m_array(new bucket_ptr[1](), 1)
			, m_alloc(al)
        {
			m_array.get_ptr()[0] = new atomic_t[first_bucket_size]();
			generate_impl(s, std::forward<Generator>(gen));
        }

		explicit concurrent_vector(size_type s, const alloc_type& al = alloc_type())
			: concurrent_vector(generator_arg, s, []() -> T { return T(); }, al)
		{}

		explicit concurrent_vector(size_type s, const T& t, const alloc_type& al = alloc_type())
			: concurrent_vector(generator_arg, s, [&t]() -> const T& { return t; }, al)
		{}

		template <typename InputIt>
		concurrent_vector(InputIt first, InputIt last, const alloc_type& al = alloc_type())
			: concurrent_vector(generator_arg, std::distance(first, last), [&first, last]() { return *(first++); }, al)
		{}

		//! The destructor is not thread safe.
        ~concurrent_vector() noexcept
        {
			bucket_array pArray;
			bucket_size_t aSize;
			std::tie(pArray, aSize) = m_array.load(std::memory_order_relaxed);
			for (auto i = 0UL; i < aSize; ++i)
				delete[] pArray[i];

			delete[] pArray;
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

		template <typename U>
        void push_back(U&& u)
        {
			std::shared_ptr<descriptor> newDesc, pCurr;
			auto pNode = create_node(std::forward<U>(u));
            do
            {
                pCurr = get_descriptor();
                complete_write(*pCurr);
                auto bucket = detail::hibit(pCurr->size + first_bucket_size) - detail::hibit(first_bucket_size);
                bucket_array oldArray;
				bucket_size_t oldSize;
				std::tie(oldArray, oldSize) = m_array.load(std::memory_order_relaxed);
			    if(oldSize <= bucket)
                    allocate_bucket(oldArray, oldSize);
                newDesc = std::allocate_shared<descriptor>(m_alloc, pCurr->size + 1, at_impl(pCurr->size)->load(), pNode, pCurr->size);
            }
            while(!std::atomic_compare_exchange_weak(&m_descriptor, &pCurr, newDesc));
			complete_write(*newDesc);
        }

        bool pop_back(T& value)
        {
			std::shared_ptr<descriptor> newDesc, pCurr;

			//! Disable deletions while popping.
			get_scope_manager()->add_checkout();
			STK_SCOPE_EXIT(get_scope_manager()->remove_checkout());
			
			node_ptr pNode;
            do
            {
                pCurr = get_descriptor();
                complete_write(*pCurr);
				if (pCurr->size > 0)
				{
					pNode = at_impl(pCurr->size - 1)->load();
					newDesc = std::allocate_shared<descriptor>(m_alloc, pCurr->size - 1);
				}
				else
					return false;
            }
            while(!std::atomic_compare_exchange_weak(&m_descriptor, &pCurr, newDesc));
            value = std::move(pNode->value());
			register_node_for_deletion(pNode);
			return true;
        }

		void pop_back()
        {
			std::shared_ptr<descriptor> newDesc, pCurr;
			node_ptr pNode;
            do
            {
                pCurr = get_descriptor();
                complete_write(*pCurr);
				if (pCurr->size > 0)
				{
					pNode = at_impl(pCurr->size - 1)->load();
					newDesc = std::allocate_shared<descriptor>(m_alloc, pCurr->size - 1);
				}
				else
					return;
            }
            while(!std::atomic_compare_exchange_weak(&m_descriptor, &pCurr, newDesc));
			register_node_for_deletion(pNode);
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
			return iterator(this, get_scope_manager(), pNode, 0);
		}

		const_iterator begin() const
		{
			auto pNode = at_impl(0)->load(std::memory_order_relaxed);
			return const_iterator(this, get_scope_manager(), pNode, 0);
		}

        const_iterator cbegin() const
		{
			auto pNode = at_impl(0)->load(std::memory_order_relaxed);
			return const_iterator(this, get_scope_manager(), pNode, 0);
		}

		iterator end() 
		{
			return iterator(this);
		}

		const_iterator end() const
		{
			return const_iterator(this);
		}

        const_iterator cend() const
		{
			return const_iterator(this);
		}

		size_type capacity() const
		{
			size_type s = first_bucket_size;
			std::uint32_t exponent = detail::hibit(first_bucket_size);
			auto nBuckets = m_array.get_stamp();
			for (auto i = 1UL; i < nBuckets; ++i)
				s += (1 << ++exponent);
			return s;
		}

		void clear()
		{
			while (!empty())
				pop_back();
		}

    private:

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
                if(at_impl(desc.location)->compare_exchange_strong(desc.old_value, desc.new_value));
					desc.set_state(flags::write_complete);
            }
        }

        void allocate_bucket(bucket_array oldArray, bucket_size_t oldSize)
        {
			//! Allocate a new array of bucket pointers.
			auto newArray = new bucket_ptr[oldSize + 1];

			//! Copy the old pointers over.
			std::copy(oldArray, oldArray + oldSize, newArray);

			//! Create and add the new bucket.
            auto bucket_size = (first_bucket_size) << (oldSize + 1);
			auto newBucket = new atomic_t[bucket_size]();//m_alloc.allocate(bucket_size);
			newArray[oldSize] = newBucket;
			if (!m_array.compare_exchange_strong(oldArray, oldSize, newArray, oldSize + 1))
			{
				delete[] newBucket;//m_alloc.deallocate(mem, bucket_size);
				delete[] newArray;
			}
        }

		std::shared_ptr<descriptor> get_descriptor() const
		{
			return std::atomic_load(&m_descriptor);
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

        mutable std::shared_ptr<descriptor> m_descriptor;
		atomic_stampable_ptr<bucket_ptr>    m_array;
		alloc_type                          m_alloc;

    };

}//! namespace stk;

