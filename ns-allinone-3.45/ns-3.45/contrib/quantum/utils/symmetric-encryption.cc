/*
 * Copyright (c) 2025 NITK Surathkal
 *
 * Authors: Shashank G <shashankgirish07@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */
#include "symmetric-encryption.h"

#include "ns3/enum.h"
#include "ns3/log.h"

#include <cryptopp/aes.h>
#include <cryptopp/cryptlib.h>
#include <cryptopp/des.h>
#include <cryptopp/filters.h>
#include <cryptopp/gcm.h>
#include <cryptopp/hex.h>
#include <cryptopp/modes.h>
#include <cryptopp/osrng.h>

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("SymmetricEncryption");

SymmetricEncryption::SymmetricEncryption()
{
    NS_LOG_FUNCTION(this);
}

SymmetricEncryption::~SymmetricEncryption()
{
    NS_LOG_FUNCTION(this);
}

std::string
SymmetricEncryption::encrypt(std::string& data,
                             SymmetricEncryptionAlgo algo,
                             std::string key,
                             std::string iv)
{
    NS_LOG_FUNCTION(this);
    switch (algo)
    {
    case AES_128_GCM:
        return aes128Encrypt(data, key, iv);
    case AES_256_GCM:
        return aes256Encrypt(data, key, iv);
    case DES_ECB:
        return desEncrypt(data, key);
    default:
        NS_LOG_ERROR("Invalid symmetric encryption algorithm");
        return "";
    }
}

std::string
SymmetricEncryption::decrypt(std::string& data,
                             SymmetricEncryptionAlgo algo,
                             std::string key,
                             std::string iv)
{
    NS_LOG_FUNCTION(this);

    switch (algo)
    {
    case AES_128_GCM:
        return aes128Decrypt(data, key, iv);
    case AES_256_GCM:
        return aes256Decrypt(data, key, iv);
    case DES_ECB:
        return desDecrypt(data, key);
    default:
        NS_LOG_ERROR("Invalid symmetric encryption algorithm");
        return "";
    }
}

std::string
SymmetricEncryption::aes128Encrypt(std::string& data, std::string key, std::string iv)
{
    NS_LOG_FUNCTION(this);

    CryptoPP::GCM<CryptoPP::AES>::Encryption encryptor;
    if (key.size() != 16)
    {
        NS_LOG_ERROR("Key size must be 16 bytes for AES-128");
        return "";
    }
    if (iv.size() != 16)
    {
        NS_LOG_ERROR("IV size must be 16 bytes for AES-128");
        return "";
    }

    encryptor.SetKeyWithIV(reinterpret_cast<const CryptoPP::byte*>(key.data()),
                           key.size(),
                           reinterpret_cast<const CryptoPP::byte*>(iv.data()),
                           iv.size());

    std::string cipher;
    CryptoPP::StringSource(
        data,
        true,
        new CryptoPP::AuthenticatedEncryptionFilter(encryptor, new CryptoPP::StringSink(cipher)));

    return cipher;
}

std::string
SymmetricEncryption::aes128Decrypt(std::string& data, std::string key, std::string iv)
{
    NS_LOG_FUNCTION(this);

    if (key.size() != 16)
    {
        NS_LOG_ERROR("Key size must be 16 bytes for AES-128");
        return "";
    }
    if (iv.size() != 16)
    {
        NS_LOG_ERROR("IV size must be 16 bytes for AES-128");
        return "";
    }

    CryptoPP::GCM<CryptoPP::AES>::Decryption decryptor;
    decryptor.SetKeyWithIV(reinterpret_cast<const CryptoPP::byte*>(key.data()),
                           key.size(),
                           reinterpret_cast<const CryptoPP::byte*>(iv.data()),
                           iv.size());

    std::string plain;
    CryptoPP::StringSource(
        data,
        true,
        new CryptoPP::AuthenticatedDecryptionFilter(decryptor, new CryptoPP::StringSink(plain)));

    return plain;
}

std::string
SymmetricEncryption::aes256Encrypt(std::string& data, std::string key, std::string iv)
{
    NS_LOG_FUNCTION(this);

    if (key.size() != 32)
    {
        NS_LOG_ERROR("Key size must be 32 bytes for AES-256");
        return "";
    }
    if (iv.size() != 32)
    {
        NS_LOG_ERROR("IV size must be 32 bytes for AES-256");
        return "";
    }

    CryptoPP::GCM<CryptoPP::AES>::Encryption encryptor;
    encryptor.SetKeyWithIV(reinterpret_cast<const CryptoPP::byte*>(key.data()),
                           key.size(),
                           reinterpret_cast<const CryptoPP::byte*>(iv.data()),
                           iv.size());

    std::string cipher;
    CryptoPP::StringSource(
        data,
        true,
        new CryptoPP::AuthenticatedEncryptionFilter(encryptor, new CryptoPP::StringSink(cipher)));

    return cipher;
}

std::string
SymmetricEncryption::aes256Decrypt(std::string& data, std::string key, std::string iv)
{
    NS_LOG_FUNCTION(this);

    if (key.size() != 32)
    {
        NS_LOG_ERROR("Key size must be 32 bytes for AES-256");
        return "";
    }
    if (iv.size() != 32)
    {
        NS_LOG_ERROR("IV size must be 32 bytes for AES-256");
        return "";
    }

    CryptoPP::GCM<CryptoPP::AES>::Decryption decryptor;
    decryptor.SetKeyWithIV(reinterpret_cast<const CryptoPP::byte*>(key.data()),
                           key.size(),
                           reinterpret_cast<const CryptoPP::byte*>(iv.data()),
                           iv.size());

    std::string plain;
    CryptoPP::StringSource(
        data,
        true,
        new CryptoPP::AuthenticatedDecryptionFilter(decryptor, new CryptoPP::StringSink(plain)));

    return plain;
}

std::string
SymmetricEncryption::desEncrypt(std::string& data, std::string key)
{
    NS_LOG_FUNCTION(this);
    if (key.size() != 8)
    {
        NS_LOG_ERROR("Key size must be 8 bytes for DES");
        return "";
    }

    CryptoPP::ECB_Mode<CryptoPP::DES>::Encryption encryptor;
    encryptor.SetKey(reinterpret_cast<const CryptoPP::byte*>(key.data()), key.size());

    std::string cipher;
    CryptoPP::StringSource(
        data,
        true,
        new CryptoPP::StreamTransformationFilter(encryptor, new CryptoPP::StringSink(cipher)));

    return cipher;
}

std::string
SymmetricEncryption::desDecrypt(std::string& data, std::string key)
{
    NS_LOG_FUNCTION(this);
    if (key.size() != 8)
    {
        NS_LOG_ERROR("Key size must be 8 bytes for DES");
        return "";
    }

    CryptoPP::ECB_Mode<CryptoPP::DES>::Decryption decryptor;
    decryptor.SetKey(reinterpret_cast<const CryptoPP::byte*>(key.data()), key.size());

    std::string plain;
    CryptoPP::StringSource(
        data,
        true,
        new CryptoPP::StreamTransformationFilter(decryptor, new CryptoPP::StringSink(plain)));

    return plain;
}
} // namespace ns3
