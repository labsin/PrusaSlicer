#ifndef slic3r_Config_hpp_
#define slic3r_Config_hpp_

#include <assert.h>
#include <map>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "libslic3r.h"
#include "clonable_ptr.hpp"
#include "Point.hpp"

#include <boost/format.hpp>
#include <boost/property_tree/ptree.hpp>

#include <cereal/access.hpp>
#include <cereal/types/base_class.hpp>

namespace Slic3r {

// Name of the configuration option.
typedef std::string                 t_config_option_key;
typedef std::vector<std::string>    t_config_option_keys;

extern std::string  escape_string_cstyle(const std::string &str);
extern std::string  escape_strings_cstyle(const std::vector<std::string> &strs);
extern bool         unescape_string_cstyle(const std::string &str, std::string &out);
extern bool         unescape_strings_cstyle(const std::string &str, std::vector<std::string> &out);

/// Specialization of std::exception to indicate that an unknown config option has been encountered.
class UnknownOptionException : public std::runtime_error {
public:
    UnknownOptionException() :
        std::runtime_error("Unknown option exception") {}
    UnknownOptionException(const std::string &opt_key) :
        std::runtime_error(std::string("Unknown option exception: ") + opt_key) {}
};

/// Indicate that the ConfigBase derived class does not provide config definition (the method def() returns null).
class NoDefinitionException : public std::runtime_error
{
public:
    NoDefinitionException() :
        std::runtime_error("No definition exception") {}
    NoDefinitionException(const std::string &opt_key) :
        std::runtime_error(std::string("No definition exception: ") + opt_key) {}
};

// Type of a configuration value.
enum ConfigOptionType {
    coVectorType    = 0x4000,
    coNone          = 0,
    // single float
    coFloat         = 1,
    // vector of floats
    coFloats        = coFloat + coVectorType,
    // single int
    coInt           = 2,
    // vector of ints
    coInts          = coInt + coVectorType,
    // single string
    coString        = 3,
    // vector of strings
    coStrings       = coString + coVectorType,
    // percent value. Currently only used for infill.
    coPercent       = 4,
    // percents value. Currently used for retract before wipe only.
    coPercents      = coPercent + coVectorType,
    // a fraction or an absolute value
    coFloatOrPercent = 5,
    // single 2d point (Point2f). Currently not used.
    coPoint         = 6,
    // vector of 2d points (Point2f). Currently used for the definition of the print bed and for the extruder offsets.
    coPoints        = coPoint + coVectorType,
    coPoint3        = 7,
//    coPoint3s       = coPoint3 + coVectorType,
    // single boolean value
    coBool          = 8,
    // vector of boolean values
    coBools         = coBool + coVectorType,
    // a generic enum
    coEnum          = 9,
};

enum ConfigOptionMode {
    comSimple = 0,
    comAdvanced,
    comExpert
};

enum PrinterTechnology
{
    // Fused Filament Fabrication
    ptFFF,
    // Stereolitography
    ptSLA,
    // Unknown, useful for command line processing
    ptUnknown,
    // Any technology, useful for parameters compatible with both ptFFF and ptSLA
    ptAny
};

// A generic value of a configuration option.
class ConfigOption {
public:
    virtual ~ConfigOption() {}

    virtual ConfigOptionType    type() const = 0;
    virtual std::string         serialize() const = 0;
    virtual bool                deserialize(const std::string &str, bool append = false) = 0;
    virtual ConfigOption*       clone() const = 0;
    // Set a value from a ConfigOption. The two options should be compatible.
    virtual void                set(const ConfigOption *option) = 0;
    virtual int                 getInt()        const { throw std::runtime_error("Calling ConfigOption::getInt on a non-int ConfigOption"); return 0; }
    virtual double              getFloat()      const { throw std::runtime_error("Calling ConfigOption::getFloat on a non-float ConfigOption"); return 0; }
    virtual bool                getBool()       const { throw std::runtime_error("Calling ConfigOption::getBool on a non-boolean ConfigOption"); return 0; }
    virtual void                setInt(int /* val */) { throw std::runtime_error("Calling ConfigOption::setInt on a non-int ConfigOption"); }
    virtual bool                operator==(const ConfigOption &rhs) const = 0;
    bool                        operator!=(const ConfigOption &rhs) const { return ! (*this == rhs); }
    bool                        is_scalar()     const { return (int(this->type()) & int(coVectorType)) == 0; }
    bool                        is_vector()     const { return ! this->is_scalar(); }
};

typedef ConfigOption*       ConfigOptionPtr;
typedef const ConfigOption* ConfigOptionConstPtr;

// Value of a single valued option (bool, int, float, string, point, enum)
template <class T>
class ConfigOptionSingle : public ConfigOption {
public:
    T value;
    explicit ConfigOptionSingle(T value) : value(value) {}
    operator T() const { return this->value; }
    
    void set(const ConfigOption *rhs) override
    {
        if (rhs->type() != this->type())
            throw std::runtime_error("ConfigOptionSingle: Assigning an incompatible type");
        assert(dynamic_cast<const ConfigOptionSingle<T>*>(rhs));
        this->value = static_cast<const ConfigOptionSingle<T>*>(rhs)->value;
    }

    bool operator==(const ConfigOption &rhs) const override
    {
        if (rhs.type() != this->type())
            throw std::runtime_error("ConfigOptionSingle: Comparing incompatible types");
        assert(dynamic_cast<const ConfigOptionSingle<T>*>(&rhs));
        return this->value == static_cast<const ConfigOptionSingle<T>*>(&rhs)->value;
    }

    bool operator==(const T &rhs) const { return this->value == rhs; }
    bool operator!=(const T &rhs) const { return this->value != rhs; }

private:
	friend class cereal::access;
	template<class Archive> void serialize(Archive & ar) { ar(this->value); }
};

// Value of a vector valued option (bools, ints, floats, strings, points)
class ConfigOptionVectorBase : public ConfigOption {
public:
    // Currently used only to initialize the PlaceholderParser.
    virtual std::vector<std::string> vserialize() const = 0;
    // Set from a vector of ConfigOptions. 
    // If the rhs ConfigOption is scalar, then its value is used,
    // otherwise for each of rhs, the first value of a vector is used.
    // This function is useful to collect values for multiple extrder / filament settings.
    virtual void set(const std::vector<const ConfigOption*> &rhs) = 0;
    // Set a single vector item from either a scalar option or the first value of a vector option.vector of ConfigOptions. 
    // This function is useful to split values from multiple extrder / filament settings into separate configurations.
    virtual void set_at(const ConfigOption *rhs, size_t i, size_t j) = 0;
    // Resize the vector of values, copy the newly added values from opt_default if provided.
    virtual void resize(size_t n, const ConfigOption *opt_default = nullptr) = 0;
    // Clear the values vector.
    virtual void clear() = 0;

    // Get size of this vector.
    virtual size_t size()  const = 0;
    // Is this vector empty?
    virtual bool   empty() const = 0;

protected:
    // Used to verify type compatibility when assigning to / from a scalar ConfigOption.
    ConfigOptionType scalar_type() const { return static_cast<ConfigOptionType>(this->type() - coVectorType); }
};

// Value of a vector valued option (bools, ints, floats, strings, points), template
template <class T>
class ConfigOptionVector : public ConfigOptionVectorBase
{
public:
    ConfigOptionVector() {}
    explicit ConfigOptionVector(size_t n, const T &value) : values(n, value) {}
    explicit ConfigOptionVector(std::initializer_list<T> il) : values(std::move(il)) {}
    explicit ConfigOptionVector(const std::vector<T> &values) : values(values) {}
    explicit ConfigOptionVector(std::vector<T> &&values) : values(std::move(values)) {}
    std::vector<T> values;
    
    void set(const ConfigOption *rhs) override
    {
        if (rhs->type() != this->type())
            throw std::runtime_error("ConfigOptionVector: Assigning an incompatible type");
        assert(dynamic_cast<const ConfigOptionVector<T>*>(rhs));
        this->values = static_cast<const ConfigOptionVector<T>*>(rhs)->values;
    }

    // Set from a vector of ConfigOptions. 
    // If the rhs ConfigOption is scalar, then its value is used,
    // otherwise for each of rhs, the first value of a vector is used.
    // This function is useful to collect values for multiple extrder / filament settings.
    void set(const std::vector<const ConfigOption*> &rhs) override
    {
        this->values.clear();
        this->values.reserve(rhs.size());
        for (const ConfigOption *opt : rhs) {
            if (opt->type() == this->type()) {
                auto other = static_cast<const ConfigOptionVector<T>*>(opt);
                if (other->values.empty())
                    throw std::runtime_error("ConfigOptionVector::set(): Assigning from an empty vector");
                this->values.emplace_back(other->values.front());
            } else if (opt->type() == this->scalar_type())
                this->values.emplace_back(static_cast<const ConfigOptionSingle<T>*>(opt)->value);
            else
                throw std::runtime_error("ConfigOptionVector::set():: Assigning an incompatible type");
        }
    }

    // Set a single vector item from either a scalar option or the first value of a vector option.vector of ConfigOptions. 
    // This function is useful to split values from multiple extrder / filament settings into separate configurations.
    void set_at(const ConfigOption *rhs, size_t i, size_t j) override
    {
        // It is expected that the vector value has at least one value, which is the default, if not overwritten.
        assert(! this->values.empty());
        if (this->values.size() <= i) {
            // Resize this vector, fill in the new vector fields with the copy of the first field.
            T v = this->values.front();
            this->values.resize(i + 1, v);
        }
        if (rhs->type() == this->type()) {
            // Assign the first value of the rhs vector.
            auto other = static_cast<const ConfigOptionVector<T>*>(rhs);
            if (other->values.empty())
                throw std::runtime_error("ConfigOptionVector::set_at(): Assigning from an empty vector");
            this->values[i] = other->get_at(j);
        } else if (rhs->type() == this->scalar_type())
            this->values[i] = static_cast<const ConfigOptionSingle<T>*>(rhs)->value;
        else
            throw std::runtime_error("ConfigOptionVector::set_at(): Assigning an incompatible type");
    }

    T& get_at(size_t i)
    {
        assert(! this->values.empty());
        return (i < this->values.size()) ? this->values[i] : this->values.front();
    }

    const T& get_at(size_t i) const { return const_cast<ConfigOptionVector<T>*>(this)->get_at(i); }

    // Resize this vector by duplicating the /*last*/first value.
    // If the current vector is empty, the default value is used instead.
    void resize(size_t n, const ConfigOption *opt_default = nullptr) override
    {
        assert(opt_default == nullptr || opt_default->is_vector());
//        assert(opt_default == nullptr || dynamic_cast<ConfigOptionVector<T>>(opt_default));
        assert(! this->values.empty() || opt_default != nullptr);
        if (n == 0)
            this->values.clear();
        else if (n < this->values.size())
            this->values.erase(this->values.begin() + n, this->values.end());
        else if (n > this->values.size()) {
            if (this->values.empty()) {
                if (opt_default == nullptr)
                    throw std::runtime_error("ConfigOptionVector::resize(): No default value provided.");
                if (opt_default->type() != this->type())
                    throw std::runtime_error("ConfigOptionVector::resize(): Extending with an incompatible type.");
                this->values.resize(n, static_cast<const ConfigOptionVector<T>*>(opt_default)->values.front());
            } else {
                // Resize by duplicating the last value.
                this->values.resize(n, this->values./*back*/front());
            }
        }
    }

    // Clear the values vector.
    void   clear() override { this->values.clear(); }
    size_t size()  const override { return this->values.size(); }
    bool   empty() const override { return this->values.empty(); }

    bool operator==(const ConfigOption &rhs) const override
    {
        if (rhs.type() != this->type())
            throw std::runtime_error("ConfigOptionVector: Comparing incompatible types");
        assert(dynamic_cast<const ConfigOptionVector<T>*>(&rhs));
        return this->values == static_cast<const ConfigOptionVector<T>*>(&rhs)->values;
    }

    bool operator==(const std::vector<T> &rhs) const { return this->values == rhs; }
    bool operator!=(const std::vector<T> &rhs) const { return this->values != rhs; }

private:
	friend class cereal::access;
	template<class Archive> void serialize(Archive & ar) { ar(this->values); }
};

class ConfigOptionFloat : public ConfigOptionSingle<double>
{
public:
    ConfigOptionFloat() : ConfigOptionSingle<double>(0) {}
    explicit ConfigOptionFloat(double _value) : ConfigOptionSingle<double>(_value) {}

    static ConfigOptionType static_type() { return coFloat; }
    ConfigOptionType        type()      const override { return static_type(); }
    double                  getFloat()  const override { return this->value; }
    ConfigOption*           clone()     const override { return new ConfigOptionFloat(*this); }
    bool                    operator==(const ConfigOptionFloat &rhs) const { return this->value == rhs.value; }
    
    std::string serialize() const override
    {
        std::ostringstream ss;
        ss << this->value;
        return ss.str();
    }
    
    bool deserialize(const std::string &str, bool append = false) override
    {
        UNUSED(append);
        std::istringstream iss(str);
        iss >> this->value;
        return !iss.fail();
    }

    ConfigOptionFloat& operator=(const ConfigOption *opt)
    {   
        this->set(opt);
        return *this;
    }

private:
	friend class cereal::access;
	template<class Archive> void serialize(Archive &ar) { ar(cereal::base_class<ConfigOptionSingle<double>>(this)); }
};

class ConfigOptionFloats : public ConfigOptionVector<double>
{
public:
    ConfigOptionFloats() : ConfigOptionVector<double>() {}
    explicit ConfigOptionFloats(size_t n, double value) : ConfigOptionVector<double>(n, value) {}
    explicit ConfigOptionFloats(std::initializer_list<double> il) : ConfigOptionVector<double>(std::move(il)) {}
    explicit ConfigOptionFloats(const std::vector<double> &vec) : ConfigOptionVector<double>(vec) {}
    explicit ConfigOptionFloats(std::vector<double> &&vec) : ConfigOptionVector<double>(std::move(vec)) {}

    static ConfigOptionType static_type() { return coFloats; }
    ConfigOptionType        type()  const override { return static_type(); }
    ConfigOption*           clone() const override { return new ConfigOptionFloats(*this); }
    bool                    operator==(const ConfigOptionFloats &rhs) const { return this->values == rhs.values; }

    std::string serialize() const override
    {
        std::ostringstream ss;
        for (std::vector<double>::const_iterator it = this->values.begin(); it != this->values.end(); ++it) {
            if (it - this->values.begin() != 0) ss << ",";
            ss << *it;
        }
        return ss.str();
    }
    
    std::vector<std::string> vserialize() const override
    {
        std::vector<std::string> vv;
        vv.reserve(this->values.size());
        for (std::vector<double>::const_iterator it = this->values.begin(); it != this->values.end(); ++it) {
            std::ostringstream ss;
            ss << *it;
            vv.push_back(ss.str());
        }
        return vv;
    }
    
    bool deserialize(const std::string &str, bool append = false) override
    {
        if (! append)
            this->values.clear();
        std::istringstream is(str);
        std::string item_str;
        while (std::getline(is, item_str, ',')) {
            std::istringstream iss(item_str);
            double value;
            iss >> value;
            this->values.push_back(value);
        }
        return true;
    }

    ConfigOptionFloats& operator=(const ConfigOption *opt)
    {   
        this->set(opt);
        return *this;
    }

private:
	friend class cereal::access;
	template<class Archive> void serialize(Archive &ar) { ar(cereal::base_class<ConfigOptionVector<double>>(this)); }
};

class ConfigOptionInt : public ConfigOptionSingle<int>
{
public:
    ConfigOptionInt() : ConfigOptionSingle<int>(0) {}
    explicit ConfigOptionInt(int value) : ConfigOptionSingle<int>(value) {}
    explicit ConfigOptionInt(double _value) : ConfigOptionSingle<int>(int(floor(_value + 0.5))) {}
    
    static ConfigOptionType static_type() { return coInt; }
    ConfigOptionType        type()   const override { return static_type(); }
    int                     getInt() const override { return this->value; }
    void                    setInt(int val) { this->value = val; }
    ConfigOption*           clone()  const override { return new ConfigOptionInt(*this); }
    bool                    operator==(const ConfigOptionInt &rhs) const { return this->value == rhs.value; }
    
    std::string serialize() const override 
    {
        std::ostringstream ss;
        ss << this->value;
        return ss.str();
    }
    
    bool deserialize(const std::string &str, bool append = false) override
    {
        UNUSED(append);
        std::istringstream iss(str);
        iss >> this->value;
        return !iss.fail();
    }

    ConfigOptionInt& operator=(const ConfigOption *opt) 
    {   
        this->set(opt);
        return *this;
    }

private:
	friend class cereal::access;
	template<class Archive> void serialize(Archive &ar) { ar(cereal::base_class<ConfigOptionSingle<int>>(this)); }
};

class ConfigOptionInts : public ConfigOptionVector<int>
{
public:
    ConfigOptionInts() : ConfigOptionVector<int>() {}
    explicit ConfigOptionInts(size_t n, int value) : ConfigOptionVector<int>(n, value) {}
    explicit ConfigOptionInts(std::initializer_list<int> il) : ConfigOptionVector<int>(std::move(il)) {}

    static ConfigOptionType static_type() { return coInts; }
    ConfigOptionType        type()  const override { return static_type(); }
    ConfigOption*           clone() const override { return new ConfigOptionInts(*this); }
    ConfigOptionInts&       operator=(const ConfigOption *opt) { this->set(opt); return *this; }
    bool                    operator==(const ConfigOptionInts &rhs) const { return this->values == rhs.values; }

    std::string serialize() const override {
        std::ostringstream ss;
        for (std::vector<int>::const_iterator it = this->values.begin(); it != this->values.end(); ++it) {
            if (it - this->values.begin() != 0) ss << ",";
            ss << *it;
        }
        return ss.str();
    }
    
    std::vector<std::string> vserialize() const override 
    {
        std::vector<std::string> vv;
        vv.reserve(this->values.size());
        for (std::vector<int>::const_iterator it = this->values.begin(); it != this->values.end(); ++it) {
            std::ostringstream ss;
            ss << *it;
            vv.push_back(ss.str());
        }
        return vv;
    }
    
    bool deserialize(const std::string &str, bool append = false) override
    {
        if (! append)
            this->values.clear();
        std::istringstream is(str);
        std::string item_str;
        while (std::getline(is, item_str, ',')) {
            std::istringstream iss(item_str);
            int value;
            iss >> value;
            this->values.push_back(value);
        }
        return true;
    }

private:
	friend class cereal::access;
	template<class Archive> void serialize(Archive &ar) { ar(cereal::base_class<ConfigOptionVector<int>>(this)); }
};

class ConfigOptionString : public ConfigOptionSingle<std::string>
{
public:
    ConfigOptionString() : ConfigOptionSingle<std::string>("") {}
    explicit ConfigOptionString(const std::string &value) : ConfigOptionSingle<std::string>(value) {}
    
    static ConfigOptionType static_type() { return coString; }
    ConfigOptionType        type()  const override { return static_type(); }
    ConfigOption*           clone() const override { return new ConfigOptionString(*this); }
    ConfigOptionString&     operator=(const ConfigOption *opt) { this->set(opt); return *this; }
    bool                    operator==(const ConfigOptionString &rhs) const { return this->value == rhs.value; }

    std::string serialize() const override
    { 
        return escape_string_cstyle(this->value); 
    }

    bool deserialize(const std::string &str, bool append = false) override 
    {
        UNUSED(append);
        return unescape_string_cstyle(str, this->value);
    }

private:
	friend class cereal::access;
	template<class Archive> void serialize(Archive &ar) { ar(cereal::base_class<ConfigOptionSingle<std::string>>(this)); }
};

// semicolon-separated strings
class ConfigOptionStrings : public ConfigOptionVector<std::string>
{
public:
    ConfigOptionStrings() : ConfigOptionVector<std::string>() {}
    explicit ConfigOptionStrings(size_t n, const std::string &value) : ConfigOptionVector<std::string>(n, value) {}
    explicit ConfigOptionStrings(const std::vector<std::string> &values) : ConfigOptionVector<std::string>(values) {}
    explicit ConfigOptionStrings(std::vector<std::string> &&values) : ConfigOptionVector<std::string>(std::move(values)) {}
    explicit ConfigOptionStrings(std::initializer_list<std::string> il) : ConfigOptionVector<std::string>(std::move(il)) {}

    static ConfigOptionType static_type() { return coStrings; }
    ConfigOptionType        type()  const override { return static_type(); }
    ConfigOption*           clone() const override { return new ConfigOptionStrings(*this); }
    ConfigOptionStrings&    operator=(const ConfigOption *opt) { this->set(opt); return *this; }
    bool                    operator==(const ConfigOptionStrings &rhs) const { return this->values == rhs.values; }

    std::string serialize() const override
    {
        return escape_strings_cstyle(this->values);
    }
    
    std::vector<std::string> vserialize() const override
    {
        return this->values;
    }
    
    bool deserialize(const std::string &str, bool append = false) override
    {
        if (! append)
            this->values.clear();
        return unescape_strings_cstyle(str, this->values);
    }

private:
	friend class cereal::access;
	template<class Archive> void serialize(Archive &ar) { ar(cereal::base_class<ConfigOptionVector<std::string>>(this)); }
};

class ConfigOptionPercent : public ConfigOptionFloat
{
public:
    ConfigOptionPercent() : ConfigOptionFloat(0) {}
    explicit ConfigOptionPercent(double _value) : ConfigOptionFloat(_value) {}
    
    static ConfigOptionType static_type() { return coPercent; }
    ConfigOptionType        type()  const override { return static_type(); }
    ConfigOption*           clone() const override { return new ConfigOptionPercent(*this); }
    ConfigOptionPercent&    operator=(const ConfigOption *opt) { this->set(opt); return *this; }
    bool                    operator==(const ConfigOptionPercent &rhs) const { return this->value == rhs.value; }
    double                  get_abs_value(double ratio_over) const { return ratio_over * this->value / 100; }
    
    std::string serialize() const override 
    {
        std::ostringstream ss;
        ss << this->value;
        std::string s(ss.str());
        s += "%";
        return s;
    }
    
    bool deserialize(const std::string &str, bool append = false) override
    {
        UNUSED(append);
        // don't try to parse the trailing % since it's optional
        std::istringstream iss(str);
        iss >> this->value;
        return !iss.fail();
    }

private:
	friend class cereal::access;
	template<class Archive> void serialize(Archive &ar) { ar(cereal::base_class<ConfigOptionFloat>(this)); }
};

class ConfigOptionPercents : public ConfigOptionFloats
{
public:
    ConfigOptionPercents() : ConfigOptionFloats() {}
    explicit ConfigOptionPercents(size_t n, double value) : ConfigOptionFloats(n, value) {}
    explicit ConfigOptionPercents(std::initializer_list<double> il) : ConfigOptionFloats(std::move(il)) {}

    static ConfigOptionType static_type() { return coPercents; }
    ConfigOptionType        type()  const override { return static_type(); }
    ConfigOption*           clone() const override { return new ConfigOptionPercents(*this); }
    ConfigOptionPercents&   operator=(const ConfigOption *opt) { this->set(opt); return *this; }
    bool                    operator==(const ConfigOptionPercents &rhs) const { return this->values == rhs.values; }

    std::string serialize() const override
    {
        std::ostringstream ss;
        for (const auto &v : this->values) {
            if (&v != &this->values.front()) ss << ",";
            ss << v << "%";
        }
        std::string str = ss.str();
        return str;
    }
    
    std::vector<std::string> vserialize() const override
    {
        std::vector<std::string> vv;
        vv.reserve(this->values.size());
        for (const auto v : this->values) {
            std::ostringstream ss;
            ss << v;
            std::string sout = ss.str() + "%";
            vv.push_back(sout);
        }
        return vv;
    }

    bool deserialize(const std::string &str, bool append = false) override
    {
        if (! append)
            this->values.clear();
        std::istringstream is(str);
        std::string item_str;
        while (std::getline(is, item_str, ',')) {
            std::istringstream iss(item_str);
            double value;
            // don't try to parse the trailing % since it's optional
            iss >> value;
            this->values.push_back(value);
        }
        return true;
    }

private:
	friend class cereal::access;
	template<class Archive> void serialize(Archive &ar) { ar(cereal::base_class<ConfigOptionFloats>(this)); }
};

class ConfigOptionFloatOrPercent : public ConfigOptionPercent
{
public:
    bool percent;
    ConfigOptionFloatOrPercent() : ConfigOptionPercent(0), percent(false) {}
    explicit ConfigOptionFloatOrPercent(double _value, bool _percent) : ConfigOptionPercent(_value), percent(_percent) {}

    static ConfigOptionType     static_type() { return coFloatOrPercent; }
    ConfigOptionType            type()  const override { return static_type(); }
    ConfigOption*               clone() const override { return new ConfigOptionFloatOrPercent(*this); }
    ConfigOptionFloatOrPercent& operator=(const ConfigOption *opt) { this->set(opt); return *this; }
    bool                        operator==(const ConfigOption &rhs) const override
    {
        if (rhs.type() != this->type())
            throw std::runtime_error("ConfigOptionFloatOrPercent: Comparing incompatible types");
        assert(dynamic_cast<const ConfigOptionFloatOrPercent*>(&rhs));
        return *this == *static_cast<const ConfigOptionFloatOrPercent*>(&rhs);
    }
    bool                        operator==(const ConfigOptionFloatOrPercent &rhs) const 
        { return this->value == rhs.value && this->percent == rhs.percent; }
    double                      get_abs_value(double ratio_over) const 
        { return this->percent ? (ratio_over * this->value / 100) : this->value; }

    void set(const ConfigOption *rhs) override {
        if (rhs->type() != this->type())
            throw std::runtime_error("ConfigOptionFloatOrPercent: Assigning an incompatible type");
        assert(dynamic_cast<const ConfigOptionFloatOrPercent*>(rhs));
        *this = *static_cast<const ConfigOptionFloatOrPercent*>(rhs);
    }

    std::string serialize() const override
    {
        std::ostringstream ss;
        ss << this->value;
        std::string s(ss.str());
        if (this->percent) s += "%";
        return s;
    }
    
    bool deserialize(const std::string &str, bool append = false) override
    {
        UNUSED(append);
        this->percent = str.find_first_of("%") != std::string::npos;
        std::istringstream iss(str);
        iss >> this->value;
        return !iss.fail();
    }

private:
	friend class cereal::access;
	template<class Archive> void serialize(Archive &ar) { ar(cereal::base_class<ConfigOptionPercent>(this), percent); }
};

class ConfigOptionPoint : public ConfigOptionSingle<Vec2d>
{
public:
    ConfigOptionPoint() : ConfigOptionSingle<Vec2d>(Vec2d(0,0)) {}
    explicit ConfigOptionPoint(const Vec2d &value) : ConfigOptionSingle<Vec2d>(value) {}
    
    static ConfigOptionType static_type() { return coPoint; }
    ConfigOptionType        type()  const override { return static_type(); }
    ConfigOption*           clone() const override { return new ConfigOptionPoint(*this); }
    ConfigOptionPoint&      operator=(const ConfigOption *opt) { this->set(opt); return *this; }
    bool                    operator==(const ConfigOptionPoint &rhs) const { return this->value == rhs.value; }

    std::string serialize() const override
    {
        std::ostringstream ss;
        ss << this->value(0);
        ss << ",";
        ss << this->value(1);
        return ss.str();
    }
    
    bool deserialize(const std::string &str, bool append = false) override
    {
        UNUSED(append);
        char dummy;
        return sscanf(str.data(), " %lf , %lf %c", &this->value(0), &this->value(1), &dummy) == 2 ||
               sscanf(str.data(), " %lf x %lf %c", &this->value(0), &this->value(1), &dummy) == 2;
    }

private:
	friend class cereal::access;
	template<class Archive> void serialize(Archive &ar) { ar(cereal::base_class<ConfigOptionSingle<Vec2d>>(this)); }
};

class ConfigOptionPoints : public ConfigOptionVector<Vec2d>
{
public:
    ConfigOptionPoints() : ConfigOptionVector<Vec2d>() {}
    explicit ConfigOptionPoints(size_t n, const Vec2d &value) : ConfigOptionVector<Vec2d>(n, value) {}
    explicit ConfigOptionPoints(std::initializer_list<Vec2d> il) : ConfigOptionVector<Vec2d>(std::move(il)) {}
    explicit ConfigOptionPoints(const std::vector<Vec2d> &values) : ConfigOptionVector<Vec2d>(values) {}

    static ConfigOptionType static_type() { return coPoints; }
    ConfigOptionType        type()  const override { return static_type(); }
    ConfigOption*           clone() const override { return new ConfigOptionPoints(*this); }
    ConfigOptionPoints&     operator=(const ConfigOption *opt) { this->set(opt); return *this; }
    bool                    operator==(const ConfigOptionPoints &rhs) const { return this->values == rhs.values; }

    std::string serialize() const override
    {
        std::ostringstream ss;
        for (Pointfs::const_iterator it = this->values.begin(); it != this->values.end(); ++it) {
            if (it - this->values.begin() != 0) ss << ",";
            ss << (*it)(0);
            ss << "x";
            ss << (*it)(1);
        }
        return ss.str();
    }
    
    std::vector<std::string> vserialize() const override
    {
        std::vector<std::string> vv;
        for (Pointfs::const_iterator it = this->values.begin(); it != this->values.end(); ++it) {
            std::ostringstream ss;
            ss << *it;
            vv.push_back(ss.str());
        }
        return vv;
    }
    
    bool deserialize(const std::string &str, bool append = false) override
    {
        if (! append)
            this->values.clear();
        std::istringstream is(str);
        std::string point_str;
        while (std::getline(is, point_str, ',')) {
            Vec2d point(Vec2d::Zero());
            std::istringstream iss(point_str);
            std::string coord_str;
            if (std::getline(iss, coord_str, 'x')) {
                std::istringstream(coord_str) >> point(0);
                if (std::getline(iss, coord_str, 'x')) {
                    std::istringstream(coord_str) >> point(1);
                }
            }
            this->values.push_back(point);
        }
        return true;
    }

private:
	friend class cereal::access;
	template<class Archive> void save(Archive& archive) const {
		size_t cnt = this->values.size();
		archive(cnt);
		archive.saveBinary((const char*)this->values.data(), sizeof(Vec2d) * cnt);
	}
	template<class Archive> void load(Archive& archive) {
		size_t cnt;
		archive(cnt);
		this->values.assign(cnt, Vec2d());
		archive.loadBinary((char*)this->values.data(), sizeof(Vec2d) * cnt);
	}
};

class ConfigOptionPoint3 : public ConfigOptionSingle<Vec3d>
{
public:
    ConfigOptionPoint3() : ConfigOptionSingle<Vec3d>(Vec3d(0,0,0)) {}
    explicit ConfigOptionPoint3(const Vec3d &value) : ConfigOptionSingle<Vec3d>(value) {}
    
    static ConfigOptionType static_type() { return coPoint3; }
    ConfigOptionType        type()  const override { return static_type(); }
    ConfigOption*           clone() const override { return new ConfigOptionPoint3(*this); }
    ConfigOptionPoint3&     operator=(const ConfigOption *opt) { this->set(opt); return *this; }
    bool                    operator==(const ConfigOptionPoint3 &rhs) const { return this->value == rhs.value; }

    std::string serialize() const override
    {
        std::ostringstream ss;
        ss << this->value(0);
        ss << ",";
        ss << this->value(1);
        ss << ",";
        ss << this->value(2);
        return ss.str();
    }
    
    bool deserialize(const std::string &str, bool append = false) override
    {
        UNUSED(append);
        char dummy;
        return sscanf(str.data(), " %lf , %lf , %lf %c", &this->value(0), &this->value(1), &this->value(2), &dummy) == 2 ||
               sscanf(str.data(), " %lf x %lf x %lf %c", &this->value(0), &this->value(1), &this->value(2), &dummy) == 2;
    }

private:
	friend class cereal::access;
	template<class Archive> void serialize(Archive &ar) { ar(cereal::base_class<ConfigOptionSingle<Vec3d>>(this)); }
};

class ConfigOptionBool : public ConfigOptionSingle<bool>
{
public:
    ConfigOptionBool() : ConfigOptionSingle<bool>(false) {}
    explicit ConfigOptionBool(bool _value) : ConfigOptionSingle<bool>(_value) {}
    
    static ConfigOptionType static_type() { return coBool; }
    ConfigOptionType        type()      const override { return static_type(); }
    bool                    getBool()   const override { return this->value; }
    ConfigOption*           clone()     const override { return new ConfigOptionBool(*this); }
    ConfigOptionBool&       operator=(const ConfigOption *opt) { this->set(opt); return *this; }
    bool                    operator==(const ConfigOptionBool &rhs) const { return this->value == rhs.value; }

    std::string serialize() const override
    {
        return std::string(this->value ? "1" : "0");
    }
    
    bool deserialize(const std::string &str, bool append = false) override
    {
        UNUSED(append);
        this->value = (str.compare("1") == 0);
        return true;
    }

private:
	friend class cereal::access;
	template<class Archive> void serialize(Archive &ar) { ar(cereal::base_class<ConfigOptionSingle<bool>>(this)); }
};

class ConfigOptionBools : public ConfigOptionVector<unsigned char>
{
public:
    ConfigOptionBools() : ConfigOptionVector<unsigned char>() {}
    explicit ConfigOptionBools(size_t n, bool value) : ConfigOptionVector<unsigned char>(n, (unsigned char)value) {}
    explicit ConfigOptionBools(std::initializer_list<bool> il) { values.reserve(il.size()); for (bool b : il) values.emplace_back((unsigned char)b); }

    static ConfigOptionType static_type() { return coBools; }
    ConfigOptionType        type()  const override { return static_type(); }
    ConfigOption*           clone() const override { return new ConfigOptionBools(*this); }
    ConfigOptionBools&      operator=(const ConfigOption *opt) { this->set(opt); return *this; }
    bool                    operator==(const ConfigOptionBools &rhs) const { return this->values == rhs.values; }

    bool& get_at(size_t i) {
        assert(! this->values.empty());
        return *reinterpret_cast<bool*>(&((i < this->values.size()) ? this->values[i] : this->values.front()));
    }

    //FIXME this smells, the parent class has the method declared returning (unsigned char&).
    bool get_at(size_t i) const { return ((i < this->values.size()) ? this->values[i] : this->values.front()) != 0; }

    std::string serialize() const override
    {
        std::ostringstream ss;
        for (std::vector<unsigned char>::const_iterator it = this->values.begin(); it != this->values.end(); ++it) {
            if (it - this->values.begin() != 0) ss << ",";
            ss << (*it ? "1" : "0");
        }
        return ss.str();
    }
    
    std::vector<std::string> vserialize() const override
    {
        std::vector<std::string> vv;
        for (std::vector<unsigned char>::const_iterator it = this->values.begin(); it != this->values.end(); ++it) {
            std::ostringstream ss;
            ss << (*it ? "1" : "0");
            vv.push_back(ss.str());
        }
        return vv;
    }
    
    bool deserialize(const std::string &str, bool append = false) override
    {
        if (! append)
            this->values.clear();
        std::istringstream is(str);
        std::string item_str;
        while (std::getline(is, item_str, ',')) {
            this->values.push_back(item_str.compare("1") == 0);
        }
        return true;
    }

private:
	friend class cereal::access;
	template<class Archive> void serialize(Archive &ar) { ar(cereal::base_class<ConfigOptionVector<unsigned char>>(this)); }
};

// Map from an enum integer value to an enum name.
typedef std::vector<std::string>  t_config_enum_names;
// Map from an enum name to an enum integer value.
typedef std::map<std::string,int> t_config_enum_values;

template <class T>
class ConfigOptionEnum : public ConfigOptionSingle<T>
{
public:
    // by default, use the first value (0) of the T enum type
    ConfigOptionEnum() : ConfigOptionSingle<T>(static_cast<T>(0)) {}
    explicit ConfigOptionEnum(T _value) : ConfigOptionSingle<T>(_value) {}
    
    static ConfigOptionType static_type() { return coEnum; }
    ConfigOptionType        type()  const override { return static_type(); }
    ConfigOption*           clone() const override { return new ConfigOptionEnum<T>(*this); }
    ConfigOptionEnum<T>&    operator=(const ConfigOption *opt) { this->set(opt); return *this; }
    bool                    operator==(const ConfigOptionEnum<T> &rhs) const { return this->value == rhs.value; }
    int                     getInt() const override { return (int)this->value; }

    bool operator==(const ConfigOption &rhs) const override
    {
        if (rhs.type() != this->type())
            throw std::runtime_error("ConfigOptionEnum<T>: Comparing incompatible types");
        // rhs could be of the following type: ConfigOptionEnumGeneric or ConfigOptionEnum<T>
        return this->value == (T)rhs.getInt();
    }

    void set(const ConfigOption *rhs) override {
        if (rhs->type() != this->type())
            throw std::runtime_error("ConfigOptionEnum<T>: Assigning an incompatible type");
        // rhs could be of the following type: ConfigOptionEnumGeneric or ConfigOptionEnum<T>
        this->value = (T)rhs->getInt();
    }

    std::string serialize() const override
    {
        const t_config_enum_names& names = ConfigOptionEnum<T>::get_enum_names();
        assert(static_cast<int>(this->value) < int(names.size()));
        return names[static_cast<int>(this->value)];
    }

    bool deserialize(const std::string &str, bool append = false) override
    {
        UNUSED(append);
        return from_string(str, this->value);
    }

    static bool has(T value) 
    {
        for (const std::pair<std::string, int> &kvp : ConfigOptionEnum<T>::get_enum_values())
            if (kvp.second == value)
                return true;
        return false;
    }

    // Map from an enum name to an enum integer value.
    static const t_config_enum_names& get_enum_names() 
    {
        static t_config_enum_names names;
        if (names.empty()) {
            // Initialize the map.
            const t_config_enum_values &enum_keys_map = ConfigOptionEnum<T>::get_enum_values();
            int cnt = 0;
            for (const std::pair<std::string, int> &kvp : enum_keys_map)
                cnt = std::max(cnt, kvp.second);
            cnt += 1;
            names.assign(cnt, "");
            for (const std::pair<std::string, int> &kvp : enum_keys_map)
                names[kvp.second] = kvp.first;
        }
        return names;
    }
    // Map from an enum name to an enum integer value.
    static const t_config_enum_values& get_enum_values();

    static bool from_string(const std::string &str, T &value)
    {
        const t_config_enum_values &enum_keys_map = ConfigOptionEnum<T>::get_enum_values();
        auto it = enum_keys_map.find(str);
        if (it == enum_keys_map.end())
            return false;
        value = static_cast<T>(it->second);
        return true;
    }
};

// Generic enum configuration value.
// We use this one in DynamicConfig objects when creating a config value object for ConfigOptionType == coEnum.
// In the StaticConfig, it is better to use the specialized ConfigOptionEnum<T> containers.
class ConfigOptionEnumGeneric : public ConfigOptionInt
{
public:
    ConfigOptionEnumGeneric(const t_config_enum_values* keys_map = nullptr) : keys_map(keys_map) {}
    explicit ConfigOptionEnumGeneric(const t_config_enum_values* keys_map, int value) : ConfigOptionInt(value), keys_map(keys_map) {}

    const t_config_enum_values* keys_map;
    
    static ConfigOptionType     static_type() { return coEnum; }
    ConfigOptionType            type()  const override { return static_type(); }
    ConfigOption*               clone() const override { return new ConfigOptionEnumGeneric(*this); }
    ConfigOptionEnumGeneric&    operator=(const ConfigOption *opt) { this->set(opt); return *this; }
    bool                        operator==(const ConfigOptionEnumGeneric &rhs) const { return this->value == rhs.value; }

    bool operator==(const ConfigOption &rhs) const override
    {
        if (rhs.type() != this->type())
            throw std::runtime_error("ConfigOptionEnumGeneric: Comparing incompatible types");
        // rhs could be of the following type: ConfigOptionEnumGeneric or ConfigOptionEnum<T>
        return this->value == rhs.getInt();
    }

    void set(const ConfigOption *rhs) override {
        if (rhs->type() != this->type())
            throw std::runtime_error("ConfigOptionEnumGeneric: Assigning an incompatible type");
        // rhs could be of the following type: ConfigOptionEnumGeneric or ConfigOptionEnum<T>
        this->value = rhs->getInt();
    }

    std::string serialize() const override
    {
        for (const auto &kvp : *this->keys_map)
            if (kvp.second == this->value) 
                return kvp.first;
        return std::string();
    }

    bool deserialize(const std::string &str, bool append = false) override
    {
        UNUSED(append);
        auto it = this->keys_map->find(str);
        if (it == this->keys_map->end())
            return false;
        this->value = it->second;
        return true;
    }

private:
	friend class cereal::access;
	template<class Archive> void serialize(Archive& ar) { ar(cereal::base_class<ConfigOptionInt>(this)); }
};

// Definition of a configuration value for the purpose of GUI presentation, editing, value mapping and config file handling.
class ConfigOptionDef
{
public:
	// Identifier of this option. It is stored here so that it is accessible through the by_serialization_key_ordinal map.
	t_config_option_key 				opt_key;
    // What type? bool, int, string etc.
    ConfigOptionType                    type            = coNone;
    // Default value of this option. The default value object is owned by ConfigDef, it is released in its destructor.
    Slic3r::clonable_ptr<const ConfigOption> default_value;
    void 								set_default_value(const ConfigOption* ptr) { this->default_value = Slic3r::clonable_ptr<const ConfigOption>(ptr); }
    template<typename T> const T* 		get_default_value() const { return static_cast<const T*>(this->default_value.get()); }

    // Create an empty option to be used as a base for deserialization of DynamicConfig.
    ConfigOption*						create_empty_option() const;
    // Create a default option to be inserted into a DynamicConfig.
    ConfigOption*						create_default_option() const;

    template<class Archive> ConfigOption* load_option_from_archive(Archive &archive) const {
	    switch (this->type) {
	    case coFloat:           { auto opt = new ConfigOptionFloat();  			archive(*opt); return opt; }
	    case coFloats:          { auto opt = new ConfigOptionFloats(); 			archive(*opt); return opt; }
	    case coInt:             { auto opt = new ConfigOptionInt();    			archive(*opt); return opt; }
	    case coInts:            { auto opt = new ConfigOptionInts();   			archive(*opt); return opt; }
	    case coString:          { auto opt = new ConfigOptionString(); 			archive(*opt); return opt; }
	    case coStrings:         { auto opt = new ConfigOptionStrings(); 		archive(*opt); return opt; }
	    case coPercent:         { auto opt = new ConfigOptionPercent(); 		archive(*opt); return opt; }
	    case coPercents:        { auto opt = new ConfigOptionPercents(); 		archive(*opt); return opt; }
	    case coFloatOrPercent:  { auto opt = new ConfigOptionFloatOrPercent(); 	archive(*opt); return opt; }
	    case coPoint:           { auto opt = new ConfigOptionPoint(); 			archive(*opt); return opt; }
	    case coPoints:          { auto opt = new ConfigOptionPoints(); 			archive(*opt); return opt; }
	    case coPoint3:          { auto opt = new ConfigOptionPoint3(); 			archive(*opt); return opt; }
	    case coBool:            { auto opt = new ConfigOptionBool(); 			archive(*opt); return opt; }
	    case coBools:           { auto opt = new ConfigOptionBools(); 			archive(*opt); return opt; }
	    case coEnum:            { auto opt = new ConfigOptionEnumGeneric(this->enum_keys_map); archive(*opt); return opt; }
	    default:                throw std::runtime_error(std::string("ConfigOptionDef::load_option_from_archive(): Unknown option type for option ") + this->opt_key);
	    }
	}

    template<class Archive> ConfigOption* save_option_to_archive(Archive &archive, const ConfigOption *opt) const {
	    switch (this->type) {
	    case coFloat:           archive(*static_cast<const ConfigOptionFloat*>(opt));  			break;
	    case coFloats:          archive(*static_cast<const ConfigOptionFloats*>(opt)); 			break;
	    case coInt:             archive(*static_cast<const ConfigOptionInt*>(opt)); 	 		break;
	    case coInts:            archive(*static_cast<const ConfigOptionInts*>(opt)); 	 		break;
	    case coString:          archive(*static_cast<const ConfigOptionString*>(opt)); 			break;
	    case coStrings:         archive(*static_cast<const ConfigOptionStrings*>(opt)); 		break;
	    case coPercent:         archive(*static_cast<const ConfigOptionPercent*>(opt)); 		break;
	    case coPercents:        archive(*static_cast<const ConfigOptionPercents*>(opt)); 		break;
	    case coFloatOrPercent:  archive(*static_cast<const ConfigOptionFloatOrPercent*>(opt));	break;
	    case coPoint:           archive(*static_cast<const ConfigOptionPoint*>(opt)); 			break;
	    case coPoints:          archive(*static_cast<const ConfigOptionPoints*>(opt)); 			break;
	    case coPoint3:          archive(*static_cast<const ConfigOptionPoint3*>(opt)); 			break;
	    case coBool:            archive(*static_cast<const ConfigOptionBool*>(opt)); 			break;
	    case coBools:           archive(*static_cast<const ConfigOptionBools*>(opt)); 			break;
	    case coEnum:            archive(*static_cast<const ConfigOptionEnumGeneric*>(opt)); 	break;
	    default:                throw std::runtime_error(std::string("ConfigOptionDef::save_option_to_archive(): Unknown option type for option ") + this->opt_key);
	    }
		// Make the compiler happy, shut up the warnings.
		return nullptr;
	}

    // Usually empty. 
    // Special values - "i_enum_open", "f_enum_open" to provide combo box for int or float selection,
    // "select_open" - to open a selection dialog (currently only a serial port selection).
    std::string                         gui_type;
    // Usually empty. Otherwise "serialized" or "show_value"
    // The flags may be combined.
    // "serialized" - vector valued option is entered in a single edit field. Values are separated by a semicolon.
    // "show_value" - even if enum_values / enum_labels are set, still display the value, not the enum label.
    std::string                         gui_flags;
    // Label of the GUI input field.
    // In case the GUI input fields are grouped in some views, the label defines a short label of a grouped value,
    // while full_label contains a label of a stand-alone field.
    // The full label is shown, when adding an override parameter for an object or a modified object.
    std::string                         label;
    std::string                         full_label;
    // With which printer technology is this configuration valid?
    PrinterTechnology                   printer_technology = ptUnknown;
    // Category of a configuration field, from the GUI perspective.
    // One of: "Layers and Perimeters", "Infill", "Support material", "Speed", "Extruders", "Advanced", "Extrusion Width"
    std::string                         category;
    // A tooltip text shown in the GUI.
    std::string                         tooltip;
    // Text right from the input field, usually a unit of measurement.
    std::string                         sidetext;
    // Format of this parameter on a command line.
    std::string                         cli;
    // Set for type == coFloatOrPercent.
    // It provides a link to a configuration value, of which this option provides a ratio.
    // For example, 
    // For example external_perimeter_speed may be defined as a fraction of perimeter_speed.
    t_config_option_key                 ratio_over;
    // True for multiline strings.
    bool                                multiline       = false;
    // For text input: If true, the GUI text box spans the complete page width.
    bool                                full_width      = false;
    // Not editable. Currently only used for the display of the number of threads.
    bool                                readonly        = false;
    // Height of a multiline GUI text box.
    int                                 height          = -1;
    // Optional width of an input field.
    int                                 width           = -1;
    // <min, max> limit of a numeric input.
    // If not set, the <min, max> is set to <INT_MIN, INT_MAX>
    // By setting min=0, only nonnegative input is allowed.
    int                                 min = INT_MIN;
    int                                 max = INT_MAX;
    ConfigOptionMode                    mode = comSimple;
    // Legacy names for this configuration option.
    // Used when parsing legacy configuration file.
    std::vector<t_config_option_key>    aliases;
    // Sometimes a single value may well define multiple values in a "beginner" mode.
    // Currently used for aliasing "solid_layers" to "top_solid_layers", "bottom_solid_layers".
    std::vector<t_config_option_key>    shortcut;
    // Definition of values / labels for a combo box.
    // Mostly used for enums (when type == coEnum), but may be used for ints resp. floats, if gui_type is set to "i_enum_open" resp. "f_enum_open".
    std::vector<std::string>            enum_values;
    std::vector<std::string>            enum_labels;
    // For enums (when type == coEnum). Maps enum_values to enums.
    // Initialized by ConfigOptionEnum<xxx>::get_enum_values()
    const t_config_enum_values         *enum_keys_map   = nullptr;

    bool has_enum_value(const std::string &value) const {
        for (const std::string &v : enum_values)
            if (v == value)
                return true;
        return false;
    }

    // 0 is an invalid key.
    size_t 								serialization_key_ordinal = 0;

    // Returns the alternative CLI arguments for the given option.
    // If there are no cli arguments defined, use the key and replace underscores with dashes.
    std::vector<std::string> cli_args(const std::string &key) const;

    // Assign this key to cli to disable CLI for this option.
    static std::string                  nocli;
};

// Map from a config option name to its definition.
// The definition does not carry an actual value of the config option, only its constant default value.
// t_config_option_key is std::string
typedef std::map<t_config_option_key, ConfigOptionDef> t_optiondef_map;

// Definition of configuration values for the purpose of GUI presentation, editing, value mapping and config file handling.
// The configuration definition is static: It does not carry the actual configuration values,
// but it carries the defaults of the configuration values.
class ConfigDef
{
public:
    t_optiondef_map         					options;
    std::map<size_t, const ConfigOptionDef*>	by_serialization_key_ordinal;

    bool                    has(const t_config_option_key &opt_key) const { return this->options.count(opt_key) > 0; }
    const ConfigOptionDef*  get(const t_config_option_key &opt_key) const {
        t_optiondef_map::iterator it = const_cast<ConfigDef*>(this)->options.find(opt_key);
        return (it == this->options.end()) ? nullptr : &it->second;
    }
    std::vector<std::string> keys() const {
        std::vector<std::string> out;
        out.reserve(options.size());
        for(auto const& kvp : options)
            out.push_back(kvp.first);
        return out;
    }

    /// Iterate through all of the CLI options and write them to a stream.
    std::ostream&           print_cli_help(
        std::ostream& out, bool show_defaults, 
        std::function<bool(const ConfigOptionDef &)> filter = [](const ConfigOptionDef &){ return true; }) const;

protected:
    ConfigOptionDef*        add(const t_config_option_key &opt_key, ConfigOptionType type);
};

// An abstract configuration store.
class ConfigBase
{
public:
    // Definition of configuration values for the purpose of GUI presentation, editing, value mapping and config file handling.
    // The configuration definition is static: It does not carry the actual configuration values,
    // but it carries the defaults of the configuration values.
    
    ConfigBase() {}
    virtual ~ConfigBase() {}

    // Virtual overridables:
public:
    // Static configuration definition. Any value stored into this ConfigBase shall have its definition here.
    virtual const ConfigDef*        def() const = 0;
    // Find ando/or create a ConfigOption instance for a given name.
    virtual ConfigOption*           optptr(const t_config_option_key &opt_key, bool create = false) = 0;
    // Collect names of all configuration values maintained by this configuration store.
    virtual t_config_option_keys    keys() const = 0;
protected:
    // Verify whether the opt_key has not been obsoleted or renamed.
    // Both opt_key and value may be modified by handle_legacy().
    // If the opt_key is no more valid in this version of Slic3r, opt_key is cleared by handle_legacy().
    // handle_legacy() is called internally by set_deserialize().
    virtual void                    handle_legacy(t_config_option_key &opt_key, std::string &value) const {}

public:
    // Non-virtual methods:
    bool has(const t_config_option_key &opt_key) const { return this->option(opt_key) != nullptr; }
    const ConfigOption* option(const t_config_option_key &opt_key) const
        { return const_cast<ConfigBase*>(this)->option(opt_key, false); }
    ConfigOption* option(const t_config_option_key &opt_key, bool create = false)
        { return this->optptr(opt_key, create); }
    template<typename TYPE>
    TYPE* option(const t_config_option_key &opt_key, bool create = false)
    { 
        ConfigOption *opt = this->optptr(opt_key, create);
        return (opt == nullptr || opt->type() != TYPE::static_type()) ? nullptr : static_cast<TYPE*>(opt);
    }
    template<typename TYPE>
    const TYPE* option(const t_config_option_key &opt_key) const
        { return const_cast<ConfigBase*>(this)->option<TYPE>(opt_key, false); }
    template<typename TYPE>
    TYPE* option_throw(const t_config_option_key &opt_key, bool create = false)
    { 
        ConfigOption *opt = this->optptr(opt_key, create);
        if (opt == nullptr)
            throw UnknownOptionException(opt_key);
        if (opt->type() != TYPE::static_type())
            throw std::runtime_error("Conversion to a wrong type");
        return static_cast<TYPE*>(opt);
    }
    template<typename TYPE>
    const TYPE* option_throw(const t_config_option_key &opt_key) const
        { return const_cast<ConfigBase*>(this)->option_throw<TYPE>(opt_key, false); }
    // Apply all keys of other ConfigBase defined by this->def() to this ConfigBase.
    // An UnknownOptionException is thrown in case some option keys of other are not defined by this->def(),
    // or this ConfigBase is of a StaticConfig type and it does not support some of the keys, and ignore_nonexistent is not set.
    void apply(const ConfigBase &other, bool ignore_nonexistent = false) { this->apply_only(other, other.keys(), ignore_nonexistent); }
    // Apply explicitely enumerated keys of other ConfigBase defined by this->def() to this ConfigBase.
    // An UnknownOptionException is thrown in case some option keys are not defined by this->def(),
    // or this ConfigBase is of a StaticConfig type and it does not support some of the keys, and ignore_nonexistent is not set.
    void apply_only(const ConfigBase &other, const t_config_option_keys &keys, bool ignore_nonexistent = false);
    bool equals(const ConfigBase &other) const { return this->diff(other).empty(); }
    t_config_option_keys diff(const ConfigBase &other) const;
    t_config_option_keys equal(const ConfigBase &other) const;
    std::string opt_serialize(const t_config_option_key &opt_key) const;
    // Set a configuration value from a string, it will call an overridable handle_legacy() 
    // to resolve renamed and removed configuration keys.
    bool set_deserialize(const t_config_option_key &opt_key, const std::string &str, bool append = false);

    double get_abs_value(const t_config_option_key &opt_key) const;
    double get_abs_value(const t_config_option_key &opt_key, double ratio_over) const;
    void setenv_() const;
    void load(const std::string &file);
    void load_from_ini(const std::string &file);
    void load_from_gcode_file(const std::string &file);
    // Returns number of key/value pairs extracted.
    size_t load_from_gcode_string(const char* str);
    void load(const boost::property_tree::ptree &tree);
    void save(const std::string &file) const;

private:
    // Set a configuration value from a string.
    bool set_deserialize_raw(const t_config_option_key &opt_key_src, const std::string &str, bool append);
};

// Configuration store with dynamic number of configuration values.
// In Slic3r, the dynamic config is mostly used at the user interface layer.
class DynamicConfig : public virtual ConfigBase
{
public:
    DynamicConfig() {}
    DynamicConfig(const DynamicConfig& other) { *this = other; }
    DynamicConfig(DynamicConfig&& other) : options(std::move(other.options)) { other.options.clear(); }
    virtual ~DynamicConfig() { clear(); }

    // Copy a content of one DynamicConfig to another DynamicConfig.
    // If rhs.def() is not null, then it has to be equal to this->def(). 
    DynamicConfig& operator=(const DynamicConfig &rhs) 
    {
        assert(this->def() == nullptr || this->def() == rhs.def());
        this->clear();
        for (const auto &kvp : rhs.options)
            this->options[kvp.first].reset(kvp.second->clone());
        return *this;
    }

    // Move a content of one DynamicConfig to another DynamicConfig.
    // If rhs.def() is not null, then it has to be equal to this->def(). 
    DynamicConfig& operator=(DynamicConfig &&rhs) 
    {
        assert(this->def() == nullptr || this->def() == rhs.def());
        this->clear();
        this->options = std::move(rhs.options);
        rhs.options.clear();
        return *this;
    }

    // Add a content of one DynamicConfig to another DynamicConfig.
    // If rhs.def() is not null, then it has to be equal to this->def().
    DynamicConfig& operator+=(const DynamicConfig &rhs)
    {
        assert(this->def() == nullptr || this->def() == rhs.def());
        for (const auto &kvp : rhs.options) {
            auto it = this->options.find(kvp.first);
            if (it == this->options.end())
                this->options[kvp.first].reset(kvp.second->clone());
            else {
                assert(it->second->type() == kvp.second->type());
                if (it->second->type() == kvp.second->type())
                    *it->second = *kvp.second;
                else
                    it->second.reset(kvp.second->clone());
            }
        }
        return *this;
    }

    // Move a content of one DynamicConfig to another DynamicConfig.
    // If rhs.def() is not null, then it has to be equal to this->def().
    DynamicConfig& operator+=(DynamicConfig &&rhs) 
    {
        assert(this->def() == nullptr || this->def() == rhs.def());
        for (auto &kvp : rhs.options) {
            auto it = this->options.find(kvp.first);
            if (it == this->options.end()) {
                this->options.insert(std::make_pair(kvp.first, std::move(kvp.second)));
            } else {
                assert(it->second->type() == kvp.second->type());
                it->second = std::move(kvp.second);
            }
        }
        rhs.options.clear();
        return *this;
    }

    bool           operator==(const DynamicConfig &rhs) const;
    bool           operator!=(const DynamicConfig &rhs) const { return ! (*this == rhs); }

    void swap(DynamicConfig &other) 
    { 
        std::swap(this->options, other.options);
    }

    void clear()
    { 
        this->options.clear(); 
    }

    bool erase(const t_config_option_key &opt_key)
    { 
        auto it = this->options.find(opt_key);
        if (it == this->options.end())
            return false;
        this->options.erase(it);
        return true;
    }

    // Allow DynamicConfig to be instantiated on ints own without a definition.
    // If the definition is not defined, the method requiring the definition will throw NoDefinitionException.
    const ConfigDef*        def() const override { return nullptr; };
    template<class T> T*    opt(const t_config_option_key &opt_key, bool create = false)
        { return dynamic_cast<T*>(this->option(opt_key, create)); }
    template<class T> const T* opt(const t_config_option_key &opt_key) const
        { return dynamic_cast<const T*>(this->option(opt_key)); }
    // Overrides ConfigBase::optptr(). Find ando/or create a ConfigOption instance for a given name.
    ConfigOption*           optptr(const t_config_option_key &opt_key, bool create = false) override;
    // Overrides ConfigBase::keys(). Collect names of all configuration values maintained by this configuration store.
    t_config_option_keys    keys() const override;
    bool                    empty() const { return options.empty(); }

    // Set a value for an opt_key. Returns true if the value did not exist yet.
    // This DynamicConfig will take ownership of opt.
    // Be careful, as this method does not test the existence of opt_key in this->def().
    bool                    set_key_value(const std::string &opt_key, ConfigOption *opt)
    {
        auto it = this->options.find(opt_key);
        if (it == this->options.end()) {
            this->options[opt_key].reset(opt);
            return true;
        } else {
            it->second.reset(opt);
            return false;
        }
    }

    std::string&        opt_string(const t_config_option_key &opt_key, bool create = false)     { return this->option<ConfigOptionString>(opt_key, create)->value; }
    const std::string&  opt_string(const t_config_option_key &opt_key) const                    { return const_cast<DynamicConfig*>(this)->opt_string(opt_key); }
    std::string&        opt_string(const t_config_option_key &opt_key, unsigned int idx)        { return this->option<ConfigOptionStrings>(opt_key)->get_at(idx); }
    const std::string&  opt_string(const t_config_option_key &opt_key, unsigned int idx) const  { return const_cast<DynamicConfig*>(this)->opt_string(opt_key, idx); }

    double&             opt_float(const t_config_option_key &opt_key)                           { return this->option<ConfigOptionFloat>(opt_key)->value; }
    const double        opt_float(const t_config_option_key &opt_key) const                     { return dynamic_cast<const ConfigOptionFloat*>(this->option(opt_key))->value; }
    double&             opt_float(const t_config_option_key &opt_key, unsigned int idx)         { return this->option<ConfigOptionFloats>(opt_key)->get_at(idx); }
    const double        opt_float(const t_config_option_key &opt_key, unsigned int idx) const   { return dynamic_cast<const ConfigOptionFloats*>(this->option(opt_key))->get_at(idx); }

    int&                opt_int(const t_config_option_key &opt_key)                             { return this->option<ConfigOptionInt>(opt_key)->value; }
    const int           opt_int(const t_config_option_key &opt_key) const                       { return dynamic_cast<const ConfigOptionInt*>(this->option(opt_key))->value; }
    int&                opt_int(const t_config_option_key &opt_key, unsigned int idx)           { return this->option<ConfigOptionInts>(opt_key)->get_at(idx); }
    const int           opt_int(const t_config_option_key &opt_key, unsigned int idx) const     { return dynamic_cast<const ConfigOptionInts*>(this->option(opt_key))->get_at(idx); }

    template<typename ENUM>
	ENUM                opt_enum(const t_config_option_key &opt_key) const                      { return (ENUM)dynamic_cast<const ConfigOptionEnumGeneric*>(this->option(opt_key))->value; }

    bool                opt_bool(const t_config_option_key &opt_key) const                      { return this->option<ConfigOptionBool>(opt_key)->value != 0; }
    bool                opt_bool(const t_config_option_key &opt_key, unsigned int idx) const    { return this->option<ConfigOptionBools>(opt_key)->get_at(idx) != 0; }

    // Command line processing
    void                read_cli(const std::vector<std::string> &tokens, t_config_option_keys* extra, t_config_option_keys* keys = nullptr);
    bool                read_cli(int argc, char** argv, t_config_option_keys* extra, t_config_option_keys* keys = nullptr);

    std::map<t_config_option_key, std::unique_ptr<ConfigOption>>::const_iterator cbegin() const { return options.cbegin(); }
    std::map<t_config_option_key, std::unique_ptr<ConfigOption>>::const_iterator cend()   const { return options.cend(); }
    size_t                        												 size()   const { return options.size(); }

private:
    std::map<t_config_option_key, std::unique_ptr<ConfigOption>> options;

	friend class cereal::access;
	template<class Archive> void serialize(Archive &ar) { ar(options); }
};

/// Configuration store with a static definition of configuration values.
/// In Slic3r, the static configuration stores are during the slicing / g-code generation for efficiency reasons,
/// because the configuration values could be accessed directly.
class StaticConfig : public virtual ConfigBase
{
public:
    StaticConfig() {}
    /// Gets list of config option names for each config option of this->def, which has a static counter-part defined by the derived object
    /// and which could be resolved by this->optptr(key) call.
    t_config_option_keys keys() const;

protected:
    /// Set all statically defined config options to their defaults defined by this->def().
    void set_defaults();
};

}

#endif
