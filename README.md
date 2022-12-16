# ZTScore

## 备注
### 尽量C++风格的C
```
    安卓为cpp
```

### 尽量使用malloc而非数组
```
    ios会有异常情况，原因不明
```

### 切记除法需要小数时*1.0

### 日志统一: define LOGD

### 关于PCM
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
![logo][logo]


[logo]:data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAAAXNSR0IArs4c6QAAAB9JREFUOE9jZKAQMFKon2HUAIbRMGAYDQNQPhr4vAAAJpgAEX/anFwAAAAASUVORK5CYII=
