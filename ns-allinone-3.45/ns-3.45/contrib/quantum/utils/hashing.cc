/**
 * Copyright (c) 2025 NITK Surathkal
 *
 * Authors: Shashank G <shashankgirish07@gmail.com>
 *          Navaneet Y V N <navaneetyvn.work@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#include "hashing.h"

#include "ns3/enum.h"
#include "ns3/log.h"

#include <cryptopp/cryptlib.h>
#include <cryptopp/filters.h>
#include <cryptopp/hex.h>
#include <cryptopp/sha.h>

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("HashingAlgorithm");

HashingAlgorithm::HashingAlgorithm()
{
    NS_LOG_FUNCTION(this);
}

HashingAlgorithm::~HashingAlgorithm()
{
    NS_LOG_FUNCTION(this);
}

std::string
HashingAlgorithm::HashData(std::string data, HashingAlgo algo)
{
    NS_LOG_FUNCTION(this);

    switch (algo)
    {
    case SHA1:
        return sha1(data);
    case SHA256:
        return sha256(data);
    case SHA512:
        return sha512(data);
    default:
        NS_LOG_ERROR("Invalid hashing algorithm");
        return "";
    }
}

std::string
HashingAlgorithm::sha1(std::string data)
{
    NS_LOG_FUNCTION(this);

    std::string digest;
    CryptoPP::SHA1 hash;

    CryptoPP::HashFilter filter(hash, new CryptoPP::StringSink(digest));
    CryptoPP::StringSource(data, true, new CryptoPP::Redirector(filter));

    std::string encoded;
    CryptoPP::HexEncoder encoder(new CryptoPP::StringSink(encoded), false);
    CryptoPP::StringSource(digest, true, new CryptoPP::Redirector(encoder));

    return encoded;
}

std::string
HashingAlgorithm::sha256(std::string data)
{
    NS_LOG_FUNCTION(this);

    std::string digest;
    CryptoPP::SHA256 hash;

    CryptoPP::HashFilter filter(hash, new CryptoPP::StringSink(digest));
    CryptoPP::StringSource(data, true, new CryptoPP::Redirector(filter));

    std::string encoded;
    CryptoPP::HexEncoder encoder(new CryptoPP::StringSink(encoded), false);
    CryptoPP::StringSource(digest, true, new CryptoPP::Redirector(encoder));

    return encoded;
}

std::string
HashingAlgorithm::sha512(std::string data)
{
    NS_LOG_FUNCTION(this);

    std::string digest;
    CryptoPP::SHA512 hash;

    CryptoPP::HashFilter filter(hash, new CryptoPP::StringSink(digest));
    CryptoPP::StringSource(data, true, new CryptoPP::Redirector(filter));

    std::string encoded;
    CryptoPP::HexEncoder encoder(new CryptoPP::StringSink(encoded), false);
    CryptoPP::StringSource(digest, true, new CryptoPP::Redirector(encoder));

    return encoded;
}

} // namespace ns3
