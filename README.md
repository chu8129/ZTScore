# ZTScore

## 备注
### 尽量C++风格的C
```
    原因:安卓为cpp
```

### 尽量使用malloc而非数组
```
    ios会有异常情况，原因不明
```
### mac多线程会有异常
```
zgf lyric count:31, split count:0, min distance:2147483648.000, mean:69273664.000, rangeScore:2147793664.000, minScore:999999.000
zgf loopCount:1  silence_ms:110, dBFS:0.950000
zgf get not silence range, default value, lyricIndex:0, audioLength:893664, silenceMin:110, dbfs:-42.90, channels:2, sr:48000, bits:16
ExtractData(78662,0x70000c8ee000) malloc: Heap corruption detected, free list is damaged at 0x6000007580f0
*** Incorrect guard value: 1125281431592
ExtractData(78662,0x70000c8ee000) malloc: *** set a breakpoint in malloc_error_break to debug
compile_c.sh: line 9: 78662 Abort trap: 6           ./ExtractData
```

### 切记除法需要小数时*1.0

### print统一
```
define LOGD
```

### PCM解析
```
    传输是用unsight char
    储存是小端:array[2*index + 1] << 8 | array[2*index]
    由于使用的是bit, 故>if (transValue > 32767){transValue = value - 65536;}
```

### 日志vs本读读取的pcm
![image](https://github.com/chu8129/ZTScore/blob/qw/WeChatWorkScreenshot_3df7e0b7-bbd9-4f51-9d47-a3336bd6f6a1.png)
### pcm图(上图:原始pcm值,中图下图:转int16后)
![image](https://github.com/chu8129/ZTScore/blob/qw/Figure_1.png)
### 转换后的音频
[https://github.com/chu8129/ZTScore/blob/qw/pcm_0.wav](https://github.com/chu8129/ZTScore/blob/qw/pcm_0.wav)
