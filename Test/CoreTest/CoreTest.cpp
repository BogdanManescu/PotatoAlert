// Copyright 2021 <github.com/razaqq>

#include "Core/ByteReader.hpp"
#include "Core/Blowfish.hpp"
#include "Core/Directory.hpp"
#include "Core/File.hpp"
#include "Core/FileMapping.hpp"
#include "Core/PeFileVersion.hpp"
#include "Core/PeReader.hpp"
#include "Core/Process.hpp"
#include "Core/Semaphore.hpp"
#include "Core/Sha1.hpp"
#include "Core/Sha256.hpp"
#include "Core/String.hpp"
#include "Core/Time.hpp"
#include "Core/Version.hpp"
#include "Core/Zlib.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <string>
#include <span>
#include <ranges>
#include <vector>


namespace fs = std::filesystem;
using namespace PotatoAlert::Core;
using namespace PotatoAlert;

namespace {

template<typename TByte>
static std::vector<TByte> FromString(std::string_view s) noexcept
{
	std::vector<TByte> arr(s.size());
	std::ranges::transform(s, arr.begin(), [](char c) -> TByte
	{
		return static_cast<TByte>(c);
	});
	return arr;
}

template<typename TByte, typename... Ts>
static std::array<Byte, sizeof...(Ts)> FromHex(Ts&&... args) noexcept
{
	return { static_cast<TByte>(std::forward<Ts>(args))... };
}

static fs::path GetFile(std::string_view name)
{
	if (Result<fs::path> rootPath = GetModuleRootPath())
	{
		return rootPath.value().remove_filename() / "Misc" / name;
	}

	ExitCurrentProcess(1);
}

}

TEST_CASE( "ByteReaderTest" )
{
	std::vector<Byte> data = FromString<Byte>("The quick brown fox jumps over the lazy dog");
	ByteReader<Byte> reader(data);
	
	REQUIRE(reader.Position() == 0);
	REQUIRE(data.size() == reader.Size());
	auto b1 = reader.ReadByte();
	REQUIRE((b1 && b1.value() == (Byte)'T'));
	REQUIRE(data.size()-1 == reader.Size());
	REQUIRE(!reader.Empty());
	
	REQUIRE(reader.Position() == 1);
	REQUIRE(reader.Capacity() == data.size());

	REQUIRE(reader.Seek(SeekOrigin::Current, 1));
	REQUIRE(reader.Position() == 2);

	REQUIRE_FALSE(reader.Seek(SeekOrigin::Current, 50));
	REQUIRE(reader.Position() == 2);

	REQUIRE(reader.Seek(SeekOrigin::Start, 12));
	REQUIRE(reader.Position() == 12);

	REQUIRE_FALSE(reader.Seek(SeekOrigin::End, 1));
	REQUIRE(reader.Position() == 12);

	REQUIRE(reader.Seek(SeekOrigin::End, -1));
	REQUIRE(reader.Position() == reader.Capacity() - 1);

	REQUIRE(reader.Seek(SeekOrigin::Current, -1));
	REQUIRE(reader.Position() == reader.Capacity() - 2);

	std::string out;
	REQUIRE(reader.ReadToString(out, reader.Size()));
	REQUIRE(out == "og");

	reader.Consume(999);
	REQUIRE(reader.Position() == reader.Capacity());
	REQUIRE(reader.Empty());

	reader.Unconsume(999);
	REQUIRE(!reader.Empty());
	REQUIRE(reader.Position() == 0);

	std::vector<Byte> data2;
	REQUIRE(reader.ReadToEnd(data2) == data.size());
	REQUIRE(data == data2);

	REQUIRE(reader.Seek(SeekOrigin::Start, 0));

	uint32_t x;
	REQUIRE(reader.ReadTo(x));
	REQUIRE(x == 543516756);
	REQUIRE(reader.Position() == 4);
	REQUIRE(reader.Size() == reader.Capacity() - 4);

	REQUIRE(reader.Seek(SeekOrigin::End, -3));
	uint32_t y;
	REQUIRE_FALSE(reader.ReadTo(y));
}

TEST_CASE( "BlowFishEncryptTest" )
{
	auto key = FromString<Byte>("just some random key lol");
	auto text = FromString<Byte>("just a test text");
	auto solution = FromHex<Byte>(0x6b, 0x40, 0x9e, 0x78, 0xb1, 0x7b, 0x58, 0x65, 0xd7, 0x4c, 0x28, 0x6e, 0xc4, 0xe0, 0xe6, 0x8d);

	Blowfish blowfish(key);
	std::vector out = { Byte{ 0 } };
	out.resize(text.size());

	REQUIRE(blowfish.Encrypt(text, out));
	REQUIRE(std::equal(out.begin(), out.end(), solution.begin(), solution.end()));
}

TEST_CASE( "BlowFishDecryptTest" )
{
	auto key = FromString<Byte>("just some random key lol");
	auto text = FromHex<Byte>(0x6b, 0x40, 0x9e, 0x78, 0xb1, 0x7b, 0x58, 0x65, 0xd7, 0x4c, 0x28, 0x6e, 0xc4, 0xe0, 0xe6, 0x8d);
	auto solution = FromString<Byte>("just a test text");

	Blowfish blowfish(key);
	std::vector<Byte> out;
	out.resize(text.size());

	REQUIRE(blowfish.Decrypt(text, out));
	REQUIRE(std::equal(out.begin(), out.end(), solution.begin(), solution.end()));
}

TEST_CASE( "FileMappingTest" )
{
	File file = File::Open(GetFile("lorem.txt"), File::Flags::Open | File::Flags::Read);
	REQUIRE(file);
	std::string content;
	REQUIRE(file.ReadAllString(content));
	REQUIRE(content.size() == 591);

	const uint64_t fileSize = file.Size();
	REQUIRE(content.size() == fileSize);
	FileMapping fileMapping = FileMapping::Open(file, FileMapping::Flags::Read, fileSize);
	REQUIRE(fileMapping);
	void* m = fileMapping.Map(FileMapping::Flags::Read, 0, fileSize);
	REQUIRE(m != nullptr);

	std::string_view mappedContent((char*)m, fileSize);
	REQUIRE(content == mappedContent);

	file.Close();
	fileMapping.Close();
}

TEST_CASE( "MutexTest" )
{
	constexpr std::string_view SemName = "TEST_SEMAPHORE";
	// delete mutex in case of a previous failed run
	Semaphore::Remove(SemName);

	REQUIRE_FALSE(Semaphore::Open(SemName));
	Semaphore sem1 = Semaphore::Create(SemName, 0);
	REQUIRE(sem1);
	REQUIRE(sem1.IsOpen());
	REQUIRE(sem1.IsLocked());

	Semaphore sem1_1 = Semaphore::Open(SemName);
	REQUIRE(sem1_1);
	REQUIRE(sem1_1.IsLocked());
	REQUIRE(sem1_1.Close());

	REQUIRE_FALSE(sem1.TryLock());
	REQUIRE(sem1.Unlock());
	REQUIRE_FALSE(sem1.IsLocked());
	REQUIRE(sem1.TryLock());
	REQUIRE(sem1.IsLocked());
	REQUIRE(sem1.Close());
	REQUIRE_FALSE(sem1.IsOpen());

	REQUIRE(Semaphore::Remove(SemName));

	Semaphore sem2 = Semaphore::Create(SemName, 0);
	REQUIRE(sem2);
	REQUIRE(sem2.IsLocked());
	REQUIRE_FALSE(Semaphore::Create(SemName, false));
	REQUIRE(sem2.Unlock());
	REQUIRE(sem2.Close());
	REQUIRE_FALSE(sem2.IsOpen());

	Semaphore::Remove(SemName);

	Semaphore sem3 = Semaphore::Create(SemName, 1);
	REQUIRE(sem3);
	REQUIRE_FALSE(sem3.IsLocked());
	REQUIRE(Semaphore::Open(SemName));
	REQUIRE(sem3.TryLock());
	REQUIRE(sem3.IsLocked());
	REQUIRE(sem3.Unlock());
	REQUIRE(sem3.Close());
	REQUIRE_FALSE(sem3.IsOpen());

	Semaphore::Remove(SemName);
}

TEST_CASE( "PeReaderTest" )
{
	File file = File::Open(GetFile("FooBar.exe"), File::Flags::Read | File::Flags::Open);
	REQUIRE(file);
	const uint64_t fileSize = file.Size();
	FileMapping fileMapping = FileMapping::Open(file, FileMapping::Flags::Read, fileSize);
	REQUIRE(fileMapping);
	void* ptr = fileMapping.Map(FileMapping::Flags::Read, 0, fileSize);
	REQUIRE(ptr);
	PeReader peReader(std::span<const Byte>(static_cast<const Byte*>(ptr), fileSize));
	REQUIRE(peReader.Parse());

	auto table = peReader.GetResourceTable();
	REQUIRE(table);
	REQUIRE(table->Resources.size() == 2);
	auto it = std::ranges::find_if(table->Resources, [](const Resource& res) -> bool { return res.Type == ResourceType::Version; });
	REQUIRE(it != table->Resources.end());
	REQUIRE(it->Data.size() == 752);
	auto v = VsVersionInfo::FromData(it->Data);
	REQUIRE(v);
	REQUIRE(v->Length == 752);
	REQUIRE(v->ValueLength == 52);
	REQUIRE(v->Key == std::u16string_view(u"VS_VERSION_INFO"));
	REQUIRE(v->Value.Signature == 4277077181);
	REQUIRE(v->Value.StrucVersion == 65536);
	REQUIRE(v->Value.FileVersionMS == 65538);
	REQUIRE(v->Value.FileVersionLS == 196612);
	REQUIRE(v->Value.ProductVersionMS == 65538);
	REQUIRE(v->Value.ProductVersionLS == 196608);
	REQUIRE(v->Value.FileFlagsMask == 63);
	REQUIRE(v->Value.FileFlags == 0);
	REQUIRE(v->Value.FileOS == 4);
	REQUIRE(v->Value.FileType == 1);
	REQUIRE(v->Value.FileSubtype == 0);
	REQUIRE(v->Value.FileDateMS == 0);
	REQUIRE(v->Value.FileDateLS == 0);

	Version version(
		v->Value.FileVersionMS >> 16 & 0xFF,
		v->Value.FileVersionMS >> 0 & 0xFF,
		v->Value.FileVersionLS >> 16 & 0xFF,
		v->Value.FileVersionLS >> 0 & 0xFF
	);
	REQUIRE(version == Version(1, 2, 3, 4));

	fileMapping.Close();
	file.Close();

	auto result = ReadFileVersion(GetFile("FooBar.exe"));
	REQUIRE(result);
	REQUIRE(version == *result);
}

TEST_CASE( "Sha1Test" )
{
	auto test = FromString<Byte>("The quick brown fox jumps over the lazy dog");
	Sha1 sha;
	sha.ProcessBytes(std::span{ test });
	REQUIRE(sha.GetHash() == "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12");
}

TEST_CASE( "Sha256Test" )
{
	std::string hash1;
	REQUIRE(Core::Sha256("The quick brown fox jumps over the lazy dog", hash1));
	REQUIRE(hash1 == "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592");

	std::string hash2;
	REQUIRE(Core::Sha256(FromString<Byte>("The quick brown fox jumps over the lazy dog"), hash2));
	REQUIRE(hash2 == "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592");
}

TEST_CASE( "StringTest" )
{
	REQUIRE(String::Trim(" test \n\t") == "test");

	constexpr std::string_view t2 = "test123";
	REQUIRE(String::ToUpper(t2) == "TEST123");
	REQUIRE(String::ToLower(String::ToUpper(t2)) == t2);

	REQUIRE(String::ToLower("SOME LONGER TEST") == "some longer test");

	float pi;
	REQUIRE((String::ParseNumber("3.14159265359", pi) && std::abs(pi - 3.14159265359f) <= std::numeric_limits<float>::epsilon()));

	long long i;
	REQUIRE((String::ParseNumber("485745389475347534", i) && i == 485745389475347534));

	int j;
	REQUIRE((String::ParseNumber("-48574538", j) && j == -48574538));

	REQUIRE(!String::ParseNumber("CANT PARSE 5", i));

	char k;
	REQUIRE((String::ParseNumber("123", k) && k == 123));

	uint32_t p = 0;
	REQUIRE(!String::ParseNumber("-1", p));

	bool l = false;
	REQUIRE((String::ParseBool("true", l) && l));

	bool m = true;
	REQUIRE((String::ParseBool("false", m) && !m));

	REQUIRE(!String::ParseBool("not true", l));

	bool o = false;
	REQUIRE((String::ParseBool("\n\r TRUE \t", o) && o));

	constexpr std::string_view text = "this is some text";
	REQUIRE(String::Split(text, " ") == std::vector<std::string>{ "this", "is", "some", "text" });
	REQUIRE(String::Contains(text, "this"));
	REQUIRE(!String::Contains(text, "test"));
	REQUIRE(String::Split(text, "") == std::vector<std::string>{ "this is some text" });

	constexpr std::string_view replace = "yes yes no no";
	REQUIRE(String::ReplaceAll(replace, "yes", "no") == "no no no no");

	constexpr std::string_view removeTest = "xyzabc";
	REQUIRE(String::ReplaceAll(removeTest, "xyz", "") == "abc");

	REQUIRE(String::StartsWith<char>("some long text", "some"));
	REQUIRE_FALSE(String::StartsWith<char>("some long text", "awesome"));
	REQUIRE(String::EndsWith("some long text", "text"));
	REQUIRE_FALSE(String::EndsWith("some long text", "textt"));
	REQUIRE_FALSE(String::EndsWith("", "textt"));
	REQUIRE_FALSE(String::StartsWith<char>("", "textt"));
	REQUIRE(String::StartsWith<char>("text", ""));
	REQUIRE(String::EndsWith("text", ""));
}

TEST_CASE( "TimeTest" )
{
	{
		const std::string dt = "2018-12-09 23:12:45";
		auto res = Time::StrToTime(dt, "%F %T");
		REQUIRE(res);
		auto s = Time::TimeToStr(*res, "{:%F %T}");
		REQUIRE(dt == s);
	}

	{
		const std::string dt = "2023-10-15 23:12:45";
		auto res = Time::StrToTime(dt, "%Y-%m-%d %H:%M:%S");
		REQUIRE(res);
		auto s = Time::TimeToStr(*res, "{:%Y-%m-%d %H:%M:%S}");
		REQUIRE(dt == s);
	}

	{
		REQUIRE_FALSE(Time::StrToTime("2023-10-15", "%Y-%m-%d %H:%M:%S"));
		REQUIRE_FALSE(Time::StrToTime("2023-10-15", "%Y-%m-%d %H"));
		REQUIRE_FALSE(Time::StrToTime("2023-10-15", "%Y-%d"));
		REQUIRE_FALSE(Time::StrToTime("2023-10-15", "%m-%d"));
		REQUIRE(Time::StrToTime("2023-10-15", "%Y-%m-%d"));
	}

	{
		const std::string dt = "2023-10-15 23:12:45";
		auto res = Time::StrToTime(dt, "%Y-%m-%d %H:%M:%S");
		REQUIRE(res);
		auto s = Time::TimeToStr(*res, "{:%Y-%m-%d %H:%M}");
		REQUIRE(s == "2023-10-15 23:12");
	}
}

TEST_CASE( "VersionTest" )
{
	REQUIRE(Version("3.7.8.0") == Version("3.7.8.0"));
	REQUIRE(Version("3.7.8.0") == Version("3.7.8"));
	REQUIRE(Version("3.7.9") > Version("3.7.8"));
	REQUIRE(Version("3") < Version("3.7.9"));
	REQUIRE(Version("1.7.9") < Version("3.1"));
	REQUIRE(Version("zzz") < Version("0.0.1"));
	REQUIRE_FALSE(Version("zzz"));
	REQUIRE(Version("2.16.0") != Version("3.0.0"));
	REQUIRE_FALSE(Version("3.0.0") < Version("2.16.0"));
	REQUIRE(Version(1, 2, 3, 4) == Version("1,2,3,4"));
	REQUIRE(Version("abc 3,7,8") == Version(3, 7, 8));
	REQUIRE(Version("3,7,8 abc") == Version(3, 7, 8));
	REQUIRE(Version("3, 7, 8") == Version(3, 7, 8));
	REQUIRE(Version(1, 2, 3, 4).ToString() == "1.2.3.4");
	REQUIRE(Version(0, 9, 4, 0).ToString() == "0.9.4.0");
	REQUIRE(Version("0.9.4.0.1") == Version("0.9.4.0"));
	REQUIRE(Version(1, 2, 3, 4) >= Version("1,2,3,4"));
	REQUIRE(Version(1, 2, 3, 5) >= Version(1, 2, 3, 4));
	REQUIRE(Version(1, 2, 4) <= Version(1, 2, 5));
	REQUIRE(Version(1, 2, 4, 5) <= Version(1, 2, 5));
	REQUIRE(Version("0.11.7.0") >= Version("0.10.9.0"));
	REQUIRE_FALSE(Version("0.11.7.0") < Version("0.10.9.0"));
	REQUIRE_FALSE(Version("0.11.7.0") == Version("0.10.9.0"));
}

TEST_CASE( "ZlibTest" )
{
	constexpr std::string_view string =
			"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt "
			"ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation "
			"ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in "
			"reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur "
			"sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id "
			"est laborum. Curabitur pretium tincidunt lacus. Nulla gravida orci a odio. Nullam varius, "
			"turpis et commodo pharetra, est eros bibendum elit, nec luctus magna felis sollicitudin "
			"mauris. Integer in mauris eu nibh euismod gravida. Duis ac tellus et risus vulputate "
			"vehicula. Donec lobortis risus a elit. Etiam tempor. Ut ullamcorper, ligula eu tempor "
			"congue, eros est euismod turpis, id tincidunt sapien risus a quam. Maecenas fermentum "
			"consequat mi. Donec fermentum.Pellentesque malesuada nulla a mi. Duis sapien sem, aliquet "
			"nec, commodo eget, consequat quis, neque. Aliquam faucibus, elit ut dictum aliquet, felis "
			"nisl adipiscing sapien, sed malesuada diam lacus eget erat. Cras mollis scelerisque nunc. "
			"Nullam arcu. Aliquam consequat. Curabitur augue lorem, dapibus quis, laoreet et, pretium "
			"ac, nisi. Aenean magna nisl, mollis quis, molestie eu, feugiat in, orci. In hac habitasse "
			"platea dictumst.";

	const auto binary = FromHex<Byte>(
		0x78, 0x9C, 0x4D, 0x54, 0x59, 0x8E, 0xDB, 0x30, 0x0C, 0xBD, 0x0A, 0x0F, 0x60, 0xF8, 0x0E, 0xC5,
		0xB4, 0x1F, 0x05, 0xDA, 0xA2, 0x3F, 0x3D, 0x00, 0x23, 0x31, 0x09, 0x01, 0x2D, 0x1E, 0x49, 0x0C,
		0xE6, 0xF8, 0x7D, 0x94, 0x9C, 0xCC, 0x7C, 0xD9, 0xD6, 0x42, 0xBE, 0x8D, 0xFE, 0x55, 0x9B, 0x64,
		0xD2, 0xA3, 0x5B, 0xA6, 0x58, 0x53, 0x6D, 0xD4, 0x75, 0x10, 0x67, 0x19, 0x1B, 0x85, 0x5A, 0xBA,
		0x84, 0x21, 0xC3, 0x1A, 0x71, 0xD4, 0x43, 0x7B, 0xD0, 0x72, 0x23, 0x49, 0x8A, 0xCD, 0x2E, 0x11,
		0x17, 0x48, 0xD4, 0x7A, 0xAE, 0x91, 0x86, 0xE4, 0x03, 0x97, 0xB5, 0x04, 0x8D, 0x1A, 0xAD, 0x0C,
		0xB2, 0x41, 0x89, 0x2F, 0x28, 0x4F, 0x32, 0x56, 0x69, 0xA1, 0xCC, 0xB7, 0xC2, 0xC4, 0x49, 0xDF,
		0x8D, 0x77, 0xFA, 0x37, 0x48, 0x8A, 0x66, 0xD4, 0xA6, 0xAC, 0xFE, 0xF2, 0xC0, 0x27, 0xE7, 0x8D,
		0xDE, 0x4D, 0x3B, 0x95, 0xDA, 0x47, 0xB3, 0x48, 0xF2, 0x21, 0x2D, 0xE8, 0xE0, 0xA1, 0xB5, 0x90,
		0xA5, 0xC4, 0x39, 0xD4, 0x55, 0xD9, 0x0F, 0x69, 0x57, 0xEF, 0x34, 0x4B, 0xEA, 0x81, 0xC3, 0x24,
		0x0C, 0xE0, 0x19, 0x98, 0xEA, 0x22, 0x80, 0x56, 0x63, 0xA7, 0xEF, 0x5E, 0x92, 0x6D, 0x08, 0x69,
		0x33, 0x20, 0x59, 0x5C, 0xB5, 0x50, 0x93, 0xA3, 0xC9, 0x5D, 0x4A, 0x94, 0x06, 0xE2, 0x58, 0x78,
		0xD4, 0x64, 0x07, 0xDA, 0x09, 0xE0, 0x80, 0x29, 0x49, 0xEF, 0x42, 0x41, 0x53, 0x7A, 0x2A, 0x04,
		0x42, 0x46, 0x57, 0xBB, 0x29, 0x0F, 0x2A, 0x0E, 0x88, 0x0E, 0x6E, 0xF8, 0xB0, 0xB6, 0xD3, 0x8F,
		0x8F, 0x20, 0xC7, 0x10, 0x73, 0x19, 0xA1, 0x41, 0x0D, 0x81, 0x25, 0xE0, 0x5C, 0xB0, 0x43, 0x23,
		0x0F, 0xBF, 0x01, 0x16, 0x47, 0xAB, 0x1A, 0xA5, 0xB8, 0x8A, 0xAE, 0x14, 0x9A, 0x06, 0x4B, 0x07,
		0x3B, 0x6F, 0xAA, 0xD7, 0xAB, 0x06, 0x65, 0x8A, 0xD2, 0xA5, 0xF9, 0x6E, 0xAE, 0xC9, 0x61, 0xB0,
		0x0B, 0xA4, 0x90, 0xA3, 0x9F, 0xBA, 0x5A, 0xDE, 0xE9, 0xCD, 0x1A, 0x5F, 0xD4, 0xFD, 0x01, 0x89,
		0xA1, 0x40, 0x38, 0xA6, 0x03, 0x7E, 0x2F, 0x71, 0xB0, 0xBE, 0xD3, 0x9F, 0x89, 0xF0, 0xD6, 0xF8,
		0x01, 0x00, 0x54, 0x21, 0x25, 0xE1, 0x11, 0xB5, 0x9E, 0x5B, 0x50, 0x1D, 0xE8, 0xAD, 0x6F, 0x84,
		0x32, 0xB0, 0xD8, 0xDD, 0x7A, 0xEA, 0x77, 0xDC, 0x19, 0x65, 0x1B, 0x6F, 0xB3, 0xAB, 0xB4, 0xDA,
		0xE9, 0xA2, 0x17, 0x68, 0x85, 0x46, 0x2B, 0x05, 0x45, 0x02, 0x25, 0x0B, 0xC3, 0xFA, 0xE9, 0xED,
		0x15, 0xEB, 0x9D, 0xBA, 0x63, 0x86, 0x69, 0x16, 0xC1, 0x2D, 0xB3, 0xC1, 0xAA, 0x9D, 0x7E, 0x96,
		0x21, 0x37, 0x99, 0xA2, 0xAF, 0x25, 0xD7, 0xB1, 0xE8, 0xE5, 0x8E, 0xA7, 0xCE, 0x10, 0x9D, 0x28,
		0x9F, 0x6E, 0x05, 0xA4, 0x0A, 0xB2, 0x4F, 0x48, 0x38, 0x8E, 0x97, 0x07, 0x64, 0xB2, 0xD3, 0x9B,
		0xBB, 0x42, 0x34, 0x3F, 0x5B, 0x27, 0x88, 0x0A, 0x49, 0x06, 0x6E, 0xAD, 0x83, 0x3C, 0xF1, 0xC1,
		0x8F, 0x81, 0x40, 0x9D, 0xE1, 0x9C, 0x81, 0x3B, 0x03, 0xD4, 0x0E, 0x69, 0x1B, 0x25, 0xBD, 0xA1,
		0x84, 0xC3, 0x38, 0xE3, 0x8B, 0xC0, 0xDC, 0x4C, 0xB6, 0x45, 0x75, 0x72, 0x3E, 0x91, 0x2D, 0x6D,
		0x36, 0x37, 0xE0, 0x53, 0xE1, 0xCE, 0x87, 0x4A, 0x79, 0x75, 0x44, 0xD0, 0x60, 0xC9, 0x6F, 0x18,
		0x2E, 0x85, 0x3B, 0x94, 0x68, 0x19, 0x1E, 0x43, 0xAA, 0x57, 0x0C, 0x11, 0xF3, 0x27, 0xDE, 0xD7,
		0xEE, 0xFE, 0x17, 0x1C, 0xF1, 0x26, 0xFD, 0xDD, 0x7C, 0x40, 0x92, 0x74, 0x63, 0x38, 0xB5, 0x92,
		0xC5, 0xEB, 0x8A, 0xCB, 0x71, 0x76, 0xEB, 0x82, 0x01, 0x99, 0x71, 0x87, 0x2A, 0xA8, 0xB4, 0xBD,
		0xDC, 0x82, 0xB8, 0xCF, 0xA1, 0x9D, 0xCD, 0x7C, 0x8A, 0xDC, 0x21, 0x9C, 0xDC, 0xE9, 0xDB, 0x9C,
		0xB9, 0x4C, 0x57, 0xB6, 0xA0, 0x17, 0xB7, 0x7B, 0x86, 0x1B, 0xA3, 0x13, 0x35, 0x38, 0xC8, 0xB3,
		0xE4, 0x76, 0x3A, 0x88, 0xC1, 0x4A, 0x5F, 0x87, 0x7E, 0x75, 0x5F, 0x63, 0xFF, 0x09, 0x32, 0xBA,
		0xBA, 0x33, 0x68, 0xB3, 0x3B, 0x74, 0xF3, 0x59, 0x7B, 0x6B, 0xA0, 0x3F, 0x63, 0x0B, 0xD4, 0x41,
		0x12, 0x26, 0x6B, 0x92, 0x2B, 0x56, 0xC2, 0x2B, 0x75, 0xDC, 0x82, 0x7D, 0xC2, 0xFA, 0x32, 0xA9,
		0x9F, 0x99, 0x66, 0x83, 0x19, 0xE4, 0x13, 0x07, 0xCA, 0x11, 0x00, 0x80, 0xFB, 0x64, 0x95, 0x18,
		0xAB, 0xDE, 0x10, 0x80, 0x9F, 0xD1, 0x67, 0x68, 0xE1, 0xFF, 0x03, 0x54, 0x95, 0x22, 0x5C, 0xCE,
		0x44, 0x3A, 0x93, 0xED, 0x09, 0x67, 0xDD, 0xC6, 0x07, 0xCC, 0x55, 0x1F, 0x64, 0xE7, 0xBB, 0x46,
		0x59, 0xC1, 0xCE, 0xC7, 0xC3, 0x83, 0x4A, 0x77, 0x84, 0xEF, 0xEE, 0x30, 0xD8, 0xA7, 0xFF, 0x48,
		0x88, 0x1C, 0x9F, 0x4A, 0xF5, 0xB1, 0xFF, 0x07, 0xAC, 0x4C, 0xE7, 0xD3
	);

	auto vec = Zlib::Inflate(binary);

	REQUIRE(vec.size() == string.size());
	CHECK(std::memcmp(vec.data(), string.data(), vec.size()) == 0);
}
