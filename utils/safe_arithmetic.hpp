/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <typeinfo>

// signed integer
template <typename Type>
using IsSignedIntegral = std::integral_constant<bool, std::is_integral<Type>::value and std::is_signed<Type>::value>;

template <typename Type>
using EnableIfSignedIntegral = std::enable_if<IsSignedIntegral<Type>::value, Type>;

template <typename Type>
using EnableIfSignedIntegralType = typename EnableIfSignedIntegral<Type>::type;

// signed integral

// unsigned integral
template <typename Type>
using IsUnsignedIntegral =
    std::integral_constant<bool, std::is_integral<Type>::value and std::is_unsigned<Type>::value>;

template <typename Type>
using EnableIfUnSignedIntegral = std::enable_if<IsUnsignedIntegral<Type>::value, Type>;

template <typename Type>
using EnableIfUnSignedIntegralType = typename EnableIfUnSignedIntegral<Type>::type;
// unsigned integral

// float
template <typename Type>
using IsFloating = std::is_floating_point<Type>;

template <typename Type>
using EnableIfFloating = std::enable_if<IsFloating<Type>::value, Type>;

template <typename Type>
using EnableIfFloatingType = typename EnableIfFloating<Type>::type;
// float

// helper
template <typename Type>
using EnableIfArithmetic = std::enable_if<std::is_arithmetic<Type>::value, Type>;

template <typename Type>
using EnableIfArithmeticType = typename EnableIfArithmetic<Type>::type;
//

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
inline bool is_add_overflow(const T a, const T b) {
    if (((a < 0) && (b > 0)) || ((a > 0) && (b < 0)))
        return false;

    if ((a > 0) && (a > std::numeric_limits<T>::max() - b))
        return true;

    if ((a < 0) && (a < std::numeric_limits<T>::min() - b))
        return true;

    return false;
}

template <typename T, EnableIfSignedIntegralType<T> * = nullptr>
inline bool is_mul_overflow(const T a, const T b) {
    if (a == std::numeric_limits<T>::min())
        return !(b == 1 || b == 0);

    if (b == std::numeric_limits<T>::min())
        return !(a == 1 || a == 0);

    return a != 0 && b != 0 && (abs(a) > std::numeric_limits<T>::max() / abs(b));
}

template <typename T, EnableIfUnSignedIntegralType<T> * = nullptr>
inline bool is_mul_overflow(const T a, const T b) {
    return a != 0 && b != 0 && (a > std::numeric_limits<T>::max() / b);
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
inline T safe_add(const T a, const T b) {
    if (is_add_overflow(a, b))
        throw std::overflow_error("Overflow during addition operation");
    return a + b;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
inline T safe_mul(const T a, const T b) {
    if (is_mul_overflow(a, b))
        throw std::overflow_error("Overflow during multiplication operation");
    return a * b;
}

// Convert from floating 4,8 bytes to integer signed/unsigned 1,2,4,8 bytes, and from double to float
template <typename ReturnType, typename ValueType, typename = EnableIfFloatingType<ValueType>>
inline EnableIfArithmeticType<ReturnType> safe_convert(const ValueType value) {
    ReturnType max = std::numeric_limits<ReturnType>::max();
    ReturnType min = std::numeric_limits<ReturnType>::min();

    if (value > max) {
        return max;
    }
    if (value < min) {
        return min;
    }

    return static_cast<ReturnType>(value);
}

template <typename ReturnType, typename ValueType, typename = EnableIfSignedIntegralType<ValueType>>
inline EnableIfUnSignedIntegralType<ReturnType> safe_convert(const ValueType value) {
    if (value < 0) {
        return 0;
    }
    ReturnType max = std::numeric_limits<ReturnType>::max();
    if (static_cast<typename std::make_unsigned<ValueType>::type>(value) > max) {
        return max;
    }
    return static_cast<ReturnType>(value);
}

template <typename ReturnType, typename ValueType, typename = EnableIfUnSignedIntegralType<ValueType>>
inline EnableIfSignedIntegralType<ReturnType> safe_convert(const ValueType value) {
    ReturnType max = std::numeric_limits<ReturnType>::max();
    auto value_signed_max = std::numeric_limits<typename std::make_signed<ValueType>::type>::max();
    if (value_signed_max >= max && ValueType(max) < value) {
        return max;
    }
    return static_cast<ReturnType>(value);
}

template <typename ReturnType, typename ValueType, EnableIfUnSignedIntegralType<ValueType> * = nullptr>
inline EnableIfUnSignedIntegralType<ReturnType> safe_convert(const ValueType value) {
    ReturnType max = std::numeric_limits<ReturnType>::max();
    if (value > max) {
        return max;
    }
    return static_cast<ReturnType>(value);
}

template <typename ReturnType, typename ValueType, EnableIfSignedIntegralType<ValueType> * = nullptr>
inline EnableIfSignedIntegralType<ReturnType> safe_convert(const ValueType value) {
    ReturnType max = std::numeric_limits<ReturnType>::max();
    if (value > max) {
        return max;
    }
    return static_cast<ReturnType>(value);
}

template <typename Type>
inline Type safe_convert(const Type value) {
    return value;
}
