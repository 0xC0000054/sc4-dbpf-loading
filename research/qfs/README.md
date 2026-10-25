# QFS decoder research

The tools used to profile and test the QFS decoder in `src/qfs-decoder`. They are not part of the DLL build.

- `tools/qfs_experiment.py` is the first Python DBPF/QFS scanner, used for an inventory of the compressed entries in a Plugins folder.
- `tools/qfs_native_bench.cpp` has a decoder that follows the game's code, the first AVX2 decoder, the hybrid decoder that ended up in the DLL, and the DBPF scanner they share.
- `tools/qfs_native_bench_windowsish.cpp`, `tools/qfs_native_bench_simd.cpp` and `tools/qfs_native_bench_hybrid.cpp` build `qfs_native_bench.cpp` with one decoder each.
- `tools/qfs_native_profile.cpp` checks that the three decoders give the same output byte for byte, reports invalid streams, and shows the distribution of the QFS commands and match lengths.

## Results

Measured on 2,190,096 FSH entries from my Plugins folder, 7.96 GB compressed and 30.34 GB uncompressed.

| Decoder | Total decode time | vs. game | vs. SIMD |
| --- | ---: | ---: | ---: |
| Game (`/Os /Ob0`) | 22.34 s | 1.00x | - |
| SIMD | 17.59 s | 1.27x | 1.00x |
| Hybrid | 9.84 s | 2.27x | 1.79x |

The SIMD decoder calls an AVX2 copy function for every match. Most FSH matches are 3 to 15 bytes, so the hybrid decoder copies those inline and only calls the AVX2 copy for matches over 31 bytes.

## Building

From an x86 Visual Studio Developer PowerShell, because SimCity 4 is a 32-bit process:

```powershell
cd research\qfs\tools

cl /nologo /std:c++20 /EHsc /Os /Ob0 /Oy- qfs_native_bench_windowsish.cpp /Fe:qfs_native_bench_windowsish.exe
cl /nologo /std:c++20 /EHsc /O2 /arch:AVX2 qfs_native_bench_simd.cpp /Fe:qfs_native_bench_simd.exe
cl /nologo /std:c++20 /EHsc /O2 /arch:AVX2 qfs_native_bench_hybrid.cpp /Fe:qfs_native_bench_hybrid.exe
cl /nologo /std:c++20 /EHsc /O2 /arch:AVX2 qfs_native_profile.cpp /Fe:qfs_native_profile.exe
```

The profiler needs a CPU with AVX2.

## Running

Both tools take `.dat` files or folders, which are scanned recursively.

```powershell
.\qfs_native_profile.exe --repeats=3 'D:\path\to\SimCity 4\Plugins'
.\qfs_native_profile.exe --scan-errors 'D:\path\to\SimCity 4\Plugins'
uv run .\qfs_experiment.py 'D:\path\to\SimCity 4\Plugins' --repeats=3
```

`--scan-errors` only checks the streams and prints the details of any invalid ones.

The parsers in these tools do not check their input as carefully as they would need to for untrusted files.
