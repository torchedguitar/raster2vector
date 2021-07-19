/*
* Copyright 2014-2021  Jason Cohen.  All rights reserved.
*
* Licensed under the Apache License v2.0 with LLVM Exceptions.
* See https://llvm.org/LICENSE.txt for license information.
* SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

#pragma once

#include <map>
#include <string>

// Use the following preprocessor macro to define enums:
//
//     ENUM_WITH_NAME_MAP( NinjaTurtles \
//         Leonardo,                    \
//         Raphael,                     \
//         Michelangelo,                \
//         Donatello                    \
//     )
//
// This defines an enum class (in the scope where it is used):
//
//     enum class NinjaTurtles {
//         Leonardo,
//         Raphael,
//         Michelangelo,
//         Donatello
//     };
//
// It also defines an inline function EnumNameMapFor(NinjaTurtles) at the same scope,
// which returns a const& to a static singleton object with bi-directional mappings
// between the enum values and their names (as std::strings).  These maps can be used
// to look up names from values, or values from names.  The maps are ordered, so they
// are also useful for printing all the enum values in name-order or value-order.
//
// C++'s argument-dependent lookup (ADL) allows calling EnumNameMapFor without
// namespace-qualification on enum values defined in namespaces.  For example:
//
//     ENUM_WITH_NAME_MAP(Foo, a, b, c)
//     namespace Somewhere { ENUM_WITH_NAME_MAP(Bar, x, y, z) }
//     
//     template <class Map> void PrettyPrint(Map const& valToNameMap)
//     {
//         for (auto& [val, name] : valToNameMap)
//             std::cout << name << " = " << static_cast<int>(val) << "\n";
//     }
//
//     // In any namespace, these calls will work:
//     PrettyPrint(EnumNameMapFor(Foo{}).valToName);
//     PrettyPrint(EnumNameMapFor(Somewhere::Bar{}).valToName);
//
// Unfortunately, using ENUM_WITH_NAME_MAP inside a class or struct prevents ADL from
// finding EnumNameMapFor, since it is a member function.  For member enums, defining
// EnumNameMapFor as a friend function makes it a non-member even though the body is
// defined inside the class, thus allowing ADL to find it.  This requires using the
// MEMBER_ENUM_WITH_NAME_MAP inside classes and structs.

// Define an enum class and a name-map at global or namespace scope
#define ENUM_WITH_NAME_MAP(enumName, ...) \
    ENUM_WITH_NAME_MAP_HELPER(enumName, inline, __VA_ARGS__)

// Define an enum class and a name-map within a class or struct
#define MEMBER_ENUM_WITH_NAME_MAP(enumName, ...) \
    ENUM_WITH_NAME_MAP_HELPER(enumName, friend, __VA_ARGS__)


#define ENUMNAMEMAP_Value(ctx, x) x
#define ENUMNAMEMAP_ValueNamePair(ctx, x) {ctx::x, #x}

#define ENUM_WITH_NAME_MAP_HELPER(enumName, funcType, ...)                         \
    enum class enumName { ENUMNAMEMAP_MAP(ENUMNAMEMAP_Value, , __VA_ARGS__) };     \
                                                                                   \
    funcType ::EnumNameMap::Map<enumName> const& EnumNameMapFor(enumName)          \
    {                                                                              \
        static ::EnumNameMap::Map<enumName> m                                      \
        {                                                                          \
            ENUMNAMEMAP_MAP(ENUMNAMEMAP_ValueNamePair, enumName, __VA_ARGS__)      \
        };                                                                         \
        return m;                                                                  \
    }                                                                              \

namespace EnumNameMap {

    inline std::string const& UnknownStr()
    {
        static const std::string unknown("<unknown>");
        return unknown;
    }

    template <typename E>
    struct Map
    {
        using ValToName = std::map<E, std::string>;
        using NameToVal = std::map<std::string, E>;

        ValToName valToName; // Sorted by enum value
        NameToVal nameToVal; // Sorted by name

        Map(std::initializer_list<typename ValToName::value_type> vals)
        : valToName(vals)
        {
            for (auto const& pair : valToName)
            {
                nameToVal[pair.second] = pair.first;
            }
        }

        std::string const& Name(E val) const
        {
            auto it = valToName.find(val);
            return (it == valToName.end())
                ? UnknownStr()
                : it->second;
        }

        struct Result
        {
            E val;
            bool found;
        };

        Result Val(std::string const& name) const
        {
            auto it = nameToVal.find(name);
            return (it == nameToVal.end())
                ? Result{E{}, false}
                : Result{it->second, true};
        }

        size_t Size() const { return valToName.size(); }
    };

    // Convenient helpers for converting between values and names

    template <typename E>
    std::string const& ToName(E e)
    {
        return EnumNameMapFor(e).Name(e);
    }

    template <typename E>
    bool ToVal(E& e, std::string const& name)
    {
        auto result = EnumNameMapFor(e).Val(name);
        if (result.found)
        {
            e = result.val;
            return true;
        }
        else
        {
            return false;
        }
    }

} // namespace EnumNameMap

// -------------------------------------------------------------------------------------------------
// Everything below here is preprocessor helper code to implement ENUMNAMEMAP_MAP.
// This could be factored out to a separate header, but is kept here to reduce dependencies.
// 
// The ENUMNAMEMAP_MAP higher-order macro function turns this:
//    ENUMNAMEMAP_MAP(func, context, a, b, c, ...)
// into this:
//    func(context, a), func(context, b), func(context, c), ...
//
// Inspiration drawn from here:
// https://stackoverflow.com/questions/28828957/enum-to-string-in-modern-c11-c14-c17-and-future-c20
// - Added the ENUMNAMEMAP_ prefix to avoid conflicts with other macro definitions
// - Added support for up to 39 enum values.  Easy to make more, I just stopped there.
// - Added inner commas between elements
// - Added the "ctx" parameter so invocations of ENUMNAMEMAP_MAP can pass along some context to be
// used within the macro being mapped onto the varargs.

#define ENUMNAMEMAP_MAP(macro, ctx, ...) \
    ENUMNAMEMAP_IDENTITY( \
        ENUMNAMEMAP_APPLY(ENUMNAMEMAP_CHOOSE_MAP_START, ENUMNAMEMAP_COUNT(__VA_ARGS__)) \
            (macro, ctx, __VA_ARGS__))

#define ENUMNAMEMAP_CHOOSE_MAP_START(count) ENUMNAMEMAP_MAP ## count

#define ENUMNAMEMAP_APPLY(macro, ...) ENUMNAMEMAP_IDENTITY(macro(__VA_ARGS__))

#define ENUMNAMEMAP_IDENTITY(x) x

#define ENUMNAMEMAP_MAP1(m, ctx, x)      m(ctx, x)
#define ENUMNAMEMAP_MAP2(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP1(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP3(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP2(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP4(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP3(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP5(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP4(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP6(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP5(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP7(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP6(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP8(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP7(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP9(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP8(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP10(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP9(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP11(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP10(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP12(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP11(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP13(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP12(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP14(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP13(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP15(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP14(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP16(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP15(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP17(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP16(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP18(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP17(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP19(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP18(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP20(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP19(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP21(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP20(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP22(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP21(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP23(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP22(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP24(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP23(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP25(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP24(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP26(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP25(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP27(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP26(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP28(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP27(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP29(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP28(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP30(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP29(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP31(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP30(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP32(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP31(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP33(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP32(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP34(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP33(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP35(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP34(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP36(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP35(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP37(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP36(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP38(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP37(m, ctx, __VA_ARGS__))
#define ENUMNAMEMAP_MAP39(m, ctx, x, ...) m(ctx, x), ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_MAP38(m, ctx, __VA_ARGS__))

#define ENUMNAMEMAP_EVALUATE_COUNT( \
    _1, _2, _3, _4, _5, _6, _7, _8, _9, \
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, \
    _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, \
    _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, \
    count, ...) \
    count

#define ENUMNAMEMAP_COUNT(...) \
    ENUMNAMEMAP_IDENTITY(ENUMNAMEMAP_EVALUATE_COUNT(__VA_ARGS__, \
    39, 38, 37, 36, 35, 34, 33, 32, 31, 30, \
    29, 28, 27, 26, 25, 24, 23, 22, 21, 20, \
    19, 18, 17, 16, 15, 14, 13, 12, 11, 10, \
    9, 8, 7, 6, 5, 4, 3, 2, 1 \
    ))

