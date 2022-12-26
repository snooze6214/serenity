/*
 * Copyright (c) 2022, Alaap Surendran <snooze6214@protonmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "BencodeElement.h"
#include <AK/NonnullOwnPtrVector.h>

namespace Torrent {

class BencodeDecoder {
public:
    static ErrorOr<NonnullOwnPtrVector<BencodeElement>> decode(StringView bencodeString);

private:
    static ErrorOr<NonnullOwnPtr<BencodeElement>> decodeElement(StringView bencodeString, size_t& index);
    static ErrorOr<NonnullOwnPtr<BencodeElement>> decodeString(StringView bencodeString, size_t& index);
    static ErrorOr<NonnullOwnPtr<BencodeElement>> decodeInteger(StringView bencodeString, size_t& index);
    static ErrorOr<NonnullOwnPtr<BencodeElement>> decodeList(StringView bencodeString, size_t& index);
    static ErrorOr<NonnullOwnPtr<BencodeElement>> decodeDictionary(StringView bencodeString, size_t& index);
};

}
