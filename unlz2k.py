""" Python implementation of the extraction algorithm used by TT Games """

import os, sys, time

########
# DATA #
########

tmp_src = None # Used by load_into_bitstream
tmp_src_size = None # Used by load_into_bitstream
bitstream = None
last_byte_read = None
previous_bit_align = None
chunks_with_current_setup = None
read_offset = None
literals_to_copy = None
tmp_chunk = None
byte_dict0 = None # 510 byte dictionary
byte_dict1 = None # 20 byte dictionary before tempChunk
word_dict0 = None # 256 word dictionary
word_dict1 = None # 1024 word dictionary
word_dict2 = None # 1024 word dictionary
word_dict3 = None # 4096 word dictionary

#############
# FUNCTIONS #
#############

def memset(src, src_offset, value, length):
    for i in range(length):
        src[src_offset + i] = value

def load_into_bitstream(bits):
    global bitstream
    global previous_bit_align
    global last_byte_read
    global tmp_src
    global tmp_src_size
    bitstream <<= bits
    last_op_diff = previous_bit_align
    data = last_byte_read
    if bits > last_op_diff:
        while bits > last_op_diff:
            bits -= last_op_diff
            data <<= bits
            bitstream |= data
            if tmp_src_size == 0:
                data = 0
            else:
                tmp_src_size -= 1
                data = int.from_bytes(tmp_src.read(1), byteorder="little")
            last_op_diff = 8
        last_byte_read = data
    last_op_diff -= bits
    bits = last_op_diff
    data >>= bits
    previous_bit_align = last_op_diff
    bitstream |= data
    # 32 bit align
    bitstream &= 0xFFFFFFFF

def process_dicts(val0, dict0, val1, dict1):
    global word_dict1
    global word_dict2
    tmp_src_dict = dict.fromkeys(range(17), 0)
    tmp_dest_dict = dict.fromkeys(range(18), 0)
    tmp_dest_dict[1] = 0
    for i in range(val0):
        tmp = dict0[i]
        tmp_src_dict[tmp] += 1
    shift = 14
    ind = 1
    while ind <= 16:
        low = tmp_src_dict[ind]
        high = tmp_src_dict[ind + 1]
        low <<= shift + 1
        high <<= shift
        low += tmp_dest_dict[ind]
        ind += 4
        high += low
        high &= 0xFFFF
        tmp_dest_dict[ind - 3] = low
        low = tmp_src_dict[ind - 2]
        low <<= shift - 1
        tmp_dest_dict[ind - 2] = high
        low += high
        low &= 0xFFFF
        high = tmp_src_dict[ind - 1]
        high <<= shift - 2
        tmp_dest_dict[ind - 1] = low
        high += low
        high &= 0xFFFF
        tmp_dest_dict[ind] = high
        shift -= 4
    if tmp_dest_dict[17] != 0:
        print('Bad table')
        exit(1)
    shift = val1 - 1
    tmp_val = 16 - val1
    var_5c = tmp_val
    for i in range(1, val1 + 1):
        tmp_dest_dict[i] >>= tmp_val
        tmp_src_dict[i] = 1 << shift
        shift -= 1
    tmp_val = 16 - (val1 + 1)
    for i in range(val1 + 1, 17):
        tmp_src_dict[i] = 1 << tmp_val
        tmp_val -= 1
    comp1 = tmp_dest_dict[val1 + 1]
    comp1 >>= 16 - val1
    if comp1 != 0:
        comp2 = 1 << val1
        if comp1 != comp2:
            for i in range(comp1, comp2):
                dict1[i] = 0
    if val0 <= 0:
        return
    shift = 15 - val1
    mask = 1 << shift
    tmp_val0 = val0
    for i in range(0, val0):
        ind = dict0[i]
        if (ind != 0):
            tmp_dest_val = tmp_dest_dict[ind]
            tmp_src_val = tmp_src_dict[ind]
            tmp_src_val += tmp_dest_val
            if ind > val1:
                tmp_dict = dict1
                tmp_offs = tmp_dest_val >> var_5c
                new_len = ind - val1
                if new_len != 0:
                    while new_len != 0:
                        if tmp_dict[tmp_offs] == 0:
                            word_dict1[tmp_val0] = 0
                            word_dict2[tmp_val0] = 0
                            tmp_dict[tmp_offs] = tmp_val0
                            tmp_val0 += 1
                        tmp_offs = tmp_dict[tmp_offs]
                        if tmp_dest_val & mask == 0:
                            tmp_dict = word_dict1
                        else:
                            tmp_dict = word_dict2
                        tmp_dest_val += tmp_dest_val
                        new_len -= 1
                tmp_dict[tmp_offs] = i
            elif tmp_dest_val < tmp_src_val:
                for j in range(tmp_dest_val, tmp_src_val):
                    dict1[j] = i
            tmp_dest_dict[ind] = tmp_src_val

def setup_byte_and_word_dicts(length, bits, special_ind):
    global bitstream
    global byte_dict1
    global word_dict0
    tmp_val = bitstream >> (32 - bits) # bits is never 0
    load_into_bitstream(bits)
    if tmp_val != 0:
        tmp_val2 = 0
        if tmp_val > 0:
            while tmp_val2 < tmp_val:
                tmp_bitstream = bitstream >> 29
                bits = 3
                if tmp_bitstream == 7:
                    mask = 0x10000000
                    if bitstream & mask == 0:
                         bits = 4
                    else:
                        counter = 0
                        while bitstream & mask != 0:
                            mask >>= 1
                            counter += 1
                        bits = counter + 4
                        tmp_bitstream += counter
                load_into_bitstream(bits)
                byte_dict1[tmp_val2] = tmp_bitstream
                tmp_val2 += 1
                if tmp_val2 == special_ind:
                    special_len = bitstream >> 30
                    load_into_bitstream(2)
                    if special_len >= 1:
                        memset(byte_dict1, tmp_val2, 0, special_len)
                        tmp_val2 += special_len
        if tmp_val2 < length:
            memset(byte_dict1, tmp_val2, 0, length - tmp_val2)
        process_dicts(length, byte_dict1, 8, word_dict0)
        return
    tmp_val = bitstream >> (32 - bits)
    load_into_bitstream(bits)
    if length > 0:
        memset(byte_dict1, 0, 0, length)
    for i in range(256):
        word_dict0[i] = tmp_val

def setup_byte_dict0():
    global bitstream
    global byte_dict0
    global byte_dict1
    global word_dict0
    global word_dict1
    global word_dict2
    global word_dict3
    tmp_val = bitstream >> 23
    load_into_bitstream(9)
    if tmp_val == 0:
        tmp_val2 = bitstream >> 23
        load_into_bitstream(9)
        memset(byte_dict0, 0, 0, 510)
        for i in range(4096):
            word_dict3[i] = tmp_val2
        return
    num_bytes = 0
    if tmp_val > 0: # always?
        while num_bytes < tmp_val:
            tmp_len = bitstream >> 24
            tmp_val2 = word_dict0[tmp_len]
            if tmp_val2 >= 19:
                mask = 0x800000
                while tmp_val2 >= 19:
                    if mask & bitstream == 0:
                        tmp_val2 = word_dict1[tmp_val2]
                    else:
                        tmp_val2 = word_dict2[tmp_val2]
                    mask >>= 1
            bits = byte_dict1[tmp_val2]
            load_into_bitstream(bits)
            if tmp_val2 > 2:
                tmp_val2 -= 2
                byte_dict0[num_bytes] = tmp_val2
                num_bytes += 1
            else:
                if tmp_val2 == 0:
                    tmp_len = 1
                else:
                    if tmp_val2 != 1:
                        tmp_val2 = bitstream >> 23
                        load_into_bitstream(9)
                        tmp_len = tmp_val2 + 20
                    else:
                        tmp_val2 = bitstream >> 28
                        load_into_bitstream(4)
                        tmp_len = tmp_val2 + 3
                if tmp_len > 0:
                    memset(byte_dict0, num_bytes, 0, tmp_len)
                    num_bytes += tmp_len
        if num_bytes < 510:
            memset(byte_dict0, num_bytes, 0, 510 - num_bytes)
        process_dicts(510, byte_dict0, 12, word_dict3)
        return
    memset(byte_dict0, 0, 0, 510)
    process_dicts(510, byte_dict0, 12, word_dict3)
    return

def decode_bitstream():
    global chunks_with_current_setup
    global bitstream
    global word_dict1
    global word_dict2
    global word_dict3
    if chunks_with_current_setup == 0:
        chunks_with_current_setup = bitstream >> 16
        load_into_bitstream(16)
        setup_byte_and_word_dicts(19, 5, 3)
        setup_byte_dict0()
        setup_byte_and_word_dicts(14, 4, -1)
    chunks_with_current_setup -= 1
    tmp_val = word_dict3[bitstream >> 20]
    if tmp_val >= 510:
        mask = 0x80000
        while (tmp_val >= 510):
            if bitstream & mask == 0:
                tmp_val = word_dict1[tmp_val]
            else:
                tmp_val = word_dict2[tmp_val]
            mask >>= 1
    bits = byte_dict0[tmp_val]
    load_into_bitstream(bits)
    return tmp_val

def decode_bitstream_for_literals():
    global bitstream
    global byte_dict1
    global word_dict0
    global word_dict1
    global word_dict2
    tmp_offs = bitstream >> 24
    tmp_val = word_dict0[tmp_offs]
    if tmp_val >= 14:
        mask = 0x800000
        while tmp_val >= 14:
            if bitstream & mask == 0:
                tmp_val = word_dict1[tmp_val]
            else:
                tmp_val = word_dict2[tmp_val]
            mask >>= 1
    bits = byte_dict1[tmp_val]
    load_into_bitstream(bits)
    if tmp_val == 0:
        return 0
    if tmp_val == 1:
        return 2
    tmp_val -= 1
    tmp_bitstream = bitstream >> (32 - tmp_val)
    load_into_bitstream(tmp_val)
    return tmp_bitstream + (1 << tmp_val)

def read_and_decrypt(length, dest):
    global read_offset
    global literals_to_copy
    output_offs = 0
    tmp_read_offs = read_offset
    literals_to_copy -= 1
    tmp_to_copy = literals_to_copy
    if tmp_to_copy >= 0:
        while tmp_to_copy >= 0:
            dest[output_offs] = dest[tmp_read_offs]
            output_offs += 1
            tmp_read_offs += 1
            tmp_read_offs &= 0x1FFF
            if output_offs == length:
                literals_to_copy = tmp_to_copy
                read_offset = tmp_read_offs
                return
            tmp_to_copy -= 1
        literals_to_copy = tmp_to_copy
        read_offset = tmp_read_offs
    while output_offs < length:
        tmp_val = decode_bitstream()
        if tmp_val <= 255:
            dest[output_offs] = tmp_val
            output_offs += 1
            if output_offs == length:
                return
        else:
            tmp_to_copy = decode_bitstream_for_literals()
            tmp_read_offs = output_offs - tmp_to_copy - 1
            tmp_to_copy = tmp_val - 254
            tmp_read_offs &= 0x1FFF
            read_offset = tmp_read_offs
            literals_to_copy = tmp_to_copy
            while tmp_to_copy >= 0:
                dest[output_offs] = dest[tmp_read_offs]
                output_offs += 1
                tmp_read_offs += 1
                tmp_read_offs &= 0x1FFF
                read_offset = tmp_read_offs
                if output_offs == length:
                    return
                tmp_to_copy -= 1
                literals_to_copy = tmp_to_copy
    if output_offs > length:
        print('Error: went farther than given length')

def copy_to_file(file, src, length):
    """ Writes bytearray data of given length to file """
    file.write(src[:length])

def unlz2k_chunk(src, dest, src_size, dest_size):
    if (dest_size == 0):
        return 0
    # Shared variables
    global tmp_src
    tmp_src = src
    global tmp_src_size
    tmp_src_size = src_size
    global bitstream
    bitstream = 0
    global last_byte_read
    last_byte_read = 0
    global previous_bit_align
    previous_bit_align = 0
    global chunks_with_current_setup
    chunks_with_current_setup = 0
    global read_offset
    read_offset = 0
    global literals_to_copy
    literals_to_copy = 0
    global tmp_chunk
    tmp_chunk = bytearray(8192)
    # Shared dicts
    global byte_dict0
    byte_dict0 = dict.fromkeys(range(510), 0)
    global byte_dict1
    byte_dict1 = dict.fromkeys(range(20), 0)
    global word_dict0
    word_dict0 = dict.fromkeys(range(256), 0)
    global word_dict1
    word_dict1 = dict.fromkeys(range(1024), 0)
    global word_dict2
    word_dict2 = dict.fromkeys(range(1024), 0)
    global word_dict3
    word_dict3 = dict.fromkeys(range(4096), 0)
    # Local variables
    bytes_left = dest_size
    bytes_written = 0
    
    load_into_bitstream(32)
    while (bytes_left != 0):
        chunk_size = bytes_left if bytes_left < 8192 else 8192
        read_and_decrypt(chunk_size, tmp_chunk)
        copy_to_file(dest, tmp_chunk, chunk_size)
        bytes_written += chunk_size
        bytes_left -= chunk_size
    return bytes_written

def unlz2k(src, dest, src_size, dest_size):
    bytes_written = 0
    while bytes_written < dest_size:
        lz2k = src.read(4)
        if (lz2k != b'LZ2K'):
            print('Not valid LZ2K file or chunk at {:<6X}'.format(src.tell()))
            exit(1)
        unpacked = int.from_bytes(src.read(4), byteorder="little")
        packed = int.from_bytes(src.read(4), byteorder="little")
        bytes_written += unlz2k_chunk(src, dest, packed, unpacked)
    bytes_read = src.tell() - original_src
    if bytes_read != src_size:
        print('Size mismatch, got {:<8X} but read {:<8X}'.format(src_size, bytes_read))
        exit(1)
    if bytes_written != dest_size:
        print('Size mismatch, got {:<8X} but wrote {:<8X}'.format(dest_size, bytes_written))
        exit(1)
    print('Done.')

# Does not check bytes read or written
def unlz2k(src, dest):
    src.seek(0, 2)
    end = src.tell()
    src.seek(0)
    while src.tell() < end:
        lz2k = src.read(4)
        if (lz2k != b'LZ2K'):
            print('Not valid LZ2K file or chunk at {:<6X}'.format(src.tell()))
            exit(1)
        unpacked = int.from_bytes(src.read(4), byteorder="little")
        packed = int.from_bytes(src.read(4), byteorder="little")
        unlz2k_chunk(src, dest, packed, unpacked)
    print('Done.')

################
# TEST PROGRAM #
################

""" if len(sys.argv) < 2:
    print('Please specify a filename.')
    exit(1)
filename = sys.argv[1]
src = open(filename, 'rb')
dest = open(filename + '.dec', 'wb')
unlz2k(src, dest)
src.close()
dest.close()
"""
