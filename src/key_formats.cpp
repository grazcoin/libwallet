/*
 * Copyright (c) 2011-2013 libwallet developers (see AUTHORS)
 *
 * This file is part of libwallet.
 *
 * libwallet is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <wallet/key_formats.hpp>

#include <bitcoin/format.hpp>
#include <bitcoin/utility/base58.hpp>
#include <bitcoin/utility/sha256.hpp>
#include <bitcoin/address.hpp>

namespace libwallet {

typedef data_chunk private_data;

std::string secret_to_wif(const secret_parameter& secret, bool compressed)
{
    private_data unencoded_data(secret.begin(), secret.end());
    unencoded_data.insert(unencoded_data.begin(), payment_address::wif_version);
    if (compressed)
        extend_data(unencoded_data, uncast_type<uint8_t>(0x01));

    uint32_t checksum = generate_sha256_checksum(unencoded_data);
    extend_data(unencoded_data, uncast_type(checksum));
    return encode_base58(unencoded_data);
}

secret_parameter wif_to_secret(const std::string& wif)
{
    private_data decoded = decode_base58(wif);
    // 1 marker, 32 byte secret, optional 1 compressed flag, 4 checksum bytes
    if (decoded.size() != 1 + sha256_digest_size + 4 && 
        decoded.size() != 1 + sha256_digest_size + 1 + 4)
        return secret_parameter();
    // Check first byte is valid and remove it
    if (decoded[0] != payment_address::wif_version)
        return secret_parameter();
    // Proceed to verify the checksum
    private_data checksum_bytes(decoded.end() - 4, decoded.end());
    BITCOIN_ASSERT(checksum_bytes.size() == 4);
    decoded.erase(decoded.end() - 4, decoded.end());
    if (cast_chunk<uint32_t>(checksum_bytes) !=
            generate_sha256_checksum(decoded))
        return secret_parameter();
    // Checks passed. Drop the 0x80 start byte.
    decoded.erase(decoded.begin());
    // If length is still 33 and last byte is 0x01, drop it.
    if (decoded.size() == 33 && decoded[32] == (uint8_t)0x01)
        decoded.erase(decoded.begin()+32);
    secret_parameter secret;
    BITCOIN_ASSERT(secret.size() == decoded.size());
    std::copy(decoded.begin(), decoded.end(), secret.begin());
    return secret;
}

bool is_wif_compressed(const std::string& wif) {
    data_chunk decoded = decode_base58(wif);
    if (decoded.size() == 1 + sha256_digest_size + 1 + 4 &&
        decoded[33] == (uint8_t)0x01)
        return true;
    else
        return false;
}

hash_digest single_sha256(const std::string& mini)
{
    SHA256_CTX ctx;
    hash_digest digest;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, mini.c_str(), mini.size());
    SHA256_Final(digest.data(), &ctx);
    return digest;
}

bool check_minikey(const std::string& minikey)
{
    // Legacy minikeys are 22 chars long
    if (minikey.size() != 22 && minikey.size() != 30)
        return false;
    return single_sha256(minikey + "?")[0] == 0x00;
}

secret_parameter minikey_to_secret(const std::string& minikey)
{
    if (!check_minikey(minikey))
        return secret_parameter();
    return single_sha256(minikey);
}

} // libwallet

