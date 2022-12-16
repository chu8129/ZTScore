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
[![wav](https://bkimg.cdn.bcebos.com/pic/ac6eddc451da81cb0bea2ede5d66d01609243190?x-bce-process=image/watermark,image_d2F0ZXIvYmFpa2U5Mg==,g_7,xp_5,yp_5)](https://github.com/chu8129/ZTScore/blob/qw/pcm_0.wav)
