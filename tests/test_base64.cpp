/*
 * Unit tests for Base64 encode/decode in ElunaUtility.
 *
 * Tests ElunaUtil::EncodeData and ElunaUtil::DecodeData which are
 * self-contained Base64 functions with no game-server dependencies.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <string>

// Forward declarations matching ElunaUtility.h
#include "Common.h"

namespace ElunaUtil
{
    void EncodeData(const unsigned char* data, size_t input_length, std::string& output);
    unsigned char* DecodeData(const char* data, size_t *output_length);
}

// ---- Encode Tests ----

TEST(Base64Encode, EmptyInput)
{
    std::string output;
    ElunaUtil::EncodeData(reinterpret_cast<const unsigned char*>(""), 0, output);
    EXPECT_EQ(output, "");
}

TEST(Base64Encode, SingleByte)
{
    const unsigned char data[] = { 'A' };
    std::string output;
    ElunaUtil::EncodeData(data, 1, output);
    EXPECT_EQ(output, "QQ==");
}

TEST(Base64Encode, TwoBytes)
{
    const unsigned char data[] = { 'A', 'B' };
    std::string output;
    ElunaUtil::EncodeData(data, 2, output);
    EXPECT_EQ(output, "QUI=");
}

TEST(Base64Encode, ThreeBytes)
{
    const unsigned char data[] = { 'A', 'B', 'C' };
    std::string output;
    ElunaUtil::EncodeData(data, 3, output);
    EXPECT_EQ(output, "QUJD");
}

TEST(Base64Encode, HelloWorld)
{
    const char* input = "Hello, World!";
    std::string output;
    ElunaUtil::EncodeData(reinterpret_cast<const unsigned char*>(input), strlen(input), output);
    EXPECT_EQ(output, "SGVsbG8sIFdvcmxkIQ==");
}

TEST(Base64Encode, BinaryData)
{
    const unsigned char data[] = { 0x00, 0xFF, 0x80, 0x7F, 0x01 };
    std::string output;
    ElunaUtil::EncodeData(data, sizeof(data), output);
    // Verify it round-trips correctly
    size_t decoded_len;
    unsigned char* decoded = ElunaUtil::DecodeData(output.c_str(), &decoded_len);
    ASSERT_NE(decoded, nullptr);
    ASSERT_EQ(decoded_len, sizeof(data));
    EXPECT_EQ(memcmp(data, decoded, sizeof(data)), 0);
    delete[] decoded;
}

TEST(Base64Encode, AllByteValues)
{
    unsigned char data[256];
    for (int i = 0; i < 256; i++)
        data[i] = static_cast<unsigned char>(i);

    std::string output;
    ElunaUtil::EncodeData(data, 256, output);

    size_t decoded_len;
    unsigned char* decoded = ElunaUtil::DecodeData(output.c_str(), &decoded_len);
    ASSERT_NE(decoded, nullptr);
    ASSERT_EQ(decoded_len, 256u);
    EXPECT_EQ(memcmp(data, decoded, 256), 0);
    delete[] decoded;
}

TEST(Base64Encode, KnownRFC4648Vectors)
{
    // RFC 4648 test vectors
    struct TestVector { const char* input; const char* expected; };
    TestVector vectors[] = {
        { "",       "" },
        { "f",      "Zg==" },
        { "fo",     "Zm8=" },
        { "foo",    "Zm9v" },
        { "foob",   "Zm9vYg==" },
        { "fooba",  "Zm9vYmE=" },
        { "foobar", "Zm9vYmFy" },
    };

    for (auto& tv : vectors)
    {
        std::string output;
        ElunaUtil::EncodeData(
            reinterpret_cast<const unsigned char*>(tv.input),
            strlen(tv.input), output);
        EXPECT_EQ(output, tv.expected) << "Failed for input: \"" << tv.input << "\"";
    }
}

// ---- Decode Tests ----

TEST(Base64Decode, EmptyInput)
{
    size_t len;
    unsigned char* result = ElunaUtil::DecodeData("", &len);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(len, 0u);
    delete[] result;
}

TEST(Base64Decode, SingleCharPadded)
{
    size_t len;
    unsigned char* result = ElunaUtil::DecodeData("QQ==", &len);
    ASSERT_NE(result, nullptr);
    ASSERT_EQ(len, 1u);
    EXPECT_EQ(result[0], 'A');
    delete[] result;
}

TEST(Base64Decode, TwoCharsPadded)
{
    size_t len;
    unsigned char* result = ElunaUtil::DecodeData("QUI=", &len);
    ASSERT_NE(result, nullptr);
    ASSERT_EQ(len, 2u);
    EXPECT_EQ(result[0], 'A');
    EXPECT_EQ(result[1], 'B');
    delete[] result;
}

TEST(Base64Decode, ThreeCharsNoPadding)
{
    size_t len;
    unsigned char* result = ElunaUtil::DecodeData("QUJD", &len);
    ASSERT_NE(result, nullptr);
    ASSERT_EQ(len, 3u);
    EXPECT_EQ(result[0], 'A');
    EXPECT_EQ(result[1], 'B');
    EXPECT_EQ(result[2], 'C');
    delete[] result;
}

TEST(Base64Decode, HelloWorld)
{
    size_t len;
    unsigned char* result = ElunaUtil::DecodeData("SGVsbG8sIFdvcmxkIQ==", &len);
    ASSERT_NE(result, nullptr);
    std::string decoded(reinterpret_cast<char*>(result), len);
    EXPECT_EQ(decoded, "Hello, World!");
    delete[] result;
}

TEST(Base64Decode, InvalidLength)
{
    size_t len;
    unsigned char* result = ElunaUtil::DecodeData("QQ=", &len);
    EXPECT_EQ(result, nullptr);
}

TEST(Base64Decode, InvalidCharacters)
{
    size_t len;
    unsigned char* result = ElunaUtil::DecodeData("QQ!!", &len);
    EXPECT_EQ(result, nullptr);
}

TEST(Base64Decode, RFC4648Vectors)
{
    struct TestVector { const char* encoded; const char* expected; };
    TestVector vectors[] = {
        { "Zg==",     "f" },
        { "Zm8=",     "fo" },
        { "Zm9v",     "foo" },
        { "Zm9vYg==", "foob" },
        { "Zm9vYmE=", "fooba" },
        { "Zm9vYmFy", "foobar" },
    };

    for (auto& tv : vectors)
    {
        size_t len;
        unsigned char* result = ElunaUtil::DecodeData(tv.encoded, &len);
        ASSERT_NE(result, nullptr) << "Failed to decode: " << tv.encoded;
        std::string decoded(reinterpret_cast<char*>(result), len);
        EXPECT_EQ(decoded, tv.expected) << "Mismatch for: " << tv.encoded;
        delete[] result;
    }
}

// ---- Round-trip Tests ----

TEST(Base64RoundTrip, VariousLengths)
{
    for (size_t length = 0; length <= 64; ++length)
    {
        std::vector<unsigned char> data(length);
        for (size_t i = 0; i < length; i++)
            data[i] = static_cast<unsigned char>(i * 7 + 13);

        std::string encoded;
        ElunaUtil::EncodeData(data.data(), data.size(), encoded);

        size_t decoded_len;
        unsigned char* decoded = ElunaUtil::DecodeData(encoded.c_str(), &decoded_len);
        ASSERT_NE(decoded, nullptr) << "Failed at length " << length;
        ASSERT_EQ(decoded_len, length) << "Length mismatch at " << length;
        EXPECT_EQ(memcmp(data.data(), decoded, length), 0) << "Data mismatch at length " << length;
        delete[] decoded;
    }
}

TEST(Base64RoundTrip, LargeData)
{
    std::vector<unsigned char> data(4096);
    for (size_t i = 0; i < data.size(); i++)
        data[i] = static_cast<unsigned char>(i % 256);

    std::string encoded;
    ElunaUtil::EncodeData(data.data(), data.size(), encoded);

    size_t decoded_len;
    unsigned char* decoded = ElunaUtil::DecodeData(encoded.c_str(), &decoded_len);
    ASSERT_NE(decoded, nullptr);
    ASSERT_EQ(decoded_len, data.size());
    EXPECT_EQ(memcmp(data.data(), decoded, data.size()), 0);
    delete[] decoded;
}
