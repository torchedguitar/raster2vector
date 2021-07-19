/*
* Copyright 2014-2021  Jason Cohen.  All rights reserved.
*
* Licensed under the Apache License v2.0 with LLVM Exceptions.
* See https://llvm.org/LICENSE.txt for license information.
* SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

#pragma once

#include "EnumNameMap.h"

#include <cstring>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <map>
#include <string>
#include <vector>
#include <initializer_list>

namespace CommandLine {

struct cstrLess { bool operator()(char const* lhs, char const* rhs) const { return std::strcmp(lhs, rhs) > 0; }; };

class IOpt;
class argMap
{
    std::vector<std::string> args;
    std::vector<IOpt*> list;
    std::map<char const* const, IOpt*, cstrLess> map;
    friend class Parser;
    friend class IOpt;
};

class IOpt
{
public:
    bool specified = false;
    char const* const shortName;
    char const* const longName;
    char const* const helpText;

protected:
    IOpt(argMap& is, char const* shortName_, char const* longName_, char const* helpText_)
        : shortName(shortName_)
        , longName(longName_)
        , helpText(helpText_)
    {
        is.list.push_back(this);
        if (shortName) is.map[shortName] = this;
        if (longName)  is.map[longName]  = this;
    }
    // Don't need virtual dtor -- these don't need to be polymorphically deleted

public:
    // Override to show usage arguments, like "--size <bytes>".
    virtual char const* UsageSuffix() const { return ""; }
    // Override to show boilerplate help, like "Default is x. Choices:".
    virtual std::string HelpSuffix() const { return ""; }

    virtual std::vector<std::string> ExtraHelp() const { return {}; }

    // Consume gives arg classes a chance to consume sub-arguments or
    // an eq-form value. Assume argv now points to the first arg after
    // the looked-up key. Implementations that consume sub-arguments
    // must update argv to point to the next argument to consume. Post
    // condition is that argv points to the top-level argument. Return
    // true if success, or false to indicate parsing failed.
    virtual bool Consume(const char**& argv, const char* eqFormValue) { return true; }
};

class Parser
{
    bool m_valid = false;
protected:
    // This strange choice of name lets the user write English-looking things like:
    // Option verbose {is, "-v", "--verbose", "print a lot more info"}
    mutable argMap is;

    // Syntactic sugar for users, allows e.g. Value<string> instead of Value<std::string>
    using string = std::string;
    template <typename T> using vector = std::vector<T>;

private:
    IOpt* Lookup(char const* arg) const
    {
        auto it = is.map.find(arg);
        return it == is.map.end() ? nullptr : it->second;
    }

    template <typename T>
    static bool readValueFrom(char const* arg, T& value)
    {
        if (arg == nullptr) return false;

        if (strlen(arg) == 0) return false;

        std::istringstream ss(arg);
        if (ss.bad()) return false;

        ss >> value;
        if (ss.bad()) return false;

        return true;
    }

    // operator>> to a string is stupid: it takes one word. Specialize for that case.
    static bool readValueFrom(char const* arg, std::string& value)
    {
        if (arg == nullptr) return false;

        if (strlen(arg) == 0) return false;
        value = arg;

        return true;
    }

public:
    Parser() = default;
    Parser(char const** argv) : m_valid(Parse(argv)) {}
    virtual ~Parser() {}

    // Provide a public const ref to the internal (mutable) arg list
    std::vector<std::string> const& otherArgs = is.args;

    virtual bool Validate() { return true; }

    bool Parse(char const** argv)
    {
        if (m_valid)
        {
            // Leave old data as-is
            return false;
        }

        std::string eqFormArg;
        ++argv; // Skip argv[0], the executable's own name
        while (char const* arg = *argv)
        {
            // Can't look up args if they are --arg=value form, so check for = in every arg
            char const* eqFormValue = nullptr;
            if (auto eq = strchr(arg, '='))
            {
                // --arg=value form
                // Cache the arg part in a string, and get a pointer to the value part
                eqFormArg.assign(arg, eq);
                arg = eqFormArg.c_str();
                eqFormValue = eq + 1;
            }
            // Now arg points at just the arg (i.e. =value is stripped if present)
            // and eqFormValue points at the value, or null if not present

            // Look up arg, and bump argv to whatever is next
            auto pOpt = Lookup(arg);
            ++argv;
            if (pOpt == nullptr)
            {
                // Argument (not an option), so add to regular args list
                is.args.push_back(arg);
            }
            else
            {
                // Option, so give the option a chance to consume more args if desired
                pOpt->specified = true;

                // Arg can optionally consume follow-up arguments, or value from --arg=value form
                bool success = pOpt->Consume(argv, eqFormValue);
                if (!success)
                {
                    m_valid = false;
                    return false;
                }
            }
        }

        m_valid = Validate();
        return m_valid;
    }

    bool Valid() const
    {
        return m_valid;
    }

    operator bool() const
    {
        return m_valid;
    }

    virtual void ShowHelp(std::ostream& out) const
    {
        out << "Syntax:\n";
        char const* const empty = "";
        char const* const indent = "  ";

        // Find max sizes for setting field widths
        size_t maxShortName = 0;
        size_t maxLongName = 0;

        for (auto pOpt : is.list)
        {
            size_t usageSuffixLen = strlen(pOpt->UsageSuffix());

            size_t len = pOpt->shortName ? strlen(pOpt->shortName) : 0;
            if (len > 0) len += usageSuffixLen;
            if (len > maxShortName) maxShortName = len;

            len = pOpt->longName ? strlen(pOpt->longName) : 0;
            if (len > 0) len += usageSuffixLen;
            if (len > maxLongName) maxLongName = len;
        }

        for (auto pOpt : is.list)
        {
            std::string shortName = pOpt->shortName ? pOpt->shortName : empty;
            if (!shortName.empty()) shortName += pOpt->UsageSuffix();

            std::string longName = pOpt->longName ? pOpt->longName : empty;
            if (!longName.empty()) longName += pOpt->UsageSuffix();

            std::string helpText = pOpt->helpText ? pOpt->helpText : empty;
            if (helpText.empty())
            {
                helpText = pOpt->HelpSuffix();
            }
            else
            {
                helpText += ' ' + pOpt->HelpSuffix();
            }

            out << std::left
                << indent << std::setw(maxShortName) << shortName
                << indent << std::setw(maxLongName) << longName
                << indent << helpText
                << "\n";

            for (auto&& line : pOpt->ExtraHelp())
            {
                out << std::left
                    << indent << std::setw(maxShortName) << empty
                    << indent << std::setw(maxLongName) << empty
                    << indent << "- " << line
                    << "\n";
            }
        }
    }

    // Option functions like a boolean -- false if not specified, true if specified
    struct Option : IOpt
    {
        Option(argMap& is, char const* s, char const* l, char const* h) : IOpt(is, s, l, h) {}

        operator bool() const { return specified; }
    };

    template <typename T>
    struct Value : IOpt
    {
        T value;

        Value(argMap& is, char const* s, char const* l, char const* h) : IOpt(is, s, l, h), value{T()} {}
        Value(argMap& is, char const* s, char const* l, T&& def, char const* h) : IOpt(is, s, l, h), value{def} {}
        char const* UsageSuffix() const override { return " <value>"; }

        bool Consume(const char**& argv, const char* eqFormValue) override
        {
            if (eqFormValue) return false; // Not used

            char const* arg = *argv++;
            return readValueFrom(arg, value);
        }

        operator T const&() const { return value; }
    };

    template <typename T>
    struct EqValue : IOpt
    {
        T value;

        EqValue(argMap& is, char const* a, char const* b, char const* c) : IOpt(is, a, b, c), value{T()} {}
        EqValue(argMap& is, char const* a, char const* b, T&& def, char const* c) : IOpt(is, a, b, c), value{def} {}
        char const* UsageSuffix() const override { return "=<value>"; }

        bool Consume(const char**& argv, const char* eqFormValue) override
        {
            // argv not used
            return readValueFrom(eqFormValue, value);
        }

        operator T const&() const { return value; }
    };

    // Read argv values into a vector, size at least "min", stopping at next arg starting with a dash
    template <typename T, int min = 1>
    struct ValueList : IOpt
    {
        std::vector<T> values;

        ValueList(argMap& is, char const* s, char const* l, char const* h) : IOpt(is, s, l, h) {}
        ValueList(argMap& is, char const* s, char const* l, std::initializer_list<T>&& def, char const* h)
            : IOpt(is, s, l, h), values{def} {}
        char const* UsageSuffix() const override { return " <value> ..."; }

        bool Consume(const char**& argv, const char* eqFormValue) override
        {
            if (eqFormValue) return false; // Not used

            char const** argvStart = argv;
            while (*argv && (*argv)[0] != '-')
            {
                ++argv;
            }

            ptrdiff_t count = argv - argvStart;
            if (count < min) return false;

            values.clear(); // Replace default
            values.resize(count);

            int i = 0;
            for (char const** pArg = argvStart; pArg < argv; ++pArg)
            {
                if (!readValueFrom(*pArg, values[i++])) return false;
            }

            return true;
        }

        operator std::vector<T> const&() const { return values; }

        // Forward a bunch of convenience functions
        // C++14: get rid of the -> clause
        auto begin() -> decltype(values.begin()) { return values.begin(); }
        auto end() -> decltype(values.end()) { return values.end(); }
        auto size() -> decltype(values.size()) { return values.size(); }
        auto operator[](size_t i) -> decltype(values[i]) { return values[i]; }
    };

    // "Hide" wrapper for default values to prevent them from showing up in help text.  Enum usage looks like:
    //
    // Enum<Mode> mode {is, "-m", "--mode",      Mode::none,  "Selects mode."};
    //  or
    // Enum<Mode> mode {is, "-m", "--mode", Hide(Mode::none), "Selects mode."};
    //
    // ...where the former shows "Default is none." and lists "none" in the available options, while the latter
    // omits mentioning a default and removes "none" from the list of options.  This enables a sentinel value to
    // represent the "unspecified" case, so instead of separate checks for the option being specified & which
    // option was specified, a single switch/case can be used to cover the unspecified case as well, and avoids
    // confusing the user with an option value that means the same thing as not specifying the option.
    template <typename T> struct Hidden { T val; };
    template <typename T> Hidden<T> Hide(T t) { return Hidden<T>{t}; }

    template <typename T>
    struct Enum : IOpt
    {
        T value;
        bool hideDefaultVal = false;
        EnumNameMap::Map<T> const& nameMap;

        Enum(argMap& is, char const* s, char const* l, char const* h)
            : IOpt(is, s, l, h), value{T()}, nameMap{EnumNameMapFor(T{})} {}
        Enum(argMap& is, char const* s, char const* l, T def, char const* h)
            : IOpt(is, s, l, h), value{def}, nameMap{EnumNameMapFor(T{})} {}
        Enum(argMap& is, char const* s, char const* l, Hidden<T> def, char const* h)
            : IOpt(is, s, l, h), value{def.val}, hideDefaultVal{true}, nameMap{EnumNameMapFor(T{})} {}

        Enum(argMap& is, char const* s, char const* l, EnumNameMap::Map<T> const& e, char const* h)
            : IOpt(is, s, l, h), value{T()}, nameMap{e} {}
        Enum(argMap& is, char const* s, char const* l, T def, EnumNameMap::Map<T> const& e, char const* h)
            : IOpt(is, s, l, h), value{def}, nameMap{e} {}
        Enum(argMap& is, char const* s, char const* l, Hidden<T> def, EnumNameMap::Map<T> const& e, char const* h)
            : IOpt(is, s, l, h), value{def.val}, hideDefaultVal{true}, nameMap{e} {}

        char const* UsageSuffix() const override { return " <value>"; }
        std::string HelpSuffix() const override
        {
            return hideDefaultVal
                ? "Choices:"
                : std::string("Default is ") + nameMap.Name(value) + ". Choices:";
        }
        std::vector<std::string> ExtraHelp() const override
        {
            std::vector<std::string> nameList;
            nameList.reserve(nameMap.Size());
            for (auto const& pair : nameMap.valToName)
            {
                auto const& entryVal = pair.first;
                auto const& entryName = pair.second;
                if (!(hideDefaultVal && entryVal == value))
                {
                    nameList.push_back(entryName);
                }
            }

            return nameList;
        }

        bool Consume(const char**& argv, const char* eqFormValue) override
        {
            if (eqFormValue) return false; // Not used

            char const* arg = *argv++;
            std::string valName;
            if (!readValueFrom(arg, valName)) return false;

            auto result = nameMap.Val(valName);
            if (!result.found) return false;

            value = result.val;

            return true;
        }

        operator T const&() const { return value; }

        std::string const& name() const { return nameMap.Name(value); }
    };

    template <typename T, size_t min = 1>
    struct EnumList : IOpt
    {
        std::vector<T> values;
        EnumNameMap::Map<T> const& nameMap;

        EnumList(argMap& is, char const* s, char const* l, char const* h)
            : IOpt(is, s, l, h), nameMap{EnumNameMapFor(T{})} {}
        EnumList(argMap& is, char const* s, char const* l, std::initializer_list<T> def, char const* h)
            : IOpt(is, s, l, h), values{def}, nameMap{EnumNameMapFor(T{})} {}

        EnumList(argMap& is, char const* s, char const* l, EnumNameMap::Map<T> const& e, char const* h)
            : IOpt(is, s, l, h), nameMap{e} {}
        EnumList(argMap& is, char const* s, char const* l, std::initializer_list<T> def, EnumNameMap::Map<T> const& e, char const* h)
            : IOpt(is, s, l, h), values{def}, nameMap{e} {}

        char const* UsageSuffix() const override { return " <value> ..."; }
        std::string HelpSuffix() const override
        {
            std::string result("Default is ");
            if (values.size() == 0)
                result += "none";
            else if (values.size() == 1)
                result += nameMap.Name(values[0]);
            else
            {
                result += '{' + nameMap.Name(values[0]);
                for (size_t i = 1; i < values.size(); ++i)
                {
                    result += ", " + nameMap.Name(values[i]);
                }
                result += '}';
            }

            return result + ". Choices:";
        }
        std::vector<std::string> ExtraHelp() const override
        {
            std::vector<std::string> nameList;
            nameList.reserve(nameMap.Size());
            for (auto const& pair : nameMap.valToName)
            {
                auto const& entryName = pair.second;
                nameList.push_back(entryName);
            }

            return nameList;
        }

        bool Consume(const char**& argv, const char* eqFormValue) override
        {
            char const** argvStart = argv;
            while (*argv && (*argv)[0] != '-')
            {
                ++argv;
            }

            int count = argv - argvStart;
            if (count < min) return false;

            values.clear(); // Replace default
            values.reserve(count);

            std::string valName;
            for (char const** pArg = argvStart; pArg < argv; ++pArg)
            {
                if (!readValueFrom(*pArg, valName)) return false;

                auto result = nameMap.Val(valName);
                if (!result.found) return false;

                values.push_back(result.val);
            }

            return true;
        }

        operator std::vector<T> const&() const { return values; }

        // Forward a bunch of convenience functions
        // C++14: get rid of the -> clause
        auto begin() -> decltype(values.begin()) { return values.begin(); }
        auto end() -> decltype(values.end()) { return values.end(); }
        auto size() -> decltype(values.size()) { return values.size(); }
        auto operator[](size_t i) -> decltype(values[i]) { return values[i]; }

        std::vector<std::string> names() const
        {
            std::vector<std::string> result;
            result.reserve(values.size());

            for (auto const& value : values)
            {
                result.push_back(nameMap.Name(value));
            }
            return result;
        }
    };
};

template <class ParserT>
ParserT Parse(char const** argv) noexcept
{
    ParserT parser;
    try
    {
        parser.Parse(argv);
    }
    catch (...)
    {
    }
    return parser;
}

template <class ParserT>
class ParserFor : public Parser
{
public:
    static ParserT From(char const** argv) noexcept
    {
        return CommandLine::Parse<ParserT>(argv);
    }
};

template <class ParserT> // ParserT must inherit CommandLine::Parser
class Parsed : public ParserT
{
public:
    Parsed(char const** argv) { CommandLine::Parser::Parse(argv); }
};

// Use this macro to generate a direct constructor from argv as follows:
// 
// struct Options : CommandLine::Parser
// {
//     SUPPORT_CONSTRUCT_FROM_ARGV(Options)
//     ...
// };
#define SUPPORT_CONSTRUCT_FROM_ARGV(OptionsClass) OptionsClass(const char** argv) { Parse(argv); }

class ParserStrict : public Parser
{
    bool Validate() override { return otherArgs.empty(); }
};

template <typename T, typename S>
S& operator<<(S& out, Parser::Value<T>& rhs) { out << rhs.value; return out; }

}

#if 0

// Example CommandLine::Parser usage:

// Use this pattern to define enums used as commandline options.
// The enum value defined as zero will be implicit default value, and <unknown> will be shown
// for the value of an unspecified enum option with no enum value defined as zero.
// The following example defines:
//    enum class Mode { summary, trace };
// and also builds a bi-directional map between the enum values and strings for their names.
#define ENUM_VALUES_Mode(value, name)\
    value(name, summary),\
    value(name, trace)
DEFINE_ENUM_WITH_NAME_MAP(Mode)

struct Options : CommandLine::Parser
{
    // Declare options using the types in CommandLine::Parser
    // First parameter must be "is", then specify short name, long name, optional default, and help text
    Option                 verbose  {is, "-v",    nullptr,          "verbose mode"};
    Value<int>             count    {is, "-c",    "--count",        "number of things"};
    EqValue<int>           height   {is, "-h",    "--height",  4,   "how high?"};
    Value<float>           rating   {is, "-r",    "--rating",       "how many stars? (float)"};
    Value<std::string>     name     {is, "-n",    "--name",         nullptr};
    ValueList<std::string> files    {is, "-f",    "--files",        "files to compare"};
    ValueList<int>         dim      {is, nullptr, "--dim",   {7,8}, "number of things"};

    // Enums work magically, converting values <-> strings, based on the name map
    Enum<Mode>             mode     {is, "-m",    "--mode", ReportModeNames, "report mode"};

    // If some extra validation logic is desired, override Validate() to do this. Not required.
    bool Validate() override { return height.value > 0 && dim.size() >= 2 && dim[0] > 0; }
};

// Version that supports parsing while constructing, and const.  Just add a forwarding constructor:
struct COptions : CommandLine::Parser
{
    COptions(char const** argv) { Parse(argv); }
    Option                 verbose  {is, "-v", "--verbose", "spew a lot"};
    Value<int>             count    {is, "-c", "--count",   "number of things"};
    // ...
};

int main(int argc, char const** argv)
{
    // How to construct:

    Options options;
    if (!options.Parse(argv))
    {
        options.ShowHelp(std::cout);
        return 1;
    }

    // Or...

    const COptions cOptions(argv);
    if (!cOptions)
    {
        cOptions.ShowHelp(std::cout);
        return 1;
    }

    // How to use:


    if (options.verbose)
    {
        puts("Verbose mode enabled");
    }

    // A value option can be casted to value's type
    if (options.count.specified)
    {
        printf("Count = %d\n", (int)options.count);
    }

    // Options don't need to be specified -- they take the default if not
    std::cout << "Height = " << options.height << "\n";

    // Value options have a "value" member, and also forward operator << to it
    if (options.rating.specified)
    {
        bool bogus = options.rating.value < 0 || (float)options.rating > 5; // Ways to access value
        std::cout << "Rating = " << options.rating; // Note: don't need .value here
        if (bogus) std::cout << " (out of bounds, use 0 - 5)";
        std::cout << "\n";
    }

    // A value option can be casted to a const ref to a value's type, to avoid copies
    if (options.name.specified)
    {
        std::string const& name = options.name;
        std::cout << "Name = " << name << "\n";
    }

    // A value list option has a "values" vector.  The option fwds begin()/end()
    // to the values vector, so range-based for works on the option directly.
    if (options.files.specified)
    {
        std::cout << "Files:\n";
        for (auto const& file : options.files)
        {
            std::cout << " - " << file << "\n";
        }
    }

    // Note that value lists can use an initializer list as a default value if option isn't specified
    std::cout << "Dimensions: ";
    for (auto const& axis : options.dim)
    {
        std::cout << " " << axis;
    }
    std::cout << "\n";

    // Anything not consumed by options (anywhere in the argv list) go in the "args" vector
    puts("Args:");
    for (auto const& arg : options.args)
    {
        std::cout << " - " << arg << "\n";
    }

    return 0;
}

#endif