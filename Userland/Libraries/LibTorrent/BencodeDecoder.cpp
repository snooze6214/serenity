/*
 * Copyright (c) 2022, Alaap Surendran <snooze6214@protonmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/CharacterTypes.h>
#include <AK/NonnullOwnPtrVector.h>
#include <AK/StringBuilder.h>

#include "BencodeDecoder.h"
#include "BencodeElement.h"

namespace Torrent {
ErrorOr<NonnullOwnPtrVector<BencodeElement>> BencodeDecoder::decode(StringView bencodeString)
{
    NonnullOwnPtrVector<BencodeElement> root_elements;

    size_t index = 0;
    while (index < bencodeString.length()) {
        auto element = TRY(decodeElement(bencodeString, index));
        root_elements.append(move(element));
    }

    return root_elements;
}

ErrorOr<NonnullOwnPtr<BencodeElement>> BencodeDecoder::decodeElement(StringView bencodeString, size_t& index)
{
    if (index >= bencodeString.length()) {
        return Error::from_string_literal("Index out of bound");
    }

    switch (bencodeString[index]) {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        return decodeString(bencodeString, index);
    case 'i':
        return decodeInteger(bencodeString, index);
    case 'l':
        return decodeList(bencodeString, index);
    case 'd':
        return decodeDictionary(bencodeString, index);
    default:
        return Error::from_string_literal("Unexpected symbol");
    }
}

ErrorOr<NonnullOwnPtr<BencodeElement>> BencodeDecoder::decodeString(StringView bencodeString, size_t& index)
{
    size_t start_index = index;
    while (is_ascii_digit(bencodeString[index]))
        index++;

    if (bencodeString[index] != ':') {
        return Error::from_string_literal("Unexpected symbol");
    }

    size_t length = bencodeString.substring_view(start_index, index - start_index).to_uint().value();

    index++; // ignore ':' symbol
    if (index + length - 1 >= bencodeString.length())
        return Error::from_string_literal("Invalid string length"); // make sure there is enough string to consume

    auto string = bencodeString.substring_view(index, length);
    index += length;

    return make<BencodeString>(string);
}

ErrorOr<NonnullOwnPtr<BencodeElement>> BencodeDecoder::decodeInteger(StringView bencodeString, size_t& index)
{
    if (bencodeString[index] != 'i')
        return Error::from_string_literal("Attempted to parse integer but found unexpected symbol");
    index++;

    size_t start_index = index;
    while (index < bencodeString.length() && bencodeString[index] != 'e')
        index++;

    if (index != bencodeString.length() && bencodeString[index] != 'e')
        return Error::from_string_literal("Expected integer end token 'e'");
    int value = bencodeString.substring_view(start_index, index - start_index).to_int().value();

    index++; // ignore 'e'

    return make<BencodeInteger>(value);
}

ErrorOr<NonnullOwnPtr<BencodeElement>> BencodeDecoder::decodeList(StringView bencodeString, size_t& index)
{
    if (bencodeString[index] != 'l')
        return Error::from_string_literal("Attempted to parse list but found unexpected symbol");
    index++;

    auto list = make<BencodeList>();

    while (index < bencodeString.length() && bencodeString[index] != 'e') {
        auto element = TRY(decodeElement(bencodeString, index));
        list->add_element(move(element));
    }

    if (index != bencodeString.length() && bencodeString[index] != 'e')
        return Error::from_string_literal("Expected list end token 'e'");

    index++; // ignore 'e'

    return list;
}

ErrorOr<NonnullOwnPtr<BencodeElement>> BencodeDecoder::decodeDictionary(StringView bencodeString, size_t& index)
{
    if (bencodeString[index] != 'd')
        return Error::from_string_literal("Attempted to parse list but found unexpected symbol");
    index++;

    auto dict = make<BencodeDictionary>();

    while (index < bencodeString.length() && bencodeString[index] != 'e') {
        auto key_ele = TRY(decodeString(bencodeString, index));
        auto key_ele_str = TRY(key_ele->as_string());
        auto key = TRY(key_ele_str->value());
        auto value = TRY(decodeElement(bencodeString, index));

        dict->set(key, move(value));
    }

    if (index != bencodeString.length() && bencodeString[index] != 'e')
        return Error::from_string_literal("Expected dictionary end token 'e'");

    index++; // ignore 'e'

    return dict;
}

}
