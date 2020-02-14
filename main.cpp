#include <array>
#include <cstdint>
#include <experimental\filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <sstream>
#include <vector>
#include <windows.h>

typedef int8_t   i8;

typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

// searches for a group of bytes within a buffer, if it finds them; it returns the address at which they're located
template< typename t = u32 >
t pattern_search( const std::vector< u8 >& buffer, std::string signature, std::ptrdiff_t offset = 0, u32 start_addr = 4096, u32 end_addr = 0 )
{
	IMAGE_NT_HEADERS*		nt = ( IMAGE_NT_HEADERS* ) ( ( u32 ) buffer.data( ) + ( ( IMAGE_DOS_HEADER* ) buffer.data( ) )->e_lfanew );
	std::istringstream		bytes = std::istringstream( signature );
	u32						last_deleted = 0;

	if ( !end_addr )
		end_addr = nt->OptionalHeader.BaseOfCode + nt->OptionalHeader.SizeOfCode;

	// '? ? ? ?' -> '?? ??'
	if ( signature.find( "? ? " ) != std::string::npos )
	{
		for ( u32 i = 0; i < signature.length( ); ++i )
		{
			if ( i - last_deleted > 1 && signature[i - 1] == '?' && signature[i] == ' ' )
			{
				signature.erase( signature.begin( ) + i );
				last_deleted = i;
			}
		}
	}

	std::vector< u8 > pattern;
	for ( auto byte = std::istream_iterator< std::string >( bytes ); byte != std::istream_iterator< std::string >( ); ++byte )
		pattern.push_back( ( u8 ) std::strtoul( byte->c_str( ), 0, 16 ) );

	for ( u32 i = start_addr; i < end_addr; ++i )
	{
		const u8* byte = &buffer[i];

		// no match
		if ( byte[0] != pattern[0] )
			continue;

		bool found = true;
		for ( u32 n = 0; n < pattern.size( ); ++n )
		{
			if ( byte[n] != pattern[n] && pattern[n] != 0x00 )
				found = false;
		}

		if ( found )
			return ( t ) ( i + offset );
	}

	return ( t ) 0;
}

int main( )
{
	for ( auto& entry : std::experimental::filesystem::directory_iterator( std::experimental::filesystem::current_path( ) ) )
	{
		if ( entry.path( ).filename( ).string( ) != "tw3_vista.exe" )
			continue;

		constexpr u32 flag = std::ios::binary | std::ios::in | std::ios::out;

		std::basic_fstream< u8 >	file( entry, flag );
		std::basic_fstream< u8 >	copy( std::experimental::filesystem::current_path( ).string( ) + "/tw3_vista_backup.exe", flag & ~std::ios::in );
		std::vector< u8 >			buffer( std::istreambuf_iterator< u8 >( file ), {} );

		/*
			 during the 'combat' routine, if the focused window isn't tradewinds legends, the game will execute this code, pausing the game, very annoying
			.text:00452D08 E8 D3 79 FB FF	call    pause_in_combat
		*/
		const u32 combat_pause_call = pattern_search( buffer, "E8 ? ? ? ? 8B 8E ? ? ? ? 85 C9 74 05", 0xf );

		if ( combat_pause_call && combat_pause_call < buffer.size( ) )
		{
			printf( "$> found combat pause call\n" );

			printf( "   > creating backup...\n" );
			{
				std::copy( buffer.begin( ), buffer.end( ), std::ostreambuf_iterator< u8 >( copy ) );
				copy.close( );
			}

			printf( "   > patching...\n" );
			{
				// replace call instruction with NOPs
				for ( u8 i = 0; i <= 4; ++i )
					buffer[combat_pause_call + i] = 0x90;
			}

			// write patch to game
			{
				file.close( );
				file.open( entry, flag | std::ios::trunc );
				std::copy( buffer.begin( ), buffer.end( ), std::ostreambuf_iterator< u8 >( file ) );
				file.close( );
			}
		}

		// edit game config
		{
			std::string config_file = getenv( "userprofile" );
			config_file += "\\saved games\\sandlot games\\tradewinds legends\\settings.dat";

			file.open( config_file, flag );
			buffer = { std::istreambuf_iterator< u8 >( file ), {} };

			// set dialogue speed to max, i think you could alter another entry for instant sailing, but that's cheating !
			buffer[20] = 0x7f;

			// write edit to config
			{
				file.close( );
				file.open( config_file, flag | std::ios::trunc );
				std::copy( buffer.begin( ), buffer.end( ), std::ostreambuf_iterator< u8 >( file ) );
				file.close( );
			}


			printf( "$> set dialogue speed to max\n" );
		}

		printf( "$> complete\n" );
	}

	system( "pause" );
}