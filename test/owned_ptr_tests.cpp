#include <gtest/gtest.h>

#include <stk/utility/owned_ptr.hpp>

#include <type_traits>
#include <utility>

namespace
{
    struct tracked_value
    {
        tracked_value(int value, int* destroyed_count) noexcept
            : value(value)
            , destroyed_count(destroyed_count)
        {}

        ~tracked_value()
        {
            if (destroyed_count)
            {
                ++*destroyed_count;
            }
        }

        int value;
        int* destroyed_count;
    };

    void destroy_tracked_value(tracked_value* value) noexcept
    {
        delete value;
    }
}

TEST(owned_ptr_test_suite, type_is_move_only)
{
    static_assert(!std::is_copy_constructible_v<stk::owned_ptr<int>>);
    static_assert(!std::is_copy_assignable_v<stk::owned_ptr<int>>);
    static_assert(std::is_nothrow_move_constructible_v<stk::owned_ptr<int>>);
    static_assert(std::is_nothrow_move_assignable_v<stk::owned_ptr<int>>);
}

TEST(owned_ptr_test_suite, default_construct_is_empty)
{
    stk::owned_ptr<int> sut;

    EXPECT_EQ(nullptr, sut);
    EXPECT_FALSE(sut);
    EXPECT_EQ(nullptr, sut.get());
    EXPECT_NE(nullptr, sut.get_deleter());
}

TEST(owned_ptr_test_suite, make_owned_constructs_and_deletes)
{
    int destroyed_count = 0;
    {
        auto sut = stk::make_owned<tracked_value>(42, &destroyed_count);

        ASSERT_NE(nullptr, sut.get());
        EXPECT_EQ(42, sut->value);
        EXPECT_EQ(42, (*sut).value);
        EXPECT_TRUE(sut != nullptr);
    }

    EXPECT_EQ(1, destroyed_count);
}

TEST(owned_ptr_test_suite, custom_deleter_is_used_for_adopted_pointer)
{
    int destroyed_count = 0;
    {
        auto sut = stk::adopt_owned(
            new tracked_value(13, &destroyed_count),
            &destroy_tracked_value);

        EXPECT_EQ(13, sut->value);
        EXPECT_EQ(&destroy_tracked_value, sut.get_deleter());
    }

    EXPECT_EQ(1, destroyed_count);
}

TEST(owned_ptr_test_suite, lambda_deleter_can_be_used)
{
    int destroyed_count = 0;
    {
        auto sut = stk::make_owned_with_deleter<tracked_value>(
            +[](tracked_value* value) noexcept
            {
                delete value;
            },
            7,
            &destroyed_count);

        EXPECT_EQ(7, sut->value);
    }

    EXPECT_EQ(1, destroyed_count);
}

TEST(owned_ptr_test_suite, move_release_and_reset_transfer_ownership)
{
    int destroyed_count = 0;
    auto first = stk::make_owned<tracked_value>(9, &destroyed_count);
    tracked_value* raw = first.get();

    stk::owned_ptr<tracked_value> moved = std::move(first);

    EXPECT_EQ(nullptr, first);
    ASSERT_EQ(raw, moved.get());

    tracked_value* released = moved.release();
    EXPECT_EQ(nullptr, moved);
    ASSERT_EQ(raw, released);
    EXPECT_EQ(0, destroyed_count);

    moved.reset(released);
    ASSERT_EQ(raw, moved.get());

    moved.reset();
    EXPECT_EQ(nullptr, moved);
    EXPECT_EQ(1, destroyed_count);
}

TEST(owned_ptr_test_suite, reset_with_new_deleter_replaces_existing_resource)
{
    int first_destroyed = 0;
    int second_destroyed = 0;

    auto sut = stk::make_owned<tracked_value>(1, &first_destroyed);
    auto custom_deleter = +[](tracked_value* value) noexcept
    {
        delete value;
    };

    sut.reset(new tracked_value(2, &second_destroyed), custom_deleter);

    EXPECT_EQ(1, first_destroyed);
    ASSERT_NE(nullptr, sut.get());
    EXPECT_EQ(2, sut->value);
    EXPECT_EQ(custom_deleter, sut.get_deleter());

    sut.reset();
    EXPECT_EQ(1, second_destroyed);
}

TEST(owned_ptr_test_suite, swap_and_comparisons_follow_raw_pointer_identity)
{
    auto left = stk::make_owned<int>(1);
    auto right = stk::make_owned<int>(2);
    int* left_raw = left.get();
    int* right_raw = right.get();

    EXPECT_NE(left, right);
    EXPECT_EQ(left_raw < right_raw, left < right);
    EXPECT_EQ(left_raw <= right_raw, left <= right);
    EXPECT_EQ(left_raw > right_raw, left > right);
    EXPECT_EQ(left_raw >= right_raw, left >= right);

    swap(left, right);

    EXPECT_EQ(right_raw, left.get());
    EXPECT_EQ(left_raw, right.get());
}

TEST(owned_ptr_test_suite, into_unique_and_from_unique_round_trip)
{
    int destroyed_count = 0;
    auto original = stk::make_owned<tracked_value>(55, &destroyed_count);
    tracked_value* raw = original.get();

    auto unique = std::move(original).into_unique();

    EXPECT_EQ(nullptr, original);
    ASSERT_EQ(raw, unique.get());

    auto round_tripped = stk::owned_ptr<tracked_value>::from_unique(std::move(unique));

    EXPECT_EQ(nullptr, unique.get());
    ASSERT_EQ(raw, round_tripped.get());
    EXPECT_EQ(55, round_tripped->value);

    round_tripped.reset();
    EXPECT_EQ(1, destroyed_count);
}
