#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <sys/types.h> 
#include <string.h> 
#include <assert.h>
#include <ctype.h>

typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned long ulong;

typedef struct {
	word orig;
	word mapped;
} MapDef;

const MapDef map_def[16] = {
	{'A', 'T'},
	{'C', 'G'},
	{'G', 'C'},
	{'T', 'A'},
	{'U', 'A'},
	{'M', 'K'},
	{'R', 'Y'},
	{'W', 'W'},
	{'S', 'S'},
	{'Y', 'R'},
	{'K', 'M'},
	{'V', 'B'},
	{'H', 'D'},
	{'D', 'H'},
	{'B', 'V'},
	{'N', 'N'},
};

typedef word *MapDouble;
typedef byte *MapSingle;

static int
is_valid_char(const byte c1)
{
	return (c1 != '\n' && c1 != '>');
}


static void
init_maps(MapDouble map_double, MapSingle map_single)
{
	for (int i1 = 0; i1 < 256; ++i1)
	{
		map_double[ i1*256 | (word)'\n' ] = 0xFF;
		map_double[ i1 | (word)'\n' * 256 ] = 0xFF;

		map_double[ i1*256 | (word)'>' ] = 0x1FF;
		map_double[ i1 | (word)'>' * 256 ] = 0x1FF;
	}

	for (int i1 = 0; i1 < 32; ++i1)
	{
		for (int i2 = 0; i2 < 32; ++i2)
		{
			word c1 = i1 < 16 ? map_def[i1].orig : tolower(map_def[i1 % 16].orig);
			word c2 = i2 < 16 ? map_def[i2].orig : tolower(map_def[i2 % 16].orig);
			word charcode = map_def[i1 % 16].mapped * 256 | map_def[i2 % 16].mapped;
			map_double[ c1 | c2 * 256 ] = charcode;
		}
	}
	for (int i = 0; i < 32; ++i)
	{
		byte c = i < 16 ? map_def[i].orig : tolower(map_def[i % 16].orig);
		map_single[ c ] = map_def[i % 16].mapped;
	}
}

#define BUFFER_SIZE 2 * 1024 * 1024

typedef struct {
	ulong first;
	byte data[BUFFER_SIZE];
} buffer2_item;

#define BUFFER3_SIZE 16*1024*1024 // enough for 32G data
typedef buffer2_item **buffer3_item;
int last_allocated_buffer2 = -1;

byte buffer[BUFFER_SIZE];

char section_name[256];
ulong section_name_len = 0;

byte out_buffer[BUFFER_SIZE*2];
ulong out_buffer_size = 0;

buffer3_item buffer3;

ulong total_written = 0;
ulong total_read = 0;

#define unlikely(x)  __builtin_expect((x), 0)

static void
memcpy_overlapped(byte *dest, const byte *source, size_t count)
{
	for (ulong i = 0; i < count; ++i)
		*(dest + i) = *(source + i);
}

static void
out_last_buffer(ulong last_buffer)
{
	write(STDOUT_FILENO, section_name, section_name_len + 1);
	int modulo = 0;
	for (ulong buffer_index = last_buffer; buffer_index > 0; --buffer_index)
	{
		buffer2_item *current_buffer = buffer3[ buffer_index - 1 ];
		int buffer_len = &current_buffer->data[BUFFER_SIZE] - &current_buffer->data[current_buffer->first] ;
		if (buffer_len <= 0)
			continue;

		ulong newlines = 0;
		int modulo_offset = 0;
		byte *in_ptr = &current_buffer->data[current_buffer->first];
		byte *out_ptr = &out_buffer[0];
		if (modulo > 0)
		{
			int cnt = 60 - modulo;
			modulo_offset = 60 - modulo;
			if (cnt > buffer_len)
				cnt = buffer_len;
			for (word j = 0; j < cnt; ++j)
				out_ptr[j] = in_ptr[j];
			out_ptr[cnt] = '\n';
			newlines ++;
			in_ptr += modulo_offset;
			out_ptr += modulo_offset + 1;
			buffer_len -= modulo_offset;
		}
		for (int item = 0; item <= buffer_len/60 ; ++item)
		{
			byte *output =  &out_ptr[item*61];
			byte *input = &in_ptr[item*60];
			if (buffer_len <= item*60)
				break;
			int cnt = buffer_len - item*60;
			if (cnt > 60)
				cnt = 60;
			for (word j = 0; j < cnt; ++j)
			{
				output[j] = input[j];
			}
			if (cnt == 60 || buffer_index == 1) // full line or last line
			{
				out_ptr[item*61 + cnt] = '\n';
				newlines ++;
			}
		}

		total_written += write(STDOUT_FILENO, out_buffer, buffer_len + modulo_offset + newlines );

		modulo = buffer_len % 60;
	}
}

int
main() 
{
	MapDouble map_double;
	MapSingle map_single;

	ulong bytes_read = 0;
	ssize_t rslt;
	map_double = malloc(256*256 * sizeof(word));
	map_single = malloc(256 * sizeof(byte));
	init_maps(map_double, map_single);

	buffer3 = malloc(sizeof(buffer2_item*) * BUFFER3_SIZE);

	int last_buffer = 0;
	int is_start = 1;
	int is_restart = 0;

	do { // main loop
		if (! is_restart)
		{
			is_restart = 0;
			rslt = read(STDIN_FILENO, buffer, BUFFER_SIZE);
			if (rslt <= 0)
				break;
			total_read += bytes_read;
			bytes_read = rslt;
		}
		is_restart = 0;
		ulong start = 0;

		if (is_start)
		{
			is_start = 0;
			char *cbuffer = (char *) buffer;
			char *lineEnd = strchr(cbuffer, '\n');
			assert(lineEnd);
			section_name_len = lineEnd - cbuffer;
			strncpy(section_name, cbuffer, section_name_len);
			section_name[section_name_len] = '\n';
			
			start = section_name_len + 1;
		}

		if (last_buffer > last_allocated_buffer2)
		{
			buffer3[last_buffer] = malloc(sizeof(buffer2_item));
			last_allocated_buffer2 = last_buffer;
		}
		byte *current_buffer = &buffer3[last_buffer]->data[0];
		word *output = (unsigned short *) &buffer3[last_buffer]->data[0];
		ulong last = BUFFER_SIZE/2 - 1;
		ulong last_valid_char = 0;
		ulong i;
		for (i = start; i <= bytes_read - 2; i+=2, last-=1) // main mapping loop
		{
			word val = map_double[ *(word *) &buffer[i] ];

			if (unlikely((val & 0xFF) == 0xFF ))
			{
				// line return or ">" sign detected, take a slow path
				byte idx = 0;
				char *tmp_in = (char *) &buffer[i];
				char *tmp_out = (char *) &output[last-1];
				for (byte in = 0; in < 4 && i + in < bytes_read; ++in) // val & 0xFF loop
				{
					if (tmp_in[in] == '>')
					{
						is_restart = 1;
						is_start = 1;
						bytes_read = bytes_read - i - in;
						// last word written in main loop was output[last+1]
						ulong last_byte = last*2 + 2 - idx;
						buffer3[last_buffer]->first = (&current_buffer[last_byte] - &current_buffer[0]) ;
						out_last_buffer(last_buffer+1);
						memcpy_overlapped(buffer, &buffer[i+in], bytes_read);
						last_buffer = 0;
						break; // val & 0xFF loop
					}
					else if (tmp_in[in] != '\n')
					{
						byte val = tmp_in[in];
						tmp_out[3-idx] = map_single[ val ];
						idx++;
					}
				}
				if (is_restart)
					break; // main mapping loop

				assert(idx != 4);
				assert(idx >= 2 || i+3 >= bytes_read);

				last_valid_char = last*2 + 2 - idx;
				i += (4 - idx);
			}
			else
			{
				output[last] = val;
			}
		}
		if (is_restart) 
			continue; // main loop

		ulong last_byte;
		if (i > bytes_read && last_valid_char > 0)
		{
			last_byte = last_valid_char;
		}
		else if (i == bytes_read - 1 && is_valid_char(buffer[bytes_read - 1]) )
		{
			last_byte = last*2 + 1;
			byte val = buffer[bytes_read - 1];
			buffer3[last_buffer]->data[last*2 + 1] = map_single[ val ];
		}
		else
			last_byte = last*2 + 2;
	
		buffer3[last_buffer]->first = (&current_buffer[last_byte] - &current_buffer[0]) ;
		
		last_buffer ++;

	} while (bytes_read > 0);

	out_last_buffer(last_buffer);

	return 0;
}
