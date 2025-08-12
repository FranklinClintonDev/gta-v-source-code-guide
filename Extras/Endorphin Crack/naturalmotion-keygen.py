# NaturalMotion NODE LOCKED keygen

import re
import struct
import math
import itertools
import ctypes
import zlib

def _encipher(v, k):
    y, z = [ctypes.c_uint32(x)
            for x in v]
    sum = ctypes.c_uint32(0)
    delta = 0x61C88647
    for n in range(32, 0, -1):
        y.value += (k[sum.value & 3] + sum.value) ^ (z.value + ((z.value >> 5) ^ (16 * z.value)))
        sum.value -= delta
        z.value += (k[(sum.value >> 11) & 3] + sum.value) ^ (y.value + ((y.value >> 5) ^ (16 * y.value)))
    return [y.value, z.value]
def _chunks(iterable, n):
    it = iter(iterable)
    while True:
        chunk = tuple(itertools.islice(it, n))
        if not chunk:
            return
        yield chunk
def _str2vec(value, l=4):
    n = len(value)
    # Split the string into chunks
    num_chunks = math.ceil(n / l)
    chunks = [value[l * i:l * (i + 1)]
              for i in range(num_chunks)]
    return [sum([character << 8 * j
                 for j, character in enumerate(chunk)])
            for chunk in chunks]
def _vec2str(vector, l=4):
    return bytes((element >> 8 * i) & 0xff
                 for element in vector
                 for i in range(l))
def encrypt(plaintext, key):
    if not plaintext:
        return b''
    v = _str2vec(plaintext.encode() if plaintext is str else plaintext)
    return b''.join(_vec2str(_encipher(chunk, key))
                        for chunk in _chunks(v, 2))

machine_id = input("Machine ID: ").strip()
if not machine_id or not re.match(r"[\d\w]{2}\-[\d\w]{2}\-[\d\w]{2}\-[\d\w]{2}", machine_id):
    raise ValueError("Invalid Machine ID. Example format: 00-00-00-00")
machine_id = [int(a, 16) for a in machine_id.split("-")][::-1]
machine_id[1], machine_id[2] = machine_id[2], machine_id[1] # byteswap

header = 0x323a6d4e # 'Nm:2'
data = 1 | 2 | 4 # enabled features?
iat = 0x00000101 # HASH1 YEAR MON DAY
exp = 0x00FF0101 # HASH2 YEAR MON DAY

serial_data = [
    header,
    data,
    iat,
    exp
]
serial_data_encoded = bytearray(struct.pack('4i', *serial_data))

# byteswap
serial_data_encoded[3] ^= serial_data_encoded[13]
serial_data_encoded[2] ^= serial_data_encoded[12]
serial_data_encoded[1] ^= serial_data_encoded[9]
serial_data_encoded[0] ^= serial_data_encoded[8]

# set Adler hash (referenced as CRC)
hsh = zlib.adler32(serial_data_encoded) & 0xFFFFFFFF
serial_data_encoded[11] = ((hsh & 0xFF00) >> 8) ^ (hsh & 0xFF)
serial_data_encoded[15] = ((hsh & 0xFF000000) >> 24) ^ ((hsh & 0xFF0000) >> 16)

# encrypt
encrypted_serial_data = encrypt(serial_data_encoded, machine_id)

# output as serial
serial = ""
for i in range(len(encrypted_serial_data)):
    serial += ('%02X' % encrypted_serial_data[i])
    if i < 15:
        serial += "-"
print("Serial:", serial)