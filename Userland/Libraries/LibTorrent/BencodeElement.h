/*
 * Copyright (c) 2022, Alaap Surendran <snooze6214@protonmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/FixedArray.h>
#include <AK/Function.h>
#include <AK/HashMap.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/NonnullOwnPtrVector.h>
#include <AK/Optional.h>
#include <AK/String.h>
#include <AK/StringBuilder.h>
#include <AK/StringView.h>
#include <AK/WeakPtr.h>
#include <AK/Weakable.h>

namespace Torrent {

enum class BencodeElementType {
    String,
    Integer,
    List,
    Dictionary,
};

class BencodeString;
class BencodeInteger;
class BencodeList;
class BencodeDictionary;

class BencodeElement : public Weakable<BencodeElement> {
public:
    BencodeElement(BencodeElementType type)
        : m_type(type)
    {
    }

    ErrorOr<WeakPtr<BencodeString>> as_string()
    {
        if (m_type != BencodeElementType::String)
            return Error::from_string_literal("Invalid type of element");

        return this->make_weak_ptr<BencodeString>();
    }

    ErrorOr<WeakPtr<BencodeInteger>> as_integer()
    {
        if (m_type != BencodeElementType::Integer)
            return Error::from_string_literal("Invalid type of element");

        return this->make_weak_ptr<BencodeInteger>();
    }

    ErrorOr<WeakPtr<BencodeList>> as_list()
    {
        if (m_type != BencodeElementType::List)
            return Error::from_string_literal("Invalid type of element");

        return this->make_weak_ptr<BencodeList>();
    }

    ErrorOr<WeakPtr<BencodeDictionary>> as_dictionary()
    {
        if (m_type != BencodeElementType::Dictionary)
            return Error::from_string_literal("Invalid type of element");

        return this->make_weak_ptr<BencodeDictionary>();
    }

    inline BencodeElementType type() const { return m_type; }

private:
    BencodeElementType m_type;
};

class BencodeInteger : public BencodeElement {
public:
    BencodeInteger(int value = 0)
        : BencodeElement(BencodeElementType::Integer)
        , m_value(value)
    {
    }

    inline int value() const { return m_value; }

private:
    int m_value { 0 };
};

class BencodeString : public BencodeElement {
public:
    BencodeString(FixedArray<u8> value)
        : BencodeElement(BencodeElementType::String)
        , m_value(move(value))
    {
    }

    BencodeString(StringView& value)
        : BencodeElement(BencodeElementType::String)
        , m_value(MUST(FixedArray<u8>::try_create(value.length())))
    {
        for (size_t i = 0; i < value.length(); i++)
            m_value[i] = value[i];
    }

    inline FixedArray<u8> const& data() const { return m_value; }

    inline ErrorOr<String> value()
    {
        StringBuilder builder;
        for (auto c : m_value)
            builder.append(static_cast<char>(c));
        return builder.to_string();
    }

private:
    FixedArray<u8> m_value;
};

class BencodeList : public BencodeElement {
public:
    BencodeList()
        : BencodeElement(BencodeElementType::List)
    {
    }

    inline void add_element(NonnullOwnPtr<BencodeElement> element) { m_elements.append(move(element)); }
    inline NonnullOwnPtrVector<BencodeElement>& elements() { return m_elements; }
    inline void for_each_element(Function<void(BencodeElement&)> callback)
    {
        for (auto& element : m_elements) {
            callback(element);
        }
    }

private:
    NonnullOwnPtrVector<BencodeElement> m_elements;
};

class BencodeDictionary : public BencodeElement {
public:
    BencodeDictionary()
        : BencodeElement(BencodeElementType::Dictionary)
    {
    }

    inline Optional<BencodeElement&> get(StringView key)
    {
        StringBuilder builder;
        for (auto c : key)
            builder.append(c);

        auto it = m_dict.find(builder.to_string().release_value_but_fixme_should_propagate_errors());
        if (it == m_dict.end())
            return {};

        return *(it->value);
    }

    inline void set(String const& key, NonnullOwnPtr<BencodeElement> value)
    {
        m_dict.set(key, move(value));
    }

private:
    HashMap<String, NonnullOwnPtr<BencodeElement>> m_dict;
};

}
