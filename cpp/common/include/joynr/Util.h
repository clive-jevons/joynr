/*
 * #%L
 * %%
 * Copyright (C) 2011 - 2015 BMW Car IT GmbH
 * %%
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * #L%
 */
#ifndef UTIL_H_
#define UTIL_H_

#include "joynr/JoynrCommonExport.h"
#include "joynr/joynrlogging.h"

#include <QByteArray>
#include <cassert>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#include <set>
#include "joynr/exceptions/JoynrException.h"
#include "joynr/Variant.h"

namespace joynr
{

/**
  * @class Util
  * @brief Container class for helper methods
  */
class JOYNRCOMMON_EXPORT Util
{
public:
    /**
      * Splits a byte array representation of multiple JSON objects into
      * a list of byte arrays, each containing a single JSON object.
      */
    static std::vector<QByteArray> splitIntoJsonObjects(const QByteArray& jsonStream);

    static std::string attributeGetterFromName(const std::string& attributeName);

    template <typename T>
    static typename T::Enum convertVariantToEnum(const Variant& v)
    {
        std::string enumValueName = v.get<std::string>();
        return T::getEnum(enumValueName);
    }

    template <typename T>
    static std::vector<typename T::Enum> convertVariantVectorToEnumVector(
            const std::vector<Variant>& variantVector)
    {
        std::vector<typename T::Enum> enumVector;
        enumVector.reserve(variantVector.size());
        for (const Variant& variant : variantVector) {
            if (variant.is<std::string>()) {
                std::string enumValueName = variant.get<std::string>();
                enumVector.push_back(T::getEnum(enumValueName));
            }
        }
        return enumVector;
    }

    template <typename T>
    static std::vector<Variant> convertEnumVectorToVariantVector(
            const std::vector<typename T::Enum>& enumVector)
    {
        std::vector<Variant> variantVector;
        variantVector.reserve(enumVector.size());
        for (const typename T::Enum& enumValue : enumVector) {
            variantVector.push_back(Variant::make<typename T::Enum>(enumValue));
        }
        return variantVector;
    }

    template <class T>
    static std::vector<Variant> convertVectorToVariantVector(const std::vector<T>& inputVector)
    {
        std::vector<Variant> variantVector;
        variantVector.reserve(inputVector.size());
        for (const T& element : inputVector) {
            variantVector.push_back(Variant::make<T>(element));
        }
        return variantVector;
    }

    template <class T>
    static std::vector<T> convertVariantVectorToVector(const std::vector<Variant>& variantVector)
    {
        std::vector<T> typeVector;

        for (Variant variant : variantVector) {
            typeVector.push_back(variant.get<T>());
        }

        return typeVector;
    }

    template <class T>
    static std::vector<T> convertIntListToEnumList(const std::vector<int>& inputList)
    {
        std::vector<T> ret;
        ret.reserve(inputList.size());
        for (const int& i : inputList) {
            ret.push_back((T)i);
        }
        return ret;
    }

    template <class T>
    static std::vector<int> convertEnumListToIntList(const std::vector<T>& enumList)
    {
        std::vector<int> enumAsIntList;
        enumAsIntList.reserve(enumList.size());
        for (const T& e : enumList) {
            enumAsIntList.push_back(e);
        }
        return enumAsIntList;
    }

    /**
     * Create a Uuid for use in Joynr.
     *
     * This is simply a wrapper around boost::uuid
     */
    static std::string createUuid();

    /**
     * Log a serialized Joynr message
     */
    static void logSerializedMessage(joynr_logging::Logger* logger,
                                     const std::string& explanation,
                                     const std::string& message);

    static void throwJoynrException(const exceptions::JoynrException& error);

    template <typename... Ts>
    static int getTypeId();

    template <typename T>
    static T valueOf(const Variant& variant);

    template <int TupleSize>
    struct ExpandTupleIntoFunctionArguments
    {
        template <typename Function, typename FunctionClass, typename Tuple, typename... Arguments>
        static inline auto expandTupleIntoFunctionArguments(Function& func,
                                                            FunctionClass& funcClass,
                                                            Tuple& tuple,
                                                            Arguments&... args)
                -> decltype(ExpandTupleIntoFunctionArguments<
                        TupleSize - 1>::expandTupleIntoFunctionArguments(func,
                                                                         funcClass,
                                                                         tuple,
                                                                         std::get<TupleSize - 1>(
                                                                                 tuple),
                                                                         args...))
        {

            return ExpandTupleIntoFunctionArguments<
                    TupleSize - 1>::expandTupleIntoFunctionArguments(func,
                                                                     funcClass,
                                                                     tuple,
                                                                     std::get<TupleSize - 1>(tuple),
                                                                     args...);
        }
    };

    template <typename Function, typename FunctionClass, typename Tuple>
    static inline auto expandTupleIntoFunctionArguments(Function& func,
                                                        FunctionClass& funcClass,
                                                        Tuple& tuple)
            -> decltype(ExpandTupleIntoFunctionArguments<std::tuple_size<typename std::decay<
                    Tuple>::type>::value>::expandTupleIntoFunctionArguments(func, funcClass, tuple))
    {

        return ExpandTupleIntoFunctionArguments<std::tuple_size<typename std::decay<
                Tuple>::type>::value>::expandTupleIntoFunctionArguments(func, funcClass, tuple);
    }

    template <typename... Ts>
    static std::tuple<Ts...> toValueTuple(const std::vector<Variant>& list);

private:
    static joynr_logging::Logger* logger;

    template <typename T, typename... Ts>
    static int getTypeId_split()
    {
        int prime = 31;
        return JoynrTypeId<T>::getTypeId() + prime * getTypeId<Ts...>();
    }

    template <std::size_t index, typename T, typename... Ts>
    static std::tuple<T, Ts...> toValueTuple_split(const std::vector<Variant>& list)
    {
        T value = valueOf<T>(list[index]);
        return std::tuple_cat(std::make_tuple(value), toValueTuple_split<index + 1, Ts...>(list));
    }

    template <std::size_t index>
    static std::tuple<> toValueTuple_split(const std::vector<Variant>& list)
    {
        assert(list.size() == index);
        std::ignore = list;
        return std::make_tuple();
    }
};

template <typename T>
inline T Util::valueOf(const Variant& variant)
{
    return variant.get<T>();
}

template <>
inline float Util::valueOf<float>(const Variant& variant)
{
    return variant.get<double>();
}

// concrete specilization for lists of primitive datatypes
template <>
inline std::vector<int8_t> Util::valueOf<std::vector<int8_t>>(const Variant& variant)
{
    return joynr::Util::convertVariantVectorToVector<int8_t>(variant.get<std::vector<Variant>>());
}

template <>
inline std::vector<uint8_t> Util::valueOf<std::vector<uint8_t>>(const Variant& variant)
{
    return joynr::Util::convertVariantVectorToVector<uint8_t>(variant.get<std::vector<Variant>>());
}

template <>
inline std::vector<int16_t> Util::valueOf<std::vector<int16_t>>(const Variant& variant)
{
    return joynr::Util::convertVariantVectorToVector<int16_t>(variant.get<std::vector<Variant>>());
}

template <>
inline std::vector<uint16_t> Util::valueOf<std::vector<uint16_t>>(const Variant& variant)
{
    return joynr::Util::convertVariantVectorToVector<uint16_t>(variant.get<std::vector<Variant>>());
}

template <>
inline std::vector<int32_t> Util::valueOf<std::vector<int32_t>>(const Variant& variant)
{
    return joynr::Util::convertVariantVectorToVector<int32_t>(variant.get<std::vector<Variant>>());
}

template <>
inline std::vector<uint32_t> Util::valueOf<std::vector<uint32_t>>(const Variant& variant)
{
    return joynr::Util::convertVariantVectorToVector<uint32_t>(variant.get<std::vector<Variant>>());
}

template <>
inline std::vector<int64_t> Util::valueOf<std::vector<int64_t>>(const Variant& variant)
{
    return joynr::Util::convertVariantVectorToVector<int64_t>(variant.get<std::vector<Variant>>());
}

template <>
inline std::vector<uint64_t> Util::valueOf<std::vector<uint64_t>>(const Variant& variant)
{
    return joynr::Util::convertVariantVectorToVector<uint64_t>(variant.get<std::vector<Variant>>());
}

template <>
inline std::vector<float> Util::valueOf<std::vector<float>>(const Variant& variant)
{
    std::vector<double> doubles =
            joynr::Util::convertVariantVectorToVector<double>(variant.get<std::vector<Variant>>());
    std::vector<float> floats(doubles.size());
    std::copy(doubles.cbegin(), doubles.cend(), floats.begin());
    return floats;
}

template <>
inline std::vector<double> Util::valueOf<std::vector<double>>(const Variant& variant)
{
    return joynr::Util::convertVariantVectorToVector<double>(variant.get<std::vector<Variant>>());
}

template <>
inline std::vector<std::string> Util::valueOf<std::vector<std::string>>(const Variant& variant)
{
    return joynr::Util::convertVariantVectorToVector<std::string>(
            variant.get<std::vector<Variant>>());
}

template <typename... Ts>
inline int Util::getTypeId()
{
    return getTypeId_split<Ts...>();
}

template <>
inline int Util::getTypeId<>()
{
    return 0;
}

template <>
struct Util::ExpandTupleIntoFunctionArguments<0>
{
    template <typename Function, typename FunctionClass, typename Tuple, typename... Arguments>
    static inline auto expandTupleIntoFunctionArguments(Function& func,
                                                        FunctionClass& funcClass,
                                                        Tuple& tuple,
                                                        Arguments&... args)
            -> decltype(func(funcClass, args...))
    {
        std::ignore = tuple;
        return func(funcClass, args...);
    }
};

template <typename... Ts>
inline std::tuple<Ts...> Util::toValueTuple(const std::vector<Variant>& list)
{
    return toValueTuple_split<0, Ts...>(list);
}

template <typename T>
std::set<T> vectorToSet(const std::vector<T>& v)
{
    return std::set<T>(v.begin(), v.end());
}

template <typename T>
bool setContainsSet(const std::set<T>& haystack, const std::set<T>& needle)
{
    bool contains = true;
    for (const T& element : haystack) {
        contains = (needle.count(element) == 1);
    }
    return contains;
}

template <typename T>
bool vectorContains(const std::vector<T>& v, const T& e)
{
    return v.end() != std::find(v.cbegin(), v.cend(), e);
}

template <typename T>
auto removeAll(std::vector<T>& v, const T& e)
        -> decltype(v.erase(std::remove(v.begin(), v.end(), e), v.end()))
{
    return v.erase(std::remove(v.begin(), v.end(), e), v.end());
}

} // namespace joynr
#endif /* UTIL_H_ */
