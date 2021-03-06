/*
	Yelo: Open Sauce SDK

	See license\OpenSauce\OpenSauce for specific license information
*/
#include "Common/Precompile.hpp"
#include <blamlib/Halo1/cache/cache_files.hpp>

#include <blamlib/Halo1/cache/cache_file_builder.hpp>
#include <blamlib/Halo1/cache/cache_files_globals.hpp>
#include <blamlib/Halo1/cache/cache_files_structures.hpp>
#include <blamlib/Halo1/cache/data_file_structures.hpp>
#include <YeloLib/memory/memory_interface_base.hpp>
#include <YeloLib/Halo1/cache/cache_files_yelo.hpp>
#include <YeloLib/Halo1/open_sauce/settings/yelo_shared_settings.hpp>

namespace Yelo
{
	namespace Cache
	{
#if PLATFORM_TYPE == PLATFORM_TOOL
		cstring s_build_cache_file_globals::k_temp_cache_file_name = "temporary uncompressed cache file.bin";
		cstring s_build_cache_file_globals::k_cache_file_extension = K_MAP_FILE_EXTENSION;

		DWORD s_build_cache_file_globals::GetFileSize() const
		{
			return ::GetFileSize(file_handle, nullptr);
		}

		bool s_build_cache_file_globals::WriteToFile(const void* buffer, int32 buffer_size)
		{
			DWORD bytes_written;
			BOOL result = WriteFile(file_handle, buffer, CAST(DWORD, buffer_size), &bytes_written, nullptr);

			return result != FALSE && bytes_written == CAST(DWORD, buffer_size);
		}

		bool s_build_cache_file_globals::TemporaryFileOpen(cstring filename)
		{
			file_handle = CreateFileA(filename, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

			return file_handle != INVALID_HANDLE_VALUE;
		}
		void s_build_cache_file_globals::TemporaryFileClose(cstring filename)
		{
			if (file_handle != INVALID_HANDLE_VALUE)
			{
				CloseHandle(file_handle);
				file_handle = INVALID_HANDLE_VALUE;

				DeleteFileA(filename);
			}
		}
		bool s_build_cache_file_globals::TemporaryFileCopy(cstring new_filename, cstring filename)
		{
			return CopyFileA(filename, new_filename, FALSE) != FALSE;
		}

		bool BuildCacheFileForYelo()
		{
			return TEST_FLAG(BuildCacheFileGlobals()->begin_flags, Flags::_build_cache_file_begin_building_yelo_bit);
		}

		void s_build_cache_file_globals::ScenarioNameToCacheFilePath(_Out_ std::string& cache_file_path)
		{
			cache_file_path.reserve(MAX_PATH);

			cache_file_path.append(Settings::PlatformUserMapsPath());
			cache_file_path.append(scenario_name);
			cache_file_path.append(s_build_cache_file_globals::k_cache_file_extension);
		}
#endif
	};

	namespace blam
	{
#if PLATFORM_TYPE == PLATFORM_TOOL
		using namespace Cache;

		int32 build_cache_file_size()
		{
			return Cache::BuildCacheFileGlobals()->GetFileSize();
		}

		uint32 build_cache_file_checksum()
		{
			return Cache::BuildCacheFileGlobals()->crc;
		}

		static bool compress_cache_file_data(cstring filename, cstring cache_file_path)
		{
			return true;
		}

		static void build_cache_file_error(cstring message)
		{
			YELO_ERROR_FAILURE("%s (#%d)", message, GetLastError());
			SetLastError(0);
		}

		static bool build_cache_file_write(const void* buffer, int32 buffer_size, int32* return_file_offset = nullptr)
		{
			int32 pad_size = buffer_size;
			YELO_ASSERT(pad_size >= 0 && pad_size < Enums::k_cache_file_page_size);
			YELO_ASSERT(((buffer_size + pad_size) & Enums::k_cache_file_page_size_mask) == 0);

			byte pad_buffer[Enums::k_cache_file_page_size];
			memset(pad_buffer, 0, pad_size);

			auto& build_cache_file_globals = *Cache::BuildCacheFileGlobals();

			if (!build_cache_file_globals.WriteToFile(buffer, buffer_size) ||
				!build_cache_file_globals.WriteToFile(pad_buffer, pad_size))
			{
				build_cache_file_error("couldn't write to cache file");
				return false;
			}

			if (return_file_offset != nullptr)
				*return_file_offset = build_cache_file_globals.cache_stream_size;

			build_cache_file_globals.cache_stream_size += buffer_size + pad_size;

			return true;
		}

		bool build_cache_file_begin(cstring scenario_name,
			byte_flags flags)
		{
			auto& build_cache_file_globals = *Cache::BuildCacheFileGlobals();
			YELO_ASSERT(!build_cache_file_globals.building);

			bool invalid_parameters = false;
			cstring invalid_parameters_message = "specified parameters are invalid";

			bool building_yelo = TEST_FLAG(flags, Flags::_build_cache_file_begin_building_yelo_bit);

			// TODO: validate flags
			
			if (invalid_parameters)
			{
				build_cache_file_error(invalid_parameters_message);
				return false;
			}

			s_build_cache_file_globals::k_cache_file_extension = building_yelo
				? K_MAP_FILE_EXTENSION_YELO
				: K_MAP_FILE_EXTENSION;

			if (!build_cache_file_globals.TemporaryFileOpen())
			{
				build_cache_file_error("couldn't create new cache file");
				return false;
			}

			build_cache_file_globals.building = true;
			build_cache_file_globals.begin_flags = flags;
			build_cache_file_globals.cache_stream_size = 0;
			crc_new(build_cache_file_globals.crc);
			strcpy_s(build_cache_file_globals.scenario_name, scenario_name);

			s_cache_header header;
			memset(&header, NONE, sizeof(header));

			// NOTE: engine doesn't return write()'s result, always true
			return build_cache_file_write(&header, sizeof(header));
		}

		bool build_cache_file_add_resource(const void* buffer, int32 buffer_size,
			int32* return_file_offset, bool include_in_crc)
		{
			auto& build_cache_file_globals = *Cache::BuildCacheFileGlobals();
			YELO_ASSERT(build_cache_file_globals.building);

			if (include_in_crc)
				crc_checksum_buffer(build_cache_file_globals.crc, buffer, buffer_size);

			return build_cache_file_write(buffer, buffer_size, return_file_offset);
		}

		void build_cache_file_cancel()
		{
			auto& build_cache_file_globals = *Cache::BuildCacheFileGlobals();
			if (!build_cache_file_globals.building)
				return;

			build_cache_file_globals.TemporaryFileClose();
			build_cache_file_globals.canceled = true;
		}

		bool build_cache_file_end(Cache::s_cache_header& header)
		{
			auto& build_cache_file_globals = *Cache::BuildCacheFileGlobals();
			YELO_ASSERT(build_cache_file_globals.building);

			if (SetFilePointer(build_cache_file_globals.file_handle, 0, 0, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
			{
				build_cache_file_error("couldn't seek to start of new cache file");
				return false;
			}

			if (!build_cache_file_write(&header, sizeof(header)))
			{
				build_cache_file_error("couldn't write new cache file's final header");
				return false;
			}

			std::string cache_file_path;
			build_cache_file_globals.ScenarioNameToCacheFilePath(cache_file_path);
			if (!compress_cache_file_data(s_build_cache_file_globals::k_temp_cache_file_name, cache_file_path.c_str()) ||
				!build_cache_file_globals.TemporaryFileCopy(cache_file_path.c_str()))
			{
				build_cache_file_error("couldn't output new cache file");
				return false;
			}

			build_cache_file_globals.TemporaryFileClose();
			build_cache_file_globals.building = false;

			return true;
		}
#endif
	};
};