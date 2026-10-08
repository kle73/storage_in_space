/**
 * Copyright (c) 2025 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Shashank G <shashankgirish07@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#include "ns3/ptr.h"
#include "ns3/symmetric-encryption.h"
#include "ns3/test.h"

using namespace ns3;

/**
 * @ingroup quantum-test
 * @ingroup tests
 *
 * SymmetricEncryption unit tests.
 */
class SymmetricEncryptionTestCase : public TestCase
{
  public:
    SymmetricEncryptionTestCase();
    void DoRun() override;
};

SymmetricEncryptionTestCase::SymmetricEncryptionTestCase()
    : TestCase("Sanity check on the symmetric encryption implementation")
{
}

void
SymmetricEncryptionTestCase::DoRun()
{
    SymmetricEncryption symmetricEncryption;

    // Test encryption and decryption with AES_128_GCM
    std::string data = "Hello, World!";
    std::string key = "0123456789abcdef"; // 16 bytes for AES-128
    std::string iv = "abcdef9876543210";  // 16 bytes IV
    std::string encryptedData = symmetricEncryption.encrypt(data, AES_128_GCM, key, iv);
    std::string decryptedData = symmetricEncryption.decrypt(encryptedData, AES_128_GCM, key, iv);

    NS_TEST_EXPECT_MSG_EQ(data, decryptedData, "AES_128_GCM encryption and decryption failed");

    // Test encryption and decryption with AES_256_GCM
    std::string key256 = "0123456789abcdef0123456789abcdef"; // 32 bytes for AES-256
    std::string iv256 = "abcdef9876543210abcdef9876543210";  // 32 bytes IV
    std::string encryptedData256 = symmetricEncryption.encrypt(data, AES_256_GCM, key256, iv256);
    std::string decryptedData256 =
        symmetricEncryption.decrypt(encryptedData256, AES_256_GCM, key256, iv256);
    NS_TEST_EXPECT_MSG_EQ(data, decryptedData256, "AES_256_GCM encryption and decryption failed");
}

class SymmetricEncryptionTestSuite : public TestSuite
{
  public:
    SymmetricEncryptionTestSuite();
};

SymmetricEncryptionTestSuite::SymmetricEncryptionTestSuite()
    : TestSuite("symmetric-encryption", Type::SYSTEM)
{
    AddTestCase(new SymmetricEncryptionTestCase(), Duration::QUICK);
};

static SymmetricEncryptionTestSuite g_symmetricEncryptionTestSuite;
