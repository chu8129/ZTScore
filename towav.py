import io
import sys
import numpy
import struct
import warnings
import sys
import librosa
from matplotlib import pyplot as plt

class WAVE_FORMAT:
    PCM = 0x0001
    IEEE_FLOAT = 0x0003

def _array_tofile(fid, data):
    # ravel gives a c-contiguous buffer
    wd = data.ravel().view('b')
    print("py save value", wd[:20])
    print("write real data length", len(wd))
    fid.write(wd.data)

def write(filename, rate, data):
    print("wait write data length", len(data.flatten()))
    if hasattr(filename, "write"):
        fid = filename
    else:
        fid = open(filename, "wb")
    print("sample rate", rate)
    fs = rate
    try:
        dkind = data.dtype.kind
        print("dkind", dkind)
        if not (dkind == "i" or dkind == "f" or (dkind == "u" and data.dtype.itemsize == 1)):
            raise ValueError("Unsupported data type '%s'" % data.dtype)

        header_data = b""
        header_data += b"RIFF"
        header_data += b"\x00\x00\x00\x00"
        header_data += b"WAVE"
        header_data += b"fmt "
        if dkind == "f":
            format_tag = WAVE_FORMAT.IEEE_FLOAT
        else:
            format_tag = WAVE_FORMAT.PCM
        if data.ndim == 1:
            channels = 1
        else:
            channels = data.shape[1]
        print("channels", channels)
        bit_depth = data.dtype.itemsize * 8
        bytes_per_second = fs * (bit_depth // 8) * channels
        block_align = channels * (bit_depth // 8)

        fmt_chunk_data = struct.pack("<HHIIHH", format_tag, channels, fs, bytes_per_second, block_align, bit_depth)
        if not (dkind == "i" or dkind == "u"):
            fmt_chunk_data += b"\x00\x00"

        header_data += struct.pack("<I", len(fmt_chunk_data))
        header_data += fmt_chunk_data

        if not (dkind == "i" or dkind == "u"):
            header_data += b"fact"
            header_data += struct.pack("<II", 4, data.shape[0])

        if ((len(header_data) - 4 - 4) + (4 + 4 + data.nbytes)) > 0xFFFFFFFF:
            raise ValueError("Data exceeds wave file size limit")

        fid.write(header_data)

        fid.write(b"data")
        fid.write(struct.pack("<I", data.nbytes))
        print("data.dtype.byteorder", data.dtype.byteorder, "sys.byteorder", sys.byteorder)
        if data.dtype.byteorder == ">" or (data.dtype.byteorder == "=" and sys.byteorder == "big"):
            data = data.byteswap()
        _array_tofile(fid, data)

        size = fid.tell()
        fid.seek(4)
        fid.write(struct.pack("<I", size - 8))

    finally:
        if not hasattr(filename, "write"):
            fid.close()
        else:
            fid.seek(0)



filename = sys.argv[1]
channel = int(sys.argv[2])
sampleRate = int(sys.argv[3])
bit = int(sys.argv[4])

lines = open(filename).readlines()
lines = [int(line) for line in lines]
print("raw pcm file", lines[:40])
print("raw pcm", lines[::2][:20])
#elines = [(lines[2 * index + 1] << 8) + lines[2 * index] for index in range(int(len(lines) / 2))]
import numpy
elines = numpy.array(lines).reshape(-1,channel)
elines = (elines / (2**bit-1) * (2**15)).astype(int)


# write(filename + ".wav", 48000, numpy.array(elines).reshape((-1, 2)).astype(numpy.int16))
write(filename + ".wav", sampleRate, numpy.array(elines).astype(numpy.int16))


plt.figure(figsize=(18, 8), dpi=100)
plt.subplots_adjust(wspace=1, hspace=1)

plt.subplot(3, 1, 1)
plt.plot(numpy.arange(0, elines.shape[0]) * (1.0 / sampleRate), elines[:,0])
plt.title("raw pcm")

def displayWaveform(filepath, plt):
    samples, sr = librosa.load(filepath, sr=None, mono=False)
    samples = numpy.array([samples]) if len(samples.shape) == 1 else samples
    print("samples", samples.shape)
    time = numpy.arange(0, len(samples[0]))  * (1.0 / sr)
    return samples, time

samples, time = displayWaveform(filename + ".wav", plt)
print("read py save wav file", list(samples[0][:20] * 2 ** 15))

plt.subplot(3, 1, 2)
plt.plot(time, samples[0])
plt.title("py:0 channel")
plt.xlabel("time(S)")
plt.ylabel("zf")

if samples.shape[0] >1:
    plt.subplot(3, 1, 3)
    plt.plot(time, samples[1])
    plt.title("py:1 channel")
    plt.xlabel("time(S)")
    plt.ylabel("zf")

plt.draw()
# plt.show()
# input("close?")
